#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

// ============================================================================
//  Interface dessinée entièrement en code, d'après la capture fournie.
//  Tout est dessiné dans un "espace de conception" de 1130 x 406 unités, puis mis
//  à l'échelle selon la taille de la fenêtre.
// ============================================================================

namespace
{
constexpr float kW = 1130.0f;
constexpr float kH = 406.0f;

const juce::Colour kLine   { 0xffe9e9e9 };
const juce::Colour kLabel  { 0xfff2f2f2 };
const juce::Colour kGreen  { 0xff3ddc4a };
const juce::Colour kYellow { 0xffffd52e };
const juce::Colour kAmber  { 0xffffa21f };
const juce::Colour kOrange { 0xffff8a1f };
const juce::Colour kRed    { 0xffe5392b };
const juce::Colour kCyan   { 0xff2ec8ff };

// Barre de réduction de gain : de gauche (26 dB) à droite (1 dB).
constexpr float kGrX[16]    = { 189.0f, 223.0f, 257.0f, 291.0f, 324.0f, 358.0f, 391.0f, 424.0f,
                                458.0f, 492.0f, 526.0f, 559.0f, 593.0f, 627.0f, 660.0f, 694.0f };
constexpr float kGrStep[16] = { 26.0f, 23.0f, 20.0f, 17.0f, 14.0f, 12.0f, 10.0f, 9.0f,
                                8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f };

// Voyants de ratio.
constexpr float kRatioX[8] = { 241.0f, 292.0f, 343.0f, 394.0f, 445.0f, 496.0f, 548.0f, 600.0f };

void label (juce::Graphics& g, const juce::String& text, float cx, float cy, float height,
            bool bold = true, juce::Colour colour = kLabel)
{
    g.setColour (colour);
    g.setFont (bold ? juce::FontOptions (height).withStyle ("Bold") : juce::FontOptions (height));
    g.drawText (text, juce::Rectangle<float> (cx - 120.0f, cy - height, 240.0f, height * 2.0f),
                juce::Justification::centred, false);
}

void drawLed (juce::Graphics& g, float cx, float cy, float r, juce::Colour on, bool lit)
{
    const float bezel = r + 2.2f;
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillEllipse (cx - bezel, cy - bezel, bezel * 2.0f, bezel * 2.0f);

    if (lit)
    {
        const float glowR = r * 2.8f;
        juce::ColourGradient glow (on.withAlpha (0.55f), cx, cy, on.withAlpha (0.0f), cx + glowR, cy, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);
    }

    juce::ColourGradient body (lit ? on.brighter (0.9f) : on.darker (1.4f), cx - r * 0.3f, cy - r * 0.35f,
                               lit ? on.darker (0.15f)  : on.darker (2.6f), cx + r * 1.1f, cy + r * 1.1f, true);
    g.setGradientFill (body);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
}

// Icône "passe-haut" : une pente qui monte puis un plateau.
void drawHpIcon (juce::Graphics& g, float x, float y)
{
    juce::Path p;
    p.startNewSubPath (x, y + 20.0f);
    p.lineTo (x + 9.0f, y);
    p.lineTo (x + 20.0f, y);
    g.setColour (kLabel);
    g.strokePath (p, juce::PathStrokeType (2.2f));
}

// Icône "cloche" : plateau, bosse, plateau.
void drawBellIcon (juce::Graphics& g, float x, float y)
{
    juce::Path p;
    p.startNewSubPath (x, y + 19.0f);
    p.lineTo (x + 7.0f, y + 19.0f);
    p.cubicTo (x + 12.0f, y + 19.0f, x + 13.0f, y + 2.0f, x + 18.0f, y + 2.0f);
    p.cubicTo (x + 23.0f, y + 2.0f, x + 24.0f, y + 19.0f, x + 29.0f, y + 19.0f);
    p.lineTo (x + 36.0f, y + 19.0f);
    g.setColour (kLabel);
    g.strokePath (p, juce::PathStrokeType (2.2f));
}

// Icône "sinusoïde écrêtée" (niveau de sortie / distorsion).
void drawClippedSineIcon (juce::Graphics& g, float x, float y)
{
    juce::Path p;
    for (int i = 0; i <= 48; ++i)
    {
        const float t  = (float) i / 48.0f;
        const float yy = juce::jlimit (-11.0f, 11.0f, -17.0f * std::sin (t * 6.2831853f));
        if (i == 0) p.startNewSubPath (x, y + yy);
        else        p.lineTo (x + 56.0f * t, y + yy);
    }
    g.setColour (kLabel);
    g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Potard à échelle rotative : la graduation tourne, la valeur lue est celle en haut.
void drawDial (juce::Graphics& g, float cx, float cy, float R, float value)
{
    const float u = R / 88.0f;

    // ombre portée
    {
        const float sx = cx + 5.0f * u, sy = cy + 6.0f * u;
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.7f), sx, sy,
                                     juce::Colours::black.withAlpha (0.0f), sx + 82.0f * u, sy, true);
        shadow.addColour (0.9, juce::Colours::black.withAlpha (0.45f));
        g.setGradientFill (shadow);
        g.fillEllipse (sx - 82.0f * u, sy - 82.0f * u, 164.0f * u, 164.0f * u);
    }

