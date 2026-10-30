#pragma once

// Tank configuration — SPEC-001.
//
// A plain struct so presets, the processor and the tests all configure a tank the
// same way. No JUCE: this header has to be includable from the test target, which
// does not link JUCE (ADR-004).
//
// Every value below carries the source it came from, or the word INFERRED. That
// convention is the most important one in this codebase — see
// docs/documentation/development-standards.md, "The research rule". A bare
// constant in a DSP file is a bug waiting to happen.
//
// Note what this file does NOT contain: echo spacings and transition
// frequencies. Those are DERIVED from the spring geometries below by the closed
// forms in Dispersion.h, so the only transcribed numbers here are caliper
// readings off a published table. Change a geometry and every dependent number
// follows. See docs/documentation/deep-dives/spring-dispersion-and-allpass-fitting.md.

#include <algorithm>
#include <cstddef>

#include "Dispersion.h"

namespace bv
{

enum class TankType
{
    short2Spring = 0,   ///< Accutronics Type 4, short pan, 2 springs
    long2Spring,        ///< Accutronics Type 4 long / 4AB3C1B — the Fender blackface tank
    long3Spring         ///< Accutronics Type 9, long pan, 3 springs
};

inline constexpr int numTankTypes = 3;
inline constexpr int maxSprings   = 3;

/** Build a geometry from a caliper table's own units: centimetres, and
    DIAMETERS rather than radii, exactly as [BP09] Appendix A prints them.
    Transcribing the table through this function means a typo shows up as a
    wrong number rather than as a silently halved radius.
*/
inline dispersion::SpringGeometry fromCalipers (double helixLengthCm,
                                                double helixDiameterCm,
                                                int    turns,
                                                double wireDiameterCm)
{
    return { helixLengthCm   / 100.0,
             helixDiameterCm / 200.0,
             wireDiameterCm  / 200.0,
             turns };
}

/** One modelled spring. */
struct SpringParams
{
    dispersion::SpringGeometry geometry {};

    /// True when `geometry` is a published caliper measurement of a real spring.
    /// False means INTERPOLATED — see the Long 2-Spring case below.
    bool measured = false;

    /// Echo spacing T_D, milliseconds. [BP09] eq. (2).
    double echoSpacingMs() const
    {
        return dispersion::echoSpacingSeconds (geometry) * 1000.0;
    }

    /// One-way travel time down the spring. T_D is a round trip.
    double singlePassMs() const { return echoSpacingMs() * 0.5; }

    /// Transition frequency F_C, Hz. [BP09] eq. (4). Group delay peaks here and
    /// the main chirp series does not exist above it.
    double transitionHz() const
    {
        return dispersion::transitionHz (geometry);
    }
};

struct TankConfig
{
    /// Number of springs modelled in parallel.
    /// SOURCE: circuits/accutronics-spring-tanks.md — Type 4 is a two-spring
    /// type, Type 9 a three-spring one.
    /// Parallel rather than series: both units measured in [BP09] are parallel.
    /// Series would roughly double the echo spacing. Open question in the
    /// dispersion study.
    int numSprings = 2;

    SpringParams springs[maxSprings] {};

    /// Target RT60, seconds.
    /// SOURCE: accutronics-spring-tanks.md — datasheet decay code 3 is
    /// 2.75..4.0 s, so 3.4 s is the middle of the long-decay range.
    double rt60Seconds = 3.4;

    /// Ratio of energy reflected back at each transducer. Energy makes multiple
    /// round trips in a real tank; this is what produces the decay tail.
    /// FITTED — no source gives it. Fit to the echo decay envelope of a measured
    /// impulse response.
    double reflectionCoefficient = 0.75;

    /// Cascaded allpass sections in the main (below-F_C) loop.
    /// SOURCE: [PARKER11] section 4 — "we assume initial all-pass chain lengths
    /// before modification of Mlow = 100 and Mhigh = 200, as used by Valimaki
    /// et al."
    int allpassSectionsLow = 100;

    /// Cascaded allpass sections in the secondary wideband loop. Same source.
    int allpassSectionsHigh = 200;

    /// Allpass coefficient for the secondary loop, with stretch factor 1.
    /// Negative so its group delay peaks at DC and falls with frequency — the
    /// secondary series is a DESCENDING chirp.
    /// FITTED to [PARKER11]'s "little dispersion above 10 kHz": at -0.6 with
    /// M = 200 the group delay is 16.7 ms at DC, 2.5 ms at 10 kHz, 1.0 ms at
    /// Nyquist.
    double secondaryCoeff = -0.6;

    /// Level of the secondary series relative to the main one.
    /// SOURCE: [PARKER11] section 4 — the wideband sequence is "generally at
    /// least 30 dB below the other chirp sequence".
    double secondaryLevelDb = -30.0;

    //==========================================================================
    double meanEchoSpacingMs() const
    {
        double sum = 0.0;
        for (int i = 0; i < numSprings; ++i)
            sum += springs[i].echoSpacingMs();
        return sum / (double) numSprings;
    }

