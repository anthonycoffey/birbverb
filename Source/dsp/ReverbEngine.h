#pragma once

// The engine: everything between the plugin's input and output.
//
// ============================================================================
//  STATUS: the tank is real. The circuit stages are not.
//
//  The dry path, mix, level, bypass and SpringTank are all implemented, so the
//  plugin makes spring reverb. What is still missing is SPEC-002: the send
//  high-pass, the driver and transformer, the recovery stage and the per-model
//  mix network. Until those exist the four circuit models sound the same as
//  each other, because the circuit parameter has nothing to select between yet.
// ============================================================================

#include "CircuitModel.h"
#include "EngineParams.h"
#include "SpringTank.h"
#include "TankConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace bv
{

class ReverbEngine
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double newSampleRate, int numChannels, int maxBlockSize)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        channels   = std::clamp (numChannels, 1, maxChannels);

        for (int ch = 0; ch < channels; ++ch)
            tanks[(size_t) ch].prepare (sampleRate, maxBlockSize);

        applyParams (params, true);
        reset();
    }

    void reset()
    {
        for (auto& t : tanks)
            t.reset();
        smoothedMix   = targetMix;
        smoothedLevel = targetLevel;
    }

    /** Called once per block from the processor. Cheap: no allocation, no locks. */
    void setParams (const EngineParams& p) { applyParams (p, false); }

    const EngineParams& getParams() const noexcept { return params; }

    /** In-place, per channel. */
    void process (float* const* channelData, int numChannels, int numSamples)
    {
        const int n = std::min (numChannels, channels);

        for (int ch = 0; ch < n; ++ch)
        {
            auto& tank = tanks[(size_t) ch];
            float* data = channelData[ch];

            for (int i = 0; i < numSamples; ++i)
            {
                const float dry = data[i];

                // TODO(SPEC-002): send high-pass -> driver -> transformer -> tank
                //                 -> recovery -> tone -> mix network. The tank is
                //                 real; everything around it is still missing.
                const float wet = recoveryMakeup * tank.processSample (dry);

                smoothedMix   += (targetMix   - smoothedMix)   * mixSmoothingCoeff;
                smoothedLevel += (targetLevel - smoothedLevel) * mixSmoothingCoeff;

                const float mixed = dry * (1.0f - smoothedMix) + wet * smoothedMix;
                data[i] = params.bypass ? dry : mixed * smoothedLevel;
            }
        }

        // Mono source feeding a stereo bus: copy rather than leaving silence.
        for (int ch = n; ch < numChannels; ++ch)
            std::copy (channelData[0], channelData[0] + numSamples, channelData[ch]);
    }

private:
    void applyParams (const EngineParams& p, bool force)
    {
        const bool tankChanged  = force || p.tank != params.tank;
        const bool decayChanged = force || p.decay != params.decay;
        params = p;

        targetMix   = std::clamp (p.mix, 0.0f, 100.0f) * 0.01f;
        targetLevel = std::pow (10.0f, std::clamp (p.level, -12.0f, 12.0f) / 20.0f);

        if (tankChanged)
        {
            auto config = tankConfigFor (p.tank);
            config.rt60Seconds = std::clamp (p.decay, 0.8, 4.5);
            for (int ch = 0; ch < channels; ++ch)
                tanks[(size_t) ch].configure (config);

            // Stands in for SPEC-002's recovery stage. The pickup transducer
            // passes on only what it does not reflect, so the tank's tap is
            // scaled by (1 - reflection) — and on the hardware a recovery amp
            // is what puts that back before the mix network sees it. Without
            // one the wet path sits about 17 dB under the dry and the plugin
            // reads as broken rather than as quiet. This is not a fitted
            // level: it is exactly the tap fraction, inverted. SPEC-002
            // replaces it with the real stage, which has a gain per circuit.
            recoveryMakeup = (float) (1.0 / std::max (0.05, 1.0 - config.reflectionCoefficient));
        }
        else if (decayChanged)
        {
            // Decay alone never needs a reconfigure, and must not get one:
            // configure() re-derives every spring, while setDecaySeconds only
            // moves the loop gain, which is already smoothed at 100 ms.
            for (int ch = 0; ch < channels; ++ch)
                tanks[(size_t) ch].setDecaySeconds (std::clamp (p.decay, 0.8, 4.5));
        }

        // 20 ms one-pole smoothing, per SPEC-003's smoothing table.
        mixSmoothingCoeff = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));

        if (force)
        {
            smoothedMix   = targetMix;
            smoothedLevel = targetLevel;
        }
    }

    std::array<SpringTank, (size_t) maxChannels> tanks {};
    EngineParams params {};

    double sampleRate = 48000.0;
    int    channels   = 2;

    float targetMix = 0.3f,   smoothedMix = 0.3f;
    float targetLevel = 1.0f, smoothedLevel = 1.0f;
    float mixSmoothingCoeff = 0.001f;
    float recoveryMakeup    = 4.0f;
};

} // namespace bv
