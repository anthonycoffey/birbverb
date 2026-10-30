// SpringLine — SPEC-001, one modelled spring.
//
// The tank-level criteria (spring mismatch, the secondary series, the summed
// echo train) belong to SpringTank and stay in test_dispersion.cpp. What is
// asserted here is what ONE spring has to do: repeat at its echo spacing,
// decay at the requested RT60, decay faster at the top than the bottom, lose
// the main series above F_C, and stay bounded.
//
// Requirement 7 is the one to watch. It is not a tone control being checked —
// it is a structural tension. Group delay peaks at F_C, so energy there makes
// roughly half as many round trips per second, which on its own would make
// F_C decay SLOWER than the rest of the band. The band limit has to overcome
// that before it can start making the top decay faster.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/Approx.h"
#include "support/Generate.h"
#include "support/Measure.h"

#include "dsp/SpringLine.h"
#include "dsp/TankConfig.h"

#include <algorithm>
#include <vector>

using namespace bv::test;
using Catch::Matchers::WithinRel;

namespace
{
    bv::SpringParams mainSpring()
    {
        return bv::tankConfigFor (bv::TankType::long2Spring).springs[0];
    }

    std::vector<float> impulseResponse (bv::SpringLine& line, int numSamples)
    {
        auto in = impulse (numSamples);
        std::vector<float> out ((size_t) numSamples);
        line.process (in.data(), out.data(), numSamples);
        return out;
    }

    /** Band-limited copy of an impulse response, so RT60 can be measured per
        band. A plain FFT brick wall is fine here: this is offline test code and
        the pre-ringing lands before the decay we are fitting. */
    std::vector<float> bandLimited (const std::vector<float>& x, double lowHz,
                                    double highHz, double fs)
    {
        int n = 1;
        while (n < (int) x.size()) n <<= 1;

        auto s = spectrum (x, n);
        const int lo = hzToBin (lowHz,  n, fs);
        const int hi = hzToBin (highHz, n, fs);
        for (int k = 0; k <= n / 2; ++k)
            if (k < lo || k > hi)
            {
                s[(size_t) k] = { 0.0, 0.0 };
                if (k > 0 && k < n / 2) s[(size_t) (n - k)] = { 0.0, 0.0 };
            }

        // Inverse via conjugation.
        for (auto& v : s) v = std::conj (v);
        fft (s);
        std::vector<float> out (x.size());
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = (float) (std::conj (s[i]).real() / (double) n);
        return out;
    }
}

//==============================================================================
TEST_CASE ("spring line repeats at its echo spacing", "[spec-001][spring-line]")
{
    // SPEC-001 requirement 5 and acceptance criterion 4: multiple arrivals
    // spaced at T_D, within 10%. The loop is a round trip by construction, so
    // this is checking that the plain delay was sized against the cascade's own
    // group delay rather than added on top of it.
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    bv::SpringLine line;
    line.prepare (fs, s, 3.4);

    const auto ir = impulseResponse (line, (int) (fs * 2.0));
    const int  expected = (int) std::lround (s.echoSpacingMs() * 0.001 * fs);

    const auto lag = autocorrelationPeak (ir, expected / 2, expected * 2, 0.05);
    REQUIRE (lag.has_value());
    REQUIRE_THAT ((double) *lag, WithinRel ((double) expected, 0.10));
}

TEST_CASE ("spring line RT60 tracks the decay parameter", "[spec-001][spring-line]")
{
    // SPEC-001 requirement 2 and criterion 2/3: within 15% across the whole
    // declared range of the decay control.
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    for (double target : { 0.8, 1.6, 2.5, 3.4, 4.5 })
    {
        bv::SpringLine line;
        line.prepare (fs, s, target);

        const auto ir = impulseResponse (line, (int) (fs * (target * 2.0 + 1.0)));
        const auto r  = rt60 (ir, fs);

        INFO ("target " << target << " s");
        REQUIRE (r.sufficientDecay);
        REQUIRE_THAT (r.seconds, WithinSeconds (target, Percent (15.0)));
    }
}

TEST_CASE ("spring line RT60 is sample-rate independent", "[spec-001][spring-line]")
{
    // SPEC-001 requirement 9 and criterion 3. Everything time-domain is derived
    // from seconds and the sample rate, so a rate change must move nothing.
    const auto s = mainSpring();

    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        bv::SpringLine line;
        line.prepare (fs, s, 3.4);

        const auto ir = impulseResponse (line, (int) (fs * 8.0));
        const auto r  = rt60 (ir, fs);

        INFO ("at " << fs << " Hz");
        REQUIRE (r.sufficientDecay);
        REQUIRE_THAT (r.seconds, WithinSeconds (3.4, Percent (15.0)));
    }
}

