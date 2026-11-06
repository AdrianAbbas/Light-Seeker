#pragma once
// Host parameters. IDs are stable and versioned (ParameterID{id, 1}); JUCE hashes the
// string IDs into AU parameter IDs, so parameters added in later milestones never
// disturb saved automation. Location order is fixed (automation stores the index).
//
//   Location, Sub-location        where you are (both automatable)
//   Intensity, Mix, Input, Output the main controls
//   Travel Time, Travel Sync      how long a journey takes (free seconds or bars)
//   Macro 1..4                    the location's themed macros (0.5 = as designed)
//   Adv ...                       the Advanced drawer: one per module parameter
//   Bypass                        the host bypass
#include <juce_audio_processors/juce_audio_processors.h>
#include "World/Locations.h"
#include "DSP/Common.h"
#include <functional>

namespace dc::ids {
inline constexpr const char* location = "location";
inline constexpr const char* subLocation = "subLocation";
inline constexpr const char* intensity = "intensity";
inline constexpr const char* mix = "mix";
inline constexpr const char* input = "input";
inline constexpr const char* output = "output";
inline constexpr const char* bypass = "bypass";
inline constexpr const char* travelTime = "travelTime";
inline constexpr const char* travelSync = "travelSync";
inline constexpr std::array<const char*, macroCount> macros { "macro1", "macro2", "macro3", "macro4" };
// Milestone 9: journeys, roaming, MIDI travel.
inline constexpr const char* journeyOn = "journeyOn";
inline constexpr const char* journeyMode = "journeyMode";
inline constexpr const char* journeyBars = "journeyBars";
inline constexpr const char* roamOn = "roamOn";
inline constexpr const char* roamBars = "roamBars";
inline constexpr const char* roamRandomness = "roamRandomness";
inline constexpr const char* roamSeed = "roamSeed";
inline constexpr const char* midiTravel = "midiTravel";
inline constexpr const char* midiOctave = "midiOctave";
// Milestone 10: the Atmosphere layer.
inline constexpr const char* atmosLevel = "atmosLevel";
inline constexpr const char* atmosDensity = "atmosDensity";
inline constexpr const char* atmosSeed = "atmosSeed";
// Milestone 11: Eco / Standard / High (a setting, not automatable).
inline constexpr const char* quality = "quality";
// Direction Update 1: the instrument edition's own parameters (only in the instrument).
inline constexpr const char* instTone = "instTone";
inline constexpr const char* instAttack = "instAttack";
inline constexpr const char* instRelease = "instRelease";
inline constexpr const char* instCharacter = "instCharacter";
inline constexpr const char* instLevel = "instLevel";
inline constexpr const char* instOctave = "instOctave";
inline constexpr const char* instLock = "instLock";
// The recordings update: the voice instruments' Vowel and Ensemble macros (added last, so
// sessions saved before them load unchanged).
inline constexpr const char* instVowel = "instVowel";
inline constexpr const char* instEnsemble = "instEnsemble";
// Final touches: the Shimmer control (both plug-ins; added last, so sessions saved
// before it open at its default, 100 %).
inline constexpr const char* shimmer = "shimmer";
// Mix and Punch (both plug-ins unless noted; added last, so sessions saved before them
// open at the defaults: every MIX group at 100 % = as designed, the reverb helpers off).
inline constexpr const char* mixReverb = "mixReverb";
inline constexpr const char* mixEchoes = "mixEchoes";
inline constexpr const char* mixModulation = "mixModulation";
inline constexpr const char* mixCharacter = "mixCharacter";
inline constexpr const char* mixAtmosphere = "mixAtmosphere";
inline constexpr const char* mixLowCut = "mixLowCut";
inline constexpr const char* mixPreDelay = "mixPreDelay";
inline constexpr const char* mixDuck = "mixDuck";
inline constexpr const char* atmosOn = "atmosOn";
inline constexpr const char* punch = "punch";          // the instrument only
inline constexpr const char* punchTime = "punchTime";  // the instrument only
// The real woodwinds update: the instrument's second macro (the chord pad's Woodwinds; dimmed
// on instruments without one). Added last, so sessions saved before it open at 50 %.
inline constexpr const char* instLayer = "instLayer";  // the instrument only
inline juce::String advanced(std::size_t index) { return juce::String("adv_") + moduleSpecs[index].id; }
}

