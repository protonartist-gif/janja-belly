#include "PluginEditor.h"

// ------------------------------------------------------------ look and feel

JBLook::JBLook()
{
    using namespace juce;
    setColour (Slider::rotarySliderFillColourId, jbcol::sky);
    setColour (Slider::rotarySliderOutlineColourId, jbcol::slate0);
    setColour (Slider::thumbColourId, jbcol::ice);
    setColour (Slider::textBoxTextColourId, jbcol::slate3);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId, jbcol::slate2);
    setColour (ComboBox::backgroundColourId, jbcol::navy3);
    setColour (ComboBox::textColourId, jbcol::ice);
    setColour (ComboBox::outlineColourId, jbcol::slate1);
    setColour (ComboBox::arrowColourId, jbcol::amber);
    setColour (PopupMenu::backgroundColourId, jbcol::navy2);
    setColour (PopupMenu::textColourId, jbcol::slate3);
    setColour (PopupMenu::highlightedBackgroundColourId, jbcol::blue);
    setColour (PopupMenu::highlightedTextColourId, jbcol::ice);
    setColour (ToggleButton::textColourId, jbcol::slate3);
    setColour (ToggleButton::tickColourId, jbcol::amber);
    setColour (ToggleButton::tickDisabledColourId, jbcol::slate1);
}

void JBLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f);
    const bool round = b.getProperties().contains ("round");
    auto c = round ? jbcol::amber : jbcol::teal;
    if (down) c = c.darker (0.25f); else if (over) c = c.brighter (0.12f);

    if (round)
    {
        for (int i = 3; i >= 1; --i)   // soft coral/amber glow
        {
            g.setColour (jbcol::coral.withAlpha (0.07f * (float) (4 - i)));
            g.drawEllipse (r.expanded ((float) i * 3.0f), 2.0f);
        }
        g.setGradientFill (juce::ColourGradient (c.brighter (0.35f), r.getCentreX(), r.getY(),
                                                 jbcol::coral, r.getCentreX(), r.getBottom(), false));
        g.fillEllipse (r);
        g.setColour (jbcol::navy0.withAlpha (0.35f));
        g.drawEllipse (r.reduced (6.0f), 1.5f);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (c.brighter (0.2f), r.getX(), r.getY(),
                                                 c.darker (0.3f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
}

void JBLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool round = b.getProperties().contains ("round");
    g.setColour (jbcol::navy0);
    g.setFont (juce::Font (juce::FontOptions (round ? 20.0f : 14.0f, juce::Font::bold)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ------------------------------------------------------------ phrase preview

void PhraseView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (jbcol::navy2, 0, 0, jbcol::navy1, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (jbcol::slate0);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    g.setColour (jbcol::slate2);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("SEED " + juce::String::toHexString (proc.getSeed()).toUpperCase().substring (0, 6),
                getLocalBounds().reduced (10, 4), juce::Justification::topRight);

    const auto notes = proc.getPhraseCopy();
    if (notes.empty()) return;

    const int bars = proc.getPhraseBars();
    const double total = bars * 4.0;
    int lo = 127, hi = 0;
    for (auto& n : notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
    const float range = (float) std::max (12, hi - lo + 1);
    const auto inner = r.reduced (8.0f, 14.0f);

    for (int b = 0; b <= bars * 4; ++b)   // beat + bar grid
    {
        const float x = inner.getX() + inner.getWidth() * (float) (b / total);
        g.setColour ((b % 4 == 0 ? jbcol::slate1 : jbcol::slate0).withAlpha (b % 4 == 0 ? 0.9f : 0.5f));
        g.drawVerticalLine ((int) x, r.getY() + 6, r.getBottom() - 6);
    }

    for (auto& n : notes)
    {
        const float x = inner.getX() + inner.getWidth() * (float) (n.start / total);
        const float w = std::max (2.5f, inner.getWidth() * (float) (n.length / total) - 1.0f);
        const float y = inner.getBottom() - inner.getHeight() * ((float) (n.pitch - lo) + 1.0f) / range;
        const float h = inner.getHeight() / range * 0.85f;
        auto c = jbcol::blue.interpolatedWith (jbcol::ice, n.velocity);
        if (n.velocity > 0.8f) c = c.interpolatedWith (jbcol::amber, 0.6f);   // accents glow amber
        g.setColour (c.withAlpha (0.55f + 0.45f * n.velocity));
        g.fillRoundedRectangle (x, y, w, std::max (2.0f, h), 1.5f);
    }
}

// ------------------------------------------------------------ MIDI drag zone

void DragZone::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (jbcol::navy2);
    g.fillRoundedRectangle (r, 8.0f);

    juce::Path outline, dashed;
    outline.addRoundedRectangle (r, 8.0f);
    const float dashes[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (jbcol::sky);
    g.fillPath (dashed);

    g.setColour (jbcol::slate3);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("DRAG MIDI INTO YOUR DAW", getLocalBounds(), juce::Justification::centred);
}

void DragZone::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 8) return;
    dragging = true;

    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                 .getChildFile ("JanjaBelly_" + juce::String::toHexString (proc.getSeed()) + ".mid");
    if (proc.writeMidiFile (f))
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                    [this] { dragging = false; });
    else
        dragging = false;
}

// ------------------------------------------------------------ editor

JanjaBellyEditor::JanjaBellyEditor (JanjaBellyProcessor& p)
    : AudioProcessorEditor (&p), proc (p), phraseView (p), dragZone (p)
{
    setLookAndFeel (&look);

    initKnob (reverb,   "reverb",   "REVERB",   jbcol::sky);
    initKnob (chorus,   "chorus",   "CHORUS",   jbcol::sky);
    initKnob (ensemble, "ensemble", "ENSEMBLE", jbcol::sky);
    initKnob (swing,    "swing",    "SWING",    jbcol::teal);
    initKnob (jitter,   "jitter",   "JITTER",   jbcol::teal);
    initKnob (tone,     "bright",   "TONE",     jbcol::teal);

    initCombo (barsBox,  barsAtt,  "bars");
    initCombo (moodBox,  moodAtt,  "mood");
    initCombo (keyBox,   keyAtt,   "key");
    initCombo (soundBox, soundAtt, "sound");

    // swing / jitter / bars / mood / key re-render the current phrase live (same seed)
    swing.slider.onDragEnd  = [this] { regenerateSameSeed(); };
    jitter.slider.onDragEnd = [this] { regenerateSameSeed(); };
    barsBox.onChange = [this] { regenerateSameSeed(); };
    moodBox.onChange = [this] { regenerateSameSeed(); };
    keyBox.onChange  = [this] { regenerateSameSeed(); };

    halfAtt = std::make_unique<ButtonAtt> (proc.apvts, "halftime", halfTime);
    lockAtt = std::make_unique<ButtonAtt> (proc.apvts, "locksound", lockSound);
    addAndMakeVisible (halfTime);
    addAndMakeVisible (lockSound);

    addAndMakeVisible (preview);
    preview.setToggleState (proc.previewing.load(), juce::dontSendNotification);
    preview.onClick = [this] { proc.previewing = preview.getToggleState(); };

    generateBtn.getProperties().set ("round", true);
    addAndMakeVisible (generateBtn);
    generateBtn.onClick = [this] { proc.generatePhrase(); phraseView.repaint(); };

    addAndMakeVisible (spawnBtn);
    spawnBtn.onClick = [this] { proc.spawnSound(); };

    addAndMakeVisible (phraseView);
    addAndMakeVisible (dragZone);

    setSize (700, 470);
}

JanjaBellyEditor::~JanjaBellyEditor()
{
    setLookAndFeel (nullptr);
}

void JanjaBellyEditor::regenerateSameSeed()
{
    proc.generateFromSeed (proc.getSeed());
    phraseView.repaint();
}

void JanjaBellyEditor::initKnob (Knob& k, const juce::String& id, const juce::String& text, juce::Colour fill)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, fill);
    addAndMakeVisible (k.slider);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void JanjaBellyEditor::initCombo (juce::ComboBox& cb, std::unique_ptr<ComboAtt>& att, const juce::String& id)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        cb.addItemList (p->choices, 1);
    addAndMakeVisible (cb);
    att = std::make_unique<ComboAtt> (proc.apvts, id, cb);
}

void JanjaBellyEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (jbcol::navy1, 0, 0, jbcol::navy0, 0, (float) getHeight(), false));
    g.fillAll();

    // soft blue glow behind the centre
    g.setGradientFill (juce::ColourGradient (jbcol::blue.withAlpha (0.18f), (float) getWidth() * 0.5f, 265.0f,
                                             juce::Colours::transparentBlack, (float) getWidth() * 0.5f, 420.0f, true));
    g.fillAll();

    // title
    const auto font = juce::Font (juce::FontOptions (28.0f, juce::Font::bold));
    g.setFont (font);
    const int cx = getWidth() / 2;
    g.setColour (jbcol::ice);
    g.drawText ("JANJA", cx - 160, 6, 154, 34, juce::Justification::centredRight);
    g.setColour (jbcol::amber);
    g.drawText ("BELLY", cx + 6, 6, 154, 34, juce::Justification::centredLeft);
    g.setColour (jbcol::slate2);
    g.setFont (11.0f);
    g.drawText ("MELODIC GARAGE & JUNGLE KEYS", 0, 36, getWidth(), 14, juce::Justification::centred);

    // panels
    auto panel = [&] (juce::Rectangle<float> r, const juce::String& title, juce::Colour tint)
    {
        g.setColour (jbcol::navy2.withAlpha (0.85f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (jbcol::slate0);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (tint);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (title, r.withHeight (16.0f).translated (0, 4), juce::Justification::centred);
    };
    panel ({ 20.0f, 196.0f, 268.0f, 138.0f }, "FX", jbcol::sky);
    panel ({ 412.0f, 196.0f, 268.0f, 138.0f }, "FEEL", jbcol::teal);
}

void JanjaBellyEditor::resized()
{
    barsBox.setBounds  (30, 58, 80, 26);
    moodBox.setBounds  (118, 58, 110, 26);
    keyBox.setBounds   (236, 58, 84, 26);
    soundBox.setBounds (328, 58, 124, 26);
    halfTime.setBounds (466, 58, 96, 26);
    lockSound.setBounds (566, 58, 104, 26);

    phraseView.setBounds (30, 96, 640, 92);

    generateBtn.setBounds (290, 205, 120, 120);
    spawnBtn.setBounds (300, 340, 100, 36);

    auto place = [] (Knob& k, int x, int y)
    {
        k.label.setBounds (x, y, 80, 14);
        k.slider.setBounds (x, y + 14, 80, 100);
    };
    place (reverb,   30, 218);
    place (chorus,   115, 218);
    place (ensemble, 200, 218);
    place (swing,    420, 218);
    place (jitter,   505, 218);
    place (tone,     590, 218);

    preview.setBounds (30, 410, 100, 28);
    dragZone.setBounds (240, 404, 220, 40);
}
