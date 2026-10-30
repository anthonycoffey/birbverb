#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ui/SpriteAtlas.h"
#include "ui/SpriteStore.h"

//==============================================================================
/** Fonts, popup colours, and the procedural fallback drawing for every control
    type — SPEC-005 requirement 6. The fallback is deliberately plain: it is a
    development aid so DSP work never waits on art, not a second skin. */
class BirbLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BirbLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

//==============================================================================
/** A rotary control that paints one frame of a sprite atlas, or falls back to
    the LookAndFeel when it has no usable art. */
class SpriteKnob : public juce::Slider
{
public:
    SpriteKnob();
    void setAtlas (const bv::ui::SpriteAtlas* a) { atlas = a; }
    void paint (juce::Graphics&) override;

private:
    const bv::ui::SpriteAtlas* atlas = nullptr;
};

//==============================================================================
/** An N-position switch bound to a choice parameter. */
class SpriteSwitch : public juce::Component
{
public:
    SpriteSwitch (juce::RangedAudioParameter& param, juce::StringArray labels,
                  juce::String switchName);

    void setAtlas (const bv::ui::SpriteAtlas* a) { atlas = a; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& parameter;
    juce::StringArray positionLabels;
    juce::String name;
    int position = 0;
    const bv::ui::SpriteAtlas* atlas = nullptr;
    juce::ParameterAttachment attachment;
};

//==============================================================================
class SpriteFootswitch : public juce::Component
{
public:
    explicit SpriteFootswitch (juce::RangedAudioParameter& bypassParam);

    void setAtlas (const bv::ui::SpriteAtlas* a) { atlas = a; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& parameter;
    bool bypassed = false;
    const bv::ui::SpriteAtlas* atlas = nullptr;
    juce::ParameterAttachment attachment;
};

//==============================================================================
class SpriteLED : public juce::Component
{
public:
    void setLevel (float newLevel);
    void setAtlas (const bv::ui::SpriteAtlas* a) { atlas = a; }
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
    const bv::ui::SpriteAtlas* atlas = nullptr;
};

//==============================================================================
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (PresetManager&);

    void setAtlases (const bv::ui::SpriteAtlas* screenAtlas,
                     const bv::ui::SpriteAtlas* arrowAtlas);
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void showBrowser();
    void showSaveDialog();

    PresetManager& presets;
    juce::String displayed;
    juce::Rectangle<int> leftArrow, rightArrow, screen;
    const bv::ui::SpriteAtlas* screenArt = nullptr;
    const bv::ui::SpriteAtlas* arrowArt = nullptr;
    std::unique_ptr<juce::AlertWindow> saveDialog;
};

//==============================================================================
class BirbverbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit BirbverbAudioProcessorEditor (BirbverbAudioProcessor&);
    ~BirbverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void styleKnob (SpriteKnob&, const juce::String& controlId, const char* atlasName);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    BirbverbAudioProcessor& processor;
    bv::ui::SpriteStore sprites;
    BirbLookAndFeel lnf;

    SpriteKnob dwell, decay, tone, locut, boing, mix, level;
    std::unique_ptr<SA> dwellAtt, decayAtt, toneAtt, locutAtt, boingAtt, mixAtt, levelAtt;

    std::unique_ptr<SpriteSwitch> circuitSwitch, tankSwitch;
    PresetBar presetBar;
    std::unique_ptr<SpriteFootswitch> footswitch;
    SpriteLED led;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BirbverbAudioProcessorEditor)
};