namespace dc {
// Travel Sync choices: Free (Travel Time in seconds) or a length in beats.
inline constexpr std::array<double, 6> travelSyncBeats { 0, 1, 2, 4, 8, 16 };
inline constexpr const char* travelSyncChoices = "Free|1/4|1/2|1 bar|2 bars|4 bars";
inline constexpr const char* journeyModeChoices = "Loop|One-way|Ping-pong";
// Roam: how long the plug-in stays in a sub-location before wandering on.
inline constexpr std::array<double, 6> roamBarChoices { 0.5, 1, 2, 4, 8, 16 };
inline constexpr const char* roamBarNames = "1/2 bar|1 bar|2 bars|4 bars|8 bars|16 bars";
// MIDI travel: the octave whose notes choose places (C = first location ... A = tenth;
// in the octave above, C..E choose the sub-location). Octave numbers as in most DAWs
// (middle C = C3 here, MIDI note 60).
inline constexpr const char* midiOctaveNames = "C-2|C-1|C0|C1|C2|C3|C4|C5|C6";
// Mix: the group sliders' range (0-150 %), the reverb low cut (at or below `lowCutOff`
// it is off), the pre-delay offset and the most Punch can add.
inline constexpr float mixMax = 1.5f;
inline constexpr float lowCutOff = 75.0f, lowCutMax = 600.0f;
inline constexpr float preDelayMin = -50.0f, preDelayMax = 150.0f;
using dsp::punchMaxDb;
using dsp::punchDb;
using dsp::punchControl;

inline juce::StringArray advancedChoices(const ModuleParamSpec& spec) {
    juce::StringArray choices { "Place" };
    const auto named = juce::StringArray::fromTokens(spec.choices, "|", "");
    if (named.size() > 0 && juce::String(spec.choices).isNotEmpty()) choices.addArray(named);
    else for (int v = static_cast<int>(spec.min); v <= static_cast<int>(spec.max); ++v) choices.add(juce::String(v));
    return choices;
}

// `currentLocation` names the sub-locations of the location in use (for host display).
// `punchDesign` gives the playing instrument's own Punch in dB (for the Punch text).
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(std::function<int()> currentLocation, bool instrument = false,
                                                                                 std::function<float()> punchDesign = {}) {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::StringArray places;
    for (const auto* name : locationNames) places.add(name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::location, 1 }, "Location", places, 0));
    const auto percent = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })
        .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue() / 100.0f; });
    const auto decibels = juce::AudioParameterFloatAttributes().withLabel("dB")
        .withStringFromValueFunction([](float v, int) { return juce::String(v, 1); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::intensity, 1 }, "Intensity",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mix, 1 }, "Mix",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::input, 1 }, "Input",
                                                           juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f, decibels));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::output, 1 }, "Output",
                                                           juce::NormalisableRange<float>(-24.0f, 12.0f), 0.0f, decibels));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::bypass, 1 }, "Bypass", false));

    // Milestone 1.
    auto subNames = juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([currentLocation](int v, int) { return juce::String(subLocationName(currentLocation(), v)); })
        .withValueFromStringFunction([currentLocation](const juce::String& s) {
            const int location = currentLocation();
            for (int k = 0; k <= subLocationCount(location); ++k)
                if (s.trim().equalsIgnoreCase(subLocationName(location, k))) return k;
            return s.getIntValue();
        });
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::subLocation, 1 }, "Sub-location",
                                                         0, maxSubLocations, 0, subNames));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::travelTime, 1 }, "Travel Time",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.0f, 0.4f), 1.5f,
        juce::AudioParameterFloatAttributes().withLabel("s")
            .withStringFromValueFunction([](float v, int) { return v < 0.02f ? juce::String("Instant") : juce::String(v, v < 1.0f ? 2 : 1) + " s"; })
            .withValueFromStringFunction([](const juce::String& s) { return s.trim().equalsIgnoreCase("Instant") ? 0.0f : s.getFloatValue(); })));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::travelSync, 1 }, "Travel Sync",
                                                            juce::StringArray::fromTokens(travelSyncChoices, "|", ""), 0));
    const auto macroText = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { const int p = juce::roundToInt((v - 0.5f) * 200.0f); return (p > 0 ? "+" : "") + juce::String(p) + " %"; })
        .withValueFromStringFunction([](const juce::String& s) { return 0.5f + s.getFloatValue() / 200.0f; });
    for (int m = 0; m < macroCount; ++m)
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::macros[static_cast<std::size_t>(m)], 1 },
                                                               "Macro " + juce::String(m + 1), juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
    // Milestone 9: the journey path (a route of places, bars per stop), roaming between
    // a location's places, MIDI travel. All automatable; the route itself is state.
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::journeyOn, 1 }, "Journey", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::journeyMode, 1 }, "Journey Mode",
                                                            juce::StringArray::fromTokens(journeyModeChoices, "|", ""), 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::journeyBars, 1 }, "Bars per Stop", 1, 16, 2,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return juce::String(v) + (v == 1 ? " bar" : " bars"); })));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::roamOn, 1 }, "Roam", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::roamBars, 1 }, "Roam Rate",
                                                            juce::StringArray::fromTokens(roamBarNames, "|", ""), 2));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::roamRandomness, 1 }, "Roam Randomness",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, percent));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::roamSeed, 1 }, "Roam Seed", 1, 999, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::midiTravel, 1 }, "MIDI Travel", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::midiOctave, 1 }, "MIDI Travel Octave",
                                                            juce::StringArray::fromTokens(midiOctaveNames, "|", ""), 1));

    // Milestone 10: the Atmosphere layer (off by default).
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::atmosLevel, 1 }, "Atmosphere",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::atmosDensity, 1 }, "Atmosphere Density",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f, percent));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::atmosSeed, 1 }, "Atmosphere Seed", 1, 999, 1));

    // Milestone 11: quality mode. Eco: the Wraith in mono, no animation, art at 1x;
    // Standard: full DSP, gentle animation; High: smoother animation and art.
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::quality, 1 }, "Quality",
                                                            juce::StringArray { "Eco", "Standard", "High" }, 1,
                                                            juce::AudioParameterChoiceAttributes().withAutomatable(false)));

    // The Advanced drawer: an offset from the place for every continuous module
    // parameter (0 = as designed), an override for every choice ("Place" = as designed).
    const auto offsetText = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float v, int) { const int p = juce::roundToInt(v * 100.0f); return (p > 0 ? "+" : "") + juce::String(p) + " %"; })
        .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue() / 100.0f; });
    for (std::size_t i = 0; i < Count; ++i) {
        const auto& spec = moduleSpecs[i];
        const juce::ParameterID id { ids::advanced(i), 1 };
        const juce::String name = juce::String("Adv ") + spec.name;
        if (spec.glide == Glide::Step) layout.add(std::make_unique<juce::AudioParameterChoice>(id, name, advancedChoices(spec), 0));
        else layout.add(std::make_unique<juce::AudioParameterFloat>(id, name, juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f, offsetText));
    }
    // Direction Update 1, the instrument: its macros (0.5 = as designed), level, octave
    // and Lock instrument (the instrument stays while you travel; the space still moves).
    if (instrument) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instTone, 1 }, "Instrument Tone",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instAttack, 1 }, "Instrument Attack",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instRelease, 1 }, "Instrument Release",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instCharacter, 1 }, "Instrument Character",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instLevel, 1 }, "Instrument Level",
                                                               juce::NormalisableRange<float>(-24.0f, 12.0f), 0.0f, decibels));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ids::instOctave, 1 }, "Instrument Octave", -2, 2, 0,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return (v > 0 ? "+" : "") + juce::String(v); })));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::instLock, 1 }, "Lock Instrument", false));
        // The voice instruments (choirs): Vowel morphs between the sung vowels (mmm, ooh,
        // oh, aah, ee; two either way from the instrument's own), Ensemble brings in the
        // singers (one voice at 0, the instrument's own choir at 50 %, eight at 100 %).
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instVowel, 1 }, "Instrument Vowel",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instEnsemble, 1 }, "Instrument Ensemble",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
    }
    // Final touches: Shimmer scales every shimmer, octave-shimmer and sparkle stage
    // (0-150 %; 100 % = the default, a little below the original design).
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::shimmer, 1 }, "Shimmer",
                                                           juce::NormalisableRange<float>(0.0f, 1.5f), 1.0f, percent));
    // Mix and Punch: the MIX section scales each group of effects (0-150 %; 100 % = as
    // designed; Intensity stays the master on top), three helpers for the reverbs, the
    // Atmosphere's on/off; the instrument's Punch (gain on hit) and its time.
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixReverb, 1 }, "Mix Reverb",
                                                           juce::NormalisableRange<float>(0.0f, mixMax), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixEchoes, 1 }, "Mix Echoes",
                                                           juce::NormalisableRange<float>(0.0f, mixMax), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixModulation, 1 }, "Mix Modulation",
                                                           juce::NormalisableRange<float>(0.0f, mixMax), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixCharacter, 1 }, "Mix Character",
                                                           juce::NormalisableRange<float>(0.0f, mixMax), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixAtmosphere, 1 }, "Mix Atmosphere",
                                                           juce::NormalisableRange<float>(0.0f, mixMax), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixLowCut, 1 }, "Reverb Low Cut",
        juce::NormalisableRange<float>(lowCutOff - 5.0f, lowCutMax, 0.0f, 0.5f), lowCutOff - 5.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")
            .withStringFromValueFunction([](float v, int) { return v <= lowCutOff ? juce::String("Off") : juce::String(juce::roundToInt(v)) + " Hz"; })
            .withValueFromStringFunction([](const juce::String& s) { return s.trim().equalsIgnoreCase("Off") ? lowCutOff - 5.0f : s.getFloatValue(); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixPreDelay, 1 }, "Reverb Pre-delay",
        juce::NormalisableRange<float>(preDelayMin, preDelayMax), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")
            .withStringFromValueFunction([](float v, int) { const int ms = juce::roundToInt(v); return (ms > 0 ? "+" : "") + juce::String(ms) + " ms"; })
            .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::mixDuck, 1 }, "Reverb Duck",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { ids::atmosOn, 1 }, "Atmosphere On", true));
    if (instrument) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::punch, 1 }, "Punch",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f,
            juce::AudioParameterFloatAttributes().withLabel("dB")
                .withStringFromValueFunction([punchDesign](float v, int) {
                    const float db = punchDb(v, punchDesign ? punchDesign() : 0.0f);
                    return db < 0.05f ? juce::String("Off") : "+" + juce::String(db, 1) + " dB";
                })
                .withValueFromStringFunction([punchDesign](const juce::String& s) {
                    return s.trim().equalsIgnoreCase("Off") ? 0.0f : punchControl(s.getFloatValue(), punchDesign ? punchDesign() : 0.0f);
                })));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::punchTime, 1 }, "Punch Time",
            juce::NormalisableRange<float>(20.0f, 500.0f, 0.0f, 0.5f), 120.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")
                .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + " ms"; })
                .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); })));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::instLayer, 1 }, "Instrument Layer",
                                                               juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, macroText));
    }
    return layout;
}
}