    // anneau argenté
    {
        juce::Path ring;
        ring.addEllipse (cx - 80.0f * u, cy - 80.0f * u, 160.0f * u, 160.0f * u);
        ring.addEllipse (cx - 46.0f * u, cy - 46.0f * u, 92.0f * u, 92.0f * u);
        ring.setUsingNonZeroWinding (false);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdcdcdc), cx - 60.0f * u, cy - 70.0f * u,
                                                 juce::Colour (0xff8a8a8a), cx + 60.0f * u, cy + 70.0f * u, false));
        g.fillPath (ring);

        g.setColour (juce::Colour (0xff2e2e2e));
        g.drawEllipse (cx - 80.0f * u, cy - 80.0f * u, 160.0f * u, 160.0f * u, 1.6f * u);
    }

    // graduations et chiffres (27 degrés par unité, la valeur courante est en haut)
    {
        const float stepRad = juce::degreesToRadians (27.0f);

        for (int k = 0; k <= 20; ++k)
        {
            const float n     = (float) k * 0.5f;
            const bool  major = (k % 2 == 0);

            juce::Graphics::ScopedSaveState state (g);
            g.addTransform (juce::AffineTransform::rotation ((value - n) * stepRad, cx, cy));

            g.setColour (juce::Colour (0xff1a1a1a));
            g.drawLine (cx, cy - 78.0f * u, cx, cy - (major ? 70.0f : 74.0f) * u, (major ? 1.9f : 1.2f) * u);

            if (major)
            {
                g.setFont (juce::FontOptions (15.0f * u).withStyle ("Bold"));
                g.drawText (juce::String ((int) n),
                            juce::Rectangle<float> (cx - 12.0f * u, cy - 69.0f * u, 24.0f * u, 18.0f * u),
                            juce::Justification::centred, false);
            }
        }
    }

    // cache central : fond sombre, anneau, disque argenté
    g.setColour (juce::Colour (0xff101010));
    g.fillEllipse (cx - 46.0f * u, cy - 46.0f * u, 92.0f * u, 92.0f * u);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff555555), cx - 30.0f * u, cy - 30.0f * u,
                                             juce::Colour (0xff0c0c0c), cx + 30.0f * u, cy + 30.0f * u, false));
    g.fillEllipse (cx - 42.0f * u, cy - 42.0f * u, 84.0f * u, 84.0f * u);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f6f6), cx - 24.0f * u, cy - 26.0f * u,
                                             juce::Colour (0xff858585), cx + 26.0f * u, cy + 28.0f * u, false));
    g.fillEllipse (cx - 34.0f * u, cy - 34.0f * u, 68.0f * u, 68.0f * u);

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawEllipse (cx - 33.0f * u, cy - 33.0f * u, 66.0f * u, 66.0f * u, 1.2f * u);

    // repère fixe en haut
    juce::Path mark;
    mark.addTriangle (cx, cy - 80.0f * u, cx - 5.0f * u, cy - 87.0f * u, cx + 5.0f * u, cy - 87.0f * u);
    g.setColour (juce::Colour (0xffffc46b));
    g.fillPath (mark);
}

