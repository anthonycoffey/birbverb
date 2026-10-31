#pragma once

// One modelled spring — SPEC-001.
//
//   in --+--> [half pass] --> band limit --+--> tap (the pickup)
//        |                                  |
//        +-- x g <-- [half pass] <----------+
//
//   [half pass] = delay(plain/2) --> A^(Mlow/2)(z^k)
//
// The loop is one ROUND TRIP of the spring, so its period is the echo spacing
// T_D and successive arrivals are T_D apart — SPEC-001 requirement 5, which is
// what "reflection at both transducers rather than a single pass" means in
// practice.
//
// It is split into two half passes because the pickup transducer sits at the
// FAR END of the spring, not back at the driver. Tapping after a whole round
// trip would put the first arrival at T_D instead of T_D/2 and give the tank a
// silent gap twice as long as the hardware has. Splitting also means the first
// arrival is already dispersed, which matters: it is the loudest chirp in the
// response, and an undispersed first arrival is the single most audible way to
// get this wrong.
//
// Each half carries M/2 sections at the same k and a. Group delay is linear in
// M, so two halves of M/2 disperse exactly as much as one cascade of M, at the
// same cost.
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
    static constexpr int bandLimitPoles = 4;


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

        // Odd section counts give the extra one to the first half, so the
        // total stays exactly what the caller asked for.
        const int sectionsA = (numSections + 1) / 2;
        const int sectionsB = numSections - sectionsA;
        // Each half carries HALF the round trip's dispersion, so its target
        // peak delay is T_D/2. Group delay is linear in M, so M/2 sections at
        // T_D/2 come out with the same coefficient a as M sections at T_D —
        // which is the check that this is right: for Long 2-Spring A at 48 kHz
        // it reproduces the +0.64983 printed in SPEC-001's parameter table.
        allpassA.prepare (sampleRate, transitionHz, echoSpacingMs * 0.5, sectionsA);
        allpassB.prepare (sampleRate, transitionHz, echoSpacingMs * 0.5, sectionsB);

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
        const double cascadeDc = dispersion::groupDelayAtDc (allpassA.getCoefficient(),
                                                             allpassA.getStretchFactor(),
                                                             sectionsA)
                               + dispersion::groupDelayAtDc (allpassB.getCoefficient(),
                                                             allpassB.getStretchFactor(),
                                                             sectionsB);

        // The band limit is in the loop too, so its own delay counts. A
        // one-pole with coefficient b has group delay b/(1-b) at DC.
        const double b = std::exp (-2.0 * dispersion::pi * spring.transitionHz() / sampleRate);
        const double bandDc = (double) bandLimitPoles * b / (1.0 - b);

        plainDelay = std::max (2, (int) std::lround (totalLoop - cascadeDc - bandDc));
        halfDelay  = std::max (1, plainDelay / 2);

        // The band limit sits at F_C because that is where the physics puts it:
        // the audible branch of the dispersion relation has its maximum in
        // frequency versus wavenumber at F_C, so the main series has no
        // propagating solution above it. It is DERIVED, not a tone control —
        // SPEC-001 requirement 6 calls for the series to be "low-passed out
        // rather than allowed to chirp on to Nyquist".
        //
        // INFERRED: the ORDER. No source gives one, so the rule used here is
        // that it is the smallest order meeting acceptance criterion 6 —
        // 24 dB down above 1.5*F_C — measured on the TANK rather than on one
        // spring, which is where the criterion is actually stated.
        //
        // Measured on the Long 2-Spring tank: three poles land 5.5 dB short of
        // criterion 6, four meet it, five and six buy nothing. So four.
        //
        // Note that this is the ONLY filter in the loop. An earlier version of
        // this file also carried a separate FITTED loss filter to force
        // requirement 7, which turned out to be compensating for a factor-four
        // error in the half-cascade targets rather than modelling anything.
        // With that fixed, the band limit alone produces frequency-dependent
        // decay comfortably inside criterion 7, and the fitted constant was
        // removed rather than left to look like physics. SPEC-001 still lists
        // loss filter shape as an open inference, and a real tank's measured
        // IR is what would justify reintroducing one.
        //
        // Order is cheap in the one place it could hurt: three poles add under
        // 4 samples of loop delay against an echo period of ~2500, and that is
        // subtracted below anyway.
        bandCoeff = (float) std::exp (-2.0 * dispersion::pi * transitionHz / sampleRate);


        setDecaySeconds (rt60Seconds);
        smoothedGain = targetGain;      // prepare() lands on the value, not a ramp to it

        lineA.assign ((size_t) halfDelay, 0.0f);
        lineB.assign ((size_t) (plainDelay - halfDelay), 0.0f);
        reset();
    }

    /** Pre-grow the buffers so a later prepare() cannot allocate. SpringTank
        calls this once with the worst case across every tank type, because
        switching tank type happens on the audio thread. */
    void reserveFor (int maxDelaySamples, int maxSections, int maxIntegerDelay)
    {
        lineA.reserve ((size_t) std::max (1, maxDelaySamples));
        lineB.reserve ((size_t) std::max (1, maxDelaySamples));
        allpassA.reserveFor (maxSections, maxIntegerDelay);
        allpassB.reserveFor (maxSections, maxIntegerDelay);
    }

    void reset()
    {
        std::fill (lineA.begin(), lineA.end(), 0.0f);
        std::fill (lineB.begin(), lineB.end(), 0.0f);
        posA = posB = 0;
        std::fill (std::begin (bandState), std::end (bandState), 0.0f);
        allpassA.reset();
        allpassB.reset();
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

        // First half pass: driver to pickup.
        const float toPickup = allpassA.processSample (lineA[(size_t) posA]);

        float limited = toPickup;
        for (int i = 0; i < bandLimitPoles; ++i)
        {
            bandState[i] = bandCoeff * bandState[i] + (1.0f - bandCoeff) * limited;
            limited = bandState[i];
        }

        // Second half pass: pickup back to the driver, then round again.
        const float toDriver = allpassB.processSample (lineB[(size_t) posB]);
        const float looped   = smoothedGain * toDriver;

        lineB[(size_t) posB] = limited;

        // Injection scaled by sqrt(1 - g^2), which makes the loop preserve
        // ENERGY rather than peak amplitude.
        //
        // Without any scaling the loop reaches 1/(1 - g) for sustained input —
        // about 9.8x here — which clips on a held note. But (1 - g) is the
        // wrong correction: that normalises the top of a comb tooth, and this
        // loop's teeth are 18 Hz apart, so what a real signal meets is the
        // average across them, 1/sqrt(1 - g^2). Scaling by (1 - g) was measured
        // at 20 dB under the dry — a reverb nobody would hear.
        //
        // Either way this is not merely a level trim. Without it DECAY IS A
        // VOLUME CONTROL: going from 0.8 s to 4.5 s would raise the wet path
        // by about 19 dB as a side effect, which is not what the control means
        // and not what the hardware does. sqrt(1 - g^2) cancels exactly,
        // leaving decay to change only how long it rings. Using the smoothed
        // gain keeps the two in step while decay moves.
        const float injection = std::sqrt (std::max (0.0f, 1.0f - smoothedGain * smoothedGain));
        lineA[(size_t) posA] = injection * input + looped;

        if (++posA >= (int) lineA.size()) posA = 0;
        if (++posB >= (int) lineB.size()) posB = 0;

        return tapFraction * limited;
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
    const DispersiveAllpass& getAllpass() const noexcept { return allpassA; }

    /** One-way travel time, in samples: where the first arrival lands. */
    int    getHalfDelay()      const noexcept { return halfDelay; }

private:
    DispersiveAllpass allpassA, allpassB;
    std::vector<float> lineA, lineB;

    double sampleRate    = 48000.0;
    double transitionHz  = 4471.0;
    double echoSpacingMs = 52.69;
    double rt60          = 3.4;

    int   plainDelay   = 2;
    int   halfDelay    = 1;
    int   posA         = 0;
    int   posB         = 0;
    float tapFraction  = 0.25f;
    float bandCoeff    = 0.0f;
    float bandState[bandLimitPoles] {};
    float targetGain   = 0.0f;
    float smoothedGain = 0.0f;
};

} // namespace bv
