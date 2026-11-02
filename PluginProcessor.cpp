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

    // Tous les potards suivent l'échelle 0..10 de la façade.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::input, 1 }, "Input",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 2.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::attack, 1 }, "Attack",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::release, 1 }, "Release",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::output, 1 }, "Output",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::ratio, 1 }, "Ratio",
        StringArray { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE" }, 3));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::opto, 1 }, "Opto", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::link, 1 }, "Link", true));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::detector, 1 }, "Detector",
        StringArray { "Off", "HP", "Mid", "HP + Mid" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::audio, 1 }, "Audio",
        StringArray { "Off", "HP", "Dist 2", "Dist 3" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));

    return layout;
}

strain::Params StrainAudioProcessor::readParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // ---- CALIBRATION : correspondance entre l'échelle 0..10 et les valeurs réelles ----
    strain::Params p;
    p.inputDb    = get (ParamIDs::input) * 3.0f - 6.0f;        // 0..10 -> -6..+24 dB
    p.attack     = 1.0f + get (ParamIDs::attack)  * 0.9f;      // 0..10 -> 1..10
    p.release    = 1.0f + get (ParamIDs::release) * 0.9f;
    p.outputDb   = (get (ParamIDs::output) - 5.0f) * 3.6f;     // 0..10 -> -18..+18 dB (5 = 0 dB)

    p.ratioIndex = (int) std::lround (get (ParamIDs::ratio));
    p.opto       = get (ParamIDs::opto) > 0.5f;
    p.link       = get (ParamIDs::link) > 0.5f;

    const int det = (int) std::lround (get (ParamIDs::detector));
    p.detHP  = (det == 1 || det == 3);
    p.detMid = (det == 2 || det == 3);

    const int aud = (int) std::lround (get (ParamIDs::audio));
    p.audioHP = (aud == 1);
    p.dist2   = (aud == 2);
    p.dist3   = (aud == 3);
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

    dryBuffer.setSize (2, samplesPerBlock * 2 + 16);
    dryBuffer.clear();

    mixSmoothed.reset (sampleRate * 2.0, 0.02);
    mixSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f
                                              ? 0.0f
                                              : apvts.getRawParameterValue (ParamIDs::mix)->load());
}

void StrainAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numChannels = std::min (buffer.getNumChannels(), 2);
    if (numChannels == 0 || buffer.getNumSamples() == 0)
        return;

    core.setParams (readParams());

    const bool bypassed = apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;
    const float mix     = apvts.getRawParameterValue (ParamIDs::mix)->load();
    mixSmoothed.setTargetValue (bypassed ? 0.0f : mix);

    dsp::AudioBlock<float> block (buffer);
    auto active = block.getSubsetChannelBlock (0, (size_t) numChannels);

    auto upBlock = oversampling.processSamplesUp (active);
    const int n  = (int) upBlock.getNumSamples();

    // Copie "sèche" prise au même endroit que le signal traité : parallèle parfaitement aligné.
    if (dryBuffer.getNumSamples() < n)
        dryBuffer.setSize (2, n, false, false, true);

    float* channels[2] = { nullptr, nullptr };
    for (int c = 0; c < numChannels; ++c)
    {
        channels[c] = upBlock.getChannelPointer ((size_t) c);
        dryBuffer.copyFrom (c, 0, channels[c], n);
    }

    core.process (channels, numChannels, n);

    // Mix dry/comp (et bypass, qui est un mix à 0 avec transition douce).
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = mixSmoothed.getNextValue();

        for (int c = 0; c < numChannels; ++c)
        {
            const float dry = dryBuffer.getSample (c, i);
            const float wet = channels[c][i];
            const float out = dry + m * (wet - dry);
            channels[c][i] = out;
            peak = std::max (peak, std::abs (out));
        }
    }

    oversampling.processSamplesDown (active);

    if (core.getGainReductionDb() > grMeter.load())
        grMeter.store (core.getGainReductionDb());

    if (peak > outPeak.load())
        outPeak.store (peak);
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
