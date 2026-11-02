#include "PluginEditor.h"

namespace
{
    const juce::Colour kPanelTop    { 0xff222830 };
    const juce::Colour kPanelBottom { 0xff0f1215 };
    const juce::Colour kCopper      { 0xffc9772b };
    const juce::Colour kLedOn       { 0xffff7a1a };
    const juce::Colour kLedOff      { 0xff3a2a1e };
    const juce::Colour kText        { 0xffd8dde3 };
    const juce::Colour kTextDim     { 0xff8a93a0 };
}

//==============================================================================
// Look and feel
//==============================================================================
void StrainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();

    // Graduations 0..10
    g.setColour (kTextDim);
    for (int i = 0; i <= 10; ++i)
    {
        const float a  = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto p1  = centre.getPointOnCircumference (radius, a);
        const auto p2  = centre.getPointOnCircumference (radius - (i % 5 == 0 ? 7.0f : 4.0f), a);
        g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.2f);
    }

    // Corps du potard
    const float body = radius - 10.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff59616c), centre.x, centre.y - body,
                                             juce::Colour (0xff14171b), centre.x, centre.y + body, false));
    g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.5f);

    // Repère
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto tip    = centre.getPointOnCircumference (body - 3.0f, angle);
    const auto base   = centre.getPointOnCircumference (body * 0.35f, angle);

    g.setColour (juce::Colour (0xfff2e6c9));
    g.drawLine (base.x, base.y, tip.x, tip.y, 3.0f);
}

void StrainLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                          bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (down ? juce::Colour (0xff2e353d) : highlighted ? juce::Colour (0xff282e35) : juce::Colour (0xff1e2329));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff3a424c));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const bool on = b.getToggleState();
    const auto led = juce::Rectangle<float> (r.getCentreX() - 4.0f, r.getY() + 6.0f, 8.0f, 8.0f);

    if (on)
    {
        g.setColour (kLedOn.withAlpha (0.25f));
        g.fillEllipse (led.expanded (3.0f));
    }

    g.setColour (on ? kLedOn : kLedOff);
    g.fillEllipse (led);

    g.setColour (on ? kText : kTextDim);
    g.setFont (11.5f);
    g.drawText (b.getButtonText(), r.withTrimmedTop (20.0f).toNearestInt(), juce::Justification::centred, false);
}

//==============================================================================
// Vumètre à LED
//==============================================================================
void LedMeter::setLevel (float grDb)
{
    level = std::max (grDb, level * 0.88f); // retombée progressive
    repaint();
}

void LedMeter::paint (juce::Graphics& g)
{
    static constexpr float steps[10] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 10.0f, 14.0f, 18.0f, 24.0f };

    const auto area = getLocalBounds().toFloat();
    const float w   = area.getWidth() / 10.0f;

    for (int i = 0; i < 10; ++i)
    {
        const auto cell = juce::Rectangle<float> (area.getX() + (float) i * w, area.getY(), w, 22.0f).reduced (2.0f);
        const bool lit  = level >= steps[i];
        const auto col  = i >= 7 ? juce::Colour (0xffe0412c) : kLedOn;

        if (lit)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillRoundedRectangle (cell.expanded (2.0f), 3.0f);
        }

        g.setColour (lit ? col : kLedOff);
        g.fillRoundedRectangle (cell, 2.0f);

        g.setColour (kTextDim);
        g.setFont (9.5f);
        g.drawText (juce::String ((int) steps[i]),
                    juce::Rectangle<float> (area.getX() + (float) i * w, area.getY() + 24.0f, w, 14.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }
}