// Petit potard noir (Mix) avec trait blanc : Dry à gauche, Comp à droite.
void drawMixKnob (juce::Graphics& g, float cx, float cy, float R, float pos)
{
    const float u = R / 40.0f;

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (cx - 28.0f * u + 4.0f * u, cy - 28.0f * u + 5.0f * u, 56.0f * u, 56.0f * u);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4d4d4d), cx - 14.0f * u, cy - 18.0f * u,
                                             juce::Colour (0xff090909), cx + 18.0f * u, cy + 22.0f * u, false));
    g.fillEllipse (cx - 28.0f * u, cy - 28.0f * u, 56.0f * u, 56.0f * u);

    g.setColour (juce::Colour (0xff777777).withAlpha (0.5f));
    g.drawEllipse (cx - 28.0f * u, cy - 28.0f * u, 56.0f * u, 56.0f * u, 1.2f * u);

    const float angle = juce::degreesToRadians (-135.0f + pos * 270.0f);
    const juce::Point<float> centre (cx, cy);
    const auto p1 = centre.getPointOnCircumference (5.0f * u, angle);
    const auto p2 = centre.getPointOnCircumference (25.0f * u, angle);

    g.setColour (juce::Colour (0xfff2f2f2));
    g.drawLine (p1.x, p1.y, p2.x, p2.y, 2.6f * u);
}
} // namespace

//==============================================================================
// Look and feel
//==============================================================================
void StrainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float pos, float, float, juce::Slider& slider)
{
    const juce::Rectangle<float> b ((float) x, (float) y, (float) width, (float) height);
    const float R = std::min (b.getWidth(), b.getHeight()) * 0.5f;

    if (slider.getName() == "mix")
        drawMixKnob (g, b.getCentreX(), b.getCentreY(), R, pos);
    else
        drawDial (g, b.getCentreX(), b.getCentreY(), R, pos * 10.0f);
}

//==============================================================================
// Bouton poussoir
//==============================================================================
void PushButton::paint (juce::Graphics& g)
{
    if (! metal)
        return;

    const auto r = getLocalBounds().toFloat().reduced (2.0f).translated (0.0f, down ? 1.5f : 0.0f);
    const float radius = std::min (9.0f, r.getHeight() * 0.3f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (1.5f, 2.5f), radius);

    g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xff9c9c9c) : juce::Colour (0xffd6d6d6), 0.0f, r.getY(),
                                             down ? juce::Colour (0xff5a5a5a) : juce::Colour (0xff767676), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    g.setColour (juce::Colour (0xff262626));
    g.drawRoundedRectangle (r, radius, 1.6f);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (r.getX() + radius, r.getY() + 3.0f, r.getRight() - radius, r.getY() + 3.0f, 1.4f);
}

void PushButton::mouseDown (const juce::MouseEvent&)
{
    down = true;
    repaint();
}

void PushButton::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDown = down;
    down = false;
    repaint();

    if (wasDown && contains (e.getPosition()) && onClick)
        onClick();
}

