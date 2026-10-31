// SPEC-001 acceptance criteria 1, 4, 5, 6, 12 — dispersion and echo structure.
//
// These were tagged [.red] and hidden from the default run while SpringTank was
// a skeleton returning silence. They have now been watched to fail, SpringTank
// was built against them, and the dot is gone — they are part of the default
// suite. A test that cannot be seen to fail has not been shown to test anything,
// and these were all seen to fail.
//
// The closed forms these assertions are built on are already tested, and not
// red, in test_dispersion_curve.cpp. What is red here is the tank.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "support/Approx.h"
#include "support/Generate.h"
#include "support/Measure.h"

#include "dsp/SpringTank.h"

#include <algorithm>
#include <vector>

using namespace bv::test;

namespace
{
constexpr double fs = 48000.0;
constexpr int fftSize = 16384;

std::vector<float> tankImpulseResponse (bv::TankType type, int numSamples, double rt60 = 3.4)
{
    bv::SpringTank tank;
    tank.prepare (fs);

    auto config = bv::tankConfigFor (type);
    config.rt60Seconds = rt60;
    tank.configure (config);

    auto in = impulse (numSamples);
    std::vector<float> out ((size_t) numSamples);
    tank.process (in.data(), out.data(), numSamples);
    return out;
}
} // namespace

//==============================================================================
TEST_CASE ("tank disperses: group delay RISES as frequency rises", "[spec-001]")
{
    // SPEC-001 acceptance criterion 1. This is the defining spring artefact —
    // without it we have a filtered delay, not a spring.
    //
    // RISES, not falls. Group delay peaks at F_C, so low frequencies arrive
    // first and each echo in the main series is an ASCENDING chirp. An earlier
    // version of this test asserted the opposite; see the dispersion study for
    // the correction and its sources. The descending chirp is the secondary
    // series, 30 dB down, and it has its own test at the bottom of this file.
    const auto config = bv::tankConfigFor (bv::TankType::long2Spring);
    const double fc = config.minTransitionHz();      // 3784 Hz for this tank

    // Windowed to the FIRST ARRIVAL, which is the only place this question has
    // a single answer.
    //
    // Group delay of a whole reverb tail is not a property of the chirp, it is
    // a property of the chirp AND the decay AND the analysis window: low
    // frequencies ring longest, so a longer window keeps collecting late
    // low-frequency energy and drags the low-band figure up with it. Measured
    // on the full tail, the 200-500 Hz band reads 18 ms at a 341 ms window,
    // 37 ms at 683 ms and 48 ms at 1.4 s. The ascending order survived at one
    // window length and inverted at the others, so a test on the full tail
    // would have been measuring its own FFT size.
    //
    // One arrival has no decay in it to confound. Cutting at T_D takes
    // everything before the second arrival.
    const auto tail = tankImpulseResponse (bv::TankType::long2Spring, (int) (fs * 2.0));
    const int  firstArrival = (int) (config.meanEchoSpacingMs() * 0.001 * fs);

    std::vector<float> ir (tail.begin(), tail.begin() + firstArrival);
    ir.resize ((size_t) fftSize, 0.0f);

    // Bands from 1.2 kHz up, and the lower end deliberately left out.
    //
    // Two limits meet below that. Eq. (4) is nearly flat far below F_C — it
    // predicts only 0.3 ms of rise between 350 Hz and 1 kHz for this spring —
    // while a window of one echo period is 19 cycles at 350 Hz, which resolves
    // group delay to no better than about +/-1.3 ms. Measured across nine
    // bands, everything under 1.2 kHz scatters inside that figure and nothing
    // under it is a real reading. Above 1.2 kHz the predicted steps are whole
    // milliseconds and the measurement is steady.
    //
    // The curve below 1.2 kHz is not untested, it is tested somewhere the
    // measurement works: test_dispersive_allpass.cpp checks the cascade from
    // 200 Hz against dispersion::groupDelaySamples() to within 2%, on a clean
    // impulse response with no echo train or second spring in it. This test
    // is about the TANK still dispersing once a loop is wrapped around it.
    const double lowGd  = groupDelayInBand (ir, fftSize, fs, 1200.0, 1800.0);
    const double midGd  = groupDelayInBand (ir, fftSize, fs, 2000.0, 2600.0);
    const double nearFc = groupDelayInBand (ir, fftSize, fs, fc * 0.80, fc * 0.90);

    REQUIRE (lowGd < midGd);
    REQUIRE (midGd < nearFc);

    // And the sweep has to be wide enough to hear.
    //
    // The original floor was a factor of five, on the reasoning that the design
    // puts ~2.4 ms at DC against ~53 ms at F_C. Those are the CASCADE's numbers
    // across its whole range, and they are not what reaches a listener. [BP09]
    // derive T_D as the reciprocal of group velocity as beta -> 0, the point of
    // MAXIMUM velocity, so T_D is the round trip at DC and the tank's delay at
    // DC is the single pass — the cascade's 2.4 ms sits inside that rather than
    // instead of it. And the cascade only reaches its peak AT F_C, which is
    // exactly where the band limit takes the energy away, so the measurable
    // sweep stops short of the peak by design.
    REQUIRE (nearFc - lowGd > 0.002);
}

