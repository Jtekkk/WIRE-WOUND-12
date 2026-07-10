/*
    FLUXCORE·12 — ParameterLayout.h

    Builds the APVTS parameter layout straight from the shared Parameters.h table
    and snapshots the current values into the engine's plain structs. Keeping the
    layout and the reader in one place guarantees the host parameters and the DSP
    engine can never disagree.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/Parameters.h"
#include "../dsp/Engine.h"

namespace fluxcore
{
//==============================================================================
inline juce::StringArray coreChoiceNames()
{
    juce::StringArray a;
    for (int i = 0; i < kNumCores; ++i)
        a.add (juce::String (i + 1).paddedLeft ('0', 2) + "  " + kCoreMaterials[(size_t) i].name);
    return a;
}

inline const juce::StringArray kStereoChoices { "Stereo", "Dual-Mono", "Mid-Side" };
inline const juce::StringArray kOversampleChoices { "Off", "2x", "4x", "8x", "16x", "Auto" };
inline const juce::StringArray kQualityChoices { "Eco", "Standard", "Insane" };
inline const juce::StringArray kHumFreqChoices { "50 Hz", "60 Hz" };

//==============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // CORE selector
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::core, 1 }, "Core", coreChoiceNames(), kNumCores - 1)); // default: Hybrid

    // continuous parameters from the shared table
    for (const auto& p : kContinuousParams)
    {
        NormalisableRange<float> range (p.min, p.max, 0.0001f, p.skew);
        auto attr = AudioParameterFloatAttributes().withLabel (p.unit);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { p.id, 1 }, p.name, range, p.def, attr));
    }

    // discrete / utility
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::stereoMode, 1 }, "Stereo Mode", kStereoChoices, 0));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::oversample, 1 }, "Oversample", kOversampleChoices, 5)); // Auto
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::quality, 1 }, "Quality", kQualityChoices, 1));          // Standard
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::humFreq, 1 }, "Hum Freq", kHumFreqChoices, 0));         // 50 Hz
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { pid::autoTrim, 1 }, "Auto-Trim", false));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { pid::bypass, 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
inline float rawValue (juce::AudioProcessorValueTreeState& s, const char* id)
{
    if (auto* a = s.getRawParameterValue (id)) return a->load();
    return 0.0f;
}

/** Snapshot the just-the-core controls (for UI probes). */
inline CoreControls readCoreControls (juce::AudioProcessorValueTreeState& s)
{
    CoreControls c;
    c.coreIndex  = (int) rawValue (s, pid::core);
    c.coreMorph  = rawValue (s, pid::coreMorph);
    c.driveDb    = rawValue (s, pid::drive);
    c.bias       = rawValue (s, pid::bias);
    c.coreSat    = rawValue (s, pid::coreSat);
    c.hysteresis = rawValue (s, pid::hysteresis);
    c.sourceZ    = rawValue (s, pid::sourceZ);
    c.loadZ      = rawValue (s, pid::loadZ);
    c.windingRes = rawValue (s, pid::windingRes);
    c.lfSat      = rawValue (s, pid::lfSat);
    c.age        = rawValue (s, pid::age);
    c.adaaOrder  = ((int) rawValue (s, pid::quality) == 0) ? 1 : 2;
    return c;
}

/** Snapshot the full engine parameter set (for the audio thread). */
inline EngineParameters readEngineParameters (juce::AudioProcessorValueTreeState& s)
{
    EngineParameters p;
    p.core        = readCoreControls (s);
    p.inputDb     = rawValue (s, pid::input);
    p.outputDb    = rawValue (s, pid::output);
    p.mix         = rawValue (s, pid::mix);
    p.sideDriveDb = rawValue (s, pid::sideDrive);
    p.humDepth    = rawValue (s, pid::hum);
    p.humFreq     = ((int) rawValue (s, pid::humFreq) == 0) ? 50.0 : 60.0;
    p.autoTrim    = rawValue (s, pid::autoTrim) > 0.5f;
    p.bypass      = rawValue (s, pid::bypass) > 0.5f;
    p.stereoMode  = (StereoMode)     (int) rawValue (s, pid::stereoMode);
    p.oversample  = (OversampleMode) (int) rawValue (s, pid::oversample);
    p.quality     = (QualityMode)    (int) rawValue (s, pid::quality);
    return p;
}

} // namespace fluxcore
