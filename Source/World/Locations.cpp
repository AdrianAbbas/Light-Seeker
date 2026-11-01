#include "Locations.h"
#include "LevelTrims.h"
#include <algorithm>
#include <cmath>

namespace dc {
namespace {
// Character (Chamber): 0 Marble Chapel, 1 Radiant Cathedral, 2 Crystal Chamber, 3 Endless Sky.
// Interval: 0 octave below, 1 fifth below, 2 fourth above, 3 fifth above, 4 octave above, 5 octave + fifth.
// MetalKind: 0 harp strings, 1 bell, 2 crystal. MetalOctave and TombOctave are indices (0 = lowest).
// TapeAscend: 0 off, 1 rising semitone, 2 rising fifth, 3 rising octave, 4 falling fourth.
//
// Rule from day one (docs/DEVLOG.md): at Intensity 100 % / Mix 100 % every place
// transforms the sound obviously (the space is a main voice, not a garnish); each has one
// signature effect no other place uses (Signature.cpp, kind = place number).
std::vector<LocationDesign> buildWorld() {
    std::vector<LocationDesign> w;

    //--------------------------------------------------------------------------
    w.push_back({ "Celestial Citadel", "Gates of Dawn",
        // Vast bright marble halls behind golden gates: a clear, wide marble hall, its
        // first reflections bright and brassy like a fanfare (the signature).
        { { HighPass, 55 }, { Cutoff, 20000 },
          { Character, 1 }, { Space, .8f }, { Decay, 3.8f }, { Damping, .15f }, { PreDelay, 24 }, { ReverbWidth, 1 }, { ReverbMix, .6f },
          { VaultKind, 1 }, { VaultMix, .5f } },
        {
          // A gallery of mirrors: reflections multiplying across the stereo field,
          // bright and close.
          { "Hall of Mirrors", { { VaultKind, 2 }, { VaultMix, .55f }, { Space, .65f }, { Decay, 2.8f }, { Damping, .08f }, { PreDelay, 8 }, { ReverbMix, .5f },
                                 { DelayTime, 86 }, { Feedback, .5f }, { Diffusion, .15f }, { DelayDamping, .1f }, { Spread, 1 }, { DelayMix, .32f } } },
          // The throne: regal and rich, a broad ensemble chorus, a halo of gold shimmer.
          { "Throne of Light", { { VaultKind, 3 }, { VaultMix, .55f }, { VaultShimmer, .3f }, { Space, .9f }, { Decay, 5 }, { Damping, .2f }, { PreDelay, 40 },
                                 { Shimmer, .4f }, { ReverbMix, .55f },
                                 { EnsMix, .6f }, { EnsDepth, 5 }, { EnsRate, .45f } } },
          // The top of the highest spire: thin, open air, long airy echoes far below.
          { "Spire Summit", { { HighPass, 260 }, { Cutoff, 16000 }, { Wind, .28f }, { VaultKind, 4 }, { VaultMix, .28f },
                              { Character, 3 }, { Space, .9f }, { Decay, 5.5f }, { Damping, .25f }, { PreDelay, 90 }, { ReverbLowCut, 320 }, { ReverbMix, .3f },
                              { DelayTime, 640 }, { Feedback, .4f }, { DelayDamping, .3f }, { Diffusion, .3f }, { Spread, 1 }, { DelayMix, .24f } } },
        },
        {
          { "Grandeur", { { Space, -.35f, .25f }, { Decay, -1, 1 }, { PreDelay, -15, 30 }, { VaultMix, -.2f, .2f } } },
          { "Radiance", { { Shimmer, -.1f, .35f }, { VaultShimmer, -.1f, .3f }, { Damping, .3f, -.15f }, { SigTone, -.6f, .6f }, { ReverbMix, -.15f, .12f } } },
          { "Echo", { { DelayMix, -.15f, .2f }, { Feedback, -.2f, .2f }, { SigAmount, -.4f, 0 } } },
          { "Height", { { HighPass, -.6f, 1.2f }, { ReverbMix, -.2f, .2f }, { Wind, 0, .25f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Sky Temple", "Cloud Steps",
        // Airy and sacred, high above the clouds: a long soft temple air with a rising
        // spiral of shimmer climbing the steps (the signature).
        { { HighPass, 90 }, { Cutoff, 16000 },
          { Character, 1 }, { Space, .9f }, { Decay, 6 }, { Damping, .2f }, { PreDelay, 45 }, { Shimmer, .3f }, { ReverbMix, .6f },
          { VaultKind, 5 }, { VaultMix, .45f },
          { Spiral, .5f }, { SpiralRate, .12f } },
        {
          // The sanctum: soft, warm and close, a choir blooming under the instrument.
          { "Sanctum", { { Spiral, 0 }, { Cutoff, 9000 }, { VaultKind, 6 }, { VaultMix, .32f },
                         { Character, 0 }, { Space, .7f }, { Decay, 4.5f }, { Damping, .5f }, { PreDelay, 30 }, { Shimmer, .1f }, { ReverbMix, .34f },
                         { Voices, 2 }, { Interval, 4 }, { ChoirBlend, .18f }, { Formant, .65f }, { ChoirMovement, .3f } } },
          // A garden of bells and wind chimes, tuned, ringing with the playing.
          { "Bell Garden", { { Spiral, 0 }, { Wind, .12f }, { VaultKind, 7 }, { VaultMix, .45f },
                             { Metal, .6f }, { MetalKind, 1 }, { MetalNote, 4 }, { MetalOctave, 2 }, { MetalDecay, 4 },
                             { DelayTime, 380 }, { Feedback, .3f }, { DelayDamping, .3f }, { Spread, 1 }, { DelayMix, .16f },
                             { Character, 0 }, { Space, .55f }, { Decay, 3.2f }, { Damping, .15f }, { PreDelay, 20 }, { ReverbMix, .5f } } },
        },
        {
          { "Elevation", { { Space, -.3f, .15f }, { Decay, -1, 1 }, { HighPass, -.5f, .8f } } },
          { "Devotion", { { ChoirBlend, -.1f, .2f }, { Shimmer, -.15f, .3f }, { Drone, 0, .15f } } },
          { "Bells", { { Metal, -.3f, .4f }, { MetalDecay, -1, 1 }, { SigAmount, -.4f, 0 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Elven Sanctuary", "Moonwell",
        // Lush and ancient: a silver pool under the trees; a glassy shimmer moving on the
        // water's surface (the signature) over a long soft night air.
        { { HighPass, 45 }, { Cutoff, 14000 },
          { Character, 3 }, { Space, .75f }, { Decay, 5 }, { Damping, .3f }, { PreDelay, 35 }, { Shimmer, .22f }, { ReverbMix, .6f },
          { VaultKind, 8 }, { VaultMix, .45f },
          { DelayTime, 310 }, { Feedback, .4f }, { DelayDamping, .3f }, { Diffusion, .6f }, { Spread, .9f }, { DelayMix, .25f } },
        {
          // Halls of living wood: warm wooden resonance and a gentle ensemble.
          { "Leaf Halls", { { Cutoff, 11000 }, { Shimmer, 0 }, { VaultKind, 9 }, { VaultMix, .45f },
                            { Drive, 4 }, { Texture, .2f }, { BloodMix, .2f },
                            { Character, 0 }, { Space, .45f }, { Decay, 1.9f }, { Damping, .6f }, { PreDelay, 12 }, { ReverbMix, .45f },
                            { EnsMix, .4f }, { EnsDepth, 3 }, { EnsRate, .3f }, { DelayMix, 0 } } },
          // Under the starlit canopy: sparkling high harmonics, a wide shimmering sky.
          { "Starlit Canopy", { { VaultKind, 10 }, { VaultMix, .5f }, { VaultShimmer, .1f }, { Character, 3 }, { Space, 1 }, { Decay, 7.5f }, { Damping, .15f }, { PreDelay, 55 },
                                { Shimmer, .3f }, { ReverbMix, .6f }, { DelayMix, .2f }, { DelayTime, 450 } } },
        },
        {
          { "Age", { { Decay, -1, 1 }, { Space, -.3f, .2f }, { Damping, -.2f, .2f } } },
          { "Moonlight", { { Shimmer, -.15f, .3f }, { SigAmount, -.4f, 0 }, { Cutoff, -1, .5f } } },
          { "Leaves", { { EnsMix, -.3f, .3f }, { DelayMix, -.12f, .15f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Crystal Palace", "Prism Hall",
        // Crystalline halls: a bright glassy chamber; light split into colours as an
        // octave shimmer spread across the stereo field (the signature: a spectral prism).
        { { HighPass, 110 }, { Cutoff, 20000 },
          { Character, 2 }, { Space, .75f }, { Decay, 4.2f }, { Damping, .05f }, { PreDelay, 18 }, { Shimmer, .25f }, { ReverbMix, .55f },
          { VaultKind, 11 }, { VaultMix, .5f } },
        {
          // A still lake that mirrors the palace: calm, reflective, a long tail.
          { "Mirror Lake", { { HighPass, 110 }, { Cutoff, 9000 }, { Shimmer, .05f }, { VaultKind, 12 }, { VaultMix, .25f },
                             { Character, 3 }, { Space, 1 }, { Decay, 8 }, { Damping, .45f }, { PreDelay, 120 }, { ReverbMix, .28f } } },
          // The diamond vault: everything rings, bright crystal modes.
          { "Diamond Vault", { { VaultKind, 13 }, { VaultMix, .45f },
                               { Metal, .5f }, { MetalKind, 2 }, { MetalNote, 9 }, { MetalOctave, 3 }, { MetalDecay, 3 },
                               { Space, .45f }, { Decay, 2.8f }, { PreDelay, 6 }, { Shimmer, .25f }, { ReverbMix, .5f } } },
        },
        {
          { "Facets", { { Space, -.3f, .2f }, { Decay, -1, 1 }, { VaultMix, -.15f, .15f } } },
          { "Brilliance", { { Shimmer, -.15f, .3f }, { Metal, -.2f, .25f }, { HighPass, -.6f, .6f } } },
          { "Stillness", { { ReverbMix, -.2f, .2f }, { PreDelay, -15, 40 }, { SigAmount, -.4f, 0 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Blossom Meadow", "Sunlit Meadow",
        // Open, warm and gentle: soft open air in the sun, the instrument warmed (the
        // signature: a sunny saturation), a few soft echoes off the far trees.
        { { HighPass, 45 }, { Cutoff, 13000 }, { Wind, .1f },
          { Drive, 7 }, { Texture, .3f }, { BloodMix, .35f },
          { DelayTime, 340 }, { Feedback, .3f }, { DelayDamping, .5f }, { Diffusion, .5f }, { Spread, .9f }, { DelayMix, .22f },
          { Character, 0 }, { Space, .55f }, { Decay, 2.2f }, { Damping, .5f }, { PreDelay, 18 }, { ReverbMix, .45f },
          { VaultKind, 14 }, { VaultMix, .45f } },
        {
          // A glade full of butterflies: a light flutter moving from side to side.
          { "Butterfly Glade", { { Drive, 2 }, { BloodMix, .1f }, { VaultKind, 15 }, { VaultMix, .45f },
                                 { EnsMix, .35f }, { EnsDepth, 2 }, { EnsRate, .6f },
                                 { Space, .5f }, { Decay, 2.4f }, { ReverbMix, .45f } } },
          // On the hill: long gentle echoes over the valley.
          { "Wildflower Hill", { { Drive, 3 }, { BloodMix, .15f }, { VaultKind, 16 }, { VaultMix, .26f },
                                 { DelayTime, 610 }, { Feedback, .42f }, { DelayDamping, .45f }, { Diffusion, .35f }, { Spread, 1 }, { DelayMix, .22f },
                                 { Character, 3 }, { Space, .6f }, { Decay, 3 }, { ReverbMix, .3f } } },
        },
        {
          { "Sunshine", { { Drive, -3, 5 }, { BloodMix, -.15f, .2f }, { Cutoff, -1, .5f } } },
          { "Breeze", { { Wind, -.1f, .25f }, { EnsMix, -.2f, .25f }, { SigRate, -1, 1 } } },
          { "Distance", { { DelayMix, -.12f, .2f }, { ReverbMix, -.15f, .2f }, { Decay, -1, 1 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Golden Cathedral", "Nave of Light",
        // Majestic and sacred: the longest, most radiant reverb in the world (the
        // signature: a radiant cascade from the golden vault) with long pre-delay.
        { { HighPass, 40 }, { Cutoff, 16000 },
          { Character, 1 }, { Space, 1 }, { Decay, 12 }, { Damping, .25f }, { PreDelay, 90 }, { Shimmer, .25f }, { ReverbWidth, 1 }, { ReverbMix, .36f },
          { VaultKind, 32 }, { VaultMix, .42f }, { VaultShimmer, .3f } },
        {
          // The choir loft: an angelic choir rising out of the playing.
          { "Choir Loft", { { VaultKind, 18 }, { VaultMix, .4f }, { VaultShimmer, 0 }, { Decay, 6 }, { PreDelay, 40 }, { Shimmer, .15f }, { ReverbMix, .45f },
                            { Voices, 3 }, { Interval, 5 }, { ChoirBlend, .2f }, { Formant, .75f }, { ChoirDetune, 14 }, { ChoirMovement, .35f } } },
          // A chapel lit through stained glass: colour-shifting shimmer.
          { "Stained Glass Chapel", { { VaultKind, 19 }, { VaultMix, .5f }, { VaultShimmer, 0 },
                                      { Character, 0 }, { Space, .65f }, { Decay, 4 }, { Damping, .25f }, { PreDelay, 25 }, { Shimmer, .2f }, { ReverbMix, .5f } } },
        },
        {
          { "Vastness", { { Space, -.4f, 0 }, { Decay, -1.3f, .6f }, { PreDelay, -30, 40 }, { VaultMix, -.2f, .15f } } },
          { "Choir", { { ChoirBlend, -.12f, .2f }, { Drone, 0, .15f }, { Voices, 0, 1 } } },
          { "Gold", { { Shimmer, -.15f, .3f }, { VaultShimmer, -.15f, .25f }, { SigAmount, -.4f, 0 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Floating Isles", "Cloud Bridge",
        // Weightless islands in the sky: open, airy, the sound swirling around you on
        // the bridge between the isles (the signature).
        { { HighPass, 140 }, { Cutoff, 15000 }, { Wind, .12f },
          { Character, 3 }, { Space, .75f }, { Decay, 4.5f }, { Damping, .25f }, { PreDelay, 40 }, { ReverbLowCut, 200 }, { ReverbMix, .55f },
          { VaultKind, 20 }, { VaultMix, .38f },
          { DelayTime, 420 }, { Feedback, .4f }, { DelayDamping, .35f }, { Diffusion, .5f }, { Spread, 1 }, { DelayMix, .25f } },
        {
          // A windswept isle: airy, wind-filtered, the space gusting.
          { "Windswept Isle", { { HighPass, 280 }, { Cutoff, 8500 }, { Wind, .4f }, { FilterMovement, .3f }, { FilterRate, .15f },
                                { VaultKind, 21 }, { VaultMix, .25f },
                                { WraithMix, .18f }, { Blur, .6f }, { Ghost, .2f },
                                { Space, .55f }, { Decay, 2.6f }, { ReverbMix, .32f }, { DelayMix, .12f } } },
          // The sky harbour: soaring delays that rise in pitch as they fly away.
          { "Sky Harbour", { { HighPass, 90 }, { VaultKind, 22 }, { VaultMix, .38f },
                             { TapeMix, .55f }, { TapeTime, 460 }, { TapeFeedback, .6f }, { TapeTone, 9000 }, { TapeWow, .1f }, { TapeAscend, 2 },
                             { DelayMix, 0 }, { Decay, 4 }, { ReverbMix, .45f } } },
        },
        {
          { "Altitude", { { HighPass, -.6f, .8f }, { Space, -.3f, .2f }, { Decay, -1, 1 } } },
          { "Wind", { { Wind, -.1f, .3f }, { FilterMovement, -.2f, .3f }, { SigRate, -1, 1 } } },
          { "Flight", { { DelayMix, -.12f, .2f }, { TapeFeedback, -.2f, .15f }, { SigAmount, -.4f, 0 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Royal Gardens", "Rose Garden",
        // Elegant gardens of the palace: a soft lush chorus among the roses (the
        // signature) in a gentle open garden air.
        { { HighPass, 55 }, { Cutoff, 14000 },
          { Character, 0 }, { Space, .6f }, { Decay, 2.8f }, { Damping, .35f }, { PreDelay, 20 }, { ReverbMix, .45f },
          { VaultKind, 34 }, { VaultMix, .5f },
          { EnsMix, .45f }, { EnsDepth, 4 }, { EnsRate, .35f } },
        {
          // The fountain court: sparkling, water-like modulation in a bright court.
          { "Fountain Court", { { EnsMix, 0 }, { VaultKind, 24 }, { VaultMix, .45f }, { FilterMovement, .25f }, { FilterRate, 1.4f }, { Resonance, .15f },
                                { Drops, .2f }, { Space, .5f }, { Decay, 2.6f }, { Damping, .15f }, { ReverbMix, .45f } } },
          // A colonnade of marble: rhythmic slap echoes between the columns.
          { "Marble Colonnade", { { EnsMix, 0 }, { VaultKind, 25 }, { VaultMix, .42f },
                                  { Character, 1 }, { Space, .65f }, { Decay, 2.6f }, { Damping, .12f }, { PreDelay, 12 }, { ReverbMix, .45f } } },
        },
        {
          { "Bloom", { { EnsMix, -.2f, .3f }, { EnsDepth, -.5f, .5f }, { Decay, -1, .6f } } },
          { "Sparkle", { { FilterMovement, -.15f, .3f }, { Shimmer, 0, .25f }, { SigAmount, -.4f, 0 } } },
          { "Elegance", { { Space, -.3f, .2f }, { ReverbMix, -.15f, .2f }, { Damping, .2f, -.2f } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Fairy Glade", "Mushroom Ring",
        // Playful magic in a hidden glade: a small, bright, sparkly space; inside the
        // mushroom ring every note bounces up in pitch (the signature).
        { { HighPass, 95 }, { Cutoff, 18000 },
          { Character, 2 }, { Space, .4f }, { Decay, 2.2f }, { Damping, .08f }, { PreDelay, 8 }, { Shimmer, .22f }, { ReverbMix, .5f },
          { VaultKind, 26 }, { VaultMix, .42f },
          { DelayTime, 190 }, { DelaySync, 3 }, { Feedback, .42f }, { DelayDamping, .2f }, { Spread, 1 }, { DelayMix, .25f } },
        {
          // A hollow full of fireflies: twinkling granular glitter.
          { "Firefly Hollow", { { VaultKind, 27 }, { VaultMix, .45f }, { DelayMix, .1f },
                                { WraithMix, .25f }, { Blur, .45f }, { Ghost, .3f },
                                { Space, .6f }, { Decay, 3.6f }, { Shimmer, .3f }, { ReverbMix, .5f } } },
          // The fairy court: a tiny, bright room and the ring of a harp.
          { "Fairy Court", { { VaultKind, 28 }, { VaultMix, .45f }, { DelayMix, 0 }, { Shimmer, .1f },
                             { Character, 0 }, { Space, .18f }, { Decay, 1.1f }, { Damping, .05f }, { PreDelay, 3 }, { ReverbMix, .55f } } },
        },
        {
          { "Magic", { { Shimmer, -.15f, .3f }, { SigAmount, -.4f, 0 }, { WraithMix, 0, .15f } } },
          { "Bounce", { { DelayMix, -.15f, .2f }, { Feedback, -.2f, .2f } } },
          { "Size", { { Space, -.25f, .3f }, { Decay, -1, 1 } } },
        } });

    //--------------------------------------------------------------------------
    w.push_back({ "Dragon's Peak", "Eyrie",
        // Epic and heroic: the noble dragon's mountain. At the eyrie, vast open echoes
        // thrown back off the cliffs (the signature) over a huge open sky.
        { { HighPass, 50 }, { Cutoff, 15000 },
          { Character, 3 }, { Space, .9f }, { Decay, 4.5f }, { Damping, .3f }, { PreDelay, 50 }, { ReverbMix, .3f },
          { VaultKind, 29 }, { VaultMix, .32f },
          { DelayTime, 560 }, { Feedback, .36f }, { DelayDamping, .35f }, { Diffusion, .3f }, { Spread, 1 }, { DelayMix, .18f } },
        {
          // The golden nest: warm golden harmonic richness.
          { "Golden Nest", { { VaultKind, 30 }, { VaultMix, .45f }, { DelayMix, 0 },
                             { Drive, 8 }, { Texture, .35f }, { BloodMix, .4f },
                             { Character, 1 }, { Space, .6f }, { Decay, 3.5f }, { Damping, .45f }, { PreDelay, 20 }, { ReverbMix, .5f },
                             { EnsMix, .3f } } },
          // The summit of winds: a huge heroic wide reverb with a soaring octave above.
          { "Summit of Winds", { { VaultKind, 31 }, { VaultMix, .36f }, { VaultShimmer, .35f }, { Wind, .12f },
                                 { Voices, 1 }, { Interval, 4 }, { ChoirBlend, .12f }, { Formant, .6f },
                                 { Space, 1 }, { Decay, 9 }, { Damping, .2f }, { PreDelay, 70 }, { Shimmer, .4f }, { ReverbWidth, 1 }, { ReverbMix, .36f },
                                 { DelayMix, .15f }, { DelayTime, 720 } } },
        },
        {
          { "Heroism", { { Shimmer, -.15f, .3f }, { VaultShimmer, -.15f, .25f }, { SigAmount, -.4f, 0 } } },
          { "Cliffs", { { DelayMix, -.15f, .2f }, { Feedback, -.2f, .2f }, { DelayTime, -.5f, .5f } } },
          { "Gold", { { Drive, -3, 6 }, { BloodMix, -.15f, .2f } } },
          { "Sky", { { Space, -.35f, .1f }, { Decay, -1, 1 }, { ReverbMix, -.15f, .15f } } },
        } });
    return w;
}

const std::vector<LocationDesign> world = buildWorld();

// Every place's signature (Signature.cpp), one kind per place and never shared, with
// its amount. Kinds are numbered in place order (Celestial Citadel, Gates of Dawn = 1
// ... Dragon's Peak, Summit of Winds = 31).
struct SignatureSetting { float amount; };
constexpr std::array<std::array<SignatureSetting, placesPerLocation>, LocationCount> signatureAmounts {{
    {{ { 1 }, { 1 }, { 1 }, { 1 } }},   // Celestial Citadel: Gates of Dawn, Hall of Mirrors, Throne of Light, Spire Summit
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Sky Temple: Cloud Steps, Sanctum, Bell Garden
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Elven Sanctuary: Moonwell, Leaf Halls, Starlit Canopy
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Crystal Palace: Prism Hall, Mirror Lake, Diamond Vault
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Blossom Meadow: Sunlit Meadow, Butterfly Glade, Wildflower Hill
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Golden Cathedral: Nave of Light, Choir Loft, Stained Glass Chapel
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Floating Isles: Cloud Bridge, Windswept Isle, Sky Harbour
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Royal Gardens: Rose Garden, Fountain Court, Marble Colonnade
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Fairy Glade: Mushroom Ring, Firefly Hollow, Fairy Court
    {{ { 1 }, { 1 }, { 1 }, { 0 } }},   // Dragon's Peak: Eyrie, Golden Nest, Summit of Winds
}};
int firstSignature(int location) noexcept {
    int kind = 1;
    for (int l = 0; l < location; ++l) kind += 1 + static_cast<int>(world[static_cast<std::size_t>(l)].subs.size());
    return kind;
}

float clampToSpec(P id, float v) noexcept { return std::clamp(v, moduleSpecs[id].min, moduleSpecs[id].max); }
}

int signatureKind(int location, int sub) noexcept {
    location = std::clamp(location, 0, static_cast<int>(LocationCount) - 1);
    return firstSignature(location) + clampSubLocation(location, sub);
}

const LocationDesign& design(int location) noexcept {
    return world[static_cast<std::size_t>(std::clamp(location, 0, static_cast<int>(LocationCount) - 1))];
}

int subLocationCount(int location) noexcept { return static_cast<int>(design(location).subs.size()); }
int clampSubLocation(int location, int sub) noexcept { return std::clamp(sub, 0, subLocationCount(location)); }

const char* subLocationName(int location, int sub) noexcept {
    const auto& d = design(location);
    sub = clampSubLocation(location, sub);
    return sub == 0 ? d.scene : d.subs[static_cast<std::size_t>(sub - 1)].name;
}

int macroCountFor(int location) noexcept { return static_cast<int>(design(location).macros.size()); }
const char* macroName(int location, int macro) noexcept {
    const auto& d = design(location);
    return macro >= 0 && macro < static_cast<int>(d.macros.size()) ? d.macros[static_cast<std::size_t>(macro)].name : "";
}

Frame placeFrame(int location, int sub) noexcept {
    const auto& d = design(location);
    auto f = neutralFrame();
    for (const auto& c : d.base) f[c.id] = clampToSpec(c.id, c.value);
    sub = clampSubLocation(location, sub);
    if (sub > 0)
        for (const auto& c : d.subs[static_cast<std::size_t>(sub - 1)].changes) f[c.id] = clampToSpec(c.id, c.value);
    location = std::clamp(location, 0, static_cast<int>(LocationCount) - 1);
    f[SigKind] = static_cast<float>(firstSignature(location) + sub);
    f[SigAmount] = signatureAmounts[static_cast<std::size_t>(location)][static_cast<std::size_t>(sub)].amount;
    return f;
}

void applyMacros(Frame& f, int location, const std::array<float, macroCount>& values) noexcept {
    const auto& d = design(location);
    for (std::size_t m = 0; m < d.macros.size() && m < values.size(); ++m) {
        const float v = std::clamp(values[m], 0.0f, 1.0f);
        if (std::abs(v - 0.5f) < 1.0e-6f) continue;
        const float down = v < 0.5f ? (0.5f - v) * 2.0f : 0.0f, up = v > 0.5f ? (v - 0.5f) * 2.0f : 0.0f;
        for (const auto& t : d.macros[m].targets) {
            const float offset = t.down * down + t.up * up;
            const auto& spec = moduleSpecs[t.id];
            float value = f[t.id];
            if (spec.glide == Glide::Log) value *= std::exp2(offset);
            else if (spec.glide == Glide::Step) value = std::round(value + offset);
            else value += offset;
            f[t.id] = clampToSpec(t.id, value);
        }
    }
}

float placeTrimDb(int location, int sub) noexcept {
    location = std::clamp(location, 0, static_cast<int>(LocationCount) - 1);
    sub = clampSubLocation(location, sub);
    return levelTrimsDb[static_cast<std::size_t>(location)][static_cast<std::size_t>(sub)];
}
}
