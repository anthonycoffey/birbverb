#pragma once

// The modified marker and the preset display string — SPEC-004 requirement 4.
//
// JUCE-free, because the question "what does a fresh instance display?" is a
// property of the preset table and the parameter defaults, and both of those are
// JUCE-free. Keeping the comparison here means the test suite checks the same
// code the UI calls rather than a re-implementation of it (ADR-004).
//
// UI thread only. presetDisplayName allocates; nothing on the audio thread calls
// it, and nothing on the audio thread should.

#include "ParameterDefaults.h"
#include "PresetData.h"

#include <cmath>
#include <string>

namespace bv
{

/** A snapshot of the nine stored parameters, denormalised. `bypass` is absent
    because a preset does not store it — the footswitch is not part of a voice. */
struct ParamValues
{
    int    circuit;
    int    tank;
    float  dwell;
    double decay;
    float  tone;
    double locut;
    float  boing;
    float  mix;
    float  level;
};

/** The defaults as a snapshot — the state of a freshly constructed plugin
    before any preset load or state restore. */
inline constexpr ParamValues defaultParamValues()
{
    return { (int) defaults::circuit, (int) defaults::tank, defaults::dwell,
             defaults::decay, defaults::tone, defaults::locut,
             defaults::boing, defaults::mix, defaults::level };
}

/** Per-parameter tolerances for the modified marker — SPEC-004 edge case
    "nudged and nudged back". A value that round-trips through normalisation is
    not bit-identical, so an exact comparison would leave the marker lit on a
    preset nobody touched. Each tolerance is below the smallest step the UI can
    produce for that parameter, so a real edit always registers. */
namespace tolerance
{
inline constexpr float  knob  = 0.02f;   ///< the 0..10 controls, step 0.01
inline constexpr double decay = 0.02;    ///< seconds, step 0.01
inline constexpr double locut = 1.0;     ///< Hz, step 1
inline constexpr float  gain  = 0.05f;   ///< mix % and level dB, step 0.1
} // namespace tolerance

/** True when every parameter still sits on the preset's stored value. */
inline bool matchesPreset (const Preset& p, const ParamValues& v)
{
    return v.circuit == (int) p.circuit
        && v.tank    == (int) p.tank
        && std::abs (v.dwell - p.dwell) <= tolerance::knob
        && std::abs (v.decay - p.decay) <= tolerance::decay
        && std::abs (v.tone  - p.tone)  <= tolerance::knob
        && std::abs (v.locut - p.locut) <= tolerance::locut
        && std::abs (v.boing - p.boing) <= tolerance::knob
        && std::abs (v.mix   - p.mix)   <= tolerance::gain
        && std::abs (v.level - p.level) <= tolerance::gain;
}

/** What the preset display shows for a factory selection: the preset name, with
    " *" appended once a value has drifted. An index outside the bank is not a
    factory preset at all and reads "Manual" — PresetManager substitutes the
    user preset's name when there is one. */
inline std::string presetDisplayName (int index, const ParamValues& v)
{
    if (index < 0 || index >= numFactoryPresets)
        return "Manual";

    const auto& p = factoryPresets[(size_t) index];
    return std::string (p.name) + (matchesPreset (p, v) ? "" : " *");
}

} // namespace bv
