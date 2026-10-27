#include "PluginEditor.h"

MoosikLookAndFeel::MoosikLookAndFeel()
{
    using namespace MoosikColours;
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, knob);
    setColour (juce::ComboBox::outlineColourId, track);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
}

void MoosikLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    using namespace MoosikColours;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto centre = bounds.getCentre();
    const auto lineWidth = 4.0f;
    const auto arcRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f - lineWidth;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // Background track, then the coloured arc showing the current value.
    juce::Path trackArc;
    trackArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (track);
    g.strokePath (trackArc, stroke);

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valueArc, stroke);

    // Knob body and pointer.
    const auto bodyRadius = arcRadius - lineWidth - 3.0f;
    g.setColour (knob);
    g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));

    g.setColour (juce::Colours::white);
    g.drawLine ({ centre.getPointOnCircumference (bodyRadius * 0.3f, angle),
                  centre.getPointOnCircumference (bodyRadius * 0.85f, angle) }, 2.5f);
}

MoosikSynthEditor::MoosikSynthEditor (MoosikSynthProcessor& p)
    : AudioProcessorEditor (&p),
      synthProcessor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    addSection ("OSC 1", 0, { { "osc1_wave", "Wave" }, { "osc1_pos", "WT Pos" }, { "osc1_octave", "Octave" },
                              { "osc1_semi", "Semi" }, { "osc1_level", "Level" } });
    addSection ("OSC 2", 0, { { "osc2_wave", "Wave" }, { "osc2_pos", "WT Pos" }, { "osc2_octave", "Octave" },
                              { "osc2_semi", "Semi" }, { "osc2_level", "Level" } });
    addSection ("UNISON", 0, { { "unison", "Voices" }, { "detune", "Detune" }, { "spread", "Spread" } });
    addSection ("OUTPUT", 0, { { "gain", "Gain" } });

    addSection ("AMP ENV", 1, { { "attack", "Attack" }, { "decay", "Decay" }, { "sustain", "Sustain" }, { "release", "Release" } });
    addSection ("FILTER", 1, { { "cutoff", "Cutoff" }, { "resonance", "Reso" }, { "fenv_amount", "Env Amt" } });
    addSection ("FILTER ENV", 1, { { "fenv_attack", "Attack" }, { "fenv_decay", "Decay" },
                                   { "fenv_sustain", "Sustain" }, { "fenv_release", "Release" } });
    addSection ("LFO", 1, { { "lfo_shape", "Shape" }, { "lfo_dest", "Target" }, { "lfo_rate", "Rate" }, { "lfo_amount", "Amount" } });

    keyboard.setAvailableRange (36, 96);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, MoosikColours::accent);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, MoosikColours::accent.withAlpha (0.3f));
    addAndMakeVisible (keyboard);

    setSize (1100, 470);
}

MoosikSynthEditor::~MoosikSynthEditor()
{
    setLookAndFeel (nullptr);
}

void MoosikSynthEditor::addSection (const juce::String& title, int row,
                                    std::initializer_list<std::pair<const char*, const char*>> paramIdsAndNames)
{
    using Apvts = juce::AudioProcessorValueTreeState;
    Section section { title, row, {}, {} };

    for (const auto& [paramId, name] : paramIdsAndNames)
    {
        auto control = std::make_unique<Control>();

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (synthProcessor.apvts.getParameter (paramId)))
        {
            control->combo = std::make_unique<juce::ComboBox>();
            control->combo->addItemList (choice->choices, 1);
            addAndMakeVisible (*control->combo);
            control->comboAttachment = std::make_unique<Apvts::ComboBoxAttachment> (synthProcessor.apvts, paramId, *control->combo);
        }
        else
        {
            control->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            control->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
            addAndMakeVisible (*control->slider);
            control->sliderAttachment = std::make_unique<Apvts::SliderAttachment> (synthProcessor.apvts, paramId, *control->slider);
        }

        control->label.setText (name, juce::dontSendNotification);
        control->label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (control->label);

        section.controls.push_back (control.get());
        controls.push_back (std::move (control));
    }

    sections.push_back (std::move (section));
}

void MoosikSynthEditor::paint (juce::Graphics& g)
{
    using namespace MoosikColours;
    g.fillAll (background);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("MOOSIK SYNTH", getLocalBounds().reduced (16, 0).removeFromTop (40), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));

    for (const auto& section : sections)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (section.bounds.toFloat(), 8.0f);
        g.setColour (accent);
        g.drawText (section.title, section.bounds.withHeight (24).reduced (10, 0), juce::Justification::centredLeft);
    }
}

void MoosikSynthEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (34);

    keyboard.setBounds (area.removeFromBottom (80));
    area.removeFromBottom (12);

    constexpr int gap = 10;
    constexpr int numRows = 2;
    constexpr int maxColumnsPerRow = 15;
    const int columnWidth = (area.getWidth() - gap * 3) / maxColumnsPerRow;
    const int rowHeight = (area.getHeight() - gap * (numRows - 1)) / numRows;

    for (int row = 0; row < numRows; ++row)
    {
        auto rowArea = area.removeFromTop (rowHeight);
        area.removeFromTop (gap);

        for (auto& section : sections)
        {
            if (section.row != row)
                continue;

            section.bounds = rowArea.removeFromLeft (columnWidth * (int) section.controls.size());
            rowArea.removeFromLeft (gap);

            auto inner = section.bounds.withTrimmedTop (24);

            for (auto* control : section.controls)
            {
                auto column = inner.removeFromLeft (columnWidth);
                control->label.setBounds (column.removeFromTop (18));

                if (control->slider != nullptr)
                    control->slider->setBounds (column.reduced (2, 4));
                else
                    control->combo->setBounds (column.withSizeKeepingCentre (columnWidth - 8, 26));
            }
        }
    }
}
