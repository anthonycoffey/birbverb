#pragma once

// Test signal generators — SPEC-006 requirement 3.
//
// Every generator that involves randomness takes an explicit seed. There is no
// unseeded overload, on purpose: an unseeded noise generator makes a test suite
// that fails once a week for no reason anybody can reproduce.

#include <cmath>
#include <cstdint>
#include <vector>

namespace bv::test
{

inline constexpr double kPi = 3.14159265358979323846;

/** A unit impulse at sample 0, then silence. */
inline std::vector<float> impulse (int numSamples, float amplitude = 1.0f)
{
    std::vector<float> out ((size_t) numSamples, 0.0f);
    if (numSamples > 0)
        out[0] = amplitude;
    return out;
}

inline std::vector<float> silence (int numSamples)
{
    return std::vector<float> ((size_t) numSamples, 0.0f);
}

inline std::vector<float> dc (int numSamples, float level = 1.0f)
{
    return std::vector<float> ((size_t) numSamples, level);
}

/** Sine at `frequency`, starting at phase 0. */
inline std::vector<float> sine (int numSamples, double frequency, double sampleRate,
                                float amplitude = 1.0f)
{
    std::vector<float> out ((size_t) numSamples);
    const double w = 2.0 * kPi * frequency / sampleRate;
    for (int n = 0; n < numSamples; ++n)
        out[(size_t) n] = amplitude * (float) std::sin (w * n);
    return out;
}

/** Sine at exactly the centre of FFT bin `bin` for an `fftSize`-point transform.
    Use this for THD measurement: a fundamental sitting on a bin centre has no
    spectral leakage, so harmonic ratios come out clean without windowing. */
inline std::vector<float> sineAtBin (int numSamples, int bin, int fftSize,
                                     double sampleRate, float amplitude = 1.0f)
{
    return sine (numSamples, (double) bin * sampleRate / fftSize, sampleRate, amplitude);
}

/** Linear sine sweep from `startHz` to `endHz` across the whole buffer. */
inline std::vector<float> sweep (int numSamples, double startHz, double endHz,
                                 double sampleRate, float amplitude = 1.0f)
{
    std::vector<float> out ((size_t) numSamples);
    double phase = 0.0;
    for (int n = 0; n < numSamples; ++n)
    {
        const double t = numSamples > 1 ? (double) n / (numSamples - 1) : 0.0;
        const double f = startHz + (endHz - startHz) * t;
        phase += 2.0 * kPi * f / sampleRate;
        out[(size_t) n] = amplitude * (float) std::sin (phase);
    }
    return out;
}

/** White noise from an explicit seed. xorshift64*, which is plenty for test
    signals and is reproducible across platforms and compilers — std::mt19937 is
    too, but the distribution adapters around it are not. */
inline std::vector<float> noise (int numSamples, std::uint64_t seed, float amplitude = 1.0f)
{
    std::vector<float> out ((size_t) numSamples);
    std::uint64_t s = seed != 0 ? seed : 0x9E3779B97F4A7C15ull;
    for (int n = 0; n < numSamples; ++n)
    {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        const std::uint64_t r = s * 0x2545F4914F6CDD1Dull;
        // Top 24 bits -> [-1, 1)
        const double u = (double) (r >> 40) / 8388608.0 - 1.0;
        out[(size_t) n] = amplitude * (float) u;
    }
    return out;
}

/** An exponentially decaying sine — a synthetic reverb tail with an
    analytically known RT60, for verifying the RT60 measurement itself. */
inline std::vector<float> decayingSine (int numSamples, double frequency,
                                        double rt60Seconds, double sampleRate,
                                        float amplitude = 1.0f)
{
    std::vector<float> out ((size_t) numSamples);
    // -60 dB over rt60 seconds: amplitude factor 1e-3.
    const double tau = rt60Seconds / std::log (1000.0);
    const double w = 2.0 * kPi * frequency / sampleRate;
    for (int n = 0; n < numSamples; ++n)
    {
        const double t = (double) n / sampleRate;
        out[(size_t) n] = amplitude * (float) (std::exp (-t / tau) * std::sin (w * n));
    }
    return out;
}

/** Two impulses a known distance apart, for verifying autocorrelation. */
inline std::vector<float> impulsePair (int numSamples, int spacingSamples,
                                       float secondAmplitude = 0.7f)
{
    auto out = impulse (numSamples);
    if (spacingSamples > 0 && spacingSamples < numSamples)
        out[(size_t) spacingSamples] = secondAmplitude;
    return out;
}

} // namespace bv::test
