// SPEC-001 acceptance criterion 11 — the dispersion closed forms.
//
// NOT tagged [.red]. Dispersion.h is pure functions with no dependency on
// SpringTank, so these run in the default suite from the moment the header
// exists. They are the executable form of the decision recorded in
// docs/documentation/deep-dives/spring-dispersion-and-allpass-fitting.md.
//
// Every expected value below is a number printed in, or derived in that
// document from, a paper a reader can open:
//
//   [PARKER11] Parker, EURASIP J. Adv. Signal Process. 2011:646134 (CC BY)
//   [BP09]     Bilbao & Parker, Proc. DAFx-09, pp. 416-421
//
// If one of these fails, either a formula was transcribed wrong or a geometry
// was. That is exactly the failure this file exists to catch, because every
// coefficient in the tank is computed from these two things.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/Approx.h"

#include "dsp/Dispersion.h"
#include "dsp/TankConfig.h"

using namespace bv::dispersion;
using namespace bv::test;
using Catch::Matchers::WithinRel;

//==============================================================================
TEST_CASE ("steel bar wave speed", "[spec-001][dispersion]")
{
    // sqrt(2e11 / 7800) = 5063.7 m/s. [BP09] Appendix A gives E and rho.
    REQUIRE_THAT (waveSpeed(), WithinRel (5063.7, 1.0e-4));
}

TEST_CASE ("cascade group delay matches Parker eq. 4", "[spec-001][dispersion]")
{
    // The single value that ties this header to the measurement library: at
    // a = -0.7, k = 1, M = 1 the closed form gives 5.6667 samples at DC, and
    // Measure.h's groupDelay() measured a real one-section allpass at 5.6525
    // samples at the first FFT bin. Two independent routes to the same number.
    REQUIRE_THAT (groupDelaySamples (-0.7, 1.0, 1, 0.0), WithinRel (17.0 / 3.0, 1.0e-9));
    REQUIRE_THAT (groupDelayAtDc    (-0.7, 1.0, 1),      WithinRel (17.0 / 3.0, 1.0e-9));

    // The closed form at DC and at the peak must agree with the general form.
    const double k = 5.368, a = 0.64983;
    const int    M = 100;
    REQUIRE_THAT (groupDelaySamples (a, k, M, 0.0),
                  WithinRel (groupDelayAtDc (a, k, M), 1.0e-12));
    REQUIRE_THAT (groupDelaySamples (a, k, M, pi / k),
                  WithinRel (peakGroupDelay (a, k, M), 1.0e-12));

    // Group delay is never negative for a stable allpass.
    for (double aa : { -0.9, -0.5, 0.0, 0.5, 0.9 })
        for (int i = 0; i <= 32; ++i)
            REQUIRE (groupDelaySamples (aa, 3.0, 10, pi * (double) i / 32.0) > 0.0);
}

TEST_CASE ("the group-delay peak sits at fs/(2k)", "[spec-001][dispersion]")
{
    // What makes stretchFactor() correct: the peak of eq. (4) is where
    // cos(omega*k) = -1.
    const double fs = 48000.0;

    for (double fc : { 2543.0, 3244.0, 3784.0, 4471.0, 4672.0, 4887.0 })
    {
        const double k = stretchFactor (fs, fc);
        const double a = 0.6;
        const int    M = 100;

        const double atPeak = groupDelaySamples (a, k, M, 2.0 * pi * fc / fs);

        // Nothing either side of it may be larger.
        for (double scale : { 0.5, 0.8, 0.95, 1.05, 1.2, 1.5 })
        {
            const double f = fc * scale;
            if (f >= fs * 0.5)
                continue;
            REQUIRE (groupDelaySamples (a, k, M, 2.0 * pi * f / fs) <= atPeak);
        }
    }
}

TEST_CASE ("coeffForPeakDelay round-trips", "[spec-001][dispersion]")
{
    // SPEC-001 criterion 11: every row of the tank table, to 0.01%.
    struct Row { double echoMs, fcHz; };
    const Row rows[] = {
        { 30.61, 3244.0 }, { 35.10, 2543.0 },     // Short 2-Spring
        { 52.69, 4471.0 }, { 57.28, 3784.0 },     // Long 2-Spring
        { 41.62, 4887.0 }, { 40.24, 4672.0 },     // Long 3-Spring
    };

    const double fs = 48000.0;
    const int    M  = 100;

    for (const auto& row : rows)
    {
        const double k      = stretchFactor (fs, row.fcHz);
        const double target = row.echoMs * 0.001 * fs;
        const double a      = coeffForPeakDelay (target, k, M);

        REQUIRE (a > 0.0);
        REQUIRE (a < 1.0);
        REQUIRE_THAT (peakGroupDelay (a, k, M), WithinRel (target, 1.0e-4));
        REQUIRE_THAT (groupDelaySamples (a, k, M, pi / k), WithinRel (target, 1.0e-4));

        // And the chirp has to actually sweep: the DC end must be far shorter
        // than the peak, or there is no audible dispersion.
        REQUIRE (groupDelayAtDc (a, k, M) < target * 0.5);
    }
}

