#pragma once

// Worst-case-input harness — SPEC-006 requirement 8.
//
// Throws DC, full-scale noise, and randomised parameter sweeps at a DSP block and
// reports whether the output stayed finite and bounded. A reverb with a feedback
// loop in it will eventually find a way to blow up, and it will find it in
// someone's session rather than in a test unless something like this runs.

#include "Generate.h"
#include "Measure.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace bv::test
{

struct StabilityResult
{
    bool   stable = true;
    double peak = 0.0;          ///< linear peak seen across all cases
    std::string failedCase;     ///< which case broke it, empty when stable

    explicit operator bool() const { return stable; }
};

struct StabilityOptions
{
    double sampleRate      = 48000.0;
    double secondsPerCase  = 2.0;
    double peakLimitDb     = 12.0;   ///< SPEC-001/002 bound the output here
    std::uint64_t seed     = 0x5EEDull;
    int    numSweepCases   = 8;
};

/** `process` takes a buffer and processes it in place.
    `randomiseParams` is called before each sweep case with a value in [0, 1];
    pass an empty function for a block with no parameters. */
inline StabilityResult checkStability (
    const std::function<void (std::vector<float>&)>& process,
    const std::function<void (double)>& randomiseParams = {},
    StabilityOptions options = {})
{
    StabilityResult result;
    const int n = (int) (options.sampleRate * options.secondsPerCase);
    const double limit = fromDb (options.peakLimitDb);

    auto runCase = [&] (std::vector<float> buffer, const char* name)
    {
        if (! result.stable) return;

        process (buffer);

        const double p = peak (buffer);
        result.peak = std::max (result.peak, p);

        if (! allFinite (buffer) || p > limit)
        {
            result.stable = false;
            result.failedCase = name;
        }
    };

    runCase (silence (n),                           "silence");
    runCase (dc (n, 1.0f),                          "full-scale DC");
    runCase (dc (n, -1.0f),                         "full-scale negative DC");
    runCase (noise (n, options.seed, 1.0f),         "full-scale white noise");
    runCase (impulse (n, 1.0f),                     "full-scale impulse");
    runCase (sweep (n, 20.0, 20000.0, options.sampleRate, 1.0f), "full-scale sweep");

    // Alternating full-scale: the worst case for anything with feedback.
    {
        std::vector<float> square ((size_t) n);
        for (int i = 0; i < n; ++i)
            square[(size_t) i] = (i % 2 == 0) ? 1.0f : -1.0f;
        runCase (std::move (square), "full-scale nyquist square");
    }

    if (randomiseParams)
    {
        for (int c = 0; c < options.numSweepCases && result.stable; ++c)
        {
            randomiseParams ((double) c / std::max (1, options.numSweepCases - 1));
            runCase (noise (n, options.seed + (std::uint64_t) c + 1, 1.0f),
                     "randomised parameter sweep");
        }
    }

    return result;
}

} // namespace bv::test
