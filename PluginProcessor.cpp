#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

StrainAudioProcessor::StrainAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
}

AudioProcessorValueTreeState::ParameterLayout StrainAudioProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::input, 1 }, "Input",
        NormalisableRange<float> (-6.0f, 30.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::attack, 1 }, "Attack",
        NormalisableRange<float> (1.0f, 10.0f, 0.1f), 4.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::release, 1 }, "Release",
        NormalisableRange<float> (1.0f, 10.0f, 0.1f), 5.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::output, 1 }, "Output",
        NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::ratio, 1 }, "Ratio",
        StringArray { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE" }, 3));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::dist2, 1 }, "Dist 2", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::dist3, 1 }, "Dist 3", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::detHP, 1 }, "Det HP", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::opto,  1 }, "Opto",   false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::link,  1 }, "Link",   true));

    return layout;
}

strain::Params StrainAudioProcessor::readParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    strain::Params p;
    p.inputDb    = get (ParamIDs::input);
    p.attack     = get (ParamIDs::attack);
    p.release    = get (ParamIDs::release);
    p.outputDb   = get (ParamIDs::output);
    p.ratioIndex = (int) std::lround (get (ParamIDs::ratio));
    p.dist2      = get (ParamIDs::dist2) > 0.5f;
    p.dist3      = get (ParamIDs::dist3) > 0.5f;
    p.detHP      = get (ParamIDs::detHP) > 0.5f;
    p.opto       = get (ParamIDs::opto)  > 0.5f;
    p.link       = get (ParamIDs::link)  > 0.5f;
    return p;
}

bool StrainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void StrainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    setLatencySamples ((int) std::lround (oversampling.getLatencyInSamples()));

    core.setParams (readParams());
    core.prepare (sampleRate * 2.0); // le cœur tourne à 2x la fréquence d'échantillonnage de l'hôte
}

void StrainAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numChannels = std::min (buffer.getNumChannels(), 2);
    if (numChannels == 0 || buffer.getNumSamples() == 0)
        return;

    core.setParams (readParams());

    dsp::AudioBlock<float> block (buffer);
    auto active = block.getSubsetChannelBlock (0, (size_t) numChannels);

    auto upBlock = oversampling.processSamplesUp (active);

    float* channels[2] = { nullptr, nullptr };
    for (int c = 0; c < numChannels; ++c)
        channels[c] = upBlock.getChannelPointer ((size_t) c);

    core.process (channels, numChannels, (int) upBlock.getNumSamples());

    oversampling.processSamplesDown (active);

    grMeter.store (core.getGainReductionDb());
}

AudioProcessorEditor* StrainAudioProcessor::createEditor()
{
    return new StrainAudioProcessorEditor (*this);
}

void StrainAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StrainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StrainAudioProcessor();
}
