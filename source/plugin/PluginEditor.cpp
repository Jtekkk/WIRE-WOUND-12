/*
    FLUXCORE·12 — PluginEditor.cpp
*/

#include "PluginEditor.h"

namespace fluxcore
{
//==============================================================================
int FluxcoreEditor::addKnob (const char* paramId, const juce::String& caption)
{
    auto k = std::make_unique<Knob> (caption);
    addAndMakeVisible (*k);
    auto att = std::make_unique<SAtt> (proc.getAPVTS(), paramId, k->slider);
    knobs.push_back (std::move (k));
    knobAtts.push_back (std::move (att));
    return (int) knobs.size() - 1;
}

//==============================================================================
FluxcoreEditor::FluxcoreEditor (FluxcoreProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // title
    titleLabel.setText ("FLUXCORE·12", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    // CORE selector
    coreBox.addItemList (coreChoiceNames(), 1);
    addAndMakeVisible (coreBox);
    coreAtt = std::make_unique<CAtt> (proc.getAPVTS(), pid::core, coreBox);
    coreBox.onChange = [this]
    {
        coreNameLabel.setText (proc.currentCoreName(), juce::dontSendNotification);
    };

    coreNameLabel.setJustificationType (juce::Justification::centred);
    coreNameLabel.setFont (juce::Font (15.0f, juce::Font::bold));
    coreNameLabel.setText (proc.currentCoreName(), juce::dontSendNotification);
    addAndMakeVisible (coreNameLabel);

    // utility combos
    auto setupCombo = [this] (juce::ComboBox& box, const juce::StringArray& items,
                              const char* paramId, std::unique_ptr<CAtt>& att)
    {
        box.addItemList (items, 1);
        addAndMakeVisible (box);
        att = std::make_unique<CAtt> (proc.getAPVTS(), paramId, box);
    };
    setupCombo (stereoBox,  kStereoChoices,     pid::stereoMode, stereoAtt);
    setupCombo (osBox,      kOversampleChoices, pid::oversample, osAtt);
    setupCombo (qualityBox, kQualityChoices,    pid::quality,    qualityAtt);
    setupCombo (humFreqBox, kHumFreqChoices,    pid::humFreq,    humFreqAtt);

    // preset browser
    for (int i = 0; i < proc.getPresets().numPresets(); ++i)
        presetBox.addItem (proc.getPresets().factory()[(size_t) i].name, i + 1);
    presetBox.setSelectedId (proc.getPresets().current() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        proc.getPresets().apply (idx);
    };
    addAndMakeVisible (presetBox);

    // toggles
    addAndMakeVisible (autoTrimBtn);
    autoTrimAtt = std::make_unique<BAtt> (proc.getAPVTS(), pid::autoTrim, autoTrimBtn);
    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<BAtt> (proc.getAPVTS(), pid::bypass, bypassBtn);

    skinBtn.onClick = [this] { darkSkin = ! darkSkin; refreshSkin(); };
    addAndMakeVisible (skinBtn);

    // visualisers
    addAndMakeVisible (harmonics);
    addAndMakeVisible (flux);
    addAndMakeVisible (response);
    addAndMakeVisible (levels);

    // knobs
    kInput      = addKnob (pid::input,      "INPUT");
    kDrive      = addKnob (pid::drive,      "DRIVE");
    kBias       = addKnob (pid::bias,       "BIAS");
    kCoreSat    = addKnob (pid::coreSat,    "CORE SAT");
    kHyst       = addKnob (pid::hysteresis, "HYSTERESIS");
    kOutput     = addKnob (pid::output,     "OUTPUT");
    kMix        = addKnob (pid::mix,        "MIX");

    kSourceZ    = addKnob (pid::sourceZ,    "SOURCE Z");
    kLoadZ      = addKnob (pid::loadZ,      "LOAD Z");
    kWindingRes = addKnob (pid::windingRes, "WINDING RES");
    kLfSat      = addKnob (pid::lfSat,      "LF SAT");
    kAge        = addKnob (pid::age,        "AGE");
    kHum        = addKnob (pid::hum,        "HUM");
    kMorph      = addKnob (pid::coreMorph,  "MORPH A/B");
    kSideDrive  = addKnob (pid::sideDrive,  "SIDE DRIVE");

    refreshSkin();

    setResizable (true, true);
    setResizeLimits (860, 540, 1680, 1040);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) theme::kDefaultWidth / (double) theme::kDefaultHeight);
    setSize (theme::kDefaultWidth, theme::kDefaultHeight);
}

