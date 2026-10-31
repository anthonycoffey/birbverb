// DispersiveAllpass — SPEC-001, the stretched allpass cascade.
//
// These were written tagged [.red] against a passthrough skeleton, watched to
// fail, and promoted into the default suite once DispersiveAllpass.h realised
// the structure. A test that has never been seen to fail has not been shown to
// test anything.
//
// What makes this a tight test rather than a loose one: the expected numbers do
// not come from this file. They come from dispersion::groupDelaySamples(), the
// closed form of [PARKER11] eq. (4), which tests/test_dispersion_curve.cpp
// already checks against the paper's own printed values. So a failure here is
// the STRUCTURE disagreeing with the curve it was built to realise, which is
// the only thing this file can usefully say.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/Approx.h"
#include "support/Generate.h"
#include "support/Measure.h"

#include "dsp/DispersiveAllpass.h"
#include "dsp/TankConfig.h"

#include <algorithm>
#include <vector>

using namespace bv::test;
using bv::dispersion::groupDelaySamples;
using bv::dispersion::stretchFactor;
using bv::dispersion::coeffForPeakDelay;
using bv::dispersion::pi;
using Catch::Matchers::WithinRel;

namespace
{
    /** The spring the main band tests use: Long 2-Spring A, the Fender
        blackface tank's first spring. Geometry (and therefore F_C and T_D) comes
        from TankConfig, so this test follows the tank rather than duplicating
        its numbers. */
    bv::SpringParams mainSpring()
    {
        return bv::tankConfigFor (bv::TankType::long2Spring).springs[0];
    }

    std::vector<float> impulseResponse (bv::DispersiveAllpass& ap, int numSamples)
    {
        auto in = impulse (numSamples);
        std::vector<float> out ((size_t) numSamples);
        ap.process (in.data(), out.data(), numSamples);
        return out;
    }

    /** Measured group delay in samples at `hz`. */
    double measuredGdSamples (const std::vector<float>& ir, double hz,
                              int fftSize, double fs)
    {
        const auto gd = groupDelay (ir, fftSize, fs);
        const int bin = std::min ((int) gd.size() - 1,
                                  std::max (1, hzToBin (hz, fftSize, fs)));
        return gd[(size_t) bin] * fs;   // groupDelay() returns seconds
    }
}

//==============================================================================
TEST_CASE ("allpass cascade passes every frequency at unity gain",
           "[spec-001][dispersion]")
{
    // The defining property. If this fails nothing else in the file means
    // anything, because a filter that is not allpass is not dispersing, it is
    // equalising. SPEC-001's whole premise is that the tank changes WHEN
    // frequencies arrive, not how loud they are.
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    bv::DispersiveAllpass ap;
    ap.prepare (fs, s.transitionHz(), s.echoSpacingMs(), 100);

    for (double hz : { 100.0, 440.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 })
    {
        ap.reset();
        const double db = gainDbAt ([&ap] (float x) { return ap.processSample (x); },
                                    hz, fs, 2.0);
        REQUIRE_THAT (db, WithinDb (0.0, 0.1));
    }
}

TEST_CASE ("allpass cascade group delay matches Parker eq. 4 in the main band",
           "[spec-001][dispersion]")
{
    // The headline assertion: the realised structure reproduces the closed form
    // it was designed from, across the band the main series actually occupies
    // (DC to F_C). Above F_C the main series is low-passed out by SpringLine
    // and the agreement there is not load-bearing — SPEC-001 requirement 6.
    const double fs      = 48000.0;
    const int    fftSize = 32768;
    const auto   s       = mainSpring();
    const int    M       = 100;

    const double fc = s.transitionHz();
    const double k  = stretchFactor (fs, fc);
    const double a  = coeffForPeakDelay (s.echoSpacingMs() * 0.001 * fs, k, M);

    bv::DispersiveAllpass ap;
    ap.prepare (fs, fc, s.echoSpacingMs(), M);

    // The structure realises k as an integer delay plus a first-order
    // interpolator, so it tracks the ideal z^-k closely in band and drifts
    // outside it. 2% across 200 Hz..0.9*F_C is the window SPEC-001 criterion 1
    // reasons in, and is far tighter than the +/-15% the criterion itself asks
    // of the peak location.
    const auto ir = impulseResponse (ap, fftSize);

    for (double hz = 200.0; hz <= 0.9 * fc; hz += 200.0)
    {
        const double omega    = 2.0 * pi * hz / fs;
        const double expected = groupDelaySamples (a, k, M, omega);
        const double measured = measuredGdSamples (ir, hz, fftSize, fs);

        INFO ("at " << hz << " Hz");
        REQUIRE_THAT (measured, WithinRel (expected, 0.02));
    }
}

