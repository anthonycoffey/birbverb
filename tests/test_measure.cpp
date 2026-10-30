// Verification of the measurement tools themselves — SPEC-006 acceptance
// criterion 2.
//
// This file exists because a measurement tool that is quietly wrong turns every
// other test in the repo into a lie that passes. Each tool is checked against a
// signal whose answer is known analytically, not against another measurement.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "support/Approx.h"
#include "support/Generate.h"
#include "support/Measure.h"

using namespace bv::test;

namespace
{
constexpr double fs = 48000.0;

/** TPT (topology-preserving transform) one-pole low-pass. Exactly -3.01 dB at
    its cutoff, which is what makes it a usable analytical reference. */
struct OnePoleLP
{
    explicit OnePoleLP (double cutoffHz, double sampleRate)
        : g (std::tan (kPiM * cutoffHz / sampleRate)) {}

    float operator() (float x)
    {
        const double v = ((double) x - s) * g / (1.0 + g);
        const double y = v + s;
        s = y + v;
        return (float) y;
    }

    double g, s = 0.0;
};

/** First-order allpass. With a NEGATIVE coefficient its group delay falls as
    frequency rises, which is the direction a spring disperses — so this doubles
    as the reference for the dispersion measurement.

    H(z) = (a + z^-1) / (1 + a z^-1),  group delay at DC = (1-a)/(1+a) samples. */
struct AllpassOne
{
    explicit AllpassOne (double coefficient) : a (coefficient) {}

    float operator() (float x)
    {
        const double y = a * (double) x + x1 - a * y1;
        x1 = (double) x;
        y1 = y;
        return (float) y;
    }

    double a, x1 = 0.0, y1 = 0.0;
};

/** Symmetric hard clipper: odd-order harmonics only, no even ones. */
struct HardClip
{
    explicit HardClip (float t) : threshold (t) {}
    float operator() (float x) const
    {
        return x > threshold ? threshold : (x < -threshold ? -threshold : x);
    }
    float threshold;
};

template <typename Process>
std::vector<float> runImpulse (Process&& p, int numSamples)
{
    auto in = impulse (numSamples);
    std::vector<float> out ((size_t) numSamples);
    for (int n = 0; n < numSamples; ++n)
        out[(size_t) n] = p (in[(size_t) n]);
    return out;
}
} // namespace

//==============================================================================
TEST_CASE ("RMS and peak of a unit sine are analytically known", "[measure]")
{
    const auto s = sine (4800, 1000.0, fs, 1.0f);

    // RMS of a unit sine is 1/sqrt(2).
    REQUIRE_THAT (rms (s), WithinRatio (0.70710678, 0.001));
    REQUIRE_THAT (peak (s), WithinRatio (1.0, 0.001));
    REQUIRE (allFinite (s));
}

TEST_CASE ("dB conversion round-trips", "[measure]")
{
    REQUIRE_THAT (toDb (1.0), WithinDb (0.0, 0.001));
    REQUIRE_THAT (toDb (0.5), WithinDb (-6.0206, 0.001));
    REQUIRE_THAT (fromDb (-6.0206), WithinRatio (0.5, 0.0001));
    REQUIRE_THAT (toDb (fromDb (-23.5)), WithinDb (-23.5, 0.001));

    // Silence must floor rather than return -inf.
    REQUIRE (toDb (0.0) == -200.0);
}

TEST_CASE ("FFT of a bin-centred sine puts all energy in one bin", "[measure]")
{
    constexpr int fftSize = 4096;
    constexpr int bin = 64;

    const auto s = sineAtBin (fftSize, bin, fftSize, fs, 1.0f);
    const auto mag = magnitudeSpectrum (s, fftSize);

    const double fund = mag[(size_t) bin];
    REQUIRE (fund > 0.0);

    // Every other bin should be numerically negligible.
    double worstOther = 0.0;
    for (size_t k = 1; k < mag.size(); ++k)
        if (std::abs ((long) k - (long) bin) > 1)
            worstOther = std::max (worstOther, mag[k]);

    REQUIRE (toDb (worstOther / fund) < -100.0);
}

