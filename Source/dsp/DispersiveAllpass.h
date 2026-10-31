#pragma once

// The dispersive allpass chain — SPEC-001.
//
// A cascade of M stretched first-order allpass sections, [PARKER11] eqs. (1)-(3):
//
//     A(z)        = (a + z^-1) / (1 + a*z^-1)
//     H_single(z) = A(z^k)     = (a + z^-k) / (1 + a*z^-k)
//     H_cascade   = A^M(z^k)
//
// Its group delay is the closed form in Dispersion.h ([PARKER11] eq. 4), and
// tests/test_dispersive_allpass.cpp asserts the realised structure against that
// closed form rather than against numbers written into the test.
//
// This is the component that makes a spring a spring. Every frequency comes out
// at the same level and a different time: group delay rises with frequency and
// peaks at the transition frequency F_C, so the impulse response is an
// ASCENDING chirp. Low frequencies arrive first.
//
//   [PARKER11]  J. Parker, "Efficient Dispersion Generation Structures for
//               Spring Reverb Emulation", EURASIP J. Adv. Signal Process.
//               2011:646134. Open access, CC BY.

#include "Dispersion.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace bv
{

class DispersiveAllpass
{
public:
    DispersiveAllpass() = default;

    /** Allocates. Never called from the audio thread.

        Retuning means calling this again, which is why changing tank type is a
        crossfade between two instances a layer up rather than a reconfigure in
        place — see SPEC-003's 30 ms crossfade requirement.

        @param newSampleRate     Hz.
        @param transitionFreqHz  F_C, where group delay peaks. Sets k.
        @param peakDelayMs       D(F_C). Set this to the spring's echo spacing
                                 T_D and the chirp exactly fills the echo
                                 period, which is what makes a spring sound
                                 continuous rather than like discrete taps.
        @param numSections       M. 100 for the main loop, 200 for the
                                 secondary — [PARKER11] section 4.
    */
    void prepare (double newSampleRate, double transitionFreqHz,
                  double peakDelayMs, int numSections)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        sections   = numSections > 0 ? numSections : 1;

        // k = fs / (2 * F_C), deliberately fractional. Rounding it would move
        // F_C with the sample rate — at 48 kHz, k = 5 is 4800 Hz and k = 6 is
        // 4000 Hz — and break SPEC-001 requirement 9.
        //
        // The floor of 1.5 is a structural limit, not a modelling choice: the
        // integer part of the delay has to be at least one sample or the
        // section's feedback path has no delay in it at all. k = 1.5 means
        // F_C = fs/3, far above the 2.5-4.9 kHz the measured springs give.
        stretch = std::max (1.5, dispersion::stretchFactor (sampleRate, transitionFreqHz));

        const double peakSamples = peakDelayMs * 0.001 * sampleRate;
        coeff = dispersion::coeffForPeakDelay (peakSamples, stretch, sections);
        coeff = std::clamp (coeff, -0.999, 0.999);      // the section is only stable inside this

        // The k-sample delay is split into an integer ring buffer and a
        // first-order allpass carrying the remainder. Taking the integer part
        // as round(k) - 1 puts the remainder in [0.5, 1.5], which is where a
        // first-order interpolator is well conditioned; floor(k) would allow a
        // remainder near zero, whose coefficient approaches 1 and parks a pole
        // on the unit circle.
        integerDelay = std::max (1, (int) std::lround (stretch) - 1);
        const double remainder = std::max (0.0, stretch - (double) integerDelay);

        // [PARKER11] eq. (6), designed at the midpoint of the band this section
        // carries. A first-order allpass can match a target delay exactly at
        // exactly one frequency; the section carries DC to F_C, which is
        // omega in [0, pi/k], so matching at pi/(2k) is the choice that
        // minimises the worst-case error across that band rather than
        // concentrating the accuracy at one end of it.
        //
        // Positive root: it is the one that puts the interpolator's own
        // group-delay peak at Nyquist, outside the band being carried
        // ([PARKER11] section 3.1). The negative root is the degenerate a = -1
        // at DC.
        interpCoeff = dispersion::coeffForFractionalDelay (remainder,
                                                           dispersion::pi / (2.0 * stretch),
                                                           true);
        interpCoeff = std::clamp (interpCoeff, -0.999, 0.999);

        line.assign ((size_t) sections * (size_t) integerDelay, 0.0f);
        interpX.assign ((size_t) sections, 0.0f);
        interpY.assign ((size_t) sections, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (line.begin(), line.end(), 0.0f);
        std::fill (interpX.begin(), interpX.end(), 0.0f);
        std::fill (interpY.begin(), interpY.end(), 0.0f);
        pos = 0;
    }

    /** One sample through all M sections.

        Each section is the canonical single-delay allpass:

            w[n] = x[n] - a*y[n]
            y[n] = a*x[n] + D{w}[n]

        with D the k-sample delay. Reading the ring slot before writing it is
        what makes that read `integerDelay` samples old, and the interpolator
        then adds the fractional remainder. Because the integer part is at
        least one sample, D{w}[n] never depends on w[n] and the loop stays
        computable — a fractional-only delay would be a delay-free loop.
    */
    float processSample (float input) noexcept
    {
        const float a = (float) coeff;
        const float c = (float) interpCoeff;

        float s = input;
        for (int m = 0; m < sections; ++m)
        {
            const size_t slot = (size_t) m * (size_t) integerDelay + (size_t) pos;

            const float u = line[slot];
            float v = c * u + interpX[(size_t) m] - c * interpY[(size_t) m];
            v = flush (v);

            interpX[(size_t) m] = u;
            interpY[(size_t) m] = v;

            const float y = a * s + v;
            line[slot] = flush (s - a * y);
            s = y;
        }

        if (++pos >= integerDelay) pos = 0;
        return s;
    }

    void process (const float* input, float* output, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
            output[n] = processSample (input[n]);
    }

    double getSampleRate()    const noexcept { return sampleRate; }
    int    getNumSections()   const noexcept { return sections; }
    double getStretchFactor() const noexcept { return stretch; }
    double getCoefficient()   const noexcept { return coeff; }

    /** Integer part of the k-sample delay, in samples. The remainder lives in
        the interpolator. Exposed so a test can show the split is happening. */
    int    getIntegerDelay()      const noexcept { return integerDelay; }
    double getInterpCoefficient() const noexcept { return interpCoeff; }

private:
    /** M feedback paths with coefficients near 1 will sit on denormals for a
        long time after the signal stops, and a denormal costs tens of cycles.
        SPEC-001 lists denormal handling as a task; this is it. The threshold is
        far below -200 dBFS, so nothing audible is being discarded. */
    static float flush (float v) noexcept
    {
        return std::abs (v) < 1.0e-20f ? 0.0f : v;
    }

    double sampleRate   = 48000.0;
    int    sections     = 100;
    double stretch      = 0.0;
    double coeff        = 0.0;
    double interpCoeff  = 0.0;
    int    integerDelay = 1;
    int    pos          = 0;

    std::vector<float> line;        ///< sections * integerDelay, one ring per section
    std::vector<float> interpX;     ///< per-section interpolator input history
    std::vector<float> interpY;     ///< per-section interpolator output history
};

} // namespace bv
