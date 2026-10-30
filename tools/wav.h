// Minimal WAV read/write (little-endian hosts). Write = 32-bit float; read
// supports PCM 16/24/32 and float32, any channel count (downmixable to mono).
#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace wav
{
struct Data { std::vector<float> samples; int channels = 1; int sampleRate = 48000; };

inline void w32 (std::ofstream& f, uint32_t v) { f.write (reinterpret_cast<char*> (&v), 4); }
inline void w16 (std::ofstream& f, uint16_t v) { f.write (reinterpret_cast<char*> (&v), 2); }

// interleaved float samples
inline bool write (const std::string& path, const float* data, int frames, int channels, int sampleRate)
{
    std::ofstream f (path, std::ios::binary);
    if (! f) return false;
    const uint32_t dataBytes = (uint32_t) frames * (uint32_t) channels * 4u;
    f.write ("RIFF", 4); w32 (f, 36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); w32 (f, 16); w16 (f, 3 /*float*/); w16 (f, (uint16_t) channels);
    w32 (f, (uint32_t) sampleRate);
    w32 (f, (uint32_t) (sampleRate * channels * 4));
    w16 (f, (uint16_t) (channels * 4)); w16 (f, 32);
    f.write ("data", 4); w32 (f, dataBytes);
    f.write (reinterpret_cast<const char*> (data), (std::streamsize) dataBytes);
    return f.good();
}

inline uint32_t r32 (const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24); }
inline uint16_t r16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }

inline bool read (const std::string& path, Data& out)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return false;
    std::vector<uint8_t> b ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    if (b.size() < 44 || std::memcmp (b.data(), "RIFF", 4) || std::memcmp (b.data() + 8, "WAVE", 4)) return false;

    uint16_t fmt = 1, ch = 1, bits = 16; uint32_t sr = 48000;
    size_t dataPos = 0, dataLen = 0, i = 12;
    while (i + 8 <= b.size())
    {
        const uint32_t sz = r32 (&b[i + 4]);
        if (! std::memcmp (&b[i], "fmt ", 4))
        {
            fmt = r16 (&b[i + 8]); ch = r16 (&b[i + 10]); sr = r32 (&b[i + 12]); bits = r16 (&b[i + 22]);
        }
        else if (! std::memcmp (&b[i], "data", 4))
        {
            dataPos = i + 8; dataLen = sz; break;
        }
        i += 8 + sz + (sz & 1);
    }
    if (! dataPos) return false;

    out.channels = ch; out.sampleRate = (int) sr;
    const int bytes = bits / 8;
    const size_t n = dataLen / (size_t) bytes;
    out.samples.resize (n);
    const uint8_t* p = &b[dataPos];
    for (size_t k = 0; k < n; ++k, p += bytes)
    {
        if (fmt == 3) { float v; std::memcpy (&v, p, 4); out.samples[k] = v; }
        else if (bits == 16) { out.samples[k] = (int16_t) r16 (p) / 32768.0f; }
        else if (bits == 24) { int32_t v = (p[0] | (p[1] << 8) | (p[2] << 16)); if (v & 0x800000) v |= ~0xffffff; out.samples[k] = v / 8388608.0f; }
        else if (bits == 32) { out.samples[k] = (int32_t) r32 (p) / 2147483648.0f; }
    }
    return true;
}

inline std::vector<float> toMono (const Data& d)
{
    if (d.channels <= 1) return d.samples;
    const size_t frames = d.samples.size() / (size_t) d.channels;
    std::vector<float> m (frames, 0.0f);
    for (size_t i = 0; i < frames; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < d.channels; ++c) s += d.samples[i * d.channels + c];
        m[i] = s / d.channels;
    }
    return m;
}

} // namespace wav
