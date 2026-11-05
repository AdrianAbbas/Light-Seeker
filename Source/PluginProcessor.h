#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Parameters.h"
#include "World/Journey.h"
#include "DSP/Atmosphere.h"
#include "Synth/Synth.h"

// One processor, two plug-ins (Direction Update 1): LIGHT SEEKER, the instrument (an
// instrument per sub-location played from MIDI, inside that place's space), and LIGHT
// SEEKER FX, the effect (audio in, the place's space on it). Each plug-in target builds
// it with its own flag (JucePlugin_IsSynth); the tests build both.
class LightSeekerProcessor final : public juce::AudioProcessor {
public:
    explicit LightSeekerProcessor(bool instrumentEdition = false);
    bool isInstrument() const noexcept { return instrumentMode; }
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    // Hosts call reset() on transport stops and locates; clearing the reverb then would
    // cut every tail when playback stops. Only prepareToPlay starts afresh.
    void reset() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return instrumentMode ? "LIGHT SEEKER" : "LIGHT SEEKER FX"; }
    // MIDI travel (Milestone 9): notes choose places where the host sends MIDI to effects.
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }
    // One program whose setCurrentProgram does nothing: some hosts call it on load and
    // that must never change the user's settings. Factory journeys come in Milestone 5.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current Place"; }
    void changeProgramName(int, const juce::String&) override {}
    juce::AudioProcessorParameter* getBypassParameter() const override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

private:
    static BusesProperties busesFor(bool instrument);
    const bool instrumentMode;
    // Raw parameter values (declared before `parameters`: the Sub-location display
    // function reads `location`).
    std::atomic<float>* location = nullptr;
    std::atomic<float>* subLocation = nullptr;
    std::atomic<float>* intensity = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* input = nullptr;
    std::atomic<float>* output = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* travelTime = nullptr;
    std::atomic<float>* travelSync = nullptr;
    std::array<std::atomic<float>*, dc::macroCount> macros {};
    std::array<std::atomic<float>*, dc::Count> advanced {};
    std::atomic<float>* journeyOn = nullptr;
    std::atomic<float>* journeyMode = nullptr;
    std::atomic<float>* journeyBars = nullptr;
    std::atomic<float>* roamOn = nullptr;
    std::atomic<float>* roamBars = nullptr;
    std::atomic<float>* roamRandomness = nullptr;
    std::atomic<float>* roamSeed = nullptr;
    std::atomic<float>* midiTravel = nullptr;
    std::atomic<float>* midiOctave = nullptr;
    std::atomic<float>* atmosLevel = nullptr;
    std::atomic<float>* shimmer = nullptr;   // final touches: the Shimmer control
    // Mix and Punch: the MIX section, the reverb helpers, Atmosphere on/off; Punch.
    std::atomic<float>* mixReverb = nullptr;
    std::atomic<float>* mixEchoes = nullptr;
    std::atomic<float>* mixModulation = nullptr;
    std::atomic<float>* mixCharacter = nullptr;
    std::atomic<float>* mixAtmosphere = nullptr;
    std::atomic<float>* mixLowCut = nullptr;
    std::atomic<float>* mixPreDelay = nullptr;
    std::atomic<float>* mixDuck = nullptr;
    std::atomic<float>* atmosOn = nullptr;
    std::atomic<float>* punch = nullptr;
    std::atomic<float>* punchTime = nullptr;
    std::atomic<float>* atmosDensity = nullptr;
    std::atomic<float>* atmosSeed = nullptr;
    std::atomic<float>* quality = nullptr;
    // The instrument (null in the effect).
    std::atomic<float>* instTone = nullptr;
    std::atomic<float>* instAttack = nullptr;
    std::atomic<float>* instRelease = nullptr;
    std::atomic<float>* instCharacter = nullptr;
    std::atomic<float>* instVowel = nullptr;
    std::atomic<float>* instEnsemble = nullptr;
    std::atomic<float>* instLevel = nullptr;
    std::atomic<float>* instOctave = nullptr;
    std::atomic<float>* instLock = nullptr;

