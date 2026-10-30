// SPEC-001 acceptance criteria 1, 4, 5, 6, 12 — dispersion and echo structure.
//
// These are tagged [.red]: the leading dot hides them from the default Catch2
// run, so `ctest` stays green on a scaffold while the tests that describe
// unbuilt behaviour still exist and still run on demand:
//
//     ctest --test-dir build -L red --output-on-failure
//     ./birbverb_tests "[red]"
//
// The TDD loop for this file: watch these fail, implement SpringTank, then drop
// the leading dot from each tag so they join the default suite. A test that
// cannot be seen to fail has not been shown to test anything.
//
// The closed forms these assertions are built on are already tested, and not
// red, in test_dispersion_curve.cpp. What is red here is the tank.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "support/Approx.h"
#include "support/Generate.h"
#include "support/Measure.h"

#include "dsp/SpringTank.h"

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
TEST_CASE ("tank disperses: group delay RISES as frequency rises", "[.red][spec-001]")
{
    // SPEC-001 acceptance criterion 1. This is the defining spring artefact —
    // without it we have a filtered delay, not a spring.
    //
    // RISES, not falls. Group delay peaks at F_C, so low frequencies arrive
    // first and each echo in the main series is an ASCENDING chirp. An earlier
    // version of this test asserted the opposite; see the dispersion study for
    // the correction and its sources. The descending chirp is the secondary
    // series, 30 dB down, and it has its own test at the bottom of this file.
    const auto ir = tankImpulseResponse (bv::TankType::long2Spring, fftSize);

    const auto config = bv::tankConfigFor (bv::TankType::long2Spring);
    const double fc = config.minTransitionHz();      // 3784 Hz for this tank

    const double lowGd  = groupDelayInBand (ir, fftSize, fs, 200.0, 500.0);
    const double midGd  = groupDelayInBand (ir, fftSize, fs, 800.0, 1200.0);
    const double highGd = groupDelayInBand (ir, fftSize, fs, 2000.0, 2600.0);
    const double nearFc = groupDelayInBand (ir, fftSize, fs, fc * 0.80, fc * 0.90);

    REQUIRE (lowGd  < midGd);
    REQUIRE (midGd  < highGd);
    REQUIRE (highGd < nearFc);

    // And the sweep has to be wide enough to hear. The design puts ~2.4 ms at
    // DC against ~53 ms at F_C, so a factor of five is a generous floor.
    REQUIRE (nearFc > lowGd * 5.0);
}

TEST_CASE ("group delay peaks at the transition frequency", "[.red][spec-001]")
{
    // SPEC-001 acceptance criterion 1, second half: the maximum is where the
    // physics says it is, within 15%.
    const auto config = bv::tankConfigFor (bv::TankType::long2Spring);
    const double fc = config.minTransitionHz();

    const auto ir = tankImpulseResponse (bv::TankType::long2Spring, fftSize);
    const auto gd = groupDelay (ir, fftSize, fs);

    const int from = hzToBin (200.0, fftSize, fs);
    const int to   = hzToBin (fc * 1.4, fftSize, fs);

    int    peakBin = from;
    double peakGd  = gd[(size_t) from];
    for (int k = from; k <= to && k < (int) gd.size(); ++k)
        if (gd[(size_t) k] > peakGd) { peakGd = gd[(size_t) k]; peakBin = k; }

    REQUIRE_THAT (binToHz (peakBin, fftSize, fs), WithinHz (fc, Percent (15.0)));
}

TEST_CASE ("tank produces an echo train at the spring echo spacing", "[.red][spec-001]")
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

TEST_CASE ("the main series is gone above the transition frequency", "[.red][spec-001]")
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

TEST_CASE ("springs are mismatched the way the hardware is", "[.red][spec-001]")
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

TEST_CASE ("tank stays bounded under worst-case input", "[.red][spec-001]")
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

TEST_CASE ("reset returns the tank to silence", "[.red][spec-001]")
{
    // SPEC-001 acceptance criterion 9.
    bv::SpringTank tank;
    tank.prepare (fs);
    tank.configure (bv::tankConfigFor (bv::TankType::long2Spring));

    auto in = impulse (1024);
    std::vector<float> out ((size_t) 1024);
    tank.process (in.data(), out.data(), 1024);
    REQUIRE (rms (out) > 1.0e-6);          // it rang

    tank.reset();

    auto quiet = silence ((int) fs);
    std::vector<float> after ((size_t) fs);
    tank.process (quiet.data(), after.data(), (int) fs);

    REQUIRE (toDb (peak (after)) < -120.0);
}
