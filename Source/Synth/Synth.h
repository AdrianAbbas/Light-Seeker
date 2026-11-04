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
    Sample,    // the developer's recordings (or the library brass): sample (bank id), p0 start skip (s), p1 decay T60 (s, 0 none),
               // p2 evenness, p3 zone offset (semitones: a neighbouring note's recording, played at this pitch: a second
               // player), p4 start delay (ms)
    Organ,     // pipe organ: p0..p5 the ranks 16', 8', 4', 2 2/3', 2', mixture; bright = chiff
    Voice,     // the developer's voice (the recordings update): the vowel sweeps morphed by the
               // Vowel macro, an ensemble of up to eight singers from the 5-take choir stacks
               // (Ensemble macro). p0 vowel (0 mmm, 1 ooh, 2 oh, 3 aah, 4 ee), p1 singers
               // (1..8), p2 velocity cap (0: none; 0.45 keeps to the soft takes), p3 detune
               // (cents, the ensemble's spread), p4 width, p5 timing spread (ms)
};

// The voice's recorded sets (tools/sample-prep): one sweep per vowel, three choir stacks.
inline constexpr int vowelCount = 5;
inline constexpr std::array<const char*, vowelCount> vowelSweeps { "voice-mm", "voice-ooh", "voice-oh", "voice-aah", "voice-ee" };
inline constexpr int stackCount = 3;
inline constexpr std::array<const char*, stackCount> vowelStacks { "choir-mm", "choir-ooh", "choir-aah" };

// Mode sets for Bell and Bowed.
enum Modes : int { ModesChime, ModesGlass, ModesGlock, ModesCelesta, ModesBell, ModesGlassHarp, ModesHarmonic, ModesCrystal, ModesCount };

// When a layer plays, given the recordings: always, only when its recording is loaded
// (a Sample layer), or only when it is not (its synthesised stand-in). The library brass
// (VSCO 2 Community Edition, CC0: the `vsco-*` sets, the real brass update) is a source of
// its own, independent of the developer's recordings: Library layers play when the patch's
// library sets are loaded (a sample layer, or synthesis that goes with them), NoLibrary
// layers (the synthesised brass) when they are not.
enum class Needs : uint8_t { Always, Samples, NoSamples, Library, NoLibrary };

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
    // Brass and Reed (Mix and Punch): the attack's bite (brighter, harder blown in the first
    // ~100 ms, with velocity) and a 2-4 kHz presence band; 0 = as before.
    float bite = 0, presence = 0;
    // The library brass (Sample layers): the same bite (a bright transient band in the first
    // ~70 ms, more with velocity) and presence (a 2-4 kHz band), and drive (gentle saturation:
    // density and shine without more level).
    float drive = 0;
    // The real brass for a synthesised brass layer (Engine::Brass): with the library loaded the
    // layer plays its sustain (looped, the recorded release when let go) and its staccato as
    // the attack's front, shaped (presence, bite, drive), with a little of the layer's own
    // synthesis under them (weight and shine); without the library it is the synthesis alone,
    // exactly as before (the stand-in). Levels are dB relative to the stand-in layer's own;
    // the patch's libraryTrimDb comes on top. Near and beyond the recordings' range the
    // real part crossfades into the synthesis (full real within 3 semitones of the highest
    // and lowest recorded notes, synthesis alone 9 semitones past them).
    struct Real {
        const char* sustain = nullptr;   // bank set (vsco-...-sus), nullptr: none
        const char* front = nullptr;     // bank set (vsco-...-stac), nullptr: none
        // frontDb: the front against its sustain note at equal loudness (each note levelled).
        float db = 0, frontDb = -3, synthDb = -14;
        float attack = 0, release = 0.35f, frontRelease = 0.7f;
        float offset = 0, frontOffset = 0;   // zone offsets (semitones: a neighbouring note's take, a second player)
        float delayMs = 0;                   // the real part starts this much later (a second player)
        float lowpass = 16000, velocityGain = 0.85f, velocityTone = 0.25f;
        float presence = 0, bite = 0, drive = 0;
        // The strike: the real part's gain on hit (dB, more with velocity), falling away over
        // attackMs (exponentially): a hard-tongued front on the library's softer attacks.
        float strikeDb = 0, strikeMs = 60;
        // The front starts this far into its recording (ms at full velocity, less when softer,
        // faded in over 2 ms): the library's staccatos take 40-70 ms to speak; started where
        // the tongue has already struck, the front is sharp. The strike holds while it speaks
        // (strikeMs), then falls away over 60 ms.
        float frontSkipMs = 0;
    } real;
    float vibrato = 0, vibratoHz = 5.2f, vibratoDelay = 0.35f;   // semitones
    int model = 0;               // BodyType, Modes, StringSize or SynthModel as the engine says
    std::array<float, 6> p {};
    const char* sample = nullptr;
    int lowKey = 0, highKey = 127;   // the keys this layer plays (after transposing)
    bool character = false;      // its level follows the Character macro
    // A run of grace notes before the note (a harp's glissando, Direction Update 2): how
    // many (at Character 0.5) and the time between them. The run sweeps up through the
    // notes held (chord tones and their octaves; one note alone: its octaves and fifth)
    // and lands on the played note.
    int run = 0;
    float runMs = 0;
    // A crossfade by key (equal power over `fadeSpan` semitones from `fadeKey`): a layer
    // that fades out above it (`fadeIn` false: the recordings at the top of their range)
    // or in (true: the synthesis that carries on above them). 0: none.
    float fadeKey = 0, fadeSpan = 12;
    bool fadeIn = false;
};

