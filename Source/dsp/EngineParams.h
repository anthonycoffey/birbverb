#pragma once

// Denormalised parameter values handed from the processor to the engine each
// block — SPEC-003.
//
// Plain struct, no JUCE, so the DSP tests can construct one without a host. The
// processor's job is to turn APVTS values into this; the engine's job is to
// never know APVTS exists.

#include "CircuitModel.h"
#include "TankConfig.h"

namespace bv
{

struct EngineParams
{
    CircuitModel circuit = CircuitModel::ab763;
    TankType     tank    = TankType::long2Spring;

    float  dwell  = 5.0f;    ///< 0..10   tank drive
    double decay  = 3.4;     ///< 0.8..4.5 seconds, RT60
    float  tone   = 5.0f;    ///< 0..10   wet-path tone
    double locut  = 318.0;   ///< 80..600 Hz, send high-pass
    float  boing  = 5.0f;    ///< 0..10   dispersion depth (effect control)
    float  mix    = 30.0f;   ///< 0..100 %
    float  level  = 0.0f;    ///< -12..+12 dB (effect control)
    bool   bypass = false;
};

} // namespace bv