TEST_CASE ("group delay peaks at the transition frequency", "[spec-001]")
{
    // SPEC-001 acceptance criterion 1, second half: the maximum is where the
    // physics says it is, within 15%.
    // The MEAN F_C, not the minimum. Criterion 1 says "F_C" singular, which is
    // unambiguous for one spring and needs a reading for a tank of several.
    // The minimum is specifically criterion 6's quantity — it is the frequency
    // above which the main series is gone for EVERY spring. For where the
    // summed group delay peaks, the ensemble's mean is the honest reading, and
    // the two springs here sit at 4471 and 3784 Hz.
    const auto config = bv::tankConfigFor (bv::TankType::long2Spring);
    const double fc = 0.5 * (config.minTransitionHz() + config.maxTransitionHz());

    const auto ir = tankImpulseResponse (bv::TankType::long2Spring, fftSize);
    // Measured in bands, with the same tool the ordering assertions above use.
    //
    // A raw per-bin argmax is not a stable measurement here: a tank's summed
    // impulse response has a jagged group-delay curve — many overlapping
    // echoes, each with its own phase — and the winning bin moved between
    // 735 Hz and 4535 Hz across builds that differed only in the band limit's
    // order. The band averages are steady, which is why every other assertion
    // in this file is written in terms of them. Sliding a 300 Hz window and
    // taking the band where the delay is greatest asks the same question the
    // criterion does, and gets a repeatable answer.
    double peakHz = 200.0;
    double peakGd = 0.0;
    for (double centre = 350.0; centre <= fc * 1.4; centre += 100.0)
    {
        const double g = groupDelayInBand (ir, fftSize, fs, centre - 150.0, centre + 150.0);
        if (g > peakGd) { peakGd = g; peakHz = centre; }
    }

    REQUIRE_THAT (peakHz, WithinHz (fc, Percent (15.0)));
}

TEST_CASE ("tank produces an echo train at the spring echo spacing", "[spec-001]")
{
    // SPEC-001 acceptance criterion 4. T_D is a round trip — the time for a
    // wave to traverse the spring twice — so it is the echo spacing, not the
    // single-pass delay, that autocorrelation should find.
    bv::SpringTank tank;
    tank.prepare (fs);
    tank.configure (bv::tankConfigFor (bv::TankType::long2Spring));

    const double expectedSpacing = tank.getEchoSpacingSamples();
    REQUIRE (expectedSpacing > 0.0);
    REQUIRE_THAT (expectedSpacing / fs * 1000.0, WithinMs (55.0, Percent (5.0)));

    const auto ir = tankImpulseResponse (bv::TankType::long2Spring, fftSize);
    const auto lag = autocorrelationPeak (ir,
                                          (int) (expectedSpacing * 0.5),
                                          (int) (expectedSpacing * 2.0));

    REQUIRE (lag.has_value());
    REQUIRE_THAT ((double) *lag, WithinRatio (expectedSpacing, expectedSpacing * 0.1));
}