public:
    juce::AudioProcessorValueTreeState parameters;
    // For the editor and tests (written on the audio thread, read anywhere).
    std::atomic<float> inputLevel { 0 }, outputLevel { 0 }, travelProgress { 1 }, cpuLoad { 0 };
    std::atomic<int> currentDestination { 0 }, currentSubLocation { 0 };
    const dc::Engine& engine() const noexcept { return world; }
    int locationIndex() const noexcept;
    // The journey route (Milestone 9): up to eight places, edited on the message thread,
    // read lock-free on the audio thread. Saved with the state.
    void setRoute(const std::vector<dc::Stop>&);
    std::vector<dc::Stop> route() const;
    // The place the sound is in or travelling to right now (journey, roam or MIDI may
    // differ from the Location parameters), and whether one of them is steering.
    bool steering() const noexcept { return steered.load(std::memory_order_relaxed); }
    int atmosphereVoices() const noexcept { return atmosphere.activeVoices(); }
    int atmosphereEvents() const noexcept { return atmosphere.eventsStarted(); }
    // Hotfix 1: whether a Journey (on, with a route) or Roam is set to steer.
    bool journeyRunning() const noexcept;
    bool roamRunning() const noexcept;
    // Whether the audio thread has processed a block since the last manual choice of
    // place: until then steering() and currentDestination still describe the journey
    // that was just stopped, and the editor must not follow them.
    bool steeringCurrent() const noexcept { return processedEpoch.load(std::memory_order_acquire) == manualEpoch.load(std::memory_order_acquire); }
    // A manual choice of place (message thread; map, landmark, place list, hotspot):
    // stops a running Journey or Roam and any place MIDI chose, then sets Location and
    // Sub-location. Takes effect in the next audio block. Returns what was stopped
    // ("Journey", "Roam", "Journey and Roam", or empty), for the editor's note.
    juce::String travelTo(int location, int sub);
    // Stops a running Journey or Roam where the sound is now (the Stop button):
    // Location and Sub-location take the journey's current place.
    juce::String stopSteering();

    // Milestone 11 (message thread). The window's scale (saved with the state).
    std::atomic<float> editorScale { 1.0f };
    // The window as it was left (Direction Update 2): which view (the map or a place's
    // scene), the controls strip, the Advanced drawer and the Journey panel (open, scrolled
    // to), the on-screen keyboard's position. Kept here so a closed and reopened window
    // comes back exactly as it was, and saved with the session (a project reopens it too).
    struct ViewState {
        bool valid = false;            // an editor has stored it (else: the defaults)
        bool place = false;            // a place's scene; false: the world map
        int location = 0, sub = 0;     // the scene shown then (the place follows the parameters)
        bool controls = true, advanced = false, journey = false;
        int advancedScroll = 0, journeyScroll = 0;
        int lowestKey = 36;            // the instrument panel's keyboard
        bool mix = false;              // the MIX section open (Mix and Punch)
        bool operator==(const ViewState&) const = default;
    };
    ViewState viewState() const;
    void updateViewState(const std::function<void(ViewState&)>&);
    int qualityMode() const noexcept;
    // Presets: factory (favourite spots, journeys) and user presets (state files).
    void applyFactoryPreset(int index);
    // "Init / No preset": every parameter at its default (Quality and Bypass kept), no
    // route, no journey; the preset name is "Init".
    void applyInit();
    // Whether anything changed since the preset (or Init) was loaded; the name then
    // shows as "Name *".
    bool presetModified() const;
    juce::String presetDisplayName() const { return presetName + (presetModified() ? " *" : ""); }
    juce::File userPresetFolder() const;
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::File&);
    juce::String presetName { "Init" };
    int presetIndex = -1;   // factory index, or -1
    // A/B compare: two complete settings; switching stores the current one in its slot.
    void selectSlot(int slot);
    void copyCurrentToOtherSlot();
    int currentSlot() const noexcept { return abSlot; }
    // Undo / redo of edits: a snapshot is taken when an edit begins.
    void pushUndo();
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return !undoStack.empty(); }
    bool canRedo() const noexcept { return !redoStack.empty(); }
    bool applyingSnapshot() const noexcept { return applying; }
    // Mix and Punch: the reverbs' duck gain now (1 = none; tests and the MIX section's meter).
    float reverbDuckGain() const noexcept { return duckShown.load(std::memory_order_relaxed); }
    // The playing instrument's own Punch in dB (the Punch parameter's 50 %).
    // GarageBand hotfix: a Punch control saved before the struck instruments' new defaults,
    // re-expressed so it gives the same dB on that instrument (50 %, the default, stays).
    static float migratedPunch(float control, int instrument) noexcept;
    float punchDesignDb() const noexcept;
    // Calibration only (tests): the chain's raw level, without trims or level matcher.
    void setLevelMatching(bool on) noexcept { world.setLevelMatching(on); }

    // ---- The instrument (Direction Update 1) ----
    // The instrument playing now (0..30, in place order) and whether it is locked.
    int instrumentIndex() const noexcept { return shownInstrument.load(std::memory_order_relaxed); }
    bool instrumentLocked() const noexcept { return instLock != nullptr && instLock->load() >= 0.5f; }
    // Lock instrument on/off (message thread). Locking keeps the instrument playing now.
    void setInstrumentLock(bool on);
    // Plays another sub-location's instrument in the place you are in (message thread):
    // locks it, so travelling keeps it.
    void chooseInstrument(int index);
    // The on-screen keyboard, wheels and pedal (message thread; lock-free into the next block).
    void postUiMidi(const juce::MidiMessage&);
    // What the audio thread holds now (for the keyboard's lit keys and the wheels).
    bool keyLit(int note) const noexcept;
    float bendShown() const noexcept { return bendNow.load(std::memory_order_relaxed); }
    float modShown() const noexcept { return modNow.load(std::memory_order_relaxed); }
    bool sustainShown() const noexcept { return sustainNow.load(std::memory_order_relaxed); }
    int instrumentVoices() const noexcept { return voicesNow.load(std::memory_order_relaxed); }
    // Tests: the synth itself.
    const ls::inst::Synth& synthForTest() const noexcept { return *synth; }
    // Tests: the most the output's safety limiter has taken (gain, 1 = never acted).
    float outputLimiterLowestForTest() const noexcept { return limiter.lowest; }

    // ---- Search (both plug-ins) ----
    // The last searches and results the search panel remembers ("q:text", "e:key"; newest
    // first, at most 8). Message thread; kept while the plug-in lives, not in the session.
    juce::StringArray recentSearches;
    // Whether the host's transport is playing (the last block).
    bool hostPlaying() const noexcept { return hostIsPlaying.load(std::memory_order_relaxed); }
    // The instrument edition's preview from search: a short phrase on that instrument (dry
    // of the player's held notes, through the place you are in, a little below the
    // instrument's own level) until stopPreview(); never while the host plays (refused, and
    // a preview stops when the host starts). Message thread; lock-free into the next block.
    bool startPreview(int instrument);
    void stopPreview();
    int previewVoices() const noexcept { return previewVoicesNow.load(std::memory_order_relaxed); }
    static constexpr float previewLevelDb = -4.0f;

