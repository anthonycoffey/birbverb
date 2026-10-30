#pragma once

// Spring tank model — SPEC-001.
//
//          +------------ one spring, below F_C -----------+
//   in --+-| delay -> A^100(z^k) -> band limit -> x g --+ |--+--> sum --> out
//        | +--------------------------------------------+ |  |
//        +-> [spring 2, its own T_D and F_C] --------------+  |
//        +-> [spring 3, 3-spring types only] --------------+  |
//        +-> [secondary wideband loop, A^200(z), -30 dB] -----+
//
// Two things that are easy to get backwards, both corrected by the dispersion
// study before any of this was written:
//
//   1. The main chirp ASCENDS. Group delay rises with frequency and peaks at
//      F_C. The descending chirp is the secondary series, 30 dB down. A model
//      that only descends has modelled the artefact and thrown away the sound.
//   2. The stretch factor k is FRACTIONAL. Rounding it makes F_C jump with the
//      sample rate and breaks SPEC-001 requirement 9.
//
// The springs are deliberately mismatched, per type, because the hardware is:
// a 2-spring unit matches echo spacing and spreads F_C, a 3-spring unit does
// the reverse. That comes out of the published geometries on its own — see
// TankConfig, which stores caliper readings and derives everything else.

#include "Dispersion.h"
#include "SpringLine.h"
#include "TankConfig.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace bv
{

class SpringTank
{
public:
    SpringTank() = default;

    /** Allocates everything, for the worst case across every tank type so that
        configure() can run on the audio thread. Never called from it. */
    void prepare (double newSampleRate, int /*maxBlockSize*/ = 512)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

        // Scan all three types so switching between them later cannot grow a
        // buffer. SPEC-003 crossfades a tank change over 30 ms in the engine
        // above; a reallocation underneath that would be an audio-thread
        // allocation however smooth the crossfade looked.
        int maxDelay = 1, maxInteger = 1;
        for (int t = 0; t < numTankTypes; ++t)
        {
            const auto c = tankConfigFor ((TankType) t);
            for (int i = 0; i < c.numSprings; ++i)
            {
                const double loop = c.springs[i].echoSpacingMs() * 0.001 * sampleRate;
                maxDelay = std::max (maxDelay, (int) std::lround (loop) + 2);
                maxInteger = std::max (maxInteger,
                    (int) std::lround (dispersion::stretchFactor (sampleRate,
                                                                  c.springs[i].transitionHz())) + 1);
            }
        }

        for (auto& line : springs)
            line.reserveFor (maxDelay, maxSprings > 0 ? 200 : 1, maxInteger);

        secondaryDelayLine.reserve ((size_t) maxDelay);
        secondaryDelayLineB.reserve ((size_t) maxDelay);
        secondary.reserveFor (200, 2);
        secondaryB.reserveFor (200, 2);

        configure (config);
        reset();
    }

    void reset()
    {
        for (int i = 0; i < activeSprings; ++i)
            springs[(size_t) i].reset();

        secondary.reset();
        secondaryB.reset();
        std::fill (secondaryDelayLine.begin(), secondaryDelayLine.end(), 0.0f);
        std::fill (secondaryDelayLineB.begin(), secondaryDelayLineB.end(), 0.0f);
        secondaryPos = secondaryPosB = 0;
        secondaryLoss = 0.0f;
        secondarySmoothedGain = secondaryGain;
    }

    /** Reconfigure for a tank type or a changed decay. Safe to call per block:
        prepare() has already reserved for the largest type, so nothing here
        allocates. SPEC-003's 30 ms crossfade on a tank-type change lives in the
        engine above this class, not here. */
    void configure (const TankConfig& newConfig)
    {
        config = newConfig;
        activeSprings = std::clamp (config.numSprings, 1, maxSprings);

        singlePassSamples  = config.singlePassDelayMs() * 0.001 * sampleRate;
        echoSpacingSamples = config.meanEchoSpacingMs() * 0.001 * sampleRate;

        for (int i = 0; i < activeSprings; ++i)
            springs[(size_t) i].prepare (sampleRate, config.springs[i], config.rt60Seconds,
                                         config.allpassSectionsLow,
                                         config.reflectionCoefficient);

        // The secondary wideband loop. SPEC-001 and [PARKER11] section 4 give
        // its coefficients outright — k = 1, a_hf = -0.6, M_high = 200 — rather
        // than deriving them from a geometry, so it is configured directly.
        // Negative a puts its group-delay peak at DC, which is what makes this
        // series DESCEND: 16.7 ms at DC, 2.5 ms at 10 kHz, 1.0 ms at Nyquist.
        // Split into two half passes and tapped between them, exactly as
        // SpringLine is, and for the same reason: the pickup transducer is at
        // the far end of the spring. Tapping after a whole round trip put the
        // secondary series' first arrival at T_D while the springs' was at
        // T_D/2, and a late low-frequency arrival dragged the tank's low-band
        // group delay above its high-band — which read as the chirp
        // descending. The two series travel the same spring; they arrive
        // together.
        const int halfSections = config.allpassSectionsHigh / 2;
        secondary.prepareDirect  (sampleRate, 1.0, config.secondaryCoeff, halfSections);
        secondaryB.prepareDirect (sampleRate, 1.0, config.secondaryCoeff,
                                  config.allpassSectionsHigh - halfSections);

        // Same round-trip period as the springs, so the two series stay in step
        // in time; the plain delay is the remainder after the cascade's own
        // group delay at DC, exactly as in SpringLine.
        const double loopSamples = echoSpacingSamples;
        const double cascadeDc   = dispersion::groupDelayAtDc (config.secondaryCoeff, 1.0,
                                                               config.allpassSectionsHigh);
        secondaryDelay = std::max (2, (int) std::lround (loopSamples - cascadeDc));
        const int halfDelay = std::max (1, secondaryDelay / 2);
        secondaryDelayLine.assign ((size_t) halfDelay, 0.0f);
        secondaryDelayLineB.assign ((size_t) (secondaryDelay - halfDelay), 0.0f);

        secondaryLevel = (float) std::pow (10.0, config.secondaryLevelDb / 20.0);

        // The secondary loop loses its top too. INFERRED, from consistency
        // rather than from a source: the damping that costs a spring its high
        // frequencies — internal friction and the transducers' own bandwidth —
        // is a property of the spring and its ends, not of which propagating
        // mode happens to be carrying the energy. Modelling the main series
        // with high-frequency loss and the secondary with none is the
        // inconsistent choice, and it shows up as a measurement: without this
        // the tank's high-frequency RT60 was only 24% shorter than its low, not
        // the 30% requirement 7 asks for, because a -30 dB series that never
        // loses its top still sets the tail.
        //
        // ONE pole, where the main series gets three, and at the mean F_C
        // rather than a per-spring value. [PARKER11] has this series spanning
        // the whole band, so a hard band limit would contradict the source; a
        // single gentle pole is the least that makes the damping consistent.
        const double meanFc = 0.5 * (config.minTransitionHz() + config.maxTransitionHz());
        secondaryLossCoeff = (float) std::exp (-2.0 * dispersion::pi * meanFc / sampleRate);

        // Sum normalisation. Springs are near-independent, so an incoherent sum
        // grows as sqrt(N); dividing by sqrt(N) keeps a 3-spring tank at the
        // same subjective level as a 2-spring one instead of 1.5x louder.
        springNormalise = 1.0f / (float) std::sqrt ((double) activeSprings);

        // Requirement 11 puts the secondary series "at least 30 dB below the
        // main series", so the reference is the main series' OWN level, not the
        // raw loop signal. Each spring tap is already scaled by (1 - reflection)
        // and then by springNormalise, so the secondary has to carry the same
        // scaling before -30 dB means -30 dB. Measured against the loop signal
        // instead, it came out 15 dB down and audibly dominated the top of the
        // band, which is what four of the SPEC-001 dispersion tests caught.
        secondaryScale = secondaryLevel
                       * (float) std::clamp (1.0 - config.reflectionCoefficient, 0.0, 1.0)
                       * springNormalise;

        setDecaySeconds (config.rt60Seconds);
    }

    void setDecaySeconds (double rt60)
    {
        config.rt60Seconds = rt60;
        for (int i = 0; i < activeSprings; ++i)
            springs[(size_t) i].setDecaySeconds (rt60);

        secondaryGain = (float) std::clamp (
            dispersion::loopGainForRt60 (config.meanEchoSpacingMs() * 0.001,
                                         std::max (0.05, rt60)), 0.0, 0.999);
    }

    /** One sample in, one sample out. Mono: the tank is a mono device, and any
        stereo behaviour is a liberty that belongs in a higher layer. */
    float processSample (float input) noexcept
    {
        float sum = 0.0f;
        for (int i = 0; i < activeSprings; ++i)
            sum += springs[(size_t) i].processSample (input);
        sum *= springNormalise;

        // Secondary loop. Same 100 ms smoothing on its gain as the springs use,
        // for the same reason: `decay` automated fast is the documented way to
        // destabilise a loop like this (SPEC-003).
        const float smooth = (float) std::exp (-1.0 / (0.1 * sampleRate));
        secondarySmoothedGain = smooth * secondarySmoothedGain
                              + (1.0f - smooth) * secondaryGain;

        // First half pass: driver to pickup, where the tap is.
        const float toPickup = secondary.processSample (secondaryDelayLine[(size_t) secondaryPos]);
        secondaryLoss = secondaryLossCoeff * secondaryLoss
                      + (1.0f - secondaryLossCoeff) * toPickup;

        // Second half pass: pickup back to the driver.
        const float toDriver = secondaryB.processSample (secondaryDelayLineB[(size_t) secondaryPosB]);
        const float looped   = secondarySmoothedGain * toDriver;

        // Same energy-preserving injection scaling as the springs — SpringLine.
        const float injection = std::sqrt (std::max (0.0f,
            1.0f - secondarySmoothedGain * secondarySmoothedGain));

        secondaryDelayLineB[(size_t) secondaryPosB] = secondaryLoss;
        secondaryDelayLine[(size_t) secondaryPos]   = injection * input + looped;

        if (++secondaryPos  >= (int) secondaryDelayLine.size())  secondaryPos  = 0;
        if (++secondaryPosB >= (int) secondaryDelayLineB.size()) secondaryPosB = 0;

        return sum + secondaryScale * secondaryLoss;
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
    std::array<SpringLine, (size_t) maxSprings> springs {};
    DispersiveAllpass secondary, secondaryB;
    std::vector<float> secondaryDelayLine, secondaryDelayLineB;

    TankConfig config {};
    double sampleRate         = 48000.0;
    double singlePassSamples  = 0.0;
    double echoSpacingSamples = 0.0;

    int   activeSprings  = 2;
    float springNormalise = 1.0f;

    int   secondaryDelay         = 1;
    int   secondaryPos           = 0;
    int   secondaryPosB          = 0;
    float secondaryLevel         = 0.0316f;
    float secondaryScale         = 0.0079f;
    float secondaryGain          = 0.0f;
    float secondaryLossCoeff     = 0.0f;
    float secondaryLoss          = 0.0f;
    float secondarySmoothedGain  = 0.0f;
};

} // namespace bv