TEST_CASE ("the main series is gone above the transition frequency", "[spec-001]")
{
    // SPEC-001 acceptance criterion 6. This now has an absolute frequency to
    // test against: F_C is derived per spring from Bilbao & Parker eq. (4), not
    // guessed. Two things have to hold above it — the chirp has stopped, and the
    // energy has gone with it.
    const auto ir = tankImpulseResponse (bv::TankType::long2Spring, fftSize);

    const auto config = bv::tankConfigFor (bv::TankType::long2Spring);
    const double fc = config.minTransitionHz();

    const double flatLow  = groupDelayInBand (ir, fftSize, fs, fc * 1.5, fc * 1.8);
    const double flatHigh = groupDelayInBand (ir, fftSize, fs, fc * 1.8, fc * 2.1);

    REQUIRE_THAT (flatHigh / fs * 1000.0,
                  WithinMs (flatLow / fs * 1000.0, 0.5));

    const double inBand = bandEnergyDb (ir, fftSize, fs, 300.0, fc * 0.9);
    const double above  = bandEnergyDb (ir, fftSize, fs, fc * 1.5, fc * 2.5);

    REQUIRE (above < inBand - 24.0);
}

TEST_CASE ("springs are mismatched the way the hardware is", "[spec-001]")
{
    // SPEC-001 acceptance criterion 5. The per-type philosophy is asserted
    // against the configuration, not the audio, in test_dispersion_curve.cpp.
    // What is red here is that the TANK actually uses it: three springs must
    // produce three distinguishable echo spacings in the output.
    const auto config = bv::tankConfigFor (bv::TankType::long3Spring);
    REQUIRE (config.numSprings == 3);

    const auto ir = tankImpulseResponse (bv::TankType::long3Spring, fftSize);

    // Each spring's round trip should show up as its own autocorrelation peak.
    for (int i = 0; i < config.numSprings; ++i)
    {
        const double spacing = config.springs[i].echoSpacingMs() * 0.001 * fs;
        const auto lag = autocorrelationPeak (ir,
                                              (int) (spacing * 0.92),
                                              (int) (spacing * 1.08));
        REQUIRE (lag.has_value());
    }
}

TEST_CASE ("tank stays bounded under worst-case input", "[spec-001]")
{
    // SPEC-001 acceptance criterion 8.
    bv::SpringTank tank;
    tank.prepare (fs);
    tank.configure (bv::tankConfigFor (bv::TankType::long3Spring));

    const int numSamples = (int) (fs * 10.0);
    auto in = noise (numSamples, 0xBEEFull, 1.0f);
    std::vector<float> out ((size_t) numSamples);
    tank.process (in.data(), out.data(), numSamples);

    REQUIRE (allFinite (out));
    REQUIRE (toDb (peak (out)) < 12.0);

    // And it must actually do something — silence passes the two checks above.
    REQUIRE (rms (out) > 1.0e-6);
}

TEST_CASE ("reset returns the tank to silence", "[spec-001]")
{
    // SPEC-001 acceptance criterion 9.
    bv::SpringTank tank;
    tank.prepare (fs);
    tank.configure (bv::tankConfigFor (bv::TankType::long2Spring));

    // Long enough to contain the first arrival. 1024 samples is 21 ms at
    // 48 kHz and this tank's pickup is 27 ms from its driver, so the original
    // window ended before the spring had delivered anything — and no real tank
    // would have either.
    const int window = (int) (tank.getSinglePassSamples() * 3.0);
    auto in = impulse (window);
    std::vector<float> out ((size_t) window);
    tank.process (in.data(), out.data(), window);
    REQUIRE (rms (out) > 1.0e-6);          // it rang

    tank.reset();

    auto quiet = silence ((int) fs);
    std::vector<float> after ((size_t) fs);
    tank.process (quiet.data(), after.data(), (int) fs);

    REQUIRE (toDb (peak (after)) < -120.0);
}
