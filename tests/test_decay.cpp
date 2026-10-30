// SPEC-001 acceptance criteria 2, 3, 7 — decay behaviour.
//
// Tagged [.red] until SpringTank is implemented. See the note at the top of
// test_dispersion.cpp for the loop.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "support/Approx.h"
#include "support/Generate.h"
#include "support/Measure.h"

#include "dsp/SpringTank.h"

using namespace bv::test;

namespace
{
std::vector<float> tankTail (double sampleRate, bv::TankType type, double rt60, double seconds)
{
    bv::SpringTank tank;
    tank.prepare (sampleRate);

    auto config = bv::tankConfigFor (type);
    config.rt60Seconds = rt60;
    tank.configure (config);

    const int numSamples = (int) (sampleRate * seconds);
    auto in = impulse (numSamples);
    std::vector<float> out ((size_t) numSamples);
    tank.process (in.data(), out.data(), numSamples);
    return out;
}
} // namespace

//==============================================================================
TEST_CASE ("tank RT60 matches the long-decay tank datasheet", "[.red][spec-001]")
{
    // SPEC-001 acceptance criterion 2. 3.4 s is the middle of the 2.75..4.0 s
    // range on the 4AB3C1B datasheet — see circuits/accutronics-spring-tanks.md.
    const auto tail = tankTail (48000.0, bv::TankType::long2Spring, 3.4, 12.0);
    const auto r = rt60 (tail, 48000.0);

    REQUIRE (r.sufficientDecay);
    REQUIRE_THAT (r.seconds, WithinSeconds (3.4, Percent (15.0)));
}

TEST_CASE ("RT60 tracks the decay parameter", "[.red][spec-001]")
{
    // SPEC-001 acceptance criterion 3, across the parameter's declared range.
    const double requested = GENERATE (0.8, 1.6, 2.4, 3.4, 4.5);

    const auto tail = tankTail (48000.0, bv::TankType::long2Spring, requested,
                                requested * 3.0 + 1.0);
    const auto r = rt60 (tail, 48000.0);

    REQUIRE (r.sufficientDecay);
    REQUIRE_THAT (r.seconds, WithinSeconds (requested, Percent (15.0)));
}

TEST_CASE ("RT60 is sample-rate independent", "[.red][spec-001]")
{
    // SPEC-001 acceptance criterion 3 and requirement 9. Time-domain behaviour
    // has to be expressed in seconds, not samples, or every preset changes
    // character when the session rate does.
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);

    const auto tail = tankTail (sampleRate, bv::TankType::long2Spring, 3.4, 12.0);
    const auto r = rt60 (tail, sampleRate);

    REQUIRE (r.sufficientDecay);
    REQUIRE_THAT (r.seconds, WithinSeconds (3.4, Percent (15.0)));
}

TEST_CASE ("high frequencies decay faster than low frequencies", "[.red][spec-001]")
{
    // SPEC-001 acceptance criterion 7. Real tanks lose their top first; a model
    // with flat decay sounds synthetic in a way that is hard to place until you
    // measure it.
    const auto tail = tankTail (48000.0, bv::TankType::long2Spring, 3.4, 12.0);

    // Split the tail into two bands and measure RT60 of each. A band is the
    // difference of two TPT one-pole low-passes, which is crude but perfectly
    // adequate: we are asserting an ordering with a 30% margin, not
    // characterising the decay curve.
    auto bandpassRt60 = [&tail] (double lowHz, double highHz)
    {
        struct OnePole
        {
            explicit OnePole (double hz) : g (std::tan (kPiM * hz / 48000.0)) {}
            double operator() (double x)
            {
                const double v = (x - s) * g / (1.0 + g);
                const double y = v + s;
                s = y + v;
                return y;
            }
            double g, s = 0.0;
        };

        OnePole upper { highHz }, lower { lowHz };

        std::vector<float> band (tail.size());
        for (size_t i = 0; i < tail.size(); ++i)
        {
            const double x = (double) tail[i];
            band[i] = (float) (upper (x) - lower (x));
        }
        return rt60 (band, 48000.0);
    };

    const auto lowBand  = bandpassRt60 (200.0, 500.0);
    const auto highBand = bandpassRt60 (2000.0, 6000.0);

    REQUIRE (lowBand.sufficientDecay);
    REQUIRE (highBand.sufficientDecay);
    REQUIRE (highBand.seconds < lowBand.seconds * 0.7);
}

TEST_CASE ("short tank decays faster than long tank at default settings", "[.red][spec-001]")
{
    // An ordering assertion across tank types, from the datasheet decay codes:
    // code 1 is 1.2..2.0 s, code 3 is 2.75..4.0 s.
    const auto shortConfig = bv::tankConfigFor (bv::TankType::short2Spring);
    const auto longConfig  = bv::tankConfigFor (bv::TankType::long2Spring);

    REQUIRE (shortConfig.rt60Seconds < longConfig.rt60Seconds);
    REQUIRE (shortConfig.singlePassDelayMs() < longConfig.singlePassDelayMs());

    const auto shortTail = tankTail (48000.0, bv::TankType::short2Spring,
                                     shortConfig.rt60Seconds, 8.0);
    const auto longTail  = tankTail (48000.0, bv::TankType::long2Spring,
                                     longConfig.rt60Seconds, 12.0);

    const auto shortRt = rt60 (shortTail, 48000.0);
    const auto longRt  = rt60 (longTail, 48000.0);

    REQUIRE (shortRt.sufficientDecay);
    REQUIRE (longRt.sufficientDecay);
    REQUIRE (shortRt.seconds < longRt.seconds);
}
