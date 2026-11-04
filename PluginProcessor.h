#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>

#include "StrainCore.h"

namespace ParamIDs
{
    inline constexpr const char* input    = "input";
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* release  = "release";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* ratio    = "ratio";
    inline constexpr const char* opto     = "opto";
    inline constexpr const char* link     = "link";
    inline constexpr const char* detector = "detector";   // Off / HP / Mid / HP+Mid
    inline constexpr const char* audio    = "audio";      // Off / HP / Dist 2 / Dist 3
    inline constexpr const char* power    = "power";      // interrupteur POWER : désactivé = signal d'origine
    inline constexpr const char* threshold = "threshold"; // seuil (0..10), utilisé seulement si thrOn
    inline constexpr const char* thrOn    = "thron";      // active le potard Threshold
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

    // Lus par l'éditeur (à peu près 30 fois par seconde) : renvoient le maximum
    // depuis la dernière lecture, puis remettent le compteur à zéro.
    float takeGainReductionDb() { return grMeter.exchange (0.0f); }
    float takeOutputPeak()      { return outPeak.exchange (0.0f); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    strain::Params readParams() const;

    strain::Core core;

    // Suréchantillonnage 2x (les saturations et les attaques rapides en profitent).
    juce::dsp::Oversampling<float> oversampling { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };

    juce::AudioBuffer<float> dryBuffer;          // copie du signal d'entrée (même alignement)
    juce::SmoothedValue<float> mixSmoothed;      // mix / bypass lissés

    std::atomic<float> grMeter { 0.0f };
    std::atomic<float> outPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrainAudioProcessor)
};