FluxcoreEditor::~FluxcoreEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void FluxcoreEditor::refreshSkin()
{
    theme::setDark (darkSkin);
    lnf.refreshColours();
    skinBtn.setButtonText (darkSkin ? "Light" : "Dark");

    const auto& pal = theme::current();
    titleLabel.setColour (juce::Label::textColourId, pal.accent);
    coreNameLabel.setColour (juce::Label::textColourId, pal.text);

    for (auto* comp : std::initializer_list<juce::Component*> {
             this, &harmonics, &flux, &response, &levels })
        comp->repaint();
    repaint();
}

//==============================================================================
void FluxcoreEditor::paint (juce::Graphics& g)
{
    const auto& pal = theme::current();
    g.fillAll (pal.bg);

    // panels behind the control clusters
    auto drawPanel = [&] (juce::Rectangle<int> r)
    {
        g.setColour (pal.panel);
        g.fillRoundedRectangle (r.toFloat(), theme::kCorner);
        g.setColour (pal.panelEdge);
        g.drawRoundedRectangle (r.toFloat(), theme::kCorner, 1.0f);
    };
    drawPanel (leftPanelArea);
    drawPanel (mainKnobArea);
    drawPanel (toneKnobArea);

    g.setColour (pal.textDim);
    g.setFont (11.0f);
    g.drawText ("MAIN",      mainKnobArea.reduced (8, 4), juce::Justification::topLeft);
    g.drawText ("IMPEDANCE / CHARACTER", toneKnobArea.reduced (8, 4), juce::Justification::topLeft);
    g.drawText ("CORE",      leftPanelArea.reduced (8, 4), juce::Justification::topLeft);
}

//==============================================================================
void FluxcoreEditor::resized()
{
    auto r = getLocalBounds().reduced (8);

    // --- top bar ---
    auto top = r.removeFromTop (40);
    titleLabel.setBounds (top.removeFromLeft (200));
    skinBtn.setBounds (top.removeFromRight (70).reduced (2));
    bypassBtn.setBounds (top.removeFromRight (90).reduced (2));
    autoTrimBtn.setBounds (top.removeFromRight (110).reduced (2));
    presetBox.setBounds (top.removeFromLeft (juce::jmin (280, top.getWidth())).reduced (4, 6));

    r.removeFromTop (6);

    // --- bottom knob strips ---
    auto bottom = r.removeFromBottom (200);
    mainKnobArea = bottom.removeFromTop (100);
    bottom.removeFromTop (4);
    toneKnobArea = bottom;

    auto layoutRow = [] (juce::Rectangle<int> area, std::vector<Knob*> row)
    {
        area = area.reduced (6, 18);
        const int n = (int) row.size();
        if (n == 0) return;
        const int w = area.getWidth() / n;
        for (int i = 0; i < n; ++i)
            row[(size_t) i]->setBounds (area.removeFromLeft (w).reduced (3));
    };

    auto K = [this] (int i) { return knobs[(size_t) i].get(); };
    layoutRow (mainKnobArea, { K (kInput), K (kDrive), K (kBias), K (kCoreSat),
                               K (kHyst), K (kOutput), K (kMix) });
    layoutRow (toneKnobArea, { K (kSourceZ), K (kLoadZ), K (kWindingRes), K (kLfSat),
                               K (kAge), K (kHum), K (kMorph), K (kSideDrive) });

    r.removeFromBottom (6);

    // --- left CORE panel ---
    leftPanelArea = r.removeFromLeft (290);
    auto lp = leftPanelArea.reduced (10, 22);
    coreBox.setBounds (lp.removeFromTop (30));
    lp.removeFromTop (4);
    coreNameLabel.setBounds (lp.removeFromTop (26));
    lp.removeFromTop (10);
    auto comboRow = [&lp] (juce::ComboBox& box, const juce::String&)
    {
        box.setBounds (lp.removeFromTop (28));
        lp.removeFromTop (8);
    };
    comboRow (stereoBox,  "Stereo");
    comboRow (osBox,      "Oversample");
    comboRow (qualityBox, "Quality");
    comboRow (humFreqBox, "Hum Freq");

    r.removeFromLeft (8);

    // --- display area: 2x2 visualisers ---
    auto disp = r;
    auto dtop = disp.removeFromTop (disp.getHeight() / 2);
    disp.removeFromTop (6);
    response.setBounds (dtop.removeFromLeft (dtop.getWidth() / 2).reduced (0, 0));
    dtop.removeFromLeft (6);
    harmonics.setBounds (dtop);
    flux.setBounds (disp.removeFromLeft (disp.getWidth() * 2 / 3).reduced (0, 0));
    disp.removeFromLeft (6);
    levels.setBounds (disp);
}

} // namespace fluxcore
