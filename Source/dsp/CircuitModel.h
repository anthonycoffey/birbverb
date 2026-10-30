#pragma once

// Per-amp circuit configuration — SPEC-002.
//
// ============================================================================
//  STATUS: data complete, DSP not implemented.
//
//  The configurations below carry the researched values from
//  docs/documentation/circuits/. The stages that consume them (SendFilter,
//  TankLoad, DriveTransformer, TriodeStage, PentodeStage, RecoveryStage,
//  MixNetwork) are PLAN-001 Phase 2.
// ============================================================================
//
// EVERY value carries its source or the word INFERRED / FITTED. This is the most
// important convention in the codebase: the project's claim is that behaviour
// traces to hardware, and a constant without provenance quietly breaks that
// claim. See docs/documentation/development-standards.md, "The research rule".

#include <cstddef>

namespace bv
{

enum class CircuitModel
{
    ab763 = 0,      ///< Fender blackface — Twin Reverb, Super Reverb, Vibroverb '64
    vibroverb6G16,  ///< Fender Vibroverb 6G16, 1963 brown
    reverbUnit6G15, ///< Fender outboard Reverb Unit 6G15
    ampegRocket     ///< Ampeg Reverberocket R-12-R
};

inline constexpr int numCircuitModels = 4;

enum class DriverType
{
    triodePair = 0, ///< 12AT7, both halves in parallel (AB763)
    triodeSingle,   ///< one triode half (6G16, Ampeg)
    powerPentode    ///< 6K6GT (6G15)
};

enum class MixTopology
{
    highImpedanceBright = 0, ///< AB763: 3.3 M mix resistor with 10 pF across it
    resistive,               ///< 6G16: simple ~470 k resistive mix, no bright cap
    linearPot                ///< 6G15: 250 k linear-taper blend
};

struct CircuitConfig
{
    //--------------------------------------------------------------------------
    // Send

    /// High-pass corner before the driver, Hz. This is the single most
    /// consequential component in a Fender reverb: the tank's input impedance
    /// rises with frequency, so without it bass dominates the tank drive.
    double sendHighPassHz = 318.0;

    //--------------------------------------------------------------------------
    // Driver

    DriverType driver = DriverType::triodePair;

    /// Is the tank transformer-coupled? The Ampeg Reverberocket has no reverb
    /// transformer at all, and that single fact is why it sounds nothing like a
    /// Fender.
    bool hasTransformer = true;

    /// Transformer primary impedance, ohms. Fender 125A20B / Hammond 1750A.
    double transformerPrimaryOhms = 22800.0;

    /// Grid drive for nominal tank current (28 mA RMS) at 1 kHz, volts peak.
    double nominalDriveVoltsPeak = 0.460;

    /// Relative tank drive capability, 1.0 = the AB763 reference. The ordering
    /// between models is what the cross-model tests assert.
    double driveScale = 1.0;

    //--------------------------------------------------------------------------
    // Recovery and mix

    /// Recovery stage gain, linear.
    double recoveryGain = 20.0;

    MixTopology mixTopology = MixTopology::highImpedanceBright;

    /// Wet injection level relative to the AB763 reference, linear.
    double wetInjectionScale = 1.0;

    /// Does the unit have a wet-path tone control in hardware?
    bool hasToneControl = false;

