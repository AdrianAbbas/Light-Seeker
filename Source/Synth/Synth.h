#pragma once
// LIGHT SEEKER's instruments (Direction Update 1): one instrument per sub-location, played
// from MIDI. Built on WORLDBUILDER's sound engine (Source/Synth/wb: its voices and sample
// bank, copied). A patch is up to four layers, each one of the engines below with its own
// tuning, level, envelope and tone; the Synth is a pool of voices that plays a patch.
//
// Real-time rules: everything is allocated in prepare(); note, controller and patch
// changes and render() never allocate, lock or log.
#include <juce_audio_basics/juce_audio_basics.h>
// PlaceFx.h first: Instruments.h's shimmer is built on its SmallSpace (WORLDBUILDER
// includes them in this order too).
#include "wb/PlaceFx.h"
#include "wb/Instruments.h"
#include "wb/SampleBank.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace ls::inst {

// What a layer plays. The engines come from WORLDBUILDER (Voices.h, Instruments.h) except
// Sample (the recordings, after WORLDBUILDER's SamplePart) and Organ (written here).
enum class Engine : uint8_t {
    None,
    Brass,     // SynthVoice, brass model: p0 formant Hz, p1 formant level, p2 breath, p3 second player (cents)
    Analog,    // SynthVoice, analog model: p0 cutoff Hz, p1 filter-envelope octaves, p2 filter decay (s), p3 detune (cents), p4 sub level, p5 width
    Reed,      // SynthVoice, free reed: p0 cutoff Hz, p1 formant Hz, p2 breath, p3 second reed (cents)
    Pluck,     // Pluck (waveguide): model BodyType, p0 pluck position, p1 loss, p2 stiffness, p3 course, p4 pick noise
    Bell,      // Bell (modal): model Modes, p0 hardness, p1 doublet beat Hz, p2 variation
    Tine,      // TineVoice: bright = bell and bark
    Fm,        // FmVoice: p0 ratio, p1 index, p2 index T60 (s), p3 index floor, p4 feedback
    Choir,     // ChoirVoice: p0 vowel (0 oo .. 4 ee), p1 breath, p2 detune (cents)
    Wind,      // WindVoice: p0 h2, p1 h3, p2 h4, p3 h5, p4 breath, p5 chiff
    Bowed,     // BowedVoice (glass, metal): model Modes, p0 rosin, p1 noisiness
    Strings,   // StringVoice: model StringSize, p0 players, p1 spread (cents), p2 bow position, p3 bow noise
    Sample,    // the developer's recordings: sample (bank id), p0 start skip (s), p1 decay T60 (s, 0 none), p2 evenness
    Organ,     // pipe organ: p0..p5 the ranks 16', 8', 4', 2 2/3', 2', mixture; bright = chiff
};

// Mode sets for Bell and Bowed.
enum Modes : int { ModesChime, ModesGlass, ModesGlock, ModesCelesta, ModesBell, ModesGlassHarp, ModesHarmonic, ModesCrystal, ModesCount };

// When a layer plays, given the recordings: always, only when its recording is loaded
// (a Sample layer), or only when it is not (its synthesised stand-in).
enum class Needs : uint8_t { Always, Samples, NoSamples };

struct Layer {
    Engine engine = Engine::None;
    Needs needs = Needs::Always;
    float level = 0;             // dB
    float transpose = 0;         // semitones
    float cents = 0;
    float pan = 0;               // -1 .. 1
    float keyPan = 0.2f;         // low notes to the left, high to the right
    float attack = 0.005f, decay = 1.0f, sustain = 1.0f, release = 0.3f;
    float ring = 2.0f;           // plucked and struck engines: T60 of the note (s)
    float bright = 0.5f;         // the engine's brightness, 0..1
    float lowpass = 18000, highpass = 30;
    float velocityGain = 0.6f;   // 0: every velocity as loud; 1: fully velocity sensitive
    float velocityTone = 0.5f;   // octaves the low-pass closes on the softest note
    float vibrato = 0, vibratoHz = 5.2f, vibratoDelay = 0.35f;   // semitones
    int model = 0;               // BodyType, Modes, StringSize or SynthModel as the engine says
    std::array<float, 6> p {};
    const char* sample = nullptr;
    int lowKey = 0, highKey = 127;   // the keys this layer plays (after transposing)
    bool character = false;      // its level follows the Character macro
    // A run of grace notes before the note (a harp's glissando): how many, the time
    // between them, and the scale they climb (semitones below the note, nearest last).
    int run = 0;
    float runMs = 0;
};

