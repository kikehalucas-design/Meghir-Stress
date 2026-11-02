#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>

#include "StrainCore.h"

namespace ParamIDs
{
    inline constexpr const char* input   = "input";
    inline constexpr const char* attack  = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* output  = "output";
    inline constexpr const char* ratio   = "ratio";
    inline constexpr const char* dist2   = "dist2";
    inline constexpr const char* dist3   = "dist3";
    inline constexpr const char* detHP   = "dethp";
    inline constexpr const char* opto    = "opto";
    inline constexpr const char* link    = "link";
}

class StrainAudioProcessor : public juce::AudioProcessor
{
public:
    StrainAudioProcessor();
    ~StrainAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Réduction de gain (dB, positive) du dernier bloc : lue par le vumètre de l'éditeur.
    float getGainReductionDb() const { return grMeter.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    strain::Params readParams() const;

    strain::Core core;

    // Suréchantillonnage 2x (les saturations et les attaques rapides en profitent).
    juce::dsp::Oversampling<float> oversampling { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };

    std::atomic<float> grMeter { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrainAudioProcessor)
};
