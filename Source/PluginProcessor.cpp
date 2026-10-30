#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/CircuitModel.h"
#include "dsp/TankConfig.h"

using namespace juce;

//==============================================================================
// The parameter layout IS the contract in SPEC-003. tests/test_parameters.cpp
// asserts this table, so a change here without a spec change is a test failure.

AudioProcessorValueTreeState::ParameterLayout
BirbverbAudioProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    constexpr int v = bv::ids::version;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { bv::ids::circuit, v }, "Circuit",
        StringArray { bv::circuitModelName (bv::CircuitModel::ab763),
                      bv::circuitModelName (bv::CircuitModel::vibroverb6G16),
                      bv::circuitModelName (bv::CircuitModel::reverbUnit6G15),
                      bv::circuitModelName (bv::CircuitModel::ampegRocket) },
        0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { bv::ids::tank, v }, "Tank",
        StringArray { bv::tankTypeName (bv::TankType::short2Spring),
                      bv::tankTypeName (bv::TankType::long2Spring),
                      bv::tankTypeName (bv::TankType::long3Spring) },
        1));   // Long 2-Spring — the Fender blackface tank

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { bv::ids::dwell, v }, "Dwell",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { bv::ids::decay, v }, "Decay",
        NormalisableRange<float> (0.8f, 4.5f, 0.01f, 0.8f), 3.4f,
        AudioParameterFloatAttributes().withLabel ("s")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { bv::ids::tone, v }, "Tone",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));

    // 318 Hz is the AB763 send high-pass corner — 500 pF into a 1 M grid leak.
    // See circuits/fender-ab763-reverb.md.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { bv::ids::locut, v }, "Lo Cut",
        NormalisableRange<float> (80.0f, 600.0f, 1.0f, 0.5f), 318.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { bv::ids::boing, v }, "Boing",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { bv::ids::mix, v }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { bv::ids::level, v }, "Level",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { bv::ids::bypass, v }, "Bypass", false));

    return layout;
}

//==============================================================================
BirbverbAudioProcessor::BirbverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "birbverb", createParameterLayout())
{
}

//==============================================================================
void BirbverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, getTotalNumOutputChannels(), samplesPerBlock);
    engine.setParams (gatherParams());
}

bool BirbverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

bv::EngineParams BirbverbAudioProcessor::gatherParams() const
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    bv::EngineParams p;
    p.circuit = (bv::CircuitModel) (int) raw (bv::ids::circuit);
    p.tank    = (bv::TankType)     (int) raw (bv::ids::tank);
    p.dwell   = raw (bv::ids::dwell);
    p.decay   = (double) raw (bv::ids::decay);
    p.tone    = raw (bv::ids::tone);
    p.locut   = (double) raw (bv::ids::locut);
    p.boing   = raw (bv::ids::boing);
    p.mix     = raw (bv::ids::mix);
    p.level   = raw (bv::ids::level);
    p.bypass  = raw (bv::ids::bypass) > 0.5f;
    return p;
}

void BirbverbAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setParams (gatherParams());
    engine.process (buffer.getArrayOfWritePointers(), numOut, buffer.getNumSamples());

    // Published for the UI's LED. Peak over the block is plenty at ~30 fps.
    float peak = 0.0f;
    for (int ch = 0; ch < numOut; ++ch)
        peak = jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    wetLevel.store (peak, std::memory_order_relaxed);
}

//==============================================================================
AudioProcessorEditor* BirbverbAudioProcessor::createEditor()
{
    return new BirbverbAudioProcessorEditor (*this);
}

void BirbverbAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BirbverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    auto tree = ValueTree::fromXml (*xml);
    if (! tree.isValid()) return;

    // replaceState keeps parameters it recognises and leaves the rest at their
    // defaults, so state from an older or newer version loads rather than
    // failing — SPEC-003 edge cases.
    apvts.replaceState (tree);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BirbverbAudioProcessor();
}