//==============================================================================
// Éditeur
//==============================================================================
StrainAudioProcessorEditor::StrainAudioProcessorEditor (StrainAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&laf);

    auto setupDial = [this] (juce::Slider& s, double defaultValue, const juce::String& name)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity (260);
        s.setSliderSnapsToMousePosition (false);
        s.setDoubleClickReturnValue (true, defaultValue);
        addAndMakeVisible (s);
    };

    // Zones cliquables invisibles (voyants de ratio, Opto, Link)
    for (auto& z : ratioZones)
    {
        z.setMetal (false);
        addAndMakeVisible (z);
    }
    for (int i = 0; i < 8; ++i)
        ratioZones[(size_t) i].onClick = [this, i] { setChoiceValue (ParamIDs::ratio, i); };

    optoZone.setMetal (false);
    optoZone.onClick = [this] { setBoolValue (ParamIDs::opto, ! getBoolValue (ParamIDs::opto)); };
    addAndMakeVisible (optoZone);

    linkZone.setMetal (false);
    linkZone.onClick = [this] { setBoolValue (ParamIDs::link, ! getBoolValue (ParamIDs::link)); };
    addAndMakeVisible (linkZone);

    // Boutons poussoirs métalliques
    bypassButton.onClick   = [this] { setBoolValue (ParamIDs::bypass, ! getBoolValue (ParamIDs::bypass)); };
    ratioButton.onClick    = [this] { cycleChoice (ParamIDs::ratio, 8); };
    detectorButton.onClick = [this] { cycleChoice (ParamIDs::detector, 4); };
    audioButton.onClick    = [this] { cycleChoice (ParamIDs::audio, 4); };

    addAndMakeVisible (bypassButton);
    addAndMakeVisible (ratioButton);
    addAndMakeVisible (detectorButton);
    addAndMakeVisible (audioButton);

    // Potards
    setupDial (inputKnob,   2.0, "dial");
    setupDial (attackKnob,  5.0, "dial");
    setupDial (releaseKnob, 5.0, "dial");
    setupDial (outputKnob,  5.0, "dial");
    setupDial (mixKnob,     1.0, "mix");

    auto& apvts = audioProcessor.apvts;
    inputAtt   = std::make_unique<SliderAttachment> (apvts, ParamIDs::input,   inputKnob);
    attackAtt  = std::make_unique<SliderAttachment> (apvts, ParamIDs::attack,  attackKnob);
    releaseAtt = std::make_unique<SliderAttachment> (apvts, ParamIDs::release, releaseKnob);
    outputAtt  = std::make_unique<SliderAttachment> (apvts, ParamIDs::output,  outputKnob);
    mixAtt     = std::make_unique<SliderAttachment> (apvts, ParamIDs::mix,     mixKnob);

    setResizable (true, true);
    setResizeLimits (565, 203, 1695, 609);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (1000, 359);

    startTimerHz (30);
}

StrainAudioProcessorEditor::~StrainAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int StrainAudioProcessorEditor::getChoiceValue (const char* id) const
{
    return (int) std::lround (audioProcessor.apvts.getRawParameterValue (id)->load());
}

bool StrainAudioProcessorEditor::getBoolValue (const char* id) const
{
    return audioProcessor.apvts.getRawParameterValue (id)->load() > 0.5f;
}

void StrainAudioProcessorEditor::setChoiceValue (const char* id, int index)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (id)))
    {
        choice->beginChangeGesture();
        *choice = index;
        choice->endChangeGesture();
    }
}

void StrainAudioProcessorEditor::setBoolValue (const char* id, bool value)
{
    if (auto* flag = dynamic_cast<juce::AudioParameterBool*> (audioProcessor.apvts.getParameter (id)))
    {
        flag->beginChangeGesture();
        *flag = value;
        flag->endChangeGesture();
    }
}

void StrainAudioProcessorEditor::cycleChoice (const char* id, int numChoices)
{
    setChoiceValue (id, (getChoiceValue (id) + 1) % numChoices);
}

void StrainAudioProcessorEditor::timerCallback()
{
    const float gr = audioProcessor.takeGainReductionDb();
    const float pk = audioProcessor.takeOutputPeak();

    grDisplay = std::max (getBoolValue (ParamIDs::bypass) ? 0.0f : gr, grDisplay * 0.86f);
    peakHold  = std::max (pk, peakHold * 0.94f);

    const float s = (float) getWidth() / kW;
    repaint (0, 0, getWidth(), (int) std::ceil (192.0f * s));
}

void StrainAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat());

    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kW));
    drawDynamicParts (g);
}

void StrainAudioProcessorEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const float s = (float) getWidth() / kW;

    auto R = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> ((int) std::lround (x * s), (int) std::lround (y * s),
                                     (int) std::lround (w * s), (int) std::lround (h * s));
    };

    const float half = 88.0f, cy = 312.0f;
    inputKnob.setBounds   (R (182.0f - half, cy - half, half * 2.0f, half * 2.0f));
    attackKnob.setBounds  (R (411.0f - half, cy - half, half * 2.0f, half * 2.0f));
    releaseKnob.setBounds (R (624.0f - half, cy - half, half * 2.0f, half * 2.0f));
    outputKnob.setBounds  (R (839.0f - half, cy - half, half * 2.0f, half * 2.0f));
    mixKnob.setBounds     (R (977.0f, 269.0f, 80.0f, 80.0f));

    bypassButton.setBounds   (R (342.0f, 79.0f, 72.0f, 36.0f));
    ratioButton.setBounds    (R (639.0f, 119.0f, 70.0f, 34.0f));
    detectorButton.setBounds (R (746.0f, 119.0f, 72.0f, 35.0f));
    audioButton.setBounds    (R (851.0f, 119.0f, 73.0f, 35.0f));

    for (int i = 0; i < 8; ++i)
        ratioZones[(size_t) i].setBounds (R (kRatioX[i] - 24.0f, 126.0f, 48.0f, 32.0f));

    optoZone.setBounds (R (470.0f, 160.0f, 52.0f, 20.0f));
    linkZone.setBounds (R (756.0f, 83.0f, 68.0f, 22.0f));

    // Façade fixe mise en cache (en double résolution pour rester nette sur écran Retina)
    const int iw = std::max (1, (int) std::lround ((float) getWidth()  * 2.0f));
    const int ih = std::max (1, (int) std::lround ((float) getHeight() * 2.0f));
    background = juce::Image (juce::Image::ARGB, iw, ih, true);

    juce::Graphics ig (background);
    ig.addTransform (juce::AffineTransform::scale (2.0f * s));
    drawStaticFace (ig);
}

