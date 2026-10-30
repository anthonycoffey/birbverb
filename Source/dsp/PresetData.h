#pragma once

// Factory preset data — SPEC-004.
//
// JUCE-free so the preset table is reachable from the DSP tests; PresetManager
// wraps it with the file and APVTS handling. SPEC-004's Constraints require this
// split for exactly that reason.
//
// A preset is a FULL snapshot of all ten parameters, including circuit and tank.
// That is the whole idea: selecting a preset should put the player on a complete,
// coherent foundation, not on four parameters plus whatever was left over.

#include "CircuitModel.h"
#include "TankConfig.h"

#include <array>

namespace bv
{

struct Preset
{
    const char*  name;              ///< <= 16 chars, to fit the UI preset display
    const char*  description;
    int          derivedFrom;       ///< index of the foundation preset, or -1 if this IS one

    CircuitModel circuit;
    TankType     tank;
    float        dwell;
    double       decay;
    float        tone;
    double       locut;
    float        boing;
    float        mix;
    float        level;
};

/** Foundation presets first (indices 0..4), then the derived presets.

    Foundation values are the circuit-correct settings from the circuit studies.
    Derived values are a FIRST DRAFT written without an engine to listen to —
    SPEC-004's task list requires listening to each one before these are
    considered final, and that task is not optional. They will be wrong.

    Descriptions are worded to the confidence of the study behind them:
    medium-confidence circuits are described as the circuit, low-confidence ones
    as "voiced after" it. SPEC-004 requirement 8.
*/
inline constexpr std::array<Preset, 19> factoryPresets
{{
    //--- Foundations -----------------------------------------------------------
    { "Twin '64", "Blackface AB763 reverb. Bright, splashy, sitting behind the dry.",
      -1, CircuitModel::ab763,          TankType::long2Spring,  5.0f, 3.4, 5.5f, 318.0, 5.0f, 28.0f, 0.0f },

    { "Super '63", "The same AB763 circuit, voiced a touch wetter and brighter.",
      -1, CircuitModel::ab763,          TankType::long2Spring,  5.5f, 3.2, 6.0f, 318.0, 5.0f, 32.0f, 0.0f },

    { "Vibroverb '63", "Voiced after the 1963 brown Vibroverb. Darker and more present than a blackface.",
      -1, CircuitModel::vibroverb6G16,  TankType::long2Spring,  4.5f, 3.0, 3.5f, 318.0, 4.5f, 38.0f, 0.0f },

    { "Drip Tank", "The outboard 6G15. A power tube drives the tank, so dwell goes from clean to crashing.",
      -1, CircuitModel::reverbUnit6G15, TankType::long2Spring,  7.5f, 3.6, 6.5f, 300.0, 6.5f, 45.0f, 0.0f },

    // Named for the model designation, not the year: Ampeg's plain Rocket (R-12)
    // is a different amp with no reverb at all, so "Rocket '61" read ambiguously.
    // The revision modelled is the first-edition R-12-R, May 1961 — the one with
    // a dated factory schematic. See circuits/ampeg-reverberocket.md.
    { "Rocket R-12-R", "Voiced after the 1961 Ampeg Reverberocket, the first production amp with reverb. No reverb transformer, so the springs are driven gently.",
      -1, CircuitModel::ampegRocket,    TankType::short2Spring, 6.0f, 2.0, 3.0f, 280.0, 3.5f, 35.0f, 1.0f },

    //--- Derived ---------------------------------------------------------------
    { "Surf Crash", "Dwell cranked until the tank splatters.",
      3,  CircuitModel::reverbUnit6G15, TankType::long2Spring,  9.5f, 4.2, 7.5f, 300.0, 8.0f, 60.0f, 0.0f },

    { "Wet Twin", "A blackface with the reverb knob past where it's polite.",
      0,  CircuitModel::ab763,          TankType::long2Spring,  6.5f, 3.8, 5.5f, 318.0, 5.0f, 55.0f, 0.0f },

    { "Dry Kiss", "Just enough air to tell it's there.",
      0,  CircuitModel::ab763,          TankType::long2Spring,  3.5f, 3.4, 5.0f, 318.0, 5.0f, 12.0f, 0.0f },

    { "Dark Wash", "Lo cut opened up and the top rolled off. Thick.",
      2,  CircuitModel::vibroverb6G16,  TankType::long2Spring,  4.5f, 3.6, 2.0f, 200.0, 4.5f, 45.0f, 0.0f },

    { "Clang", "Dispersion pushed past any real spring. Metallic, chirping.",
      1,  CircuitModel::ab763,          TankType::long2Spring,  5.5f, 2.2, 8.0f, 318.0, 9.5f, 40.0f, 0.0f },

    { "Slapback", "Short tank, fast decay. Nearly a slapback.",
      0,  CircuitModel::ab763,          TankType::short2Spring, 6.0f, 0.9, 5.5f, 318.0, 5.0f, 30.0f, 0.0f },

    { "Cavern", "Three springs at full decay.",
      0,  CircuitModel::ab763,          TankType::long3Spring,  5.0f, 4.5, 4.5f, 318.0, 5.0f, 55.0f, 0.0f },

    { "Pedal Steel", "Bass cut hard so chords stay clear under the wash.",
      0,  CircuitModel::ab763,          TankType::long2Spring,  4.5f, 3.0, 7.0f, 420.0, 5.0f, 48.0f, 0.0f },

    { "Bloom", "The Ampeg's soft drive into a long tank. Slow swell, no drip.",
      4,  CircuitModel::ampegRocket,    TankType::long3Spring,  7.0f, 4.0, 3.5f, 280.0, 2.5f, 50.0f, 1.0f },

    { "Hard Pick", "Saturating driver, short tail, cut low. Reacts to attack.",
      3,  CircuitModel::reverbUnit6G15, TankType::long2Spring,  8.5f, 2.6, 6.0f, 380.0, 6.5f, 25.0f, 0.0f },

    // Ampeg revised the Reverberocket's reverb chain in September 1962: V3a plate
    // 5.6k -> 2.2k and cathode 1.5k -> 330R, a large gain increase with far less
    // cathode degeneration. Those values are ANECDOTAL (forum comparison of two
    // factory schematics), so this ships as a preset approximation — more dwell,
    // hotter return — rather than as a second circuit model. It gets promoted if
    // a primary source confirms them.
    { "Rocket '62", "The September 1962 Reverberocket revision, approximated: more gain into the tank, hotter return.",
      4,  CircuitModel::ampegRocket,    TankType::short2Spring, 7.5f, 2.1, 3.5f, 280.0, 4.0f, 40.0f, 1.0f },

    // The Echo Jet is a DIFFERENT design — 1963, 6D10 preamp, 2x 7591A, and a
    // speaker-driven tank on its own physically separated speaker. We have no
    // schematic, so it is voiced on the Reverberocket circuit rather than given a
    // circuit model built out of guesses. Its quasi-stereo wet/dry separation is
    // not modelled at all; that needs a parameter birbverb does not have.
    { "Echo Jet '63", "Voiced after the Ampeg Echo Jet. Softer and wetter than the Reverberocket. The real one's separated reverb speaker is not modelled.",
      4,  CircuitModel::ampegRocket,    TankType::short2Spring, 5.0f, 1.8, 4.0f, 300.0, 3.0f, 55.0f, 1.5f },

    { "Brown Verb", "Mid-forward and close. The brownface voicing without the wash.",
      2,  CircuitModel::vibroverb6G16,  TankType::long2Spring,  5.5f, 2.6, 4.0f, 250.0, 5.0f, 42.0f, 0.0f },

    { "Big Stage", "Three springs, long decay, plenty of wet. A 4x10 in a room.",
      1,  CircuitModel::ab763,          TankType::long3Spring,  6.0f, 4.0, 6.5f, 318.0, 5.5f, 50.0f, 0.0f },
}};

inline constexpr int numFoundationPresets = 5;
inline constexpr int numFactoryPresets = (int) factoryPresets.size();

} // namespace bv
