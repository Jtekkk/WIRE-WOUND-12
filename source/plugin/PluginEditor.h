/*
    FLUXCORE·12 — PluginEditor.h
    Resizable vector UI: CORE selector, main/impedance/character controls, the
    four visualisers (Harmonic Analyzer, Flux B–H meter, Response curve, Level
    meters), a preset browser and a dark/light skin toggle.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

#include "PluginProcessor.h"
#include "../gui/Theme.h"
#include "../gui/LookAndFeel.h"
#include "../gui/HarmonicAnalyzer.h"
#include "../gui/FluxMeter.h"
#include "../gui/ResponseCurve.h"
#include "../gui/LevelMeters.h"

namespace fluxcore
{
//==============================================================================
/** A rotary knob paired with a caption; handles its own internal layout. */
class Knob : public juce::Component
{
public:
    Knob (const juce::String& caption)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 16);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (label);
    }
    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (16));
        slider.setBounds (b);
    }
    juce::Slider slider;
    juce::Label  label;
};

//==============================================================================
class FluxcoreEditor : public juce::AudioProcessorEditor
{
public:
    explicit FluxcoreEditor (FluxcoreProcessor&);
    ~FluxcoreEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS  = juce::AudioProcessorValueTreeState;
    using SAtt   = APVTS::SliderAttachment;
    using CAtt   = APVTS::ComboBoxAttachment;
    using BAtt   = APVTS::ButtonAttachment;

    int  addKnob (const char* paramId, const juce::String& caption);
    void refreshSkin();

    FluxcoreProcessor& proc;
    FluxLookAndFeel    lnf;

    // controls
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<SAtt>> knobAtts;

    juce::ComboBox coreBox, stereoBox, osBox, qualityBox, humFreqBox, presetBox;
    std::unique_ptr<CAtt> coreAtt, stereoAtt, osAtt, qualityAtt, humFreqAtt;

    juce::ToggleButton autoTrimBtn { "Auto-Trim" }, bypassBtn { "Bypass" };
    std::unique_ptr<BAtt> autoTrimAtt, bypassAtt;
    juce::TextButton skinBtn { "Light" };

    juce::Label titleLabel, coreNameLabel;

    // visualisers
    HarmonicAnalyzer harmonics { proc };
    FluxMeter        flux       { proc };
    ResponseCurve    response   { proc };
    LevelMeters      levels     { proc };

    // named knob indices (into `knobs`) for layout
    int kInput=-1, kDrive=-1, kBias=-1, kCoreSat=-1, kHyst=-1, kOutput=-1, kMix=-1;
    int kSourceZ=-1, kLoadZ=-1, kWindingRes=-1, kLfSat=-1;
    int kAge=-1, kHum=-1, kMorph=-1, kSideDrive=-1;

    bool darkSkin = true;

    // panel rectangles shared between resized() and paint()
    juce::Rectangle<int> leftPanelArea, mainKnobArea, toneKnobArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluxcoreEditor)
};

} // namespace fluxcore
