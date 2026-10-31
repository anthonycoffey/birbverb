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
#include "dsp/ParameterDefaults.h"
#include "dsp/PresetData.h"
#include "dsp/PresetState.h"

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

        // Boots on the SPEC-003 boot preset. No load() here: the APVTS defaults
        // ARE that preset's values (dsp/ParameterDefaults.h), so selecting it is
        // all it takes, and the host's own "reset to default" lands on the same
        // state instead of re-lighting the modified marker.
        if (! apvts.state.hasProperty (indexProp))
            apvts.state.setProperty (indexProp, bv::defaults::bootPresetIndex, nullptr);
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
    /** The nine stored parameters, denormalised, as the JUCE-free comparison in
        dsp/PresetState.h wants them. */
    bv::ParamValues currentValues() const
    {
        auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

        return { (int) v (bv::ids::circuit), (int) v (bv::ids::tank),
                 v (bv::ids::dwell), (double) v (bv::ids::decay),
                 v (bv::ids::tone),  (double) v (bv::ids::locut),
                 v (bv::ids::boing), v (bv::ids::mix), v (bv::ids::level) };
    }

    /** True when a parameter has drifted from the loaded factory preset. The
        comparison and its per-parameter tolerances live in dsp/PresetState.h so
        the tests can exercise them without JUCE. */
    bool isDirty() const
    {
        const int i = currentIndex();
        if (! juce::isPositiveAndBelow (i, getNumPresets())) return false;   // user / manual

        return ! bv::matchesPreset (bv::factoryPresets[(size_t) i], currentValues());
    }

    juce::String displayName() const
    {
        const juce::String user = apvts.state.getProperty (nameProp, "").toString();
        if (! juce::isPositiveAndBelow (currentIndex(), getNumPresets()) && user.isNotEmpty())
            return user;

        return juce::String (bv::presetDisplayName (currentIndex(), currentValues()));
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