TEST_CASE ("bin and Hz conversions agree", "[measure]")
{
    REQUIRE_THAT (binToHz (64, 4096, fs), WithinHz (750.0, 0.01));
    REQUIRE (hzToBin (750.0, 4096, fs) == 64);
}

//==============================================================================
TEST_CASE ("frequency response of a one-pole low-pass matches theory", "[measure]")
{
    const double cutoff = 1000.0;

    OnePoleLP atCutoff { cutoff, fs };
    REQUIRE_THAT (gainDbAt (atCutoff, cutoff, fs), WithinDb (-3.01, 0.5));

    // A one-pole rolls off at 6 dB/octave, so an octave up is -3 dB further
    // down than the -3 dB corner, and two octaves another 6.
    OnePoleLP oneOctave { cutoff, fs };
    const double oneOctaveDb = gainDbAt (oneOctave, cutoff * 2.0, fs);

    OnePoleLP twoOctaves { cutoff, fs };
    const double twoOctaveDb = gainDbAt (twoOctaves, cutoff * 4.0, fs);

    REQUIRE_THAT (twoOctaveDb - oneOctaveDb, WithinDb (-6.0, 1.0));

    // Well below cutoff it should be essentially unity.
    OnePoleLP low { cutoff, fs };
    REQUIRE_THAT (gainDbAt (low, 50.0, fs), WithinDb (0.0, 0.3));
}

TEST_CASE ("corner finder locates a known cutoff", "[measure]")
{
    const double cutoff = 318.0;   // the AB763 send high-pass figure
    const double found = findCornerHz ([cutoff] { return OnePoleLP { cutoff, fs }; },
                                       20.0, 8000.0, fs, -3.01);
    REQUIRE_THAT (found, WithinHz (cutoff, Percent (3.0)));
}

//==============================================================================
TEST_CASE ("RT60 of a synthetic decay matches its time constant", "[measure]")
{
    // SPEC-006 acceptance criterion 7: a 1.0 s decay measures 1.0 s +/- 2%.
    const auto tail = decayingSine ((int) (fs * 3.0), 500.0, 1.0, fs);
    const auto r = rt60 (tail, fs);

    REQUIRE (r.sufficientDecay);
    REQUIRE_THAT (r.seconds, WithinSeconds (1.0, Percent (2.0)));
}

TEST_CASE ("RT60 tracks across the range birbverb needs", "[measure]")
{
    for (double expected : { 0.8, 1.6, 2.4, 3.4, 4.5 })
    {
        const auto tail = decayingSine ((int) (fs * (expected * 3.0 + 1.0)), 500.0, expected, fs);
        const auto r = rt60 (tail, fs);

        REQUIRE (r.sufficientDecay);
        REQUIRE_THAT (r.seconds, WithinSeconds (expected, Percent (3.0)));
    }
}

TEST_CASE ("RT60 reports insufficient decay rather than guessing", "[measure]")
{
    // A signal that never decays. Schroeder integration appears to decay at the
    // very end of any buffer because the window shrinks, and the tail guard is
    // what stops us reporting a confident wrong number.
    const auto steady = sine ((int) fs, 500.0, fs);
    REQUIRE_FALSE (rt60 (steady, fs).sufficientDecay);

    REQUIRE_FALSE (rt60 (silence (1000), fs).sufficientDecay);
    REQUIRE_FALSE (rt60 ({}, fs).sufficientDecay);
}

//==============================================================================
TEST_CASE ("group delay of a pure delay line is flat and correct", "[measure]")
{
    // SPEC-006 acceptance criterion 8: 480 samples at 48 kHz is 10 ms, flat.
    constexpr int fftSize = 4096;
    constexpr int delaySamples = 480;

    std::vector<float> ir ((size_t) fftSize, 0.0f);
    ir[(size_t) delaySamples] = 1.0f;

    const double low  = groupDelayInBand (ir, fftSize, fs, 200.0, 1000.0);
    const double high = groupDelayInBand (ir, fftSize, fs, 5000.0, 10000.0);

    REQUIRE_THAT (low * 1000.0,  WithinMs (10.0, 0.1));
    REQUIRE_THAT (high * 1000.0, WithinMs (10.0, 0.1));
}