TEST_CASE ("allpass cascade group delay RISES with frequency — the chirp ascends",
           "[spec-001][dispersion]")
{
    // SPEC-001 requirement 1 and acceptance criterion 1, at the component level.
    // Four documents in this repo said "descending" before the dispersion study
    // corrected them, so this assertion is deliberately blunt: low frequencies
    // arrive FIRST.
    const double fs      = 48000.0;
    const int    fftSize = 32768;
    const auto   s       = mainSpring();

    bv::DispersiveAllpass ap;
    ap.prepare (fs, s.transitionHz(), s.echoSpacingMs(), 100);
    const auto ir = impulseResponse (ap, fftSize);

    double previous = measuredGdSamples (ir, 200.0, fftSize, fs);
    for (double hz = 400.0; hz <= 0.9 * s.transitionHz(); hz += 200.0)
    {
        const double next = measuredGdSamples (ir, hz, fftSize, fs);
        INFO ("at " << hz << " Hz");
        REQUIRE (next > previous);
        previous = next;
    }
}

TEST_CASE ("allpass cascade group delay peaks at the spring's transition frequency",
           "[spec-001][dispersion]")
{
    // SPEC-001 acceptance criterion 1: the maximum falls within +/-15% of F_C.
    const double fs      = 48000.0;
    const int    fftSize = 32768;
    const auto   s       = mainSpring();
    const double fc      = s.transitionHz();

    bv::DispersiveAllpass ap;
    ap.prepare (fs, fc, s.echoSpacingMs(), 100);
    const auto ir = impulseResponse (ap, fftSize);

    // Search the main band only, and deliberately so. Eq. (4) depends on
    // cos(omega*k), so the cascade's group delay is PERIODIC in omega*k: it
    // peaks again at 3*F_C, 5*F_C and so on, with the same height. That
    // repetition is exactly what SPEC-001 requirement 6 removes by low-passing
    // the main series above F_C, which is SpringLine's job and not this
    // component's. An unbounded search here would be asking the allpass to do
    // something the design explicitly delegates.
    const auto gd = groupDelay (ir, fftSize, fs);
    const int  last = std::min ((int) gd.size() - 1, hzToBin (1.5 * fc, fftSize, fs));

    int    peakBin = 1;
    double peakGd  = gd[1];
    for (int bin = 2; bin <= last; ++bin)
        if (gd[(size_t) bin] > peakGd) { peakGd = gd[(size_t) bin]; peakBin = bin; }

    REQUIRE_THAT (binToHz (peakBin, fftSize, fs), WithinHz (fc, Percent (15.0)));

    // And the peak is T_D, because that is what coeffForPeakDelay was asked for:
    // the chirp exactly fills the echo period.
    REQUIRE_THAT (peakGd * 1000.0, WithinMs (s.echoSpacingMs(), Percent (5.0)));
}