// How the Character macro acts: the layers marked `character`, the shimmer, or the chorus.
enum class Character : uint8_t { Layers, Shimmer, Chorus };

struct Patch {
    const char* name = "";
    const char* family = "";
    const char* character = "Character";   // the macro's name on the panel
    std::array<Layer, 4> layers {};
    int layerCount = 0;
    float trimDb = 0;            // level matching (InstrumentTrims.h, calibrated)
    float standInTrimDb = 0;     // the same without the recordings (their stand-ins play)
    float bendRange = 2;         // semitones
    int polyphony = 16;
    bool tremoloMod = false;     // the mod wheel: tremolo (struck, plucked) instead of vibrato
    float modDepth = 0.5f;       // semitones of vibrato (or tremolo depth) at full mod wheel
    float air = 2.0f;            // dB of high shelf: bright and majestic
    float chorus = 0, shimmer = 0;
    Character characterMode = Character::Layers;
    bool sustaining = true;      // held notes sustain (organ, strings, pads) or ring out
};

inline constexpr int instrumentCount = 31;
const Patch& patch(int index) noexcept;
// Instruments follow the world's place order: Celestial Citadel's four, then three per location.
int instrumentIndex(int location, int sub) noexcept;
void instrumentPlace(int index, int& location, int& sub) noexcept;
// Whether a patch's recordings (if it uses any) are loaded.
bool patchSampled(const Patch&) noexcept;
bool patchUsesRecordings(const Patch&) noexcept;

// The instrument macros (0.5 = as designed) and the octave shift.
struct Macros {
    float tone = 0.5f, attack = 0.5f, release = 0.5f, character = 0.5f;
    int octave = 0;
    float levelDb = 0;
};

class Synth {
public:
    static constexpr int maxVoices = 48;
    static constexpr int maxLayers = 4;
    Synth();
    ~Synth();
    void prepare(double sampleRate);
    void reset() noexcept;          // silence, no notes

    // The instrument. Held keys (and pedalled notes) move to the new one at their
    // velocities; the old instrument's notes are released and ring out in their own
    // sound. Nothing is cut.
    void setPatch(int index) noexcept;
    int patchIndex() const noexcept { return current; }
    void setMacros(const Macros&) noexcept;

    // MIDI (the processor calls these at the event's position in the block).
    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note) noexcept;
    void sustainPedal(bool down) noexcept;
    void pitchBend(float amount) noexcept;   // -1 .. 1
    void modWheel(float amount) noexcept;    // 0 .. 1
    void allNotesOff() noexcept;             // releases everything (CC 123)
    void allSoundOff() noexcept;             // fades everything in a few ms (CC 120)
    void handle(const juce::MidiMessage&) noexcept;

    // Adds n samples of the instrument into l and r (any n).
    void render(float* l, float* r, int n) noexcept;

    int activeVoices() const noexcept;
    int heldKeys() const noexcept;
    bool keyDown(int note) const noexcept { return note >= 0 && note < 128 && keys[std::size_t(note)].down; }
    bool sustainDown() const noexcept { return pedal; }
    float bendAmount() const noexcept { return bendNow; }
    float modAmount() const noexcept { return modNow; }

private:
    struct Voice;
    struct Bus;
    void startNote(int note, float velocity, bool sustained) noexcept;
    void startLayers(Voice&) noexcept;
    void releaseVoice(Voice&) noexcept;
    Voice* freeVoice(int forPatch, bool enforceLimit) noexcept;
    void updateChunk() noexcept;
    void kill(Voice&, float seconds) noexcept;
    struct Key { bool down = false, sustained = false; float velocity = 0; };
    std::array<Key, 128> keys {};
    std::vector<std::unique_ptr<Voice>> voices;
    std::unique_ptr<Bus> bus;
    double rate = 48000;
    int current = 0;
    Macros macros;
    bool pedal = false;
    float bendNow = 0, modNow = 0;
    int chunkLeft = 0;
    uint32_t serial = 0;
    uint64_t noiseCounter = 1;
    lswb::dsp::Stream noise;
};

} // namespace ls::inst