TEST_CASE ("group delay of a negative-coefficient allpass falls with frequency", "[measure]")
{
    // This is the dispersion measurement's reference case. For
    // H(z) = (a + z^-1)/(1 + a z^-1) with a = -0.7, group delay at DC is
    // (1-a)/(1+a) = 1.7/0.3 = 5.667 samples, and it falls from there.
    constexpr int fftSize = 4096;
    constexpr double a = -0.7;

    AllpassOne ap { a };
    const auto ir = runImpulse (ap, fftSize);

    const double lowSamples  = groupDelayInBand (ir, fftSize, fs, 50.0, 200.0) * fs;
    const double highSamples = groupDelayInBand (ir, fftSize, fs, 10000.0, 15000.0) * fs;

    REQUIRE_THAT (lowSamples, WithinRatio (5.65, 0.6));
    REQUIRE (highSamples < lowSamples);

    // An allpass is, as the name says, flat in magnitude.
    AllpassOne apLow { a }, apHigh { a };
    REQUIRE_THAT (gainDbAt (apLow,  100.0,  fs), WithinDb (0.0, 0.2));
    REQUIRE_THAT (gainDbAt (apHigh, 10000.0, fs), WithinDb (0.0, 0.2));
}

//==============================================================================
TEST_CASE ("THD of a hard clipper shows the expected harmonic series", "[measure]")
{
    constexpr int fftSize = 4096;
    constexpr int bin = 64;

    const auto in = sineAtBin (fftSize, bin, fftSize, fs, 1.0f);
    HardClip clip { 0.5f };

    std::vector<float> out ((size_t) fftSize);
    for (int n = 0; n < fftSize; ++n)
        out[(size_t) n] = clip (in[(size_t) n]);

    REQUIRE (thd (out, bin, fftSize) > 0.05);

    // Symmetric clipping is an odd function, so even harmonics should be absent.
    const auto mag = magnitudeSpectrum (out, fftSize);
    const double fund   = mag[(size_t) bin];
    const double second = mag[(size_t) bin * 2];
    const double third  = mag[(size_t) bin * 3];

    REQUIRE (third > second * 10.0);
    REQUIRE (toDb (second / fund) < -60.0);
}

TEST_CASE ("THD of a clean sine is essentially zero", "[measure]")
{
    constexpr int fftSize = 4096;
    constexpr int bin = 64;
    const auto s = sineAtBin (fftSize, bin, fftSize, fs, 0.5f);
    REQUIRE (thd (s, bin, fftSize) < 1.0e-6);
}

TEST_CASE ("aliasing floor of a clean sine is far below the fundamental", "[measure]")
{
    constexpr int fftSize = 4096;
    constexpr int bin = 64;
    const auto s = sineAtBin (fftSize, bin, fftSize, fs, 0.5f);
    REQUIRE (aliasingFloorDb (s, bin, fftSize) < -100.0);
}

//==============================================================================
TEST_CASE ("autocorrelation finds a known echo spacing", "[measure]")
{
    const auto pair = impulsePair (8192, 500, 0.7f);
    const auto lag = autocorrelationPeak (pair, 10, 2000);

    REQUIRE (lag.has_value());
    REQUIRE (*lag == 500);
}

TEST_CASE ("autocorrelation reports no peak for aperiodic input", "[measure]")
{
    const auto n = noise (8192, 0xC0FFEEull);
    REQUIRE_FALSE (autocorrelationPeak (n, 100, 2000, 0.1).has_value());
}

//==============================================================================
TEST_CASE ("generators are deterministic for a given seed", "[measure]")
{
    // SPEC-006 requirement 4. Without this, a failing test is not reproducible
    // and the suite becomes something you learn to ignore.
    const auto a = noise (4096, 12345ull);
    const auto b = noise (4096, 12345ull);
    const auto c = noise (4096, 12346ull);

    REQUIRE (a == b);
    REQUIRE (a != c);
    REQUIRE (allFinite (a));
}