//==============================================================================
// Éléments fixes de la façade
//==============================================================================
void StrainAudioProcessorEditor::drawStaticFace (juce::Graphics& g)
{
    // Fond : noir légèrement chaud, avec un grain discret (effet vintage)
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a2b), 0.0f, 0.0f,
                                             juce::Colour (0xff171718), 0.0f, kH, false));
    g.fillRect (0.0f, 0.0f, kW, kH);

    juce::Random rng (7);
    for (int i = 0; i < 2600; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (rng.nextFloat() * 0.035f));
        g.fillRect (rng.nextFloat() * kW, rng.nextFloat() * kH, 1.2f, 1.2f);
    }

    auto strokeFrame = [&g] (const juce::Path& p)
    {
        g.setColour (kLine);
        g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    // ---- Cadre du haut ------------------------------------------------------
    {
        const float x0 = 10.0f, y0 = 15.0f, x1 = 1121.0f, y1 = 182.0f, r = 34.0f;
        juce::Path p;

        p.startNewSubPath (164.0f, y0);
        p.lineTo (x0 + r, y0);
        p.quadraticTo (x0, y0, x0, y0 + r);
        p.lineTo (x0, y1 - r);
        p.quadraticTo (x0, y1, x0 + r, y1);
        p.lineTo (372.0f, y1);

        p.startNewSubPath (462.0f, y1);
        p.lineTo (x1 - r, y1);
        p.quadraticTo (x1, y1, x1, y1 - r);
        p.lineTo (x1, y0 + r);
        p.quadraticTo (x1, y0, x1 - r, y0);
        p.lineTo (1057.0f, y0);

        p.startNewSubPath (719.0f, y0);
        p.lineTo (959.0f, y0);

        // crochet autour des voyants de ratio
        p.startNewSubPath (232.0f, 140.0f);
        p.quadraticTo (209.0f, 140.0f, 209.0f, 164.0f);
        p.lineTo (209.0f, y1);

        p.startNewSubPath (714.0f, 140.0f);
        p.quadraticTo (731.0f, 140.0f, 731.0f, 164.0f);
        p.lineTo (731.0f, y1);

        strokeFrame (p);
    }

    // ---- Cadre du bas -------------------------------------------------------
    {
        const float x0 = 10.0f, y0 = 219.0f, x1 = 1121.0f, y1 = 387.0f, r = 34.0f;
        juce::Path p;

        p.startNewSubPath (91.0f, y0);
        p.lineTo (x0 + r, y0);
        p.quadraticTo (x0, y0, x0, y0 + r);
        p.lineTo (x0, y1 - r);
        p.quadraticTo (x0, y1, x0 + r, y1);
        p.lineTo (x1 - r, y1);
        p.quadraticTo (x1, y1, x1, y1 - r);
        p.lineTo (x1, y0 + r);
        p.quadraticTo (x1, y0, x1 - r, y0);
        p.lineTo (1013.0f, y0);

        const float segments[4][2] = { { 165.0f, 288.0f }, { 383.0f, 496.0f }, { 600.0f, 726.0f }, { 820.0f, 964.0f } };
        for (const auto& seg : segments)
        {
            p.startNewSubPath (seg[0], y0);
            p.lineTo (seg[1], y0);
        }

        const float ticks[4] = { 195.0f, 411.0f, 626.0f, 840.0f };
        for (float tx : ticks)
        {
            p.startNewSubPath (tx, y0 - 12.0f);
            p.lineTo (tx, y0 + 13.0f);
        }

        p.startNewSubPath (1016.0f, y0 - 12.0f);
        p.lineTo (1016.0f, y0 + 58.0f);

        strokeFrame (p);
    }

    // ---- Titres des potards -------------------------------------------------
    label (g, "INPUT",   128.0f, 219.0f, 21.0f);
    label (g, "ATTACK",  335.0f, 219.0f, 21.0f);
    label (g, "RELEASE", 548.0f, 219.0f, 21.0f);
    label (g, "OUTPUT",  773.0f, 219.0f, 21.0f);
    label (g, "MIX",     988.0f, 219.0f, 21.0f);
    label (g, "POWER",  1008.0f,  15.0f, 20.0f);

    label (g, "Dry",   991.0f, 351.0f, 14.0f);
    label (g, "Comp", 1046.0f, 351.0f, 14.0f);
    label (g, "Opto(10)", 507.0f, 372.0f, 13.0f);
    label (g, "Opto(0)",  718.0f, 372.0f, 13.0f);

    // Petite vis "HR"
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8a8a), 60.0f, 306.0f,
                                                 juce::Colour (0xff2a2a2a), 72.0f, 320.0f, false));
        g.fillEllipse (55.0f, 302.0f, 20.0f, 20.0f);
        g.setColour (juce::Colour (0xff111111));
        g.drawLine (59.0f, 312.0f, 71.0f, 312.0f, 2.0f);
        label (g, "HR", 65.0f, 285.0f, 15.0f);
    }

    // ---- Nom du plugin à la place du logo ------------------------------------
    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::shear (-0.2f, 0.0f).translated (0.2f * 90.0f, 0.0f));
        g.setColour (kLabel);
        g.setFont (juce::FontOptions (40.0f).withStyle ("Bold"));
        g.drawFittedText ("Meghir Stress", 118, 62, 220, 56, juce::Justification::centredLeft, 1, 0.6f);
    }

    // ---- Barre de réduction de gain : titres, chiffres ------------------------
    for (int i = 0; i < 16; ++i)
        label (g, juce::String ((int) kGrStep[i]), kGrX[i], 17.0f, 15.0f);

    label (g, "GAIN REDUCTION", 447.0f, 54.0f, 18.0f);

    // ---- Bypass ----------------------------------------------------------------
    label (g, "BY",   450.0f, 95.0f, 17.0f);
    label (g, "PASS", 510.0f, 95.0f, 17.0f);

    // ---- Niveau de sortie -----------------------------------------------------
    drawClippedSineIcon (g, 549.0f, 80.0f);
    label (g, "REDLINE", 668.0f, 65.0f, 14.0f);
    label (g, "1% THD",  668.0f, 94.0f, 14.0f);

    // ---- Détecteur --------------------------------------------------------------
    drawHpIcon (g, 744.0f, 27.0f);
    label (g, "HP", 780.0f, 37.0f, 16.0f);
    drawBellIcon (g, 764.0f, 53.0f);
    label (g, "Link", 772.0f, 93.0f, 17.0f);
    label (g, "DETECTOR", 781.0f, 168.0f, 20.0f);

    // ---- Audio --------------------------------------------------------------------
    drawHpIcon (g, 851.0f, 27.0f);
    label (g, "HP",     887.0f, 37.0f, 16.0f);
    label (g, "Dist 2", 879.0f, 65.0f, 17.0f);
    label (g, "Dist 3", 879.0f, 93.0f, 17.0f);
    label (g, "AUDIO",  888.0f, 168.0f, 20.0f);

    // ---- Ratio ---------------------------------------------------------------------
    const char* ratioNames[8] = { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE" };
    for (int i = 0; i < 8; ++i)
        label (g, ratioNames[i], kRatioX[i], 137.0f, 15.0f);

    label (g, "RATIO", 672.0f, 165.0f, 20.0f);
    label (g, "RATIO", 417.0f, 181.0f, 24.0f);

    // ---- Interrupteur "POWER" (décoratif) --------------------------------------------
    {
        const juce::Rectangle<float> sw (974.0f, 52.0f, 70.0f, 90.0f);
        g.setColour (juce::Colour (0xff0c0c0c));
        g.fillRoundedRectangle (sw, 9.0f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff434343), 0.0f, 56.0f,
                                                 juce::Colour (0xff1c1c1c), 0.0f, 98.0f, false));
        g.fillRoundedRectangle (juce::Rectangle<float> (979.0f, 57.0f, 60.0f, 42.0f), 6.0f);

        g.setColour (juce::Colour (0xff151515));
        g.fillRoundedRectangle (juce::Rectangle<float> (979.0f, 99.0f, 60.0f, 38.0f), 6.0f);

        g.setColour (juce::Colour (0xff6a6a6a));
        g.drawRoundedRectangle (sw, 9.0f, 1.6f);
    }
}

