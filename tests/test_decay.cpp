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
TEST_CASE ("tank RT60 matches the long-decay tank datasheet", "[spec-001]")
{
    // SPEC-001 acceptance criterion 2. 3.4 s is the middle of the 2.75..4.0 s
    // range on the 4AB3C1B datasheet — see circuits/accutronics-spring-tanks.md.
    const auto tail = tankTail (48000.0, bv::TankType::long2Spring, 3.4, 12.0);
    const auto r = rt60 (tail, 48000.0);

    REQUIRE (r.sufficientDecay);
    REQUIRE_THAT (r.seconds, WithinSeconds (3.4, Percent (15.0)));
}

TEST_CASE ("RT60 tracks the decay parameter", "[spec-001]")
{
    // SPEC-001 acceptance criterion 3, across the parameter's declared range.
    const double requested = GENERATE (0.8, 1.6, 2.4, 3.4, 4.5);

    const auto tail = tankTail (48000.0, bv::TankType::long2Spring, requested,
                                requested * 3.0 + 1.0);
    const auto r = rt60 (tail, 48000.0);

    REQUIRE (r.sufficientDecay);
    REQUIRE_THAT (r.seconds, WithinSeconds (requested, Percent (15.0)));
}

TEST_CASE ("RT60 is sample-rate independent", "[spec-001]")
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

TEST_CASE ("high frequencies decay faster than low frequencies", "[spec-001]")
{
    // SPEC-001 acceptance criterion 7. Real tanks lose their top first; a model
    // with flat decay sounds synthetic in a way that is hard to place until you
    // measure it.
    const auto tail = tankTail (48000.0, bv::TankType::long2Spring, 3.4, 12.0);

    // Split the tail into two bands and measure RT60 of each.
    //
    // This used a difference of two TPT one-pole low-passes, on the grounds
    // that it was crude but adequate for an ordering with a 30% margin. It is
    // not adequate, and the reason is worth keeping: that shape leaves roughly
    // -30 dB of out-of-band signal, and low frequencies in this tank ring for
    // three seconds. Once the high band has decayed, the leaked low band is
    // all that is left, the Schroeder slope flattens onto it, and the measured
    // "high" RT60 drifts toward the low one no matter what the filter does.
    // The measurement had a floor around 0.75 that no implementation could get
    // under. Measure::bandLimited is a brick wall and has no such floor.
    auto bandRt60 = [&tail] (double lowHz, double highHz)
    {
        return rt60 (bandLimited (tail, lowHz, highHz, 48000.0), 48000.0);
    };

    const auto lowBand  = bandRt60 (200.0, 500.0);
    const auto highBand = bandRt60 (2000.0, 6000.0);

    REQUIRE (lowBand.sufficientDecay);
    REQUIRE (highBand.sufficientDecay);
    REQUIRE (highBand.seconds < lowBand.seconds * 0.7);
}

TEST_CASE ("short tank decays faster than long tank at default settings", "[spec-001]")
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
