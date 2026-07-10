/*
    FLUXCORE·12 — PresetManager.h

    Factory "Signature Chain" presets plus user-bank save/load. Factory presets
    are defined in code as real-world parameter values and applied through the
    APVTS so the host sees automatable, notified changes.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "../dsp/Parameters.h"

namespace fluxcore
{
struct Preset
{
    juce::String name;
    juce::String category;                                   // Bus / Drums / Vox / Master / Weird
    std::vector<std::pair<juce::String, float>> values;      // real-world values
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
    {
        buildFactory();
    }

    const std::vector<Preset>& factory() const { return presets; }
    int   numPresets() const { return (int) presets.size(); }
    int   current() const { return currentIndex; }

    void apply (int index)
    {
        if (index < 0 || index >= (int) presets.size()) return;
        currentIndex = index;
        for (const auto& kv : presets[(size_t) index].values)
            if (auto* p = state.getParameter (kv.first))
                p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
    }

    void applyByName (const juce::String& n)
    {
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].name == n) { apply (i); return; }
    }

    //== user banks: serialise / restore the whole state tree =================
    juce::String toXmlString() const
    {
        if (auto xml = state.copyState().createXml()) return xml->toString();
        return {};
    }
    bool fromXmlString (const juce::String& xmlText)
    {
        if (auto xml = juce::XmlDocument::parse (xmlText))
        {
            state.replaceState (juce::ValueTree::fromXml (*xml));
            return true;
        }
        return false;
    }

private:
    void add (const juce::String& name, const juce::String& cat,
              std::vector<std::pair<juce::String, float>> v)
    {
        presets.push_back ({ name, cat, std::move (v) });
    }

    void buildFactory()
    {
        using namespace fluxcore::pid;
        // core indices are 0-based; spec numbers are 1-based
        add ("Init",              "Bus",   {{ core, 11 }, { drive, 0 }, { mix, 1.0f }});

        add ("Bus Glue",          "Bus",
             {{ core, 11 }, { drive, 3 }, { bias, 0.15f }, { coreSat, 0.3f },
              { hysteresis, 0.35f }, { mix, 0.9f }, { sourceZ, 0.4f }, { loadZ, 0.55f }});

        add ("Neve-ish Weight",   "Bus",
             {{ core, 0 }, { drive, 8 }, { bias, 0.1f }, { coreSat, 0.45f },
              { hysteresis, 0.4f }, { lfSat, 0.55f }, { windingRes, 0.3f }, { mix, 1.0f }});

        add ("Drum Slam",         "Drums",
             {{ core, 3 }, { drive, 16 }, { coreSat, 0.8f }, { hysteresis, 0.5f },
              { lfSat, 0.7f }, { loadZ, 0.6f }, { windingRes, 0.45f }, { mix, 1.0f }});

        add ("Vocal Air",         "Vox",
             {{ core, 1 }, { drive, 2 }, { bias, 0.05f }, { coreSat, 0.2f },
              { windingRes, 0.6f }, { loadZ, 0.7f }, { mix, 1.0f }});

        add ("Tape-ish Round",    "Bus",
             {{ core, 7 }, { drive, 9 }, { coreSat, 0.55f }, { hysteresis, 0.6f },
              { lfSat, 0.5f }, { loadZ, 0.4f }, { mix, 0.85f }});

        add ("Mastering Sheen",   "Master",
             {{ core, 5 }, { drive, 1 }, { coreSat, 0.15f }, { hysteresis, 0.25f },
              { windingRes, 0.3f }, { loadZ, 0.55f }, { mix, 1.0f }, { autoTrim, 1.0f }});

        add ("Lo-Fi Iron",        "Weird",
             {{ core, 2 }, { drive, 18 }, { coreSat, 0.85f }, { hysteresis, 0.55f },
              { lfSat, 0.6f }, { loadZ, 0.3f }, { age, 0.5f }, { mix, 1.0f }});

        add ("Broken Broadcast",  "Weird",
             {{ core, 6 }, { drive, 12 }, { coreSat, 0.6f }, { hum, 0.5f },
              { age, 0.8f }, { windingRes, 0.7f }, { loadZ, 0.8f }, { mix, 0.9f }});
    }

    juce::AudioProcessorValueTreeState& state;
    std::vector<Preset> presets;
    int currentIndex = 0;
};

} // namespace fluxcore