TEST_CASE ("Parker eq. 6 inverts the first-order group delay", "[spec-001][dispersion]")
{
    // Both roots must reproduce the requested delay at the requested frequency.
    // This is what makes a fractional stretch factor possible, and a fractional
    // stretch factor is what makes the model sample-rate independent.
    const double fs = 48000.0;

    for (double D : { 0.15, 0.35, 0.5, 0.8 })
        for (double f : { 300.0, 1000.0, 4000.0 })
        {
            const double w = 2.0 * pi * f / fs;

            for (bool positive : { true, false })
            {
                const double a    = coeffForFractionalDelay (D, w, positive);
                const double back = (1.0 - a * a)
                                  / (1.0 + 2.0 * a * std::cos (w) + a * a);

                REQUIRE (std::abs (a) < 1.0);          // stable
                REQUIRE_THAT (back, WithinRel (D, 1.0e-9));
            }
        }
}

TEST_CASE ("loop gain hits the RT60 target", "[spec-001][dispersion]")
{
    for (double rt60 : { 0.8, 1.6, 3.4, 4.5 })
        for (double loop : { 0.0329, 0.0449, 0.0550 })
        {
            const double g = loopGainForRt60 (loop, rt60);
            REQUIRE (g > 0.0);
            REQUIRE (g < 1.0);

            // 60 dB of decay after exactly rt60 seconds of round trips.
            const double passes = rt60 / loop;
            REQUIRE_THAT (20.0 * std::log10 (std::pow (g, passes)),
                          WithinRel (-60.0, 1.0e-6));
        }
}

//==============================================================================
// Bilbao & Parker eqs. (2)-(4) against their own published measurements.
//
// These are the strongest tests in the file: the geometries are caliper
// readings off [BP09] Appendix A, and the expected T_D and F_C are what the
// formulas give for them. A 1% tolerance covers the rounding in the published
// table, not any fitting of ours.

TEST_CASE ("echo spacing and transition frequency, measured springs",
           "[spec-001][dispersion]")
{
    struct Row { const char* name; double hlCm, diaCm; int turns; double wireCm;
                 double echoMs, fcHz; };

    const Row rows[] = {
        { "Olson X-82 #1",    6.5, 0.54, 148, 0.035, 30.61, 3244.5 },
        { "Olson X-82 #2",    6.5, 0.61, 133, 0.035, 35.10, 2542.6 },
        { "Leem KA-1210 #1", 16.3, 0.44, 303, 0.035, 41.62, 4886.8 },
        { "Leem KA-1210 #2", 16.3, 0.45, 280, 0.035, 40.24, 4672.0 },
        { "Leem KA-1210 #3", 16.3, 0.46, 351, 0.035, 52.69, 4471.1 },
    };

    for (const auto& row : rows)
    {
        const auto g = bv::fromCalipers (row.hlCm, row.diaCm, row.turns, row.wireCm);

        REQUIRE_THAT (echoSpacingSeconds (g) * 1000.0, WithinRel (row.echoMs, 0.01));
        REQUIRE_THAT (transitionHz (g),                WithinRel (row.fcHz,   0.01));

        // [PARKER11]: an F_C above 5 kHz "is rarely seen in real spring
        // reverberators". Five springs from two units, measured by someone
        // else, all land under it. That agreement is the reason to trust these
        // formulas at all.
        REQUIRE (transitionHz (g) < 5000.0);
        REQUIRE (transitionHz (g) > 2000.0);
    }
}

TEST_CASE ("F_C does not depend on spring length", "[spec-001][dispersion]")
{
    // F_C is proportional to r / R^2 with no L in it, which is why a
    // manufacturer can match echo spacing across springs while spreading their
    // transition frequencies. If this ever fails, eq. (4) was mistyped.
    const auto shortSpring = bv::fromCalipers ( 6.5, 0.46, 140, 0.035);
    const auto longSpring  = bv::fromCalipers (16.3, 0.46, 351, 0.035);

    REQUIRE_THAT (transitionHz (shortSpring), WithinRel (transitionHz (longSpring), 1.0e-9));
    REQUIRE (echoSpacingSeconds (shortSpring) < echoSpacingSeconds (longSpring));
}

