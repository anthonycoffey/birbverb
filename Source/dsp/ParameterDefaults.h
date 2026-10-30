#pragma once

// The parameter default column — SPEC-003.
//
// JUCE-free so the tests can read the defaults without linking a host, which is
// what makes the boot state assertable at all (ADR-004: the test target has no
// JUCE). createParameterLayout() builds the APVTS layout from this table, so
// there is exactly one place a default value is written.
//
// These are not neutral round numbers. They are the AB763 circuit-correct
// position, which is also SPEC-004's "Twin '64" foundation row, because the
// plugin boots with that preset selected (bootPresetIndex below) and a boot
// state that disagrees with the selected preset lights the modified marker
// before the player has touched anything. tests/test_preset_boot.cpp asserts
// the two tables agree; if one moves, the other moves with it.

#include "CircuitModel.h"
#include "TankConfig.h"

namespace bv::defaults
{

inline constexpr CircuitModel circuit = CircuitModel::ab763;      ///< blackface AB763
inline constexpr TankType     tank    = TankType::long2Spring;    ///< 4AB3C1B, the blackface tank
inline constexpr float        dwell   = 5.0f;
inline constexpr double       decay   = 3.4;   ///< middle of the 4AB3C1B datasheet range
inline constexpr float        tone    = 5.5f;  ///< AB763 wet path: slightly bright of centre
inline constexpr double       locut   = 318.0; ///< the AB763 send high-pass, 500 pF into 1 M
inline constexpr float        boing   = 5.0f;  ///< the fitted dispersion curve, unexaggerated
inline constexpr float        mix     = 28.0f; ///< where Fender sat the recovery behind the dry
inline constexpr float        level   = 0.0f;
inline constexpr bool         bypass  = false;

/** The factory preset the plugin boots with — SPEC-003's boot state.

    Index into bv::factoryPresets. Deliberately a foundation preset: the first
    thing a player hears should be a researched circuit, not a blend of round
    numbers. The values above ARE this preset's values, which is the whole
    reason the boot state reads as unmodified.
*/
inline constexpr int bootPresetIndex = 0;   // "Twin '64"

} // namespace bv::defaults