    /// Does the unit expose a dwell control in hardware?
    bool hasDwellControl = false;
};

/** Configuration for one of the four modelled circuits.

    Source keys:
      [AB763]  circuits/fender-ab763-reverb.md
      [6G16]   circuits/fender-6g16-vibroverb.md
      [6G15]   circuits/fender-6g15-reverb-unit.md
      [AMPEG]  circuits/ampeg-reverberocket.md
*/
inline CircuitConfig circuitConfigFor (CircuitModel model)
{
    CircuitConfig c;

    switch (model)
    {
        case CircuitModel::ab763:
            // [AB763] 500 pF into a 1 M grid leak gives a 318 Hz corner, -12 dB
            // at low E. Both halves of a 12AT7 in parallel into a 22.8 k : 8 ohm
            // transformer. Mix is a 3.3 M resistor with 10 pF across it.
            c.sendHighPassHz          = 318.0;
            c.driver                  = DriverType::triodePair;
            c.hasTransformer          = true;
            c.transformerPrimaryOhms  = 22800.0;
            c.nominalDriveVoltsPeak   = 0.460;   // [AB763] source gives 447 and 477; open question
            c.driveScale              = 1.0;     // the reference
            c.recoveryGain            = 20.0;    // FITTED — recovery component values unsourced
            c.mixTopology             = MixTopology::highImpedanceBright;
            c.wetInjectionScale       = 1.0;
            c.hasToneControl          = false;   // [AB763] one reverb knob, nothing else
            c.hasDwellControl         = false;   // [AB763] stock mod point, not fitted
            break;

        case CircuitModel::vibroverb6G16:
            // [6G16] Documented difference from AB763 is the mixer: a simple
            // ~470 k resistive mix with no bright cap, reported darker and more
            // present. 3.3 M / 470 k is roughly +17 dB of wet injection.
            c.sendHighPassHz          = 318.0;   // INFERRED — borrowed from AB763
            c.driver                  = DriverType::triodeSingle;  // [6G16] reported 12AX7; unresolved
            c.hasTransformer          = true;    // INFERRED
            c.transformerPrimaryOhms  = 22800.0; // INFERRED
            c.nominalDriveVoltsPeak   = 0.460;   // INFERRED
            c.driveScale              = 0.7;     // INFERRED from a lower-current driver
            c.recoveryGain            = 20.0;    // FITTED
            c.mixTopology             = MixTopology::resistive;
            c.wetInjectionScale       = 3.3e6 / 470.0e3;   // [6G16] ~7x, about +17 dB
            c.hasToneControl          = false;
            c.hasDwellControl         = false;   // [6G16] open question: a rear-panel dwell is reported
            break;

        case CircuitModel::reverbUnit6G15:
            // [6G15] A 6K6GT power pentode drives the tank, behind a high-pass
            // rolling off below about 300 Hz. Mix is a 250 k linear pot. This is
            // the unit that can actually slam the springs.
            c.sendHighPassHz          = 300.0;
            c.driver                  = DriverType::powerPentode;
            c.hasTransformer          = true;
            c.transformerPrimaryOhms  = 14000.0; // INFERRED — [6G15] says a different primary, value unknown
            c.nominalDriveVoltsPeak   = 0.460;   // INFERRED
            c.driveScale              = 2.5;     // INFERRED from a power tube versus a small-signal triode
            c.recoveryGain            = 20.0;    // FITTED
            c.mixTopology             = MixTopology::linearPot;
            c.wetInjectionScale       = 1.5;     // INFERRED
            c.hasToneControl          = true;    // [6G15] Dwell, Mix and Tone
            c.hasDwellControl         = true;    // [6G15] Dwell is upstream of the driver
            break;

        case CircuitModel::ampegRocket:
            // [AMPEG] No reverb transformer. One 6SN7 half drives the tank
            // directly, the other recovers. A triode into an 8 ohm reactive load
            // is a severe mismatch, so the springs are excited gently: dark,
            // soft, diffuse. SPEC-002 asserts this as an ordering, not a value.
            c.sendHighPassHz          = 280.0;   // INFERRED
            c.driver                  = DriverType::triodeSingle;
            c.hasTransformer          = false;   // [AMPEG] the defining fact
            c.transformerPrimaryOhms  = 0.0;
            c.nominalDriveVoltsPeak   = 0.460;   // INFERRED
            c.driveScale              = 0.2;     // INFERRED — SPEC-002 requires below 25% of AB763
            c.recoveryGain            = 14.0;    // INFERRED — drive and recovery share one envelope
            c.mixTopology             = MixTopology::resistive;  // INFERRED
            c.wetInjectionScale       = 0.8;     // INFERRED
            c.hasToneControl          = false;   // not documented
            c.hasDwellControl         = false;   // not documented
            break;
    }

    return c;
}

inline const char* circuitModelName (CircuitModel model)
{
    switch (model)
    {
        case CircuitModel::ab763:          return "AB763";
        case CircuitModel::vibroverb6G16:  return "6G16";
        case CircuitModel::reverbUnit6G15: return "6G15";
        case CircuitModel::ampegRocket:    return "Ampeg";
    }
    return "AB763";
}

} // namespace bv
