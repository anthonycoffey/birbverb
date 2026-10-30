#pragma once

// One modelled spring — SPEC-001.
//
//   in --+--> delay(plain) --> A^Mlow(z^k) --> band limit --> x g --+--> tap
//        |                                                          |
//        +----------------------------------------------------------+
//
// The loop is one ROUND TRIP of the spring, so its period is the echo spacing
// T_D and successive arrivals are T_D apart — SPEC-001 requirement 5, which is
// what "reflection at both transducers rather than a single pass" means in
// practice.
//
// Three things in here are worth reading the comments for: how the plain delay
// is sized against the cascade's own group delay, why the band limit is DERIVED
// rather than fitted, and why requirement 7 falls out of the band limit instead
// of needing a tone control bolted into the loop.

#include "DispersiveAllpass.h"
#include "TankConfig.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace bv
{

class SpringLine
{
public:
    /** Order of the in-loop band limit at F_C. INFERRED — see prepare(). */
    static constexpr int bandLimitPoles = 3;

    SpringLine() = default;

    /** Allocates. Never called from the audio thread. */
    void prepare (double newSampleRate, const SpringParams& spring,
                  double rt60Seconds, int numSections = 100,
                  double reflection = 0.75)
    {
        sampleRate    = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        transitionHz  = spring.transitionHz();
        echoSpacingMs = spring.echoSpacingMs();
        tapFraction   = (float) std::clamp (1.0 - reflection, 0.0, 1.0);

        allpass.prepare (sampleRate, transitionHz, echoSpacingMs, numSections);

        // The loop has to come out at T_D in total, and the cascade already
        // contributes its own group delay — D(DC) at the bottom of the band,
        // rising to T_D itself at F_C. Sizing the plain delay as the REMAINDER
        // at DC is what makes the echo period T_D where the energy actually is.
        //
        // The consequence is the model, not a side effect: at F_C the loop is
        // about twice as long, so that energy makes half as many round trips
        // per second. That is dispersion expressed as decay, and it is the
        // tension SPEC-001 requirement 7 is about.
        const double totalLoop = echoSpacingMs * 0.001 * sampleRate;
        const double cascadeDc = dispersion::groupDelayAtDc (allpass.getCoefficient(),
                                                             allpass.getStretchFactor(),
                                                             numSections);

        // The band limit is in the loop too, so its own delay counts. A
        // one-pole with coefficient b has group delay b/(1-b) at DC.
        const double b = std::exp (-2.0 * dispersion::pi * spring.transitionHz() / sampleRate);
        const double bandDc = (double) bandLimitPoles * b / (1.0 - b);

        plainDelay = std::max (1, (int) std::lround (totalLoop - cascadeDc - bandDc));

        // The band limit sits at F_C because that is where the physics puts it:
        // the audible branch of the dispersion relation has its maximum in
        // frequency versus wavenumber at F_C, so the main series has no
        // propagating solution above it. It is DERIVED, not a tone control —
        // SPEC-001 requirement 6 calls for the series to be "low-passed out
        // rather than allowed to chirp on to Nyquist".
        //
        // INFERRED: the ORDER. No source gives one, so the rule used here is
        // that it is the smallest order meeting acceptance criterion 6 —
        // 24 dB down above 1.5*F_C. Measured: two poles reach 18.8 dB and
        // three reach criterion. Order is cheap in the one place it could
        // hurt, since three poles add under 4 samples of loop delay against an
        // echo period of about 2500, and that is subtracted below anyway.
        bandCoeff = (float) std::exp (-2.0 * dispersion::pi * transitionHz / sampleRate);

        setDecaySeconds (rt60Seconds);
        smoothedGain = targetGain;      // prepare() lands on the value, not a ramp to it

        line.assign ((size_t) plainDelay, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (line.begin(), line.end(), 0.0f);
        writePos = 0;
        std::fill (std::begin (bandState), std::end (bandState), 0.0f);
        allpass.reset();
        smoothedGain = targetGain;
    }

    /** Per-round-trip loop gain for the requested RT60, g = 10^(-3*T/RT60).

        Safe to call per block. The coefficient is smoothed at 100 ms rather
        than the 20 ms the other parameters use, because `decay` automated fast
        is the one documented way to destabilise this loop — SPEC-003. */
    void setDecaySeconds (double rt60Seconds)
    {
        rt60 = std::max (0.05, rt60Seconds);
        targetGain = (float) std::clamp (
            dispersion::loopGainForRt60 (echoSpacingMs * 0.001, rt60), 0.0, 0.999);
    }

    float processSample (float input) noexcept
    {
        // 100 ms one-pole on the loop gain. Computed here rather than cached so
        // a sample-rate change cannot leave a stale coefficient behind.
        const float smooth = (float) std::exp (-1.0 / (0.1 * sampleRate));
        smoothedGain = smooth * smoothedGain + (1.0f - smooth) * targetGain;

        const float delayed   = line[(size_t) writePos];
        const float dispersed = allpass.processSample (delayed);

        float limited = dispersed;
        for (int i = 0; i < bandLimitPoles; ++i)
        {
            bandState[i] = bandCoeff * bandState[i] + (1.0f - bandCoeff) * limited;
            limited = bandState[i];
        }

        const float looped = smoothedGain * limited;

        line[(size_t) writePos] = input + looped;
        if (++writePos >= plainDelay) writePos = 0;

        // The pickup transducer passes on what it does not reflect. The
        // reflection coefficient scales the TAP and not the decay, because
        // decay has to track the `decay` control to within 15% and that is the
        // loss filter's job — see TankConfig, where the coefficient is FITTED
        // with no measurement behind it.
        return tapFraction * looped;
    }

    void process (const float* input, float* output, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
            output[n] = processSample (input[n]);
    }

    double getSampleRate()     const noexcept { return sampleRate; }
    double getEchoSpacingMs()  const noexcept { return echoSpacingMs; }
    double getTransitionHz()   const noexcept { return transitionHz; }
    double getRt60Seconds()    const noexcept { return rt60; }
    int    getPlainDelay()     const noexcept { return plainDelay; }
    double getLoopGain()       const noexcept { return targetGain; }
    const DispersiveAllpass& getAllpass() const noexcept { return allpass; }

private:
    DispersiveAllpass allpass;
    std::vector<float> line;

    double sampleRate    = 48000.0;
    double transitionHz  = 4471.0;
    double echoSpacingMs = 52.69;
    double rt60          = 3.4;

    int   plainDelay   = 1;
    int   writePos     = 0;
    float tapFraction  = 0.25f;
    float bandCoeff    = 0.0f;
    float bandState[bandLimitPoles] {};
    float targetGain   = 0.0f;
    float smoothedGain = 0.0f;
};

} // namespace bv