    /// Mean one-way travel time. What the autocorrelation test keys off.
    double singlePassDelayMs() const { return meanEchoSpacingMs() * 0.5; }

    /// Lowest F_C across the springs — above this the main series is gone for
    /// every spring, which is where SPEC-001 criterion 6 measures.
    double minTransitionHz() const
    {
        double lowest = springs[0].transitionHz();
        for (int i = 1; i < numSprings; ++i)
            lowest = std::min (lowest, springs[i].transitionHz());
        return lowest;
    }

    double maxTransitionHz() const
    {
        double highest = springs[0].transitionHz();
        for (int i = 1; i < numSprings; ++i)
            highest = std::max (highest, springs[i].transitionHz());
        return highest;
    }

    /// Spread of a per-spring quantity, as (max - min) / mean. SPEC-001
    /// requirement 4 is stated in these terms because the two design
    /// philosophies in the hardware differ in which of the two is spread.
    double echoSpacingSpread() const
    {
        double lo = springs[0].echoSpacingMs(), hi = lo, sum = 0.0;
        for (int i = 0; i < numSprings; ++i)
        {
            const double v = springs[i].echoSpacingMs();
            lo = std::min (lo, v);
            hi = std::max (hi, v);
            sum += v;
        }
        return (hi - lo) / (sum / (double) numSprings);
    }

    double transitionSpread() const
    {
        double sum = 0.0;
        for (int i = 0; i < numSprings; ++i)
            sum += springs[i].transitionHz();
        return (maxTransitionHz() - minTransitionHz()) / (sum / (double) numSprings);
    }

    /// Loop gain that hits rt60Seconds at the mean echo spacing.
    double loopGain() const
    {
        return dispersion::loopGainForRt60 (meanEchoSpacingMs() * 0.001, rt60Seconds);
    }
};

/** Configuration for one of the three modelled tank types.

    Geometries are caliper measurements from [BP09] Appendix A, which published
    two complete units. Neither is an Accutronics tank — nobody in the
    literature has calipered one — so each type names the unit it stands in for
    and whether that is a measurement or an interpolation.
*/
inline TankConfig tankConfigFor (TankType type)
{
    TankConfig c;
    switch (type)
    {
        case TankType::short2Spring:
            // Proxy: Olson X-82, a small two-spring unit from the early 1960s.
            // SOURCE: [BP09] Table 1 — both springs measured.
            // Its design matches echo spacing (13.7% spread) and deliberately
            // spreads F_C (24.3%), which [BP09] states in prose and which falls
            // out of these geometries on its own.
            // Derived: T_D 30.6 / 35.1 ms, F_C 3244 / 2543 Hz.
            c.numSprings = 2;
            c.springs[0] = { fromCalipers (6.5, 0.54, 148, 0.035), true };
            c.springs[1] = { fromCalipers (6.5, 0.61, 133, 0.035), true };
            c.rt60Seconds = 1.6;    // datasheet decay code 1: 1.2..2.0 s
            break;

        case TankType::long2Spring:
            // The Fender blackface tank, and the one with no published geometry.
            // INTERPOLATED: the long-class wire and helix radii of the Leem
            // springs below, arranged the way the Olson's TWO-spring unit is —
            // matched T_D, spread F_C — because that is the documented 2-spring
            // philosophy. Spring A is Leem #3 verbatim; spring B is a wider,
            // fewer-turn helix chosen to hold T_D while lowering F_C.
            // Derived: T_D 52.7 / 57.3 ms, F_C 4471 / 3784 Hz.
            // Closable with a micrometer on a real 4AB3C1B — eqs. (2)-(4) then
            // give both numbers with no fitting. Open question in the study.
            c.numSprings = 2;
            c.springs[0] = { fromCalipers (16.3, 0.46, 351, 0.035), true  };
            c.springs[1] = { fromCalipers (16.3, 0.50, 323, 0.035), false };
            c.rt60Seconds = 3.4;    // datasheet decay code 3: 2.75..4.0 s
            break;

        case TankType::long3Spring:
            // Proxy: Leem Pro KA-1210, a three-spring tank from an early-1990s
            // guitar amp. SOURCE: [BP09] Table 2 — all three springs measured.
            // The mirror image of the Olson: spread T_D (27.8%), matched F_C
            // (8.9%).
            // Derived: T_D 41.6 / 40.2 / 52.7 ms, F_C 4887 / 4672 / 4471 Hz.
            c.numSprings = 3;
            c.springs[0] = { fromCalipers (16.3, 0.44, 303, 0.035), true };
            c.springs[1] = { fromCalipers (16.3, 0.45, 280, 0.035), true };
            c.springs[2] = { fromCalipers (16.3, 0.46, 351, 0.035), true };
            c.rt60Seconds = 3.0;    // tank study
            break;
    }
    return c;
}

inline const char* tankTypeName (TankType type)
{
    switch (type)
    {
        case TankType::short2Spring: return "Short 2-Spring";
        case TankType::long2Spring:  return "Long 2-Spring";
        case TankType::long3Spring:  return "Long 3-Spring";
    }
    return "Long 2-Spring";
}

} // namespace bv
