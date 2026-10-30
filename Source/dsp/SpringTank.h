#pragma once

// Spring tank model — SPEC-001.
//
// ============================================================================
//  STATUS: SKELETON. Not implemented.
//
//  This compiles and runs so the plugin builds and the RED tests in
//  tests/test_dispersion.cpp and tests/test_decay.cpp can execute and fail. It
//  currently outputs silence.
//
//  Implementing it is PLAN-001 Phase 1. SPEC-001 is now `ready`: the dispersion
//  curve is chosen and recorded (Parker 2011 eq. 4, physical targets from
//  Bilbao & Parker eqs. 2-4), the closed forms are in Dispersion.h, and every
//  coefficient this class needs comes out of TankConfig.
// ============================================================================
//
// Target structure, per ADR-001 and the dispersion study:
//
//          ┌──────────── one spring, below F_C ───────────┐
//   in ──┬─│ delay(T_D) → A^100(z^k) → loss LPF ──┬───────│──┬─→ sum → out
//        │ │              ↑                       │       │  │
//        │ │              └──── reflection ───────┘       │  │
//        │ └─────────────────────────────────────────────┘   │
//        ├─→ [spring 2, its own T_D and F_C] ────────────────┤
//        ├─→ [spring 3, 3-spring types only] ────────────────┤
//        └─→ [secondary wideband loop, A^200(z), -30 dB] ────┘
//
// Two things to get right that are easy to get wrong:
//
//   1. The main chirp ASCENDS. Group delay rises with frequency and peaks at
//      F_C. The descending chirp is the secondary series, 30 dB down. A model
//      that only descends has modelled the artefact and thrown away the sound.
//   2. The stretch factor k is FRACTIONAL. Rounding it makes F_C jump with the
//      sample rate and breaks SPEC-001 requirement 9. Use
//      dispersion::coeffForFractionalDelay for the remainder.

#include "TankConfig.h"

#include <cmath>
#include <vector>

namespace bv
{

class SpringTank
{
public:
    SpringTank() = default;

    /** Allocates everything. Never called from the audio thread. */
    void prepare (double newSampleRate, int /*maxBlockSize*/ = 512)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        configure (config);
        reset();
    }

    void reset()
    {
        // TODO(SPEC-001): clear delay lines and filter state.
    }

    /** Reconfigure for a tank type or a changed decay. Safe to call per block;
        SPEC-003 requires a 30 ms crossfade when the tank type changes, which
        lives in the engine above this class, not here. */
    void configure (const TankConfig& newConfig)
    {
        config = newConfig;
        singlePassSamples  = config.singlePassDelayMs()  * 0.001 * sampleRate;
        echoSpacingSamples = config.meanEchoSpacingMs() * 0.001 * sampleRate;
        // TODO(SPEC-001): resize delay lines, recompute loss filter coefficients
        //                 from config.loopGain(), and derive each spring's
        //                 cascade from dispersion::stretchFactor() and
        //                 dispersion::coeffForPeakDelay().
    }

    void setDecaySeconds (double rt60)
    {
        config.rt60Seconds = rt60;
        // TODO(SPEC-001): recompute the in-loop loss filter.
    }

    /** One sample in, one sample out. Mono: the tank is a mono device, and any
        stereo behaviour is a liberty that belongs in a higher layer. */
    float processSample (float /*input*/)
    {
        // TODO(SPEC-001): the actual model. Returns silence until then, which is
        //                 why test_dispersion and test_decay are tagged [.red].
        return 0.0f;
    }

    void process (const float* input, float* output, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
            output[n] = processSample (input[n]);
    }

    const TankConfig& getConfig() const noexcept { return config; }
    double getSampleRate()        const noexcept { return sampleRate; }

    /** One-way travel time down the mean spring, in samples. */
    double getSinglePassSamples() const noexcept { return singlePassSamples; }

    /** Spacing between echoes (T_D, a round trip), in samples. This is what the
        autocorrelation test in SPEC-001 criterion 4 keys off. */
    double getEchoSpacingSamples() const noexcept { return echoSpacingSamples; }

private:
    TankConfig config {};
    double sampleRate         = 48000.0;
    double singlePassSamples  = 0.0;
    double echoSpacingSamples = 0.0;
};

} // namespace bv