private:
    void process(juce::AudioBuffer<float>&, bool forcedBypass);
    void renderInstrument(juce::AudioBuffer<float>&, int numSamples);
    std::unique_ptr<ls::inst::Synth> synth;
    // Search's preview: its own small synth, a phrase scheduled in samples.
    std::unique_ptr<ls::inst::Synth> previewSynth;
    std::atomic<int> previewRequest { -2 };   // -2 nothing new, -1 stop, >= 0 play that instrument
    std::atomic<bool> hostIsPlaying { false };
    std::atomic<int> previewVoicesNow { 0 };
    bool previewOn = false;
    long long previewClock = 0;
    int previewStep = 0;
    std::array<std::array<float, dc::maxChunk>, 2> previewScratch {};
    void renderPreview(float* left, float* right, int numSamples);
    juce::MidiBuffer* blockMidi = nullptr;
    juce::MidiBuffer uiMidiScratch;
    juce::AbstractFifo uiFifo { 512 };
    std::array<uint32_t, 512> uiMessages {};
    std::atomic<int> requestedLock { -1 }, lockedInstrument { -1 }, shownInstrument { 0 };
    int lockedPlace = -1;
    bool wasLocked = false;
    std::array<std::atomic<uint32_t>, 4> litKeys {};
    std::atomic<float> bendNow { 0 }, modNow { 0 };
    std::atomic<bool> sustainNow { false };
    std::atomic<int> voicesNow { 0 };
    std::atomic<float> duckShown { 1.0f };
    static constexpr float neutralIntensity = 1.0e-4f;   // at or below: the dry path
    dc::Controls controls;
    struct Snapshot { std::vector<float> values; std::vector<dc::Stop> route; bool operator==(const Snapshot& o) const; };
    Snapshot snapshot() const;
    void restore(const Snapshot&);
    std::vector<Snapshot> undoStack, redoStack;
    std::array<Snapshot, 2> slots;
    std::array<bool, 2> slotUsed { false, false };
    int abSlot = 0;
    bool applying = false;
    std::array<std::atomic<int>, dc::maxStops> routeStops {};
    std::atomic<int> routeLength { 0 };
    std::atomic<bool> steered { false };
    std::atomic<int> manualEpoch { 0 }, processedEpoch { 0 };
    std::atomic<bool> midiCancel { false };
    juce::String stopSteeringLocked();
    mutable juce::SpinLock viewLock;
    ViewState view;
    void setPlain(const char* id, float plain);
    // The settings the current preset loaded (for "modified"); none after loading a
    // project saved with a modified preset.
    Snapshot presetBaseline;
    bool hasBaseline = false;
    void markPresetBaseline() { presetBaseline = snapshot(); hasBaseline = true; }
    double freeBeats = 0;          // the position when the host does not play
    int midiLocation = -1, midiSub = 0;
    dc::Engine world;
    dc::Atmosphere atmosphere;
    dc::SafetyLimiter limiter;
    std::array<dc::dsp::DelayLine, 2> dryDelay;   // aligns the dry path with the engine's latency
    std::array<std::array<float, dc::maxChunk>, 2> dryAligned {}, work {};
    dc::dsp::Smoothed inputGain, outputGain, wetMix;
    dc::dsp::Ramp bypassRamp;
    // Intensity 0 is a true dry path: the output crossfades (30 ms) to the latency-aligned
    // input, so it is exactly transparent instead of a neutral-sounding chain.
    dc::dsp::Ramp neutralRamp;
    double rate = 48000;
    int latency = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LightSeekerProcessor)
};