TEST_CASE ("tank types carry the right spring design philosophy",
           "[spec-001][dispersion]")
{
    // SPEC-001 criterion 5. [BP09] Appendix A states both philosophies in
    // prose; recomputing them from geometry is how we know the geometries were
    // transcribed correctly.
    const auto shortTank = bv::tankConfigFor (bv::TankType::short2Spring);
    const auto longTank  = bv::tankConfigFor (bv::TankType::long2Spring);
    const auto threeTank = bv::tankConfigFor (bv::TankType::long3Spring);

    REQUIRE (shortTank.numSprings == 2);
    REQUIRE (longTank.numSprings  == 2);
    REQUIRE (threeTank.numSprings == 3);

    // Two springs: match the delay, spread the transition.
    for (const auto& tank : { shortTank, longTank })
    {
        REQUIRE (tank.echoSpacingSpread() < 0.15);
        REQUIRE (tank.transitionSpread()  > 0.15);
        REQUIRE (tank.transitionSpread()  < 0.30);
    }

    // Three springs: the mirror image.
    REQUIRE (threeTank.echoSpacingSpread() > 0.20);
    REQUIRE (threeTank.echoSpacingSpread() < 0.35);
    REQUIRE (threeTank.transitionSpread()  < 0.12);
}

TEST_CASE ("tank types are ordered as their datasheet codes say",
           "[spec-001][dispersion]")
{
    const auto shortTank = bv::tankConfigFor (bv::TankType::short2Spring);
    const auto longTank  = bv::tankConfigFor (bv::TankType::long2Spring);

    REQUIRE (shortTank.rt60Seconds < longTank.rt60Seconds);
    REQUIRE (shortTank.meanEchoSpacingMs() < longTank.meanEchoSpacingMs());
    REQUIRE (shortTank.singlePassDelayMs() < longTank.singlePassDelayMs());

    // Shorter decay over shorter echoes still means more loss per pass.
    REQUIRE (shortTank.loopGain() < longTank.loopGain());

    // Every tank's secondary loop descends (negative coefficient) and sits
    // 30 dB down. [PARKER11] section 4.
    for (auto type : { bv::TankType::short2Spring, bv::TankType::long2Spring,
                       bv::TankType::long3Spring })
    {
        const auto c = bv::tankConfigFor (type);
        REQUIRE (c.secondaryCoeff < 0.0);
        REQUIRE (c.secondaryLevelDb <= -30.0);
        REQUIRE (c.allpassSectionsLow  == 100);
        REQUIRE (c.allpassSectionsHigh == 200);
    }
}

TEST_CASE ("the secondary loop descends and dies out above 10 kHz",
           "[spec-001][dispersion]")
{
    // SPEC-001 criterion 12, at the coefficient level. a_hf = -0.6 with M = 200
    // and k = 1 gives 16.7 ms at DC, 2.5 ms at 10 kHz, 1.0 ms at Nyquist, which
    // is [PARKER11]'s "little dispersion above 10 kHz".
    const double fs = 48000.0;
    const double a  = bv::tankConfigFor (bv::TankType::long2Spring).secondaryCoeff;
    const int    M  = 200;

    const double atDc  = groupDelaySamples (a, 1.0, M, 0.0) / fs * 1000.0;
    const double at10k = groupDelaySamples (a, 1.0, M, 2.0 * pi * 10000.0 / fs) / fs * 1000.0;
    const double atNyq = groupDelaySamples (a, 1.0, M, pi) / fs * 1000.0;

    REQUIRE_THAT (atDc,  WithinMs (16.67, Percent (1.0)));
    REQUIRE_THAT (at10k, WithinMs ( 2.54, Percent (2.0)));
    REQUIRE_THAT (atNyq, WithinMs ( 1.04, Percent (2.0)));

    // Monotonically decreasing, which is what makes it a descending chirp.
    // Compared in SAMPLES, not ms: groupDelaySamples() returns samples, and
    // atDc above has already been scaled to milliseconds for the assertions.
    double previous = groupDelaySamples (a, 1.0, M, 0.0);
    for (int i = 1; i <= 24; ++i)
    {
        const double next = groupDelaySamples (a, 1.0, M, pi * (double) i / 24.0);
        REQUIRE (next < previous);
        previous = next;
    }
}