//==============================================================================
// Éditeur
//==============================================================================
StrainAudioProcessorEditor::StrainAudioProcessorEditor (StrainAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&laf);

    auto setupKnob = [this] (juce::Slider& s, const juce::String& suffix, int decimals)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (decimals);
        s.setColour (juce::Slider::textBoxTextColourId, kText);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);
    };

    setupKnob (inputKnob,   " dB", 1);
    setupKnob (attackKnob,  "",    1);
    setupKnob (releaseKnob, "",    1);
    setupKnob (outputKnob,  " dB", 1);

    // Ratio : le ComboBox invisible porte le paramètre, les boutons à LED le pilotent.
    ratioCombo.addItemList (juce::StringArray { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE" }, 1);

    const char* ratioLabels[8] = { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE" };
    for (int i = 0; i < 8; ++i)
    {
        auto& b = ratioButtons[(size_t) i];
        b.setButtonText (ratioLabels[i]);
        b.setRadioGroupId (1001);
        b.setClickingTogglesState (true);
        b.onClick = [this, i] { ratioCombo.setSelectedItemIndex (i, juce::sendNotificationSync); };
        addAndMakeVisible (b);
    }

    for (auto* b : { &dist2Button, &dist3Button, &detHpButton, &optoButton, &linkButton })
        addAndMakeVisible (*b);

    addAndMakeVisible (meter);

    auto& apvts = audioProcessor.apvts;
    inputAtt   = std::make_unique<SliderAttachment> (apvts, ParamIDs::input,   inputKnob);
    attackAtt  = std::make_unique<SliderAttachment> (apvts, ParamIDs::attack,  attackKnob);
    releaseAtt = std::make_unique<SliderAttachment> (apvts, ParamIDs::release, releaseKnob);
    outputAtt  = std::make_unique<SliderAttachment> (apvts, ParamIDs::output,  outputKnob);
    ratioAtt   = std::make_unique<ComboBoxAttachment> (apvts, ParamIDs::ratio, ratioCombo);
    dist2Att   = std::make_unique<ButtonAttachment> (apvts, ParamIDs::dist2, dist2Button);
    dist3Att   = std::make_unique<ButtonAttachment> (apvts, ParamIDs::dist3, dist3Button);
    detHpAtt   = std::make_unique<ButtonAttachment> (apvts, ParamIDs::detHP, detHpButton);
    optoAtt    = std::make_unique<ButtonAttachment> (apvts, ParamIDs::opto,  optoButton);
    linkAtt    = std::make_unique<ButtonAttachment> (apvts, ParamIDs::link,  linkButton);

    setSize (800, 300);
    syncRatioButtons();
    startTimerHz (30);
}

StrainAudioProcessorEditor::~StrainAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void StrainAudioProcessorEditor::syncRatioButtons()
{
    const int idx = ratioCombo.getSelectedItemIndex();
    for (int i = 0; i < 8; ++i)
        ratioButtons[(size_t) i].setToggleState (i == idx, juce::dontSendNotification);
}

void StrainAudioProcessorEditor::timerCallback()
{
    meter.setLevel (audioProcessor.getGainReductionDb());
    syncRatioButtons(); // reflète l'automation / le rappel de preset
}

void StrainAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Façade
    g.setGradientFill (juce::ColourGradient (kPanelTop, 0.0f, 0.0f, kPanelBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Filet de brossage
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    for (int y = 0; y < getHeight(); y += 3)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    // Vis aux quatre coins
    const juce::Point<float> screws[4] = {
        { 12.0f, 12.0f },
        { (float) getWidth() - 12.0f, 12.0f },
        { 12.0f, (float) getHeight() - 12.0f },
        { (float) getWidth() - 12.0f, (float) getHeight() - 12.0f }
    };

    g.setColour (juce::Colour (0xff0b0d10));
    for (const auto& pt : screws)
    {
        g.fillEllipse (pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff3a424c));
        g.drawLine (pt.x - 3.0f, pt.y - 3.0f, pt.x + 3.0f, pt.y + 3.0f, 1.0f);
        g.setColour (juce::Colour (0xff0b0d10));
    }

    // Titre
    g.setColour (kText);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawText ("MEGHIR STRESS", 30, 12, 300, 30, juce::Justification::centredLeft, false);

    g.setColour (kTextDim);
    g.setFont (11.0f);
    g.drawText ("DYNAMICS PROCESSOR", getWidth() - 30 - 220, 16, 220, 22, juce::Justification::centredRight, false);

    g.setColour (kCopper);
    g.fillRect (30, 46, getWidth() - 60, 2);

    // Étiquettes des potards
    g.setColour (kText);
    g.setFont (12.0f);
    const char* names[4] = { "INPUT", "ATTACK", "RELEASE", "OUTPUT" };
    for (int i = 0; i < 4; ++i)
        g.drawText (names[i], 30 + i * 135, 56, 120, 16, juce::Justification::centred, false);

    g.drawText ("GAIN REDUCTION (dB)", 590, 88, 180, 16, juce::Justification::centred, false);

    g.setColour (kTextDim);
    g.drawText ("RATIO", 30, 196, 100, 14, juce::Justification::centredLeft, false);
    g.drawText ("MODES", 520, 196, 100, 14, juce::Justification::centredLeft, false);
}

void StrainAudioProcessorEditor::resized()
{
    inputKnob.setBounds   (30,          74, 120, 124);
    attackKnob.setBounds  (30 + 135,    74, 120, 124);
    releaseKnob.setBounds (30 + 135 * 2, 74, 120, 124);
    outputKnob.setBounds  (30 + 135 * 3, 74, 120, 124);

    meter.setBounds (595, 110, 170, 40);

    for (int i = 0; i < 8; ++i)
        ratioButtons[(size_t) i].setBounds (30 + i * 58, 214, 54, 52);

    int x = 520;
    for (auto* b : { &dist2Button, &dist3Button, &detHpButton, &optoButton, &linkButton })
    {
        b->setBounds (x, 214, 48, 52);
        x += 52;
    }
}
