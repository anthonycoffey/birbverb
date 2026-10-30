#pragma once

#include <JuceHeader.h>

#include "ParameterIDs.h"
#include "PresetManager.h"
#include "dsp/EngineParams.h"
#include "dsp/ReverbEngine.h"

//==============================================================================
class BirbverbAudioProcessor : public juce::AudioProcessor
{
public:
    BirbverbAudioProcessor();
    ~BirbverbAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()   const override { return false; }
    bool producesMidi()  const override { return false; }
    bool isMidiEffect()  const override { return false; }

    /** Long enough for the 4.5 s maximum decay plus the tail below audibility. */
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };

    /** Published for the UI's level LED. Written on the audio thread, read on the
        message thread, so it is atomic and nothing else crosses the boundary. */
    float getWetLevel() const noexcept { return wetLevel.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    bv::EngineParams gatherParams() const;

    bv::ReverbEngine engine;
    std::atomic<float> wetLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BirbverbAudioProcessor)
};