TEST_CASE ("allpass cascade is sample-rate independent",
           "[spec-001][dispersion]")
{
    // SPEC-001 requirement 9, and the entire reason the stretch factor is
    // fractional. Rounding k to an integer makes F_C jump with the sample rate:
    // at 48 kHz, k = 5 is 4800 Hz and k = 6 is 4000 Hz. The peak has to land on
    // the same frequency in Hz at every rate.
    const auto   s  = mainSpring();
    const double fc = s.transitionHz();

    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        // Constant duration rather than constant length, so the resolution of
        // the measurement scales with the rate instead of the window shrinking.
        const int fftSize = (int) std::lround (std::pow (2.0, std::ceil (
                                std::log2 (fs * 0.7))));

        bv::DispersiveAllpass ap;
        ap.prepare (fs, fc, s.echoSpacingMs(), 100);
        const auto ir = impulseResponse (ap, fftSize);

        // Main band only — see the periodicity note in the peak-location test.
        const auto gd   = groupDelay (ir, fftSize, fs);
        const int  last = std::min ((int) gd.size() - 1, hzToBin (1.5 * fc, fftSize, fs));

        int    peakBin = 1;
        double peakGd  = gd[1];
        for (int bin = 2; bin <= last; ++bin)
            if (gd[(size_t) bin] > peakGd) { peakGd = gd[(size_t) bin]; peakBin = bin; }

        INFO ("at " << fs << " Hz");
        REQUIRE_THAT (binToHz (peakBin, fftSize, fs), WithinHz (fc, Percent (10.0)));
        REQUIRE_THAT (peakGd * 1000.0, WithinMs (s.echoSpacingMs(), Percent (5.0)));
    }
}

TEST_CASE ("allpass cascade with an integer stretch factor is exact",
           "[spec-001][dispersion]")
{
    // With k integer the fractional interpolator is out of the picture, so the
    // structure IS the closed form and the only error left is the measurement.
    // Separating the two matters: if this passes and the fractional case does
    // not, the interpolator is at fault and not the cascade.
    const double fs      = 48000.0;
    const int    fftSize = 16384;
    const int    M       = 20;
    const double k       = 6.0;                  // F_C = fs/(2k) = 4000 Hz
    const double a       = 0.5;
    const double peakMs  = bv::dispersion::peakGroupDelay (a, k, M) / fs * 1000.0;

    bv::DispersiveAllpass ap;
    ap.prepare (fs, fs / (2.0 * k), peakMs, M);

    REQUIRE_THAT (ap.getStretchFactor(), WithinRel (k, 1.0e-9));
    REQUIRE_THAT (ap.getCoefficient(),   WithinRel (a, 1.0e-6));

    const auto ir = impulseResponse (ap, fftSize);
    for (double hz = 200.0; hz <= 3600.0; hz += 200.0)
    {
        const double expected = groupDelaySamples (a, k, M, 2.0 * pi * hz / fs);
        INFO ("at " << hz << " Hz");
        REQUIRE_THAT (measuredGdSamples (ir, hz, fftSize, fs),
                      WithinRel (expected, 0.005));
    }
}

TEST_CASE ("allpass cascade reset returns it to silence",
           "[spec-001][dispersion]")
{
    // SPEC-001 requirement 10. A cascade of 100 sections holds a long tail, so
    // "no residual state" is a real assertion rather than a formality.
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    bv::DispersiveAllpass ap;
    ap.prepare (fs, s.transitionHz(), s.echoSpacingMs(), 100);

    auto loud = noise (8192, 0xB12BBull, 1.0f);
    std::vector<float> scratch (loud.size());
    ap.process (loud.data(), scratch.data(), (int) loud.size());

    ap.reset();

    auto quiet = silence (8192);
    std::vector<float> out (quiet.size());
    ap.process (quiet.data(), out.data(), (int) quiet.size());

    REQUIRE (allFinite (out));
    REQUIRE (toDb (peak (out)) < -120.0);
}

TEST_CASE ("allpass cascade stays finite and bounded under full-scale input",
           "[spec-001][dispersion]")
{
    // SPEC-001 requirement 8. An allpass cascade has unity magnitude, but the
    // peak of a dispersed signal is not the peak of the input: energy that
    // arrived spread out can line up. Bounded, not unchanged.
    const double fs = 48000.0;
    const auto   s  = mainSpring();

    bv::DispersiveAllpass ap;
    ap.prepare (fs, s.transitionHz(), s.echoSpacingMs(), 100);

    auto in = noise ((int) (fs * 2.0), 20261030u, 1.0f);
    std::vector<float> out (in.size());
    ap.process (in.data(), out.data(), (int) in.size());

    REQUIRE (allFinite (out));
    REQUIRE (toDb (peak (out)) < 12.0);
}
