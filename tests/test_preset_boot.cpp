// SPEC-003 acceptance criterion 9 — the boot state.
// SPEC-004 acceptance criterion 6 — the modified marker.
//
// NOT tagged [.red]. The boot state is a property of two constexpr tables and a
// comparison over them, so this runs in the default suite and needs no engine.
//
// The bug this file exists to catch: SPEC-003's default column and SPEC-004's
// "Twin '64" foundation row are the same position written out twice. They drifted
// on `tone` (5.0 vs 5.5) and `mix` (30 vs 28), both past their tolerance, so a
// freshly launched plugin displayed "Twin '64 *" — the marker lit before the
// player had touched a control. Nothing asserted the boot display string, so it
// had to be noticed by looking at the UI. Now it does not.

#include <catch2/catch_test_macros.hpp>

#include "dsp/ParameterDefaults.h"
#include "dsp/PresetData.h"
#include "dsp/PresetState.h"

#include <string>

namespace
{
const bv::Preset& bootPreset() { return bv::factoryPresets[(size_t) bv::defaults::bootPresetIndex]; }
} // namespace

//==============================================================================
TEST_CASE ("a fresh instance displays the boot preset with no modified marker",
           "[spec-003][spec-004][presets]")
{
    // The whole point, asserted as the string the preset display shows.
    REQUIRE (bv::presetDisplayName (bv::defaults::bootPresetIndex, bv::defaultParamValues())
             == std::string ("Twin '64"));
}

TEST_CASE ("the boot preset is a researched foundation", "[spec-003][spec-004][presets]")
{
    // SPEC-003's boot state: the first thing a player hears is a circuit study,
    // not a blend of round numbers. A derived preset would be a draft value
    // (SPEC-004's task list) sitting in the boot position.
    REQUIRE (bv::defaults::bootPresetIndex >= 0);
    REQUIRE (bv::defaults::bootPresetIndex < bv::numFoundationPresets);
    REQUIRE (bootPreset().derivedFrom == -1);
}

TEST_CASE ("the parameter defaults are the boot preset's values",
           "[spec-003][spec-004][presets]")
{
    // SPEC-003's default column and SPEC-004's foundation row, compared through
    // the same tolerances the modified marker uses. If a listening pass moves
    // Twin '64, this fails and the defaults have to move with it — which is the
    // review gate, not an inconvenience.
    const auto& p = bootPreset();
    const auto  d = bv::defaultParamValues();

    CHECK (d.circuit == (int) p.circuit);
    CHECK (d.tank    == (int) p.tank);
    CHECK (d.dwell   == p.dwell);
    CHECK (d.decay   == p.decay);
    CHECK (d.tone    == p.tone);
    CHECK (d.locut   == p.locut);
    CHECK (d.boing   == p.boing);
    CHECK (d.mix     == p.mix);
    CHECK (d.level   == p.level);

    REQUIRE (bv::matchesPreset (p, d));
}

//==============================================================================
TEST_CASE ("the modified marker lights on a real edit and not on a round trip",
           "[spec-004][presets]")
{
    // SPEC-004 acceptance criterion 6 and its "nudged and nudged back" edge case.
    // Without this the test above could pass with a comparison that always says
    // "matches", which would hide the next drift instead of catching it.
    const auto& p = bootPreset();
    const int   i = bv::defaults::bootPresetIndex;

    SECTION ("one knob moved")
    {
        auto v = bv::defaultParamValues();
        v.tone += 0.5f;

        CHECK_FALSE (bv::matchesPreset (p, v));
        REQUIRE (bv::presetDisplayName (i, v) == std::string ("Twin '64 *"));
    }

    SECTION ("circuit switched")
    {
        auto v = bv::defaultParamValues();
        v.circuit = (int) bv::CircuitModel::reverbUnit6G15;

        REQUIRE (bv::presetDisplayName (i, v) == std::string ("Twin '64 *"));
    }

    SECTION ("a value that only round-tripped through normalisation")
    {
        // 0..1 normalisation and back is not bit-identical, and that must not
        // read as an edit. Half a tolerance stands in for that error.
        auto v = bv::defaultParamValues();
        v.dwell += bv::tolerance::knob * 0.5f;
        v.mix   -= bv::tolerance::gain * 0.5f;
        v.locut += bv::tolerance::locut * 0.5;

        CHECK (bv::matchesPreset (p, v));
        REQUIRE (bv::presetDisplayName (i, v) == std::string ("Twin '64"));
    }
}

TEST_CASE ("an index outside the factory bank is not a factory preset",
           "[spec-004][presets]")
{
    // PresetManager substitutes a user preset's name over this; "Manual" is what
    // is left when there is no selection at all.
    CHECK (bv::presetDisplayName (-1, bv::defaultParamValues()) == std::string ("Manual"));
    CHECK (bv::presetDisplayName (bv::numFactoryPresets, bv::defaultParamValues())
           == std::string ("Manual"));
}
