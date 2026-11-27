#pragma once
#include "PluginProcessor.h"

// Palette: navy > blue > slate greys, with amber / teal / coral as offset accents.
namespace jbcol
{
const juce::Colour navy0   { 0xff060c1a };   // deepest background
const juce::Colour navy1   { 0xff0b1730 };   // background
const juce::Colour navy2   { 0xff122545 };   // panels
const juce::Colour navy3   { 0xff1b3560 };   // raised controls
const juce::Colour slate0  { 0xff2a3750 };   // outlines
const juce::Colour slate1  { 0xff4a5b78 };
const juce::Colour slate2  { 0xff8394b0 };   // secondary text
const juce::Colour slate3  { 0xffb4c2d9 };   // text
const juce::Colour blue    { 0xff3d7eea };
const juce::Colour sky     { 0xff7db8ff };
const juce::Colour ice     { 0xffd6e6ff };
const juce::Colour amber   { 0xffffb347 };   // offset accent (GENERATE)
const juce::Colour coral   { 0xffff6f61 };
const juce::Colour teal    { 0xff35d0c0 };   // offset accent (SPAWN / feel knobs)
}

class JBLook : public juce::LookAndFeel_V4
{
public:
    JBLook();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class PhraseView : public juce::Component
{
public:
    explicit PhraseView (JanjaBellyProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    JanjaBellyProcessor& proc;
};

class DragZone : public juce::Component
{
public:
    explicit DragZone (JanjaBellyProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    JanjaBellyProcessor& proc;
    bool dragging = false;
};

class JanjaBellyEditor : public juce::AudioProcessorEditor
{
public:
    explicit JanjaBellyEditor (JanjaBellyProcessor&);
    ~JanjaBellyEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    void initKnob (Knob&, const juce::String& id, const juce::String& text, juce::Colour fill);
    void initCombo (juce::ComboBox&, std::unique_ptr<ComboAtt>&, const juce::String& id);
    void regenerateSameSeed();

    JanjaBellyProcessor& proc;
    JBLook look;

    Knob reverb, chorus, ensemble, swing, jitter, tone;
    juce::ComboBox barsBox, moodBox, keyBox, soundBox;
    std::unique_ptr<ComboAtt> barsAtt, moodAtt, keyAtt, soundAtt;
    juce::ToggleButton halfTime { "Half-time" }, lockSound { "Lock sound" }, preview { "Preview" };
    std::unique_ptr<ButtonAtt> halfAtt, lockAtt;
    juce::TextButton generateBtn { "GENERATE" }, spawnBtn { "SPAWN" };
    PhraseView phraseView;
    DragZone dragZone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JanjaBellyEditor)
};