// How the Character macro acts: the layers marked `character`, the shimmer, or the chorus.
enum class Character : uint8_t { Layers, Shimmer, Chorus };

struct Patch {
    const char* name = "";
    const char* family = "";
    const char* character = "Character";   // the macro's name on the panel
    std::array<Layer, 8> layers {};
    int layerCount = 0;
    float trimDb = 0;            // level matching (InstrumentTrims.h, calibrated)
    float libraryTrimDb = 0;     // the library brass layers' level, matched to the synthesis they replace (calibrated)
    float libraryStandInTrimDb = 0;   // the same when the developer's recordings are missing (the voice's stand-in beside it)
    float standInTrimDb = 0;     // the same without the recordings (their stand-ins play)
    float bendRange = 2;         // semitones
    int polyphony = 16;
    bool tremoloMod = false;     // the mod wheel: tremolo (struck, plucked) instead of vibrato
    float modDepth = 0.5f;       // semitones of vibrato (or tremolo depth) at full mod wheel
    float air = 2.0f;            // dB of high shelf: bright and majestic
    float chorus = 0, shimmer = 0;
    Character characterMode = Character::Layers;
    bool sustaining = true;      // held notes sustain (organ, strings, pads) or ring out
    // Mix and Punch. How much of the place's reverbs this instrument takes (times the MIX
    // Reverb control; 1 = the place as designed: the instruments mostly played in a mix sit
    // a little drier), the reverbs' pre-delay added and how much they dip under its attacks
    // (the dry hit first: the brass), and its own Punch (dB of gain on hit at Punch 50 %).
    float space = 1.0f, spacePreDelayMs = 0, spaceDuck = 0;
    float punchDb = 0;
};

inline constexpr int instrumentCount = 31;
const Patch& patch(int index) noexcept;
// The calibration tool (`LightSeekerTests --calibrate-library`) tries trims in place.
void setLibraryTrimForTests(int index, float db, bool standIn = false) noexcept;
// Instruments follow the world's place order: Celestial Citadel's four, then three per location.
int instrumentIndex(int location, int sub) noexcept;
void instrumentPlace(int index, int& location, int& sub) noexcept;
// Whether a patch's recordings (if it uses any) are loaded.
bool patchSampled(const Patch&) noexcept;
bool patchUsesRecordings(const Patch&) noexcept;
// Whether the Vowel and Ensemble macros do anything on this instrument (a voice or choir).
bool patchHasVoice(const Patch&) noexcept;
// Whether a patch has library brass layers, and whether they are loaded (all its library sets).
bool patchUsesLibrary(const Patch&) noexcept;
bool patchLibraryLoaded(const Patch&) noexcept;
// Whether a bank set is the library's (VSCO 2 CE: ids `vsco-*`).
bool librarySet(const char* id) noexcept;

// The instrument macros (0.5 = as designed) and the octave shift.
struct Macros {
    float tone = 0.5f, attack = 0.5f, release = 0.5f, character = 0.5f;
    float vowel = 0.5f, ensemble = 0.5f;   // the voice instruments (as designed at 0.5)
    int octave = 0;
    float levelDb = 0;
    float shimmer = 1.0f;   // the Shimmer control (0..1.5), on the bus shimmer
    float modulation = 1.0f;   // the MIX Modulation control (0..1.5), on the bus chorus
    // Punch (0..1; 0.5 = the instrument's own) and the time it takes to settle.
    float punch = 0.5f, punchSeconds = 0.12f;
};

class Synth {
public:
    static constexpr int maxVoices = 48;
    static constexpr int maxLayers = 8;
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
    // Tests: the Punch gain the punch limiter applies now (1 = none), and the most it took.
    float punchLimiterGain() const noexcept;
    float punchLimiterLowest() const noexcept;
    // Tests: the pitches the glissando's grace notes started on (MIDI note numbers, in the
    // order they sounded), and the sample each started at; cleared by clearRunLog().
    struct RunNote { int pitch; uint64_t at; };
    static constexpr int runLogSize = 256;
    int runLogCount() const noexcept { return runLogged; }
    RunNote runLogEntry(int k) const noexcept { return runLog[std::size_t(k % runLogSize)]; }
    void clearRunLog() noexcept { runLogged = 0; }
    int heldKeys() const noexcept;
    bool keyDown(int note) const noexcept { return note >= 0 && note < 128 && keys[std::size_t(note)].down; }
    bool sustainDown() const noexcept { return pedal; }
    float bendAmount() const noexcept { return bendNow; }
    float modAmount() const noexcept { return modNow; }

private:
    struct Voice;
    struct Bus;
    // `moved`: a held note restarted on a new instrument by travel (no glissando).
    void startNote(int note, float velocity, bool sustained, bool moved = false) noexcept;
    int sweepPitch(Voice&) noexcept;
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
    uint64_t clock = 0;          // samples rendered
    // The glissandos in flight: notes that start within a few milliseconds of each other
    // (a chord) share one sweep that rises into the highest of them.
    struct Sweep {
        bool active = false;
        uint64_t start = 0, land = 0;
        int top = -1, last = -1;
        std::array<bool, 128> members {};
    };
    static constexpr int maxSweeps = 8;
    std::array<Sweep, maxSweeps> sweeps {};
    int nextSweep = 0;
    std::array<RunNote, runLogSize> runLog {};
    int runLogged = 0;
    lswb::dsp::Stream noise;
};

} // namespace ls::inst
