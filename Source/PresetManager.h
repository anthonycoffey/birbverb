#pragma once

// Preset handling — SPEC-004.
//
// Factory presets are the JUCE-free table in dsp/PresetData.h; this class wraps
// it with the APVTS and file handling. User presets are XML snapshots of the
// APVTS state in the plugin's application-data folder, merged into the same list.
//
// The current selection lives in the APVTS state tree (factory index, or -1 plus
// a name for a user preset) so it survives save/restore, and a factory preset
// shows a modified marker the moment a value drifts from its snapshot.

#include <JuceHeader.h>

#include "ParameterIDs.h"
#include "dsp/PresetData.h"

#include <cmath>

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        userDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                      .getChildFile ("Application Support")
                      .getChildFile ("birbverb")
                      .getChildFile ("Presets");
        userDir.createDirectory();

        if (! apvts.state.hasProperty (indexProp))
            apvts.state.setProperty (indexProp, 0, nullptr);   // boots on "Twin '64"
    }

    //==========================================================================
    int getNumPresets() const { return bv::numFactoryPresets; }

    juce::String getName (int i) const
    {
        return juce::isPositiveAndBelow (i, getNumPresets())
                 ? juce::String (bv::factoryPresets[(size_t) i].name)
                 : juce::String();
    }

    juce::String getDescription (int i) const
    {
        return juce::isPositiveAndBelow (i, getNumPresets())
                 ? juce::String (bv::factoryPresets[(size_t) i].description)
                 : juce::String();
    }

    /** -1 for a foundation preset, otherwise the index it was built from. */
    int getFoundationIndex (int i) const
    {
        return juce::isPositiveAndBelow (i, getNumPresets())
                 ? bv::factoryPresets[(size_t) i].derivedFrom
                 : -1;
    }

    bool isFoundation (int i) const { return getFoundationIndex (i) < 0; }

    juce::Array<juce::File> userPresetFiles() const
    {
        auto files = userDir.findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        return files;
    }

    int currentIndex() const { return (int) apvts.state.getProperty (indexProp, -1); }

    //==========================================================================
    /** True when a parameter has drifted from the loaded factory preset.
        Per-parameter tolerances, because a value that round-trips through
        normalisation is not bit-identical and should not read as modified. */
    bool isDirty() const
    {
        const int i = currentIndex();
        if (! juce::isPositiveAndBelow (i, getNumPresets())) return false;   // user / manual

        const auto& p = bv::factoryPresets[(size_t) i];
        auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

        return (int) v (bv::ids::circuit) != (int) p.circuit
            || (int) v (bv::ids::tank)    != (int) p.tank
            || std::abs (v (bv::ids::dwell) - p.dwell)          > 0.02f
            || std::abs (v (bv::ids::decay) - (float) p.decay)  > 0.02f
            || std::abs (v (bv::ids::tone)  - p.tone)           > 0.02f
            || std::abs (v (bv::ids::locut) - (float) p.locut)  > 1.0f
            || std::abs (v (bv::ids::boing) - p.boing)          > 0.02f
            || std::abs (v (bv::ids::mix)   - p.mix)            > 0.05f
            || std::abs (v (bv::ids::level) - p.level)          > 0.05f;
    }

    juce::String displayName() const
    {
        const int i = currentIndex();
        if (juce::isPositiveAndBelow (i, getNumPresets()))
            return getName (i) + (isDirty() ? " *" : "");

        const juce::String user = apvts.state.getProperty (nameProp, "").toString();
        return user.isNotEmpty() ? user : "Manual";
    }

    //==========================================================================
    /** Sets EVERY parameter — SPEC-004 requirement 1. A preset that sets some of
        them is a suggestion, not a foundation. */
    void load (int i)
    {
        if (! juce::isPositiveAndBelow (i, getNumPresets())) return;

        const auto& p = bv::factoryPresets[(size_t) i];
        setP (bv::ids::circuit, (float) (int) p.circuit);
        setP (bv::ids::tank,    (float) (int) p.tank);
        setP (bv::ids::dwell,   p.dwell);
        setP (bv::ids::decay,   (float) p.decay);
        setP (bv::ids::tone,    p.tone);
        setP (bv::ids::locut,   (float) p.locut);
        setP (bv::ids::boing,   p.boing);
        setP (bv::ids::mix,     p.mix);
        setP (bv::ids::level,   p.level);

        apvts.state.setProperty (indexProp, i, nullptr);
        apvts.state.setProperty (nameProp, "", nullptr);
    }

    /** Back to the foundation this preset was built on, keeping mix and level —
        SPEC-004 Nice to have. */
    void loadFoundation()
    {
        const int from = getFoundationIndex (currentIndex());
        if (from < 0) return;

        const float keepMix   = apvts.getRawParameterValue (bv::ids::mix)->load();
        const float keepLevel = apvts.getRawParameterValue (bv::ids::level)->load();
        load (from);
        setP (bv::ids::mix, keepMix);
        setP (bv::ids::level, keepLevel);
    }

    void loadUserPreset (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr) return;   // corrupt or truncated: skip, never fatal

        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;

        apvts.replaceState (tree);
        apvts.state.setProperty (indexProp, -1, nullptr);
        apvts.state.setProperty (nameProp, file.getFileNameWithoutExtension(), nullptr);
    }

    bool saveUserPreset (const juce::String& rawName)
    {
        const auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty()) return false;

        auto state = apvts.copyState();
        state.setProperty (indexProp, -1, nullptr);
        state.setProperty (nameProp, name, nullptr);

        auto xml = state.createXml();
        if (xml == nullptr) return false;

        if (! xml->writeTo (userDir.getChildFile (name + ".xml")))
            return false;   // directory not writable: report, do not throw

        apvts.state.setProperty (indexProp, -1, nullptr);
        apvts.state.setProperty (nameProp, name, nullptr);
        return true;
    }

    /** The arrows step through factory presets only — SPEC-004 requirement 9. */
    void next() { load ((juce::jmax (0, currentIndex()) + 1) % getNumPresets()); }
    void prev() { load ((juce::jmax (0, currentIndex()) - 1 + getNumPresets()) % getNumPresets()); }

private:
    void setP (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userDir;
    const juce::Identifier indexProp { "presetIndex" };
    const juce::Identifier nameProp  { "presetName" };
};
