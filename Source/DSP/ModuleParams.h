#pragma once
// The internal parameters of the six sound modules imported from VEIL. They are not
// host parameters: a location (and later a sub-location, the macros and the Advanced
// drawer) produces a Frame of these values, and the engine interpolates frames while
// travelling. Index order is the Frame order used everywhere.
#include <array>
#include <cstddef>

namespace dc {
enum P : std::size_t {
    // I. Horizon: filters, Haze, Movement, Resonator.
    Cutoff, Resonance, HighPass, Slope, Darkness, FilterMovement, FilterRate, FilterSync,
    Tomb, TombNote, TombOctave, TombRing,
    // II. Sunwarmth: saturation and gentle age.
    Drive, Texture, Age, Dust, Instability, BloodMix,
    // III. The Choir: granular pitch voices and Drone.
    Voices, Interval, ChoirDetune, Formant, ChoirMovement, ChoirBlend, Drone, DroneHold,
    // IV. Aura: spectral blur, halo and freeze.
    Blur, WraithDetune, Drift, Ghost, Freeze, WraithMix,
    // V. Echoes: delays incl. reverse and pitch steps.
    DelayTime, DelaySync, Feedback, DelayDamping, Diffusion, Reverse, Descent, Spread, DelayMix,
    // VI. Halls: FDN reverb.
    Space, Decay, PreDelay, Damping, ReverbLowCut, Shimmer, ReverbWidth, ReverbMix, Character,
    CryptMode, RiseTime, Eternal,
    // VII. Charms: flutter, struck bells/crystal/harp strings, wind and drops.
    Flicker, FlickerRate, Metal, MetalKind, MetalNote, MetalOctave, MetalDecay, Wind, Drips,
    // VIII. Signature: the one effect that belongs to a single place (Milestone 6).
    SigKind, SigAmount, SigTone, SigRate,
    // IX. Radiance: convolution with generated and captured impulse responses.
    VaultKind, VaultMix, VaultShimmer, VaultTone, VaultDuck,
    // X. Ensemble: chorus, rising phaser, tape echo, water drops.
    EnsMix, EnsDepth, EnsRate, Spiral, SpiralRate, TapeMix, TapeTime, TapeFeedback, TapeTone, TapeWow, TapeAscend, Drops,
    Count
};
using Frame = std::array<float, Count>;

// Interpolation: Lin (straight), Log (geometric, for frequencies and times) or Step
// (a choice or switch: the value changes once, half-way).
enum class Glide { Lin, Log, Step };
struct ModuleParamSpec { const char* id; const char* name; float min, max, neutral; Glide glide; const char* choices; };

// The signature effects (VIII), one per place, in kind order (0 = none). See
// Source/DSP/Signature.cpp for what each one does.
inline constexpr const char* signatureChoices =
    "None|Fanfare Reflections|Mirror Multiplication|Golden Chorus|Sky Echoes"
    "|Rising Spiral|Choir Bloom|Wind Chimes"
    "|Water Glass|Living Wood|Star Harmonics"
    "|Spectral Prism|Still Reflection|Crystal Modes"
    "|Sun Warmth|Butterfly Flutter|Valley Echoes"
    "|Radiant Cascade|Angel Choir|Colour Shift"
    "|Cloud Swirl|Wind Shroud|Soaring Echoes"
    "|Lush Chorus|Fountain Sparkle|Column Slaps"
    "|Bouncy Sparkle|Firefly Glitter|Harp Resonance"
    "|Cliff Echoes|Golden Harmonics|Soaring Octave";

// The Vault's spaces (IX), generated impulse responses; see Source/DSP/Vault.cpp.
inline constexpr const char* vaultChoices =
    "None|Dawn Hall|Mirror Gallery|Throne Hall|Open Heights|Temple Air|Sanctum Bloom|Bell Garden"
    "|Moon Pool|Wooden Hall|Star Canopy|Prism Chamber|Still Lake|Diamond Room|Meadow Air|Glade Air|Valley"
    "|Radiant Nave|Choir Loft|Glass Chapel|Sky Bridge|Wind Isle|Harbour|Rose Bower|Fountain Court|Colonnade"
    "|Mushroom Ring|Firefly Hollow|Fairy Room|Cliff Eyrie|Golden Nest|Heroic Summit"
    // Captured spaces (the developer's effect captures, tools/captures; product names only).
    "|Radiant Shimmer Hall|Cliff Hall Echo|Chorus Hall";

// `neutral` is the transparent setting: every module at its neutral values leaves the
// signal unchanged (the Abyss open, no drive, every wet layer at zero).
inline constexpr std::array<ModuleParamSpec, Count> moduleSpecs {{
    { "cutoff", "Cutoff", 40, 20000, 20000, Glide::Log, "" },
    { "resonance", "Resonance", 0, 1, 0, Glide::Lin, "" },
    { "highPass", "High-Pass", 20, 2000, 20, Glide::Log, "" },
    { "slope", "Slope", 0, 1, 0, Glide::Step, "12 dB|24 dB" },
    { "darkness", "Haze", 0, 1, 0, Glide::Lin, "" },
    { "filterMovement", "Movement", 0, 1, 0, Glide::Lin, "" },
    { "filterRate", "Movement Rate", .01f, 8, .12f, Glide::Log, "" },
    { "filterSync", "Movement Sync", 0, 5, 0, Glide::Step, "Free|1/4|1/2|1 bar|2 bars|4 bars" },
    { "tomb", "Resonator", 0, 1, 0, Glide::Lin, "" },
    { "tombNote", "Resonator Note", 0, 11, 2, Glide::Step, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B" },
    { "tombOctave", "Resonator Octave", 0, 4, 1, Glide::Step, "1|2|3|4|5" },
    { "tombRing", "Resonator Ring", 0, 1, .65f, Glide::Lin, "" },

    { "drive", "Drive", 0, 36, 0, Glide::Lin, "" },
    { "texture", "Texture", 0, 1, .35f, Glide::Lin, "" },
    { "age", "Patina", 0, 1, 0, Glide::Lin, "" },
    { "dust", "Grain", 0, 1, 0, Glide::Lin, "" },
    { "instability", "Waver", 0, 1, 0, Glide::Lin, "" },
    { "bloodMix", "Warmth Mix", 0, 1, 0, Glide::Lin, "" },

    { "voices", "Voice Count", 1, 4, 2, Glide::Step, "" },
    { "interval", "Interval", 0, 5, 0, Glide::Step, "Octave below|Fifth below|Fourth above|Fifth above|Octave above|Octave + fifth" },
    { "choirDetune", "Choir Detune", 0, 50, 12, Glide::Lin, "" },
    { "formant", "Formant", 0, 1, .4f, Glide::Lin, "" },
    { "choirMovement", "Choir Movement", 0, 1, .35f, Glide::Lin, "" },
    { "choirBlend", "Choir Blend", 0, 1, 0, Glide::Lin, "" },
    { "drone", "Drone", 0, 1, 0, Glide::Lin, "" },
    { "droneHold", "Drone Hold", .5f, 30, 6, Glide::Log, "" },

    { "blur", "Spectral Blur", 0, 1, .5f, Glide::Lin, "" },
    { "wraithDetune", "Aura Detune", 0, 100, 15, Glide::Lin, "" },
    { "drift", "Drift", 0, 1, .4f, Glide::Lin, "" },
    { "ghost", "Halo", 0, 1, .3f, Glide::Lin, "" },
    { "freeze", "Freeze", 0, 1, 0, Glide::Step, "Off|On" },
    { "wraithMix", "Aura Mix", 0, 1, 0, Glide::Lin, "" },

    { "delayTime", "Time", 20, 2000, 400, Glide::Log, "" },
    { "delaySync", "Time Sync", 0, 7, 0, Glide::Step, "Free|1/16|1/8|1/8 dotted|1/4|1/4 dotted|1/2|1 bar" },
    { "feedback", "Feedback", 0, .98f, .3f, Glide::Lin, "" },
    { "delayDamping", "Echo Damping", 0, 1, .5f, Glide::Lin, "" },
    { "diffusion", "Diffusion", 0, 1, .35f, Glide::Lin, "" },
    { "reverse", "Reverse", 0, 1, 0, Glide::Step, "Off|On" },
    { "descent", "Pitch Steps", 0, 3, 0, Glide::Step, "Off|Semitone down|Fifth down|Octave down" },
    { "spread", "Stereo Spread", 0, 1, .6f, Glide::Lin, "" },
    { "delayMix", "Echoes Mix", 0, 1, 0, Glide::Lin, "" },

    { "space", "Space", 0, 1, .5f, Glide::Lin, "" },
    { "decay", "Decay", .3f, 30, 2, Glide::Log, "" },
    { "preDelay", "Pre-Delay", 0, 250, 20, Glide::Lin, "" },
    { "damping", "Damping", 0, 1, .5f, Glide::Lin, "" },
    { "reverbLowCut", "Low Cut", 20, 1000, 100, Glide::Log, "" },
    { "shimmer", "Shimmer", 0, 1, 0, Glide::Lin, "" },
    { "reverbWidth", "Width", 0, 1, 1, Glide::Lin, "" },
    { "reverbMix", "Halls Mix", 0, 1, 0, Glide::Lin, "" },
    { "character", "Chamber", 0, 3, 1, Glide::Step, "Marble Chapel|Radiant Cathedral|Crystal Chamber|Endless Sky" },
    { "cryptMode", "Halls Mode", 0, 1, 0, Glide::Step, "Normal|Rising" },
    { "riseTime", "Rise Time", .2f, 4, 1.2f, Glide::Log, "" },
    { "eternal", "Eternal Hold", 0, 1, 0, Glide::Step, "Off|On" },

    { "flicker", "Flutter", 0, 1, 0, Glide::Lin, "" },
    { "flickerRate", "Flutter Rate", .3f, 12, 3, Glide::Log, "" },
    { "metal", "Chime", 0, 1, 0, Glide::Lin, "" },
    { "metalKind", "Chime Kind", 0, 2, 0, Glide::Step, "Harp Strings|Bell|Crystal" },
    { "metalNote", "Chime Note", 0, 11, 2, Glide::Step, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B" },
    { "metalOctave", "Chime Octave", 0, 4, 2, Glide::Step, "2|3|4|5|6" },
    { "metalDecay", "Chime Decay", .05f, 8, .8f, Glide::Log, "" },
    { "wind", "Wind", 0, 1, 0, Glide::Lin, "" },
    { "drips", "Drops", 0, 1, 0, Glide::Lin, "" },

    { "sigKind", "Signature", 0, 31, 0, Glide::Step, signatureChoices },
    { "sigAmount", "Signature Amount", 0, 1, 0, Glide::Lin, "" },
    { "sigTone", "Signature Tone", -1, 1, 0, Glide::Lin, "" },
    { "sigRate", "Signature Rate", .25f, 4, 1, Glide::Log, "" },

    { "vaultKind", "Radiance Space", 0, 34, 0, Glide::Step, vaultChoices },
    { "vaultMix", "Radiance Mix", 0, 1, 0, Glide::Lin, "" },
    { "vaultShimmer", "Radiance Shimmer", 0, 1, 0, Glide::Lin, "" },
    { "vaultTone", "Radiance Tone", 300, 20000, 20000, Glide::Log, "" },
    { "vaultDuck", "Radiance Duck", 0, 1, 0, Glide::Lin, "" },

    { "ensMix", "Ensemble", 0, 1, 0, Glide::Lin, "" },
    { "ensDepth", "Ensemble Depth", .5f, 12, 3, Glide::Log, "" },
    { "ensRate", "Ensemble Rate", .05f, 4, .4f, Glide::Log, "" },
    { "spiral", "Rising Phaser", 0, 1, 0, Glide::Lin, "" },
    { "spiralRate", "Phaser Rise Rate", .02f, 1, .1f, Glide::Log, "" },
    { "tapeMix", "Tape Echo", 0, 1, 0, Glide::Lin, "" },
    { "tapeTime", "Tape Time", 40, 1500, 380, Glide::Log, "" },
    { "tapeFeedback", "Tape Feedback", 0, .9f, .4f, Glide::Lin, "" },
    { "tapeTone", "Tape Tone", 600, 16000, 5000, Glide::Log, "" },
    { "tapeWow", "Tape Wow", 0, 1, .2f, Glide::Lin, "" },
    { "tapeAscend", "Tape Pitch", 0, 4, 0, Glide::Step, "Off|Rising semitone|Rising fifth|Rising octave|Falling fourth" },
    { "drops", "Water Beads", 0, 1, 0, Glide::Lin, "" },
}};

inline Frame neutralFrame() noexcept {
    Frame f {};
    for (std::size_t i = 0; i < Count; ++i) f[i] = moduleSpecs[i].neutral;
    return f;
}
}
