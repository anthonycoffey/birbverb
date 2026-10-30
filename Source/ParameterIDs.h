#pragma once

// The single source of truth for parameter IDs — SPEC-003 requirement 5.
//
// Nothing anywhere else in the codebase may contain a quoted parameter ID
// string. tests/test_parameter_ids.cpp enforces that, because a typo'd ID
// literal produces a parameter that silently does nothing, in a host, in
// someone else's session.
//
// No JUCE here, so the DSP and the tests can both include it.

namespace bv::ids
{

inline constexpr auto circuit = "circuit";   ///< choice: AB763 / 6G16 / 6G15 / Ampeg
inline constexpr auto tank    = "tank";      ///< choice: Short 2 / Long 2 / Long 3 spring
inline constexpr auto dwell   = "dwell";     ///< 0..10    tank drive
inline constexpr auto decay   = "decay";     ///< 0.8..4.5 seconds, tank RT60
inline constexpr auto tone    = "tone";      ///< 0..10    wet-path tone
inline constexpr auto locut   = "locut";     ///< 80..600 Hz, the send high-pass
inline constexpr auto boing   = "boing";     ///< 0..10    dispersion depth (effect control)
inline constexpr auto mix     = "mix";       ///< 0..100 % dry/wet
inline constexpr auto level   = "level";     ///< -12..+12 dB output trim (effect control)
inline constexpr auto bypass  = "bypass";    ///< bool

/// Parameter version hint for APVTS ParameterID. Bump only when a parameter's
/// meaning changes in a way that must not be restored from older sessions.
inline constexpr int version = 1;

} // namespace bv::ids
