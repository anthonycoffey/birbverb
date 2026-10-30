// SPEC-006 acceptance criterion 6 — the one that matters most.
//
// A stability harness that has never been seen to fail is not known to work. If
// checkStability() passed everything, every stability test in the repo would be
// a green light that means nothing, and we would find out when a user's session
// produced a burst of full-scale noise.
//
// So: run it against a filter that is deliberately unstable and require that it
// fails, and against a filter that is obviously fine and require that it passes.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "support/Approx.h"
#include "support/Generate.h"
#include "support/Measure.h"
#include "support/Stability.h"

using namespace bv::test;

namespace
{
/** y[n] = x[n] + a * y[n-1]. Stable for |a| < 1, divergent otherwise. */
struct OnePoleFeedback
{
    explicit OnePoleFeedback (double coefficient) : a (coefficient) {}

    void operator() (std::vector<float>& buffer)
    {
        for (auto& s : buffer)
        {
            const double y = (double) s + a * y1;
            y1 = y;
            s = (float) y;
        }
    }

    double a, y1 = 0.0;
};

StabilityOptions quickOptions()
{
    StabilityOptions o;
    o.secondsPerCase = 0.25;   // keeps the suite inside its 30 s budget
    return o;
}
} // namespace

//==============================================================================
TEST_CASE ("stability harness detects a divergent filter", "[stability]")
{
    // Pole outside the unit circle: this must be caught.
    const auto result = checkStability (
        [] (std::vector<float>& b) { OnePoleFeedback { 1.01 } (b); },
        {}, quickOptions());

    REQUIRE_FALSE (result.stable);
    REQUIRE_FALSE (result.failedCase.empty());
}

TEST_CASE ("stability harness detects a NaN-producing block", "[stability]")
{
    const auto result = checkStability (
        [] (std::vector<float>& b)
        {
            for (auto& s : b)
                s = s / 0.0f - s / 0.0f;   // NaN
        },
        {}, quickOptions());

    REQUIRE_FALSE (result.stable);
}

TEST_CASE ("stability harness passes a well-behaved filter", "[stability]")
{
    // A pole well inside the unit circle. DC gain is 1/(1-a) = 2, so full-scale
    // DC reaches +6 dB, which is inside the +12 dB limit.
    const auto result = checkStability (
        [] (std::vector<float>& b) { OnePoleFeedback { 0.5 } (b); },
        {}, quickOptions());

    REQUIRE (result.stable);
    REQUIRE (result.failedCase.empty());
    REQUIRE (toDb (result.peak) < 12.0);
}

TEST_CASE ("stability harness passes a pass-through", "[stability]")
{
    const auto result = checkStability ([] (std::vector<float>&) {}, {}, quickOptions());

    REQUIRE (result.stable);
    REQUIRE_THAT (toDb (result.peak), WithinDb (0.0, 0.1));
}

TEST_CASE ("stability harness exercises randomised parameter sweeps", "[stability]")
{
    // The sweep cases are where a real DSP block usually breaks: a coefficient
    // that is fine at both ends of its range and divergent somewhere in between.
    double coefficient = 0.5;
    int sweepCalls = 0;

    const auto result = checkStability (
        [&coefficient] (std::vector<float>& b) { OnePoleFeedback { coefficient } (b); },
        [&coefficient, &sweepCalls] (double t)
        {
            ++sweepCalls;
            coefficient = 0.5 + 0.6 * t;   // crosses 1.0 partway through
        },
        quickOptions());

    REQUIRE (sweepCalls > 0);
    REQUIRE_FALSE (result.stable);
    REQUIRE (result.failedCase == "randomised parameter sweep");
}
