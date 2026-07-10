/*
    FLUXCORE·12 — PluginProcessor.cpp
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace fluxcore
{
//==============================================================================
FluxcoreProcessor::FluxcoreProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

//==============================================================================
void FluxcoreProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int nCh = juce::jmax (getTotalNumInputChannels(), 2);
    engine.prepare (sampleRate, samplesPerBlock, nCh);

    scratch.assign ((size_t) nCh, std::vector<double> ((size_t) samplesPerBlock, 0.0));
    scratchPtrs.assign ((size_t) nCh, nullptr);

    engine.setParameters (readEngineParameters (apvts));
    reportedLatency = engine.latencySamples();
    setLatencySamples (reportedLatency);
}

bool FluxcoreProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void FluxcoreProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // push current parameters to the engine
    engine.setParameters (readEngineParameters (apvts));

    // report latency changes to the host
    const int lat = engine.latencySamples();
    if (lat != reportedLatency)
    {
        reportedLatency = lat;
        setLatencySamples (lat);
    }

    // ensure scratch is large enough
    if ((int) scratch.size() < numCh || (numSamples > 0 && (int) scratch[0].size() < numSamples))
    {
        scratch.assign ((size_t) juce::jmax (numCh, 2),
                        std::vector<double> ((size_t) juce::jmax (numSamples, 1), 0.0));
        scratchPtrs.assign (scratch.size(), nullptr);
    }

    // float -> double
    for (int c = 0; c < numCh; ++c)
    {
        const float* src = buffer.getReadPointer (c);
        double* dst = scratch[(size_t) c].data();
        for (int i = 0; i < numSamples; ++i) dst[i] = (double) src[i];
        scratchPtrs[(size_t) c] = dst;
    }

    engine.process (scratchPtrs.data(), numCh, numSamples);

    // double -> float
    for (int c = 0; c < numCh; ++c)
    {
        const double* s = scratch[(size_t) c].data();
        float* d = buffer.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i) d[i] = (float) s[i];
    }

    // publish meters for the UI
    meterIn.store   ((float) engine.inputLevel());
    meterOut.store  ((float) engine.outputLevel());
    meterTrim.store ((float) engine.autoTrimGain());
    meterField.store((float) engine.meterField());
    meterMag.store  ((float) engine.meterMagnetisation());
}

//==============================================================================
juce::AudioProcessorEditor* FluxcoreProcessor::createEditor()
{
    return new FluxcoreEditor (*this);
}

const juce::String FluxcoreProcessor::getProgramName (int index)
{
    if (index >= 0 && index < presets.numPresets())
        return presets.factory()[(size_t) index].name;
    return {};
}

juce::String FluxcoreProcessor::currentCoreName()
{
    return juce::String (getCore ((int) readCoreControls (apvts).coreIndex).name);
}

//==============================================================================
void FluxcoreProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FluxcoreProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace fluxcore

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fluxcore::FluxcoreProcessor();
}