//==============================================================================
// Éléments animés : voyants
//==============================================================================
void StrainAudioProcessorEditor::drawDynamicParts (juce::Graphics& g)
{
    // Barre de réduction de gain
    for (int i = 0; i < 16; ++i)
    {
        const float step = kGrStep[i];

        const juce::Colour colour = step >= 12.0f ? kRed
                                  : step >= 7.0f  ? kYellow
                                  : step >= 6.0f  ? juce::Colour (0xff9be23a)
                                                  : kGreen;

        drawLed (g, kGrX[i], 37.0f, 7.5f, colour, grDisplay >= step - 0.5f);
    }

    // Voyants de ratio
    const int ratio = getChoiceValue (ParamIDs::ratio);
    const juce::Colour ratioColours[8] = { kRed, kGreen, kGreen, kGreen, kYellow, kOrange, kRed, kCyan };
    for (int i = 0; i < 8; ++i)
        drawLed (g, kRatioX[i], 156.0f, 6.5f, ratioColours[i], ratio == i);

    const bool opto = getBoolValue (ParamIDs::opto);
    label (g, "Opto", 496.0f, 169.0f, 14.0f, opto, opto ? kOrange : kLabel.withAlpha (0.8f));

    // Détecteur
    const int det = getChoiceValue (ParamIDs::detector);
    drawLed (g, 811.0f, 37.0f, 6.5f, kGreen,  det == 1 || det == 3);
    drawLed (g, 811.0f, 65.0f, 6.5f, kYellow, det == 2 || det == 3);
    drawLed (g, 811.0f, 94.0f, 6.5f, kRed,    getBoolValue (ParamIDs::link));

    // Audio
    const int aud = getChoiceValue (ParamIDs::audio);
    drawLed (g, 917.0f, 38.0f, 6.5f, kGreen, aud == 1);
    drawLed (g, 917.0f, 65.0f, 6.5f, kAmber, aud == 2);
    drawLed (g, 917.0f, 94.0f, 6.5f, kRed,   aud == 3);

    // Bypass
    drawLed (g, 477.0f, 96.0f, 6.5f, kRed, getBoolValue (ParamIDs::bypass));

    // Niveau de sortie
    drawLed (g, 626.0f, 65.0f, 6.5f, kRed,    peakHold >= 0.99f);
    drawLed (g, 626.0f, 94.0f, 6.5f, kYellow, peakHold >= 0.70f);
}