TEST_CASE ("spring line decays faster at the top than the bottom",
           "[spec-001][spring-line]")
{
    // SPEC-001 requirement 7 and criterion 7: high-frequency RT60 at least 30%
    // shorter than low-frequency RT60. This has to beat the structural tension
    // described at the top of this file, not merely exist.
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    bv::SpringLine line;
    line.prepare (fs, s, 3.4);
    const auto ir = impulseResponse (line, (int) (fs * 8.0));

    const auto lowBand  = bandLimited (ir, 200.0,  500.0, fs);
    const auto highBand = bandLimited (ir, 2000.0, std::min (8000.0, fs * 0.45), fs);

    const auto lowRt  = rt60 (lowBand,  fs);
    const auto highRt = rt60 (highBand, fs);

    REQUIRE (lowRt.sufficientDecay);
    REQUIRE (highRt.sufficientDecay);

    INFO ("low " << lowRt.seconds << " s, high " << highRt.seconds << " s");
    REQUIRE (highRt.seconds < lowRt.seconds * 0.70);
}

TEST_CASE ("spring line loses the main series above the transition frequency",
           "[spec-001][spring-line]")
{
    // SPEC-001 requirement 6 and criterion 6, at the single-spring level: above
    // F_C the series is low-passed out rather than left to chirp on to Nyquist.
    // The band limit is in the LOOP, so the attenuation compounds with every
    // pass and the tail above F_C disappears far faster than one pass suggests.
    const double fs      = 48000.0;
    const int    fftSize = 65536;
    const auto   s       = mainSpring();
    const double fc      = s.transitionHz();

    bv::SpringLine line;
    line.prepare (fs, s, 3.4);
    const auto ir = impulseResponse (line, fftSize);

    const double inBand  = bandEnergyDb (ir, fftSize, fs, 200.0, 0.8 * fc);
    const double outBand = bandEnergyDb (ir, fftSize, fs, 1.5 * fc,
                                         std::min (20000.0, fs * 0.45));

    INFO ("in band " << inBand << " dB, above 1.5*F_C " << outBand << " dB");
    REQUIRE (outBand < inBand - 24.0);
}

TEST_CASE ("spring line group delay rises with frequency through the loop",
           "[spec-001][spring-line]")
{
    // Requirement 1 surviving the loop. The cascade was shown to disperse in
    // test_dispersive_allpass.cpp; this checks the delay line and band limit
    // wrapped around it did not flatten the result.
    const double fs      = 48000.0;
    const int    fftSize = 32768;
    const auto   s       = mainSpring();

    bv::SpringLine line;
    line.prepare (fs, s, 3.4);
    const auto ir = impulseResponse (line, fftSize);

    const double low  = groupDelayInBand (ir, fftSize, fs, 200.0,  400.0);
    const double high = groupDelayInBand (ir, fftSize, fs, 0.7 * s.transitionHz(),
                                                           0.9 * s.transitionHz());

    INFO ("low " << low * 1000.0 << " ms, high " << high * 1000.0 << " ms");
    REQUIRE (high > low);
}

TEST_CASE ("spring line stays bounded under worst-case input",
           "[spec-001][spring-line]")
{
    // SPEC-001 requirement 8 and criterion 8. A feedback loop at the longest
    // decay, driven full scale, is where an instability would show.
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    bv::SpringLine line;
    line.prepare (fs, s, 4.5);

    auto in = noise ((int) (fs * 10.0), 20261030u, 1.0f);
    std::vector<float> out (in.size());
    line.process (in.data(), out.data(), (int) in.size());

    REQUIRE (allFinite (out));
    REQUIRE (toDb (peak (out)) < 12.0);

    // And DC, which a loop with a low-pass in it is the most likely to run away on.
    line.reset();
    auto flat = dc ((int) (fs * 5.0), 1.0f);
    std::vector<float> dcOut (flat.size());
    line.process (flat.data(), dcOut.data(), (int) flat.size());

    REQUIRE (allFinite (dcOut));
    REQUIRE (toDb (peak (dcOut)) < 12.0);
}

TEST_CASE ("spring line survives decay automated across its whole range",
           "[spec-001][spring-line]")
{
    // SPEC-001 edge case: `decay` swept fast is the documented way to
    // destabilise this loop, which is why the loss coefficient is smoothed at
    // 100 ms rather than the 20 ms everything else uses (SPEC-003).
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    bv::SpringLine line;
    line.prepare (fs, s, 3.4);

    auto in = noise ((int) (fs * 5.0), 777u, 1.0f);
    std::vector<float> out (in.size());

    const int block = 64;
    for (int n = 0; n < (int) in.size(); n += block)
    {
        // Slam between the extremes every block — far faster than any host
        // automation, and deliberately so.
        line.setDecaySeconds (((n / block) % 2) == 0 ? 0.8 : 4.5);
        const int count = std::min (block, (int) in.size() - n);
        line.process (in.data() + n, out.data() + n, count);
    }

    REQUIRE (allFinite (out));
    REQUIRE (toDb (peak (out)) < 12.0);
}

TEST_CASE ("spring line reset returns it to silence", "[spec-001][spring-line]")
{
    // SPEC-001 requirement 10 and criterion 9.
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    bv::SpringLine line;
    line.prepare (fs, s, 4.5);

    auto in = impulse (1024, 1.0f);
    std::vector<float> scratch (in.size());
    line.process (in.data(), scratch.data(), (int) in.size());

    line.reset();

    auto quiet = silence ((int) fs);
    std::vector<float> out (quiet.size());
    line.process (quiet.data(), out.data(), (int) quiet.size());

    REQUIRE (allFinite (out));
    REQUIRE (toDb (peak (out)) < -120.0);
}
