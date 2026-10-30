// Offline renderer — a listening diagnostic, not a test.
//
// Writes wav files so you can hear what the engine is doing. SPEC-006's
// Non-goals are explicit that rendered audio is NOT a pass/fail oracle: float
// differences across platforms and compilers make golden-file comparison flaky.
// Assert on measured properties in tests/, listen here.
//
//   cmake --build build --target bv_render -j
//   ./build/bv_render out/
//
// While SpringTank is a skeleton (SPEC-001) the wet path is silent, so the
// renders are dry. That is the expected state until PLAN-001 Phase 1 lands.

#include "dsp/EngineParams.h"
#include "dsp/PresetData.h"
#include "dsp/ReverbEngine.h"
#include "wav.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

/** A plucked-string-ish test signal: a few decaying harmonics with a hard
    attack. Spring reverb is mostly judged on transients, so a sine tells you
    very little about whether it sounds right. */
std::vector<float> pluck (double seconds, double fundamentalHz)
{
    const int n = (int) (kSampleRate * seconds);
    std::vector<float> out ((size_t) n, 0.0f);

    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / kSampleRate;
        const double env = std::exp (-t * 2.5) * (1.0 - std::exp (-t * 800.0));

        double s = 0.0;
        for (int h = 1; h <= 6; ++h)
            s += std::sin (2.0 * kPi * fundamentalHz * h * t) / (double) (h * h);

        out[(size_t) i] = (float) (0.4 * env * s);
    }
    return out;
}

bv::EngineParams paramsFor (const bv::Preset& p)
{
    bv::EngineParams e;
    e.circuit = p.circuit;
    e.tank    = p.tank;
    e.dwell   = p.dwell;
    e.decay   = p.decay;
    e.tone    = p.tone;
    e.locut   = p.locut;
    e.boing   = p.boing;
    e.mix     = p.mix;
    e.level   = p.level;
    e.bypass  = false;
    return e;
}

std::string slug (const char* name)
{
    std::string s;
    for (const char* c = name; *c != '\0'; ++c)
        s += (*c == ' ' || *c == '\'') ? '-' : (char) std::tolower ((unsigned char) *c);
    return s;
}

void renderPreset (const bv::Preset& preset, const std::string& outDir)
{
    auto dry = pluck (6.0, 110.0);
    std::vector<float> left = dry, right = dry;
    float* channels[2] = { left.data(), right.data() };

    bv::ReverbEngine engine;
    engine.prepare (kSampleRate, 2, 512);
    engine.setParams (paramsFor (preset));
    engine.process (channels, 2, (int) left.size());

    std::vector<float> interleaved (left.size() * 2);
    for (size_t i = 0; i < left.size(); ++i)
    {
        interleaved[i * 2]     = left[i];
        interleaved[i * 2 + 1] = right[i];
    }

    const auto path = outDir + "/" + slug (preset.name) + ".wav";
    if (wav::write (path, interleaved.data(), (int) left.size(), 2, (int) kSampleRate))
        std::printf ("  %-16s -> %s\n", preset.name, path.c_str());
    else
        std::printf ("  %-16s -> FAILED to write %s\n", preset.name, path.c_str());
}
} // namespace

int main (int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : ".";

    std::printf ("birbverb offline render -> %s\n", outDir.c_str());
    std::printf ("NOTE: SpringTank is a skeleton (SPEC-001), so the wet path is silent.\n");
    std::printf ("      These renders are dry until PLAN-001 Phase 1 lands.\n\n");

    for (const auto& preset : bv::factoryPresets)
        renderPreset (preset, outDir);

    return 0;
}
