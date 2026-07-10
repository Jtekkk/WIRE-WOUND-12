/*
    FLUXCORE·12 — PluginProcessor.h
    The JUCE AudioProcessor: hosts the double-precision Engine, owns the APVTS
    and PresetManager, and publishes thread-safe meter data for the UI.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include "ParameterLayout.h"
#include "PresetManager.h"
#include "../dsp/Engine.h"

namespace fluxcore
{
class FluxcoreProcessor : public juce::AudioProcessor
{
public:
    FluxcoreProcessor();
    ~FluxcoreProcessor() override = default;

    //== AudioProcessor ======================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FLUXCORE-12"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return juce::jmax (1, presets.numPresets()); }
    int getCurrentProgram() override { return presets.current(); }
    void setCurrentProgram (int index) override { presets.apply (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int sizeInBytes) override;

    //== UI-facing accessors (thread-safe) ===================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    PresetManager& getPresets() { return presets; }
    double getCurrentSampleRate() const { return currentSampleRate; }

    CoreControls uiControls() { return readCoreControls (apvts); }

    float inputLevel()   const { return meterIn.load(); }
    float outputLevel()  const { return meterOut.load(); }
    float autoTrimGain() const { return meterTrim.load(); }
    float liveField()    const { return meterField.load(); }
    float liveMag()      const { return meterMag.load(); }
    juce::String currentCoreName();

private:
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };
    Engine engine;

    double currentSampleRate = 48000.0;
    int    reportedLatency = 0;

    // double-precision scratch for the engine
    std::vector<std::vector<double>> scratch;
    std::vector<double*> scratchPtrs;

    // published meters
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f }, meterTrim { 1.0f };
    std::atomic<float> meterField { 0.0f }, meterMag { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluxcoreProcessor)
};

} // namespace fluxcore
