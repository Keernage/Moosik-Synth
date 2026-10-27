#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace MoosikColours
{
    const juce::Colour background { 0xff15161c };
    const juce::Colour panel      { 0xff21232d };
    const juce::Colour knob       { 0xff2c2f3b };
    const juce::Colour track      { 0xff3a3d4a };
    const juce::Colour accent     { 0xff8b6cff };
    const juce::Colour text       { 0xffd8d9e0 };
}

// Flat dark theme with arc-style knobs.
class MoosikLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MoosikLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};

class MoosikSynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit MoosikSynthEditor (MoosikSynthProcessor&);
    ~MoosikSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // A labelled knob, or a drop-down for choice parameters.
    struct Control
    {
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    };

    struct Section
    {
        juce::String title;
        int row;
        std::vector<Control*> controls;
        juce::Rectangle<int> bounds;
    };

    void addSection (const juce::String& title, int row,
                     std::initializer_list<std::pair<const char*, const char*>> paramIdsAndNames);

    MoosikLookAndFeel lookAndFeel;
    MoosikSynthProcessor& synthProcessor;

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<Section> sections;

    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoosikSynthEditor)
};
