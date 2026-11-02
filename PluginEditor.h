#pragma once

#include <array>
#include <functional>
#include <memory>

#include "PluginProcessor.h"

// Dessine les potards à échelle rotative (façon Distressor) et le potard Mix noir.
class StrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

// Bouton poussoir métallique, ou simple zone cliquable invisible.
class PushButton : public juce::Component
{
public:
    PushButton() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void setMetal (bool shouldDrawMetal) { metal = shouldDrawMetal; }

    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool metal = true;
    bool down  = false;
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
    void drawStaticFace (juce::Graphics&);   // éléments fixes, dans l'espace de conception 1130 x 406
    void drawDynamicParts (juce::Graphics&); // LED, dans le même espace

    int  getChoiceValue (const char* id) const;
    bool getBoolValue   (const char* id) const;
    void setChoiceValue (const char* id, int index);
    void setBoolValue   (const char* id, bool value);
    void cycleChoice    (const char* id, int numChoices);

    StrainAudioProcessor& audioProcessor;
    StrainLookAndFeel laf;

    juce::Image background;
    float grDisplay = 0.0f;
    float peakHold  = 0.0f;

    juce::Slider inputKnob, attackKnob, releaseKnob, outputKnob, mixKnob;

    PushButton bypassButton, ratioButton, detectorButton, audioButton;
    PushButton optoZone, linkZone;
    std::array<PushButton, 8> ratioZones;

    // Les attachements doivent être détruits avant les composants : on les déclare en dernier.
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inputAtt, attackAtt, releaseAtt, outputAtt, mixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrainAudioProcessorEditor)
};
