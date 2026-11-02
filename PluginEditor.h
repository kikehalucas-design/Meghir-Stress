#pragma once

#include <array>
#include <memory>

#include "PluginProcessor.h"

// Look "rack" : potards dessinés en code, boutons à LED, palette sombre + cuivre.
class StrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

// Rangée de LED de réduction de gain.
class LedMeter : public juce::Component
{
public:
    void setLevel (float grDb);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
};

class StrainAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit StrainAudioProcessorEditor (StrainAudioProcessor&);
    ~StrainAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void syncRatioButtons();

    StrainAudioProcessor& audioProcessor;
    StrainLookAndFeel laf;

    juce::Slider inputKnob, attackKnob, releaseKnob, outputKnob;

    juce::ComboBox ratioCombo; // invisible : sert uniquement de relais vers le paramètre
    std::array<juce::ToggleButton, 8> ratioButtons;

    juce::ToggleButton dist2Button { "DIST 2" }, dist3Button { "DIST 3" },
                       detHpButton { "DET HP" }, optoButton  { "OPTO" },
                       linkButton  { "LINK" };

    LedMeter meter;

    // Les attachements doivent être détruits avant les composants : on les déclare en dernier.
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> inputAtt, attackAtt, releaseAtt, outputAtt;
    std::unique_ptr<ComboBoxAttachment> ratioAtt;
    std::unique_ptr<ButtonAttachment> dist2Att, dist3Att, detHpAtt, optoAtt, linkAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrainAudioProcessorEditor)
};
