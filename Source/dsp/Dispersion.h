#pragma once

// Dispersion closed forms — SPEC-001.
//
// Two papers between them give everything needed to turn spring geometry into
// allpass coefficients, and both are free to read:
//
//   [PARKER11]  J. Parker, "Efficient Dispersion Generation Structures for Spring
//               Reverb Emulation", EURASIP J. Adv. Signal Process. 2011:646134.
//               Open access, CC BY.
//   [BP09]      S. Bilbao and J. Parker, "Spring Reverberation: A Physical
//               Perspective", Proc. DAFx-09, Como, pp. 416-421. Free from the
//               DAFx archive.
//
// The originals (Abel et al. 2006, Valimaki et al. 2010) are paywalled, so no
// constant in birbverb depends on them. That was the deciding factor, not
// accuracy — see docs/documentation/deep-dives/spring-dispersion-and-allpass-fitting.md.
//
// Everything here is a pure function of its arguments. No state, no allocation,
// no JUCE, so the test target can call these directly and check them against the
// numbers printed in the papers.

#include <cmath>

namespace bv::dispersion
{

inline constexpr double pi = 3.14159265358979323846;

//==============================================================================
// Steel, as used for both measured units in [BP09] Appendix A.
// SOURCE: [BP09] Appendix A — "Both units appear to use steel wiring, and
// therefore a Young's Modulus of E = 2e11 N/m^2 and a density of
// rho = 7800 kg/m^3 are used for all the calculations in this document."
inline constexpr double steelYoungsModulus = 2.0e11;   // N/m^2
inline constexpr double steelDensity       = 7800.0;   // kg/m^3

/** Bar wave speed sqrt(E/rho). 5063.7 m/s for steel. */
inline double waveSpeed (double youngsModulus = steelYoungsModulus,
                         double density       = steelDensity)
{
    return std::sqrt (youngsModulus / density);
}

//==============================================================================
/** A spring, as a caliper sees it. Radii, not diameters — the paper's tables
    list diameters, so halve them on the way in.
*/
struct SpringGeometry
{
    double helixLengthMetres = 0.163;    ///< H_L, the coil's own length
    double helixRadiusMetres = 0.0023;   ///< R
    double wireRadiusMetres  = 0.000175; ///< r
    int    turns             = 351;      ///< N, over the whole length
};

/** Uncurled length of the wire. [BP09] eq. (3):
    L = sqrt( (2*pi*R*N)^2 + H_L^2 )
*/
inline double uncurledWireLength (const SpringGeometry& g)
{
    const double coiled = 2.0 * pi * g.helixRadiusMetres * (double) g.turns;
    return std::sqrt (coiled * coiled + g.helixLengthMetres * g.helixLengthMetres);
}

/** Echo spacing T_D in seconds — the time for a wave to traverse the spring
    TWICE, so single-pass delay is half this. [BP09] eq. (2):

        T_D ~= 4*L*R / ( r * sqrt(E/rho) )

    Derived in their Appendix B as the reciprocal of the group velocity in the
    limit beta -> 0, which is the point of maximum propagation velocity in the
    dispersive region.
*/
inline double echoSpacingSeconds (const SpringGeometry& g,
                                  double youngsModulus = steelYoungsModulus,
                                  double density       = steelDensity)
{
    return 4.0 * uncurledWireLength (g) * g.helixRadiusMetres
         / (g.wireRadiusMetres * waveSpeed (youngsModulus, density));
}

/** Transition frequency F_C in Hz. [BP09] eq. (4):

        F_C ~= 3*r*sqrt(E/rho) / ( 16*sqrt(5)*pi*R^2 )

    Note what is absent: spring LENGTH. F_C is set by wire and helix radius
    alone, which is why a manufacturer can match echo spacing across springs
    while deliberately spreading their transition frequencies (or the reverse).
*/
inline double transitionHz (const SpringGeometry& g,
                            double youngsModulus = steelYoungsModulus,
                            double density       = steelDensity)
{
    const double r = g.wireRadiusMetres;
    const double R = g.helixRadiusMetres;
    return 3.0 * r * waveSpeed (youngsModulus, density)
         / (16.0 * std::sqrt (5.0) * pi * R * R);
}

//==============================================================================
/** Group delay in samples of a cascade of M stretched first-order allpass
    sections, A^M(z^k). [PARKER11] eq. (4):

        D = k*M*(1 - a^2) / ( 1 + 2*a*cos(omega*k) + a^2 )

    Independently cross-checked: at a = -0.7, k = 1, M = 1 this returns 5.6667
    samples at DC, and tests/support/Measure.h measured a real one-section
    allpass at 5.6525 samples at the first FFT bin.
*/
inline double groupDelaySamples (double a, double k, int M, double omega)
{
    const double denom = 1.0 + 2.0 * a * std::cos (omega * k) + a * a;
    return k * (double) M * (1.0 - a * a) / denom;
}

/** Group delay at DC: cos(0) = 1, so D = k*M*(1-a)/(1+a). */
inline double groupDelayAtDc (double a, double k, int M)
{
    return k * (double) M * (1.0 - a) / (1.0 + a);
}

/** Group delay at the peak, where cos(omega*k) = -1:
    D = k*M*(1+a)/(1-a).
*/
inline double peakGroupDelay (double a, double k, int M)
{
    return k * (double) M * (1.0 + a) / (1.0 - a);
}

/** The stretch factor that puts the group-delay peak at transitionHz.

    The peak of eq. (4) is where cos(omega*k) = -1, i.e. omega*k = pi, i.e.
    f = fs/(2k). So k = fs/(2*F_C).

    The result is deliberately NOT rounded. Integer k would move F_C with the
    sample rate — at 48 kHz, k = 5 gives 4800 Hz and k = 6 gives 4000 Hz — which
    breaks SPEC-001 requirement 9. Fractional k needs an interpolating allpass
    for one internal unit delay; see coeffForFractionalDelay().
*/
inline double stretchFactor (double sampleRate, double transitionFreqHz)
{
    return sampleRate / (2.0 * transitionFreqHz);
}

/** The allpass coefficient whose cascade peaks at peakDelaySamples.

    Inverting peakGroupDelay():  a = (D_peak - k*M) / (D_peak + k*M).

    Set peakDelaySamples to T_D in samples and the chirp just fills the echo
    period, which is what makes a spring sound continuous rather than like a
    row of discrete taps.
*/
inline double coeffForPeakDelay (double peakDelaySamples, double k, int M)
{
    const double kM = k * (double) M;
    return (peakDelaySamples - kM) / (peakDelaySamples + kM);
}

/** Coefficient of a FIRST-ORDER allpass having group delay D at omega.
    [PARKER11] eq. (6):

        a = ( -D*cos(w) +/- sqrt( 1 - D^2 + D^2*cos(w)^2 ) ) / ( 1 + D )

    This is the exact algebraic solution of eq. (5) for a, and it is how a
    fractional stretch factor is realised: the fractional part becomes an
    interpolating allpass tuned at the frequency that matters rather than at DC.

    At omega = 0 and omega = pi the two roots collapse into one. Elsewhere pick
    the root that puts the interpolator's own group-delay peak out of the band
    the section is carrying: positive a for the low loop, negative for the high
    loop ([PARKER11] section 3.1 and 3.3).
*/
inline double coeffForFractionalDelay (double D, double omega, bool positiveRoot)
{
    const double c    = std::cos (omega);
    const double disc = std::sqrt (1.0 - D * D + D * D * c * c);
    return (-D * c + (positiveRoot ? disc : -disc)) / (1.0 + D);
}

//==============================================================================
/** Loop gain for a target RT60, given the loop time in seconds.

    RT60 is 60 dB of decay, so g = 10^(-3 * T_loop / RT60).

    Watch the interaction with dispersion: group delay peaks at F_C, so energy
    there makes FEWER round trips per second and a flat loss filter would make
    F_C decay SLOWER than the rest of the band. SPEC-001 requirement 7 wants the
    opposite, so the loss filter has to be a low-pass with real slope.
*/
inline double loopGainForRt60 (double loopSeconds, double rt60Seconds)
{
    return std::pow (10.0, -3.0 * loopSeconds / rt60Seconds);
}

} // namespace bv::dispersion
