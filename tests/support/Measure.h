#pragma once

// Measurement library — SPEC-006 requirement 2.
//
// Every acceptance criterion in this repo is a number with a tolerance, so these
// tools are the thing the whole test suite rests on. A measurement tool that is
// quietly wrong turns every test that uses it into a lie that passes, which is
// why test_measure.cpp verifies each one against a signal whose answer is known
// analytically.
//
// Test code: clarity beats speed. The FFT here is a plain radix-2 Cooley-Tukey.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <optional>
#include <vector>

namespace bv::test
{

inline constexpr double kPiM = 3.14159265358979323846;

//==============================================================================
// Levels

inline double rms (const std::vector<float>& x, int from = 0)
{
    if (from >= (int) x.size()) return 0.0;
    double sum = 0.0;
    for (size_t i = (size_t) from; i < x.size(); ++i)
        sum += (double) x[i] * (double) x[i];
    return std::sqrt (sum / (double) (x.size() - (size_t) from));
}

inline double peak (const std::vector<float>& x, int from = 0)
{
    double p = 0.0;
    for (size_t i = (size_t) std::max (0, from); i < x.size(); ++i)
        p = std::max (p, (double) std::abs (x[i]));
    return p;
}

inline double toDb (double linear, double floorDb = -200.0)
{
    return linear > 0.0 ? std::max (floorDb, 20.0 * std::log10 (linear)) : floorDb;
}

inline double fromDb (double db) { return std::pow (10.0, db / 20.0); }

inline bool allFinite (const std::vector<float>& x)
{
    return std::all_of (x.begin(), x.end(), [] (float v) { return std::isfinite (v); });
}

//==============================================================================
// FFT

inline bool isPowerOfTwo (int n) { return n > 0 && (n & (n - 1)) == 0; }

/** In-place radix-2 FFT. `data.size()` must be a power of two. */
inline void fft (std::vector<std::complex<double>>& data)
{
    const size_t n = data.size();
    if (n < 2) return;

    // Bit-reversal permutation.
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (data[i], data[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPiM / (double) len;
        const std::complex<double> wlen (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = data[i + k];
                const auto v = data[i + k + len / 2] * w;
                data[i + k]             = u + v;
                data[i + k + len / 2]   = u - v;
                w *= wlen;
            }
        }
    }
}

/** Complex spectrum of the first `fftSize` samples, zero-padded if short. */
inline std::vector<std::complex<double>> spectrum (const std::vector<float>& x, int fftSize)
{
    std::vector<std::complex<double>> buf ((size_t) fftSize, { 0.0, 0.0 });
    const size_t n = std::min ((size_t) fftSize, x.size());
    for (size_t i = 0; i < n; ++i)
        buf[i] = { (double) x[i], 0.0 };
    fft (buf);
    return buf;
}

/** Magnitude spectrum, bins 0 .. fftSize/2 inclusive. */
inline std::vector<double> magnitudeSpectrum (const std::vector<float>& x, int fftSize)
{
    const auto s = spectrum (x, fftSize);
    std::vector<double> mag ((size_t) (fftSize / 2 + 1));
    for (size_t k = 0; k < mag.size(); ++k)
        mag[k] = std::abs (s[k]);
    return mag;
}

inline double binToHz (int bin, int fftSize, double sampleRate)
{
    return (double) bin * sampleRate / (double) fftSize;
}

inline int hzToBin (double hz, int fftSize, double sampleRate)
{
    return (int) std::lround (hz * (double) fftSize / sampleRate);
}

/** RMS magnitude within a frequency band, in dB. Energy per bin rather than
    total, so bands of different widths compare directly. */
inline double bandEnergyDb (const std::vector<float>& x, int fftSize, double sampleRate,
                            double lowHz, double highHz)
{
    const auto mag = magnitudeSpectrum (x, fftSize);
    const int last = (int) mag.size() - 1;
    const int from = std::clamp (hzToBin (lowHz,  fftSize, sampleRate), 0, last);
    const int to   = std::clamp (hzToBin (highHz, fftSize, sampleRate), from, last);

    double sum = 0.0;
    for (int k = from; k <= to; ++k)
        sum += mag[(size_t) k] * mag[(size_t) k];

    return toDb (std::sqrt (sum / (double) (to - from + 1)));
}

//==============================================================================
// Steady-state frequency response
//
// Drive a sine, discard the first half of the buffer so transients have settled,
// and compare output RMS to input RMS. Slower than an FFT of the impulse
// response and far more robust against nonlinearity and time variance, which is
// what we are usually measuring through.

/** `process` is any callable `float(float)`. Returns gain in dB at `frequency`. */
template <typename Process>
inline double gainDbAt (Process&& process, double frequency, double sampleRate,
                        double seconds = 0.25, float amplitude = 0.25f)
{
    const int n = (int) (sampleRate * seconds);
    if (n < 16) return -200.0;

    std::vector<float> out ((size_t) n);
    const double w = 2.0 * kPiM * frequency / sampleRate;
    for (int i = 0; i < n; ++i)
        out[(size_t) i] = process (amplitude * (float) std::sin (w * i));

    const double inRms  = (double) amplitude * 0.70710678118;
    const double outRms = rms (out, n / 2);
    return toDb (outRms / inRms);
}

/** The −3 dB corner of a response, found by geometric bisection between two
    frequencies known to bracket it. Returns the frequency in Hz.

    `makeProcess` returns a FRESH process each call, because a filter carries
    state and reusing one would measure the tail of the previous frequency.

    Works for both low-pass and high-pass responses: which direction to step is
    derived from the response at the two brackets rather than assumed. */
template <typename MakeProcess>
inline double findCornerHz (MakeProcess&& makeProcess, double lowHz, double highHz,
                            double sampleRate, double targetDb = -3.0,
                            int iterations = 30)
{
    auto gainAt = [&] (double f)
    {
        auto p = makeProcess();
        return gainDbAt (p, f, sampleRate);
    };

    const bool fallsWithFrequency = gainAt (lowHz) > gainAt (highHz);

    for (int i = 0; i < iterations; ++i)
    {
        const double mid = std::sqrt (lowHz * highHz);   // geometric midpoint
        const bool belowTarget = gainAt (mid) < targetDb;

        // Low-pass: below target means the corner is lower than mid.
        // High-pass: below target means the corner is higher than mid.
        if (fallsWithFrequency == belowTarget)
            highHz = mid;
        else
            lowHz = mid;
    }

    return std::sqrt (lowHz * highHz);
}

//==============================================================================
// RT60
//
// Schroeder backward integration, then a least-squares fit over the −5 dB to
// −35 dB span of the decay curve, extrapolated to −60 dB (the standard T30
// method). Measuring to −60 dB directly is far more sensitive to the noise floor.

struct Rt60Result
{
    double seconds = 0.0;
    bool   sufficientDecay = false;   ///< false when the tail never reaches −35 dB
};

inline Rt60Result rt60 (const std::vector<float>& ir, double sampleRate)
{
    if (ir.empty() || sampleRate <= 0.0) return {};

    // Backward-integrated energy, normalised to 0 dB at its maximum.
    std::vector<double> curve (ir.size());
    double acc = 0.0;
    for (size_t i = ir.size(); i-- > 0;)
    {
        acc += (double) ir[i] * (double) ir[i];
        curve[i] = acc;
    }
    // Capture the total before normalising: dividing by curve[0] inside the loop
    // would use the already-overwritten element and send everything to +inf.
    const double totalEnergy = curve[0];
    if (totalEnergy <= 0.0) return {};

    for (auto& v : curve)
        v = 10.0 * std::log10 (std::max (v / totalEnergy, 1e-30));   // energy, so 10*log10

    // First crossings of −5 dB and −35 dB.
    auto crossing = [&curve] (double db) -> std::optional<size_t>
    {
        for (size_t i = 0; i < curve.size(); ++i)
            if (curve[i] <= db)
                return i;
        return std::nullopt;
    };

    const auto i5  = crossing (-5.0);
    const auto i35 = crossing (-35.0);
    if (! i5 || ! i35 || *i35 <= *i5)
        return {};

    // Schroeder integration always appears to decay near the end of the buffer,
    // because the integration window is shrinking rather than the signal
    // stopping. A -35 dB crossing in the last 1% of the buffer means we measured
    // the window, not the tail, so report insufficient decay instead of a
    // confident wrong number.
    const size_t tailGuard = curve.size() - curve.size() / 100 - 1;
    if (*i35 >= tailGuard)
        return {};

    // Least-squares slope (dB per sample) across the fitted span.
    const size_t a = *i5, b = *i35;
    const double n = (double) (b - a + 1);
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    for (size_t i = a; i <= b; ++i)
    {
        const double x = (double) i, y = curve[i];
        sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    const double denom = n * sxx - sx * sx;
    if (std::abs (denom) < 1e-12) return {};

    const double slope = (n * sxy - sx * sy) / denom;   // dB per sample, negative
    if (slope >= 0.0) return {};

    return { -60.0 / slope / sampleRate, true };
}

//==============================================================================
// Group delay
//
// Unwrapped phase of the impulse response, differentiated. This is what tells us
// whether the tank actually disperses: a spring's group delay falls with
// frequency, a plain delay line's is flat.

/** Group delay in seconds per bin, for bins 0 .. fftSize/2 - 1. */
inline std::vector<double> groupDelay (const std::vector<float>& ir, int fftSize,
                                       double sampleRate)
{
    const auto s = spectrum (ir, fftSize);
    const size_t half = (size_t) fftSize / 2;

    std::vector<double> phase (half + 1);
    for (size_t k = 0; k <= half; ++k)
        phase[k] = std::arg (s[k]);

    // Unwrap.
    for (size_t k = 1; k <= half; ++k)
    {
        double d = phase[k] - phase[k - 1];
        while (d >  kPiM) { phase[k] -= 2.0 * kPiM; d = phase[k] - phase[k - 1]; }
        while (d < -kPiM) { phase[k] += 2.0 * kPiM; d = phase[k] - phase[k - 1]; }
    }

    // gd = -dphi/domega, with domega = 2*pi*fs/N rad/s per bin.
    const double scale = (double) fftSize / (2.0 * kPiM * sampleRate);
    std::vector<double> gd (half);
    for (size_t k = 0; k < half; ++k)
        gd[k] = -(phase[k + 1] - phase[k]) * scale;
    return gd;
}

/** Mean group delay in seconds across a frequency band. */
inline double groupDelayInBand (const std::vector<float>& ir, int fftSize, double sampleRate,
                               double lowHz, double highHz)
{
    const auto gd = groupDelay (ir, fftSize, sampleRate);
    const int lo = std::max (1, hzToBin (lowHz, fftSize, sampleRate));
    const int hi = std::min ((int) gd.size() - 1, hzToBin (highHz, fftSize, sampleRate));
    if (hi <= lo) return 0.0;

    double sum = 0.0;
    for (int k = lo; k <= hi; ++k)
        sum += gd[(size_t) k];
    return sum / (double) (hi - lo + 1);
}

//==============================================================================
// Distortion
//
// Measure with a fundamental on an exact bin centre (Generate::sineAtBin) so
// there is no spectral leakage and no windowing is needed.

/** Total harmonic distortion as a ratio, from harmonics 2..`maxHarmonic`. */
inline double thd (const std::vector<float>& out, int fundamentalBin, int fftSize,
                   int maxHarmonic = 10)
{
    const auto mag = magnitudeSpectrum (out, fftSize);
    if (fundamentalBin <= 0 || (size_t) fundamentalBin >= mag.size()) return 0.0;

    const double fund = mag[(size_t) fundamentalBin];
    if (fund <= 0.0) return 0.0;

    double harmonicEnergy = 0.0;
    for (int h = 2; h <= maxHarmonic; ++h)
    {
        const size_t bin = (size_t) (fundamentalBin * h);
        if (bin >= mag.size()) break;
        harmonicEnergy += mag[bin] * mag[bin];
    }
    return std::sqrt (harmonicEnergy) / fund;
}

/** Loudest inharmonic product, in dB relative to the fundamental. Everything
    that is neither DC, the fundamental, nor a harmonic of it is aliasing (plus
    whatever noise the process adds), so this is the aliasing floor. */
inline double aliasingFloorDb (const std::vector<float>& out, int fundamentalBin,
                               int fftSize, int binTolerance = 2)
{
    const auto mag = magnitudeSpectrum (out, fftSize);
    if (fundamentalBin <= 0 || (size_t) fundamentalBin >= mag.size()) return -200.0;

    const double fund = mag[(size_t) fundamentalBin];
    if (fund <= 0.0) return -200.0;

    double worst = 0.0;
    for (size_t k = 1; k < mag.size(); ++k)
    {
        bool harmonic = false;
        for (int h = 1; (size_t) (fundamentalBin * h) < mag.size(); ++h)
            if (std::abs ((long) k - (long) (fundamentalBin * h)) <= binTolerance)
            {
                harmonic = true;
                break;
            }
        if (! harmonic)
            worst = std::max (worst, mag[k]);
    }
    return toDb (worst / fund);
}

//==============================================================================
// Autocorrelation
//
// For echo spacing: the first strong peak in the autocorrelation of a reverb tail
// sits at the single-pass delay.

/** Lag in samples of the strongest autocorrelation peak within
    [minLag, maxLag], or nullopt when nothing rises above `threshold` times the
    zero-lag value. */
inline std::optional<int> autocorrelationPeak (const std::vector<float>& x,
                                               int minLag, int maxLag,
                                               double threshold = 0.1)
{
    if (x.size() < 2 || minLag < 1 || maxLag <= minLag) return std::nullopt;
    maxLag = std::min (maxLag, (int) x.size() - 1);

    double zero = 0.0;
    for (float v : x)
        zero += (double) v * (double) v;
    if (zero <= 0.0) return std::nullopt;

    double best = 0.0;
    int bestLag = -1;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        for (size_t i = 0; i + (size_t) lag < x.size(); ++i)
            sum += (double) x[i] * (double) x[i + (size_t) lag];
        if (sum > best)
        {
            best = sum;
            bestLag = lag;
        }
    }
    if (bestLag < 0 || best / zero < threshold) return std::nullopt;
    return bestLag;
}

} // namespace bv::test
