// The 31 instruments (Direction Update 1, section 3): one per sub-location, each location
// a family. Uplifting, radiant, majestic; each distinct from its siblings; good dry and in
// its own space. The developer's recordings (AdrianAbbas/Samples) play where they fit:
// the classical guitar (Sunlit Meadow, Leaf Halls), the kalimba (Butterfly Glade), the
// violin's pizzicato (Mushroom Ring) and sustain (Golden Nest), the Minilogue's soft pad
// (Cloud Bridge). Each of those has a synthesised stand-in for builds without them.
// trimDb comes from `LightSeekerTests --calibrate-instruments` (a held chord's loudness).
#include "Synth.h"
#include "InstrumentTrims.h"
#include "World/Locations.h"
#include <algorithm>

namespace ls::inst {
using namespace lswb::dsp;

namespace {
Layer layer(Engine e, float level = 0.0f) {
    Layer l;
    l.engine = e;
    l.level = level;
    return l;
}

Layer brass(float formantHz, float bright, float level = 0.0f) {
    auto l = layer(Engine::Brass, level);
    l.p = { formantHz, 0.7f, 0.035f, 0.0f, 0.0f, 0.0f };
    l.bright = bright;
    l.attack = 0.02f;
    l.decay = 0.4f;
    l.sustain = 0.85f;
    l.release = 0.35f;
    l.velocityGain = 0.8f;
    l.velocityTone = 0.0f;   // the brass model brightens with velocity itself
    return l;
}

Layer strings(int size, int players, float bright, float level = 0.0f) {
    auto l = layer(Engine::Strings, level);
    l.model = size;
    l.p = { float(players), 9.0f, 0.12f, 0.22f, 0.0f, 0.0f };
    l.bright = bright;
    l.attack = 0.16f;
    l.release = 0.8f;
    l.vibrato = 0.13f;
    l.vibratoDelay = 0.3f;
    l.velocityGain = 0.45f;
    l.velocityTone = 0.4f;
    return l;
}

Layer choir(float vowel, float bright, float level = 0.0f) {
    auto l = layer(Engine::Choir, level);
    l.p = { vowel, 0.08f, 9.0f, 0.0f, 0.0f, 0.0f };
    l.bright = bright;
    l.attack = 0.3f;
    l.release = 1.0f;
    l.vibrato = 0.11f;
    l.vibratoHz = 5.1f;
    l.velocityGain = 0.4f;
    l.velocityTone = 0.3f;
    return l;
}

Layer pluck(int body, float position, float loss, float ring, float bright, float level = 0.0f) {
    auto l = layer(Engine::Pluck, level);
    l.model = body;
    l.p = { position, loss, 0.08f, 0.0f, 0.2f, 0.0f };
    l.ring = ring;
    l.bright = bright;
    l.release = 1.5f;
    l.velocityGain = 0.7f;
    l.velocityTone = 0.6f;
    return l;
}

Layer bell(int modes, float hardness, float ring, float level = 0.0f) {
    auto l = layer(Engine::Bell, level);
    l.model = modes;
    l.p = { hardness, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f };
    l.ring = ring;
    l.release = 2.0f;
    l.velocityGain = 0.75f;
    l.velocityTone = 0.5f;
    return l;
}

Layer wind(float h2, float h3, float h4, float h5, float breath, float chiff, float level = 0.0f) {
    auto l = layer(Engine::Wind, level);
    l.p = { h2, h3, h4, h5, breath, chiff };
    l.attack = 0.05f;
    l.release = 0.3f;
    l.vibrato = 0.13f;
    l.vibratoHz = 5.2f;
    l.vibratoDelay = 0.3f;
    l.velocityGain = 0.5f;
    l.velocityTone = 0.5f;
    return l;
}

Layer sample(const char* id, float level = 0.0f) {
    auto l = layer(Engine::Sample, level);
    l.needs = Needs::Samples;
    l.sample = id;
    l.p = { 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f };
    l.attack = 0.0f;
    l.release = 0.4f;
    l.velocityGain = 0.35f;   // the recorded layers already differ
    l.velocityTone = 0.2f;
    return l;
}

Layer standIn(Layer l) {
    l.needs = Needs::NoSamples;
    return l;
}

Layer character(Layer l) {
    l.character = true;
    return l;
}

Layer shift(Layer l, float semis, float cents = 0.0f, float pan = 0.0f) {
    l.transpose = semis;
    l.cents = cents;
    l.pan = pan;
    return l;
}

struct Builder {
    Patch p;
    Builder(const char* name, const char* family, const char* characterName) {
        p.name = name;
        p.family = family;
        p.character = characterName;
    }
    Builder& add(const Layer& l) {
        if (p.layerCount < int(p.layers.size())) p.layers[std::size_t(p.layerCount++)] = l;
        return *this;
    }
    Builder& air(float db) { p.air = db; return *this; }
    Builder& chorus(float c) { p.chorus = c; return *this; }
    Builder& shimmer(float s) { p.shimmer = s; return *this; }
    Builder& mode(Character c) { p.characterMode = c; return *this; }
    Builder& poly(int n) { p.polyphony = n; return *this; }
    Builder& struck(float modDepth = 0.5f) { p.tremoloMod = true; p.modDepth = modDepth; p.sustaining = false; return *this; }
    Builder& trim(float db) { p.trimDb = db; return *this; }
};

std::array<Patch, instrumentCount> build() {
    std::array<Patch, instrumentCount> all {};
    int at = 0;
    const auto put = [&](Builder& b) { all[std::size_t(at++)] = b.p; };

    // ---- Celestial Citadel: radiant brass and strings -----------------------------------
    {   // Gates of Dawn: bright fanfare trumpets (a section of two, a piccolo above).
        auto a = brass(1650.0f, 0.88f);
        a.p[3] = 6.0f;
        a.pan = -0.2f;
        a.attack = 0.012f;
        a.lowpass = 14000;
        auto b = shift(brass(1600.0f, 0.82f, -4.0f), 0.0f, 9.0f, 0.3f);
        b.attack = 0.016f;
        auto c = character(shift(brass(1700.0f, 0.7f, -9.0f), 12.0f, -4.0f, 0.1f));
        Builder p("Fanfare Trumpets", "Radiant brass and strings", "Piccolo");
        p.add(a).add(b).add(c).air(3.0f).poly(12).trim(0.0f);
        put(p);
    }
    {   // Hall of Mirrors: a shimmering string ensemble (and its octave above).
        auto a = strings(Violin, 3, 0.78f);
        a.p[1] = 11.0f;
        a.attack = 0.1f;
        auto b = shift(strings(Viola, 3, 0.55f, -4.0f), 0.0f, 0.0f, -0.25f);
        b.highKey = 76;
        auto c = character(shift(strings(Violin, 3, 0.7f, -10.0f), 12.0f, 3.0f, 0.3f));
        c.attack = 0.35f;
        Builder p("Shimmering Strings", "Radiant brass and strings", "Shimmer");
        p.add(a).add(b).add(c).air(3.0f).chorus(0.22f).trim(0.0f);
        put(p);
    }
    {   // Throne of Light: a majestic orchestral chord pad (strings, horns, voices).
        auto a = strings(Viola, 3, 0.38f);
        a.attack = 0.8f;
        a.release = 1.8f;
        a.lowpass = 7000.0f;
        auto b = brass(850.0f, 0.3f, -7.0f);
        b.attack = 0.85f;
        b.release = 1.6f;
        b.sustain = 1.0f;
        b.p[3] = 7.0f;
        auto c = character(choir(0.8f, 0.4f, -3.0f));
        c.attack = 0.9f;
        c.release = 1.8f;
        auto d = shift(strings(Cello, 2, 0.4f, -7.0f), -12.0f);
        d.attack = 0.6f;
        d.release = 1.3f;
        d.lowKey = 24;
        Builder p("Orchestral Chord Pad", "Radiant brass and strings", "Voices");
        p.add(a).add(b).add(c).add(d).air(2.0f).chorus(0.15f).trim(0.0f);
        put(p);
    }
    {   // Spire Summit: a soaring solo horn.
        auto a = brass(480.0f, 0.22f);
        a.p[1] = 1.0f;
        a.p[2] = 0.05f;
        a.attack = 0.09f;
        a.release = 0.6f;
        a.vibrato = 0.07f;
        a.vibratoDelay = 0.45f;
        a.vibratoHz = 5.0f;
        a.lowpass = 3200;
        auto b = character(shift(brass(470.0f, 0.2f, -7.0f), 0.0f, 7.0f, 0.35f));
        b.attack = 0.11f;
        b.release = 0.6f;
        b.lowpass = 3200;
        Builder p("Soaring Horn", "Radiant brass and strings", "Second horn");
        p.add(a).add(b).air(1.5f).poly(10).trim(0.0f);
        put(p);
    }

    // ---- Sky Temple: bells and voices --------------------------------------------------
    {   // Cloud Steps: a harp whose every note climbs on in a rising glissando.
        auto a = pluck(BodyHarp, 0.3f, 0.45f, 3.5f, 0.68f);
        a.run = 4;
        a.runMs = 42.0f;
        a.release = 1.2f;
        a.keyPan = 0.35f;
        Builder p("Rising Harp Glissando", "Bells and voices", "Glissando");
        p.add(a).air(3.0f).chorus(0.2f).struck().trim(0.0f);
        put(p);
    }
    {   // Sanctum: an angelic choir, soft "oo", a descant above.
        auto a = choir(0.6f, 0.35f);
        a.p[1] = 0.12f;
        a.attack = 0.45f;
        a.release = 1.0f;
        a.vibrato = 0.1f;
        auto b = character(shift(choir(1.0f, 0.3f, -9.0f), 12.0f, 4.0f));
        b.attack = 0.6f;
        b.release = 1.2f;
        Builder p("Angelic Choir", "Bells and voices", "Descant");
        p.add(a).add(b).air(2.5f).chorus(0.18f).trim(0.0f);
        put(p);
    }
    {   // Bell Garden: tuned bells with wind chimes above them.
        auto a = bell(ModesBell, 0.6f, 8.0f);
        a.p[1] = 0.8f;
        a.p[2] = 0.15f;
        a.release = 5.0f;
        auto b = character(shift(bell(ModesChime, 0.75f, 6.0f, -9.0f), 12.0f, 0.0f, 0.3f));
        b.keyPan = 0.5f;
        b.release = 5.0f;
        Builder p("Wind Chimes and Bells", "Bells and voices", "Chimes");
        p.add(a).add(b).air(2.0f).struck().trim(0.0f);
        put(p);
    }

    // ---- Elven Sanctuary: harps and flutes ---------------------------------------------
    {   // Moonwell: a glass harp (rubbed wine glasses), a soft tap as each glass speaks.
        auto a = layer(Engine::Bowed);
        a.model = ModesGlass;
        a.p = { 0.3f, 0.22f, 0, 0, 0, 0 };
        a.ring = 0.6f;
        a.attack = 0.16f;
        a.release = 0.9f;
        a.vibrato = 0.03f;
        a.vibratoHz = 4.5f;
        a.velocityGain = 0.4f;
        auto b = bell(ModesGlass, 0.25f, 1.6f, -12.0f);
        auto c = character(shift(a, 12.0f, 5.0f, 0.25f));
        c.level = -11.0f;
        Builder p("Glass Harp", "Harps and flutes", "Overtone");
        p.add(a).add(b).add(c).air(2.0f).chorus(0.12f).trim(0.0f);
        put(p);
    }
    {   // Leaf Halls: an elven lyre (the classical guitar recordings, bright), an octave shimmer.
        auto a = sample("guitar");
        a.highpass = 170.0f;
        a.lowpass = 16000.0f;
        a.p[1] = 3.5f;
        a.release = 1.2f;
        auto b = standIn(pluck(BodyLute, 0.16f, 0.35f, 3.5f, 0.75f));
        b.release = 1.2f;
        auto c = character(shift(pluck(BodyNone, 0.25f, 0.3f, 2.5f, 0.8f, -13.0f), 12.0f, 3.0f, 0.2f));
        Builder p("Elven Lyre", "Harps and flutes", "Shimmer");
        p.add(a).add(b).add(c).air(4.0f).chorus(0.15f).struck().trim(0.0f);
        put(p);
    }
    {   // Starlit Canopy: an airy flute (a piccolo above).
        auto a = wind(0.25f, 0.1f, 0.03f, 0.0f, 0.18f, 0.3f);
        a.vibrato = 0.14f;
        auto b = character(shift(wind(0.15f, 0.05f, 0.0f, 0.0f, 0.15f, 0.3f, -9.0f), 12.0f, 3.0f, 0.2f));
        Builder p("Airy Flute", "Harps and flutes", "Piccolo");
        p.add(a).add(b).air(2.5f).chorus(0.1f).poly(10).trim(0.0f);
        put(p);
    }

    // ---- Crystal Palace: crystal --------------------------------------------------------
    {   // Prism Hall: crystal bells (an octave of glass above), with a rising shimmer.
        auto a = bell(ModesCrystal, 0.8f, 4.0f);
        a.p[1] = 1.2f;
        a.p[2] = 0.12f;
        auto b = shift(bell(ModesCrystal, 0.9f, 2.5f, -9.0f), 12.0f, 4.0f, 0.25f);
        b.keyPan = 0.45f;
        Builder p("Crystal Bells", "Crystal", "Shimmer");
        p.add(a).add(b).air(3.0f).shimmer(0.35f).mode(Character::Shimmer).struck().trim(0.0f);
        put(p);
    }
    {   // Mirror Lake: a glass harmonica pad.
        auto a = layer(Engine::Bowed);
        a.model = ModesGlassHarp;
        a.p = { 0.25f, 0.12f, 0, 0, 0, 0 };
        a.ring = 0.5f;
        a.attack = 0.6f;
        a.release = 1.6f;
        a.velocityGain = 0.3f;
        auto b = layer(Engine::Fm, -6.0f);
        b.p = { 1.0f, 0.35f, 2.0f, 0.4f, 0.0f, 0.0f };
        b.attack = 0.8f;
        b.release = 1.8f;
        b.velocityGain = 0.3f;
        auto c = character(shift(a, 12.0f, 4.0f, 0.3f));
        c.level = -12.0f;
        c.attack = 1.0f;
        Builder p("Glass Harmonica Pad", "Crystal", "Overtone");
        p.add(a).add(b).add(c).air(2.0f).chorus(0.3f).trim(0.0f);
        put(p);
    }
    {   // Diamond Vault: a celesta (a tine's sparkle on top).
        auto a = bell(ModesCelesta, 0.6f, 2.2f);
        a.release = 0.45f;
        auto b = layer(Engine::Tine, -12.0f);
        b.ring = 1.6f;
        b.bright = 0.7f;
        b.release = 0.4f;
        auto c = character(shift(bell(ModesCelesta, 0.65f, 1.6f, -12.0f), 12.0f, 0.0f, 0.2f));
        c.release = 0.45f;
        Builder p("Celesta", "Crystal", "Sparkle");
        p.add(a).add(b).add(c).air(2.5f).struck().trim(0.0f);
        put(p);
    }

    // ---- Blossom Meadow: pastoral -------------------------------------------------------
    {   // Sunlit Meadow: a warm acoustic guitar (the developer's classical guitar).
        auto a = sample("guitar");
        a.lowpass = 7500.0f;
        a.highpass = 55.0f;
        a.release = 0.5f;
        auto b = standIn(pluck(BodyGuitar, 0.18f, 0.5f, 4.0f, 0.45f));
        b.release = 0.5f;
        auto c = character(shift(pluck(BodyGuitar, 0.2f, 0.45f, 3.0f, 0.5f, -12.0f), 12.0f, 4.0f, 0.25f));
        c.release = 0.5f;
        Builder p("Warm Acoustic Guitar", "Pastoral", "Octave strings");
        p.add(a).add(b).add(c).air(0.5f).struck().trim(0.0f);
        put(p);
    }
    {   // Butterfly Glade: a bouncy kalimba (the developer's kalimba).
        auto a = sample("kalimba");
        a.highpass = 120.0f;
        a.release = 0.6f;
        auto b = standIn(layer(Engine::Tine));
        b.ring = 2.0f;
        b.bright = 0.6f;
        b.release = 0.6f;
        auto c = character(shift(bell(ModesHarmonic, 0.6f, 1.2f, -14.0f), 12.0f, 0.0f, 0.3f));
        c.release = 0.6f;
        Builder p("Bouncy Kalimba", "Pastoral", "Sparkle");
        p.add(a).add(b).add(c).air(2.0f).struck().trim(0.0f);
        put(p);
    }
    {   // Wildflower Hill: a pastoral oboe (a recorder an octave above).
        auto a = wind(0.7f, 0.55f, 0.4f, 0.3f, 0.06f, 0.1f);
        a.lowpass = 7000.0f;
        a.vibrato = 0.12f;
        a.vibratoHz = 5.4f;
        a.attack = 0.06f;
        auto b = character(shift(wind(0.12f, 0.06f, 0.0f, 0.0f, 0.2f, 0.4f, -8.0f), 12.0f, 0.0f, 0.2f));
        b.vibrato = 0.05f;
        Builder p("Pastoral Oboe", "Pastoral", "Recorder");
        p.add(a).add(b).air(1.0f).poly(10).trim(0.0f);
        put(p);
    }

    // ---- Golden Cathedral: sacred, golden -----------------------------------------------
    {   // Nave of Light: a bright grand organ (a trumpet stop as the character).
        auto a = layer(Engine::Organ);
        a.p = { 0.5f, 1.0f, 0.7f, 0.35f, 0.45f, 0.3f };
        a.bright = 0.35f;
        a.attack = 0.05f;
        a.release = 0.25f;
        a.velocityGain = 0.0f;
        a.velocityTone = 0.0f;
        auto b = character(layer(Engine::Reed, -9.0f));
        b.p = { 3500.0f, 1400.0f, 0.02f, 4.0f, 0, 0 };
        b.attack = 0.05f;
        b.release = 0.25f;
        b.velocityGain = 0.0f;
        b.velocityTone = 0.0f;
        Builder p("Grand Organ", "Sacred, golden", "Trumpet stop");
        p.add(a).add(b).air(2.0f).trim(0.0f);
        put(p);
    }
    {   // Choir Loft: a radiant choir, bright "ah", basses below, sopranos above.
        auto a = choir(2.2f, 0.7f);
        a.p[1] = 0.07f;
        a.p[2] = 11.0f;
        a.attack = 0.15f;
        a.release = 0.7f;
        a.vibrato = 0.14f;
        auto b = shift(choir(2.0f, 0.55f, -7.0f), -12.0f, 0.0f, -0.25f);
        b.attack = 0.18f;
        auto c = character(shift(choir(3.0f, 0.65f, -10.0f), 12.0f, 3.0f, 0.25f));
        c.attack = 0.2f;
        Builder p("Radiant Choir", "Sacred, golden", "Sopranos");
        p.add(a).add(b).add(c).air(3.0f).chorus(0.12f).trim(0.0f);
        put(p);
    }
    {   // Stained Glass Chapel: celesta and harp together.
        auto a = bell(ModesCelesta, 0.55f, 2.4f);
        a.release = 0.8f;
        auto b = character(pluck(BodyHarp, 0.25f, 0.5f, 3.5f, 0.5f, -3.0f));
        b.release = 1.2f;
        Builder p("Celesta and Harp", "Sacred, golden", "Harp");
        p.add(a).add(b).air(2.5f).chorus(0.15f).struck().trim(0.0f);
        put(p);
    }

    // ---- Floating Isles: airy -----------------------------------------------------------
    {   // Cloud Bridge: a soft cloud pad (the developer's Minilogue soft pad).
        auto a = sample("mlg-softpad-long");
        a.attack = 0.25f;
        a.release = 1.4f;
        a.lowpass = 9000.0f;
        a.velocityGain = 0.3f;
        auto b = standIn(layer(Engine::Analog));
        b.p = { 1400.0f, 0.5f, 1.5f, 11.0f, 0.15f, 0.9f };
        b.attack = 0.5f;
        b.release = 1.6f;
        b.bright = 0.4f;
        b.velocityGain = 0.3f;
        auto c = character(shift(layer(Engine::Analog, -12.0f), 12.0f, 6.0f, 0.3f));
        c.p = { 2600.0f, 0.3f, 2.0f, 14.0f, 0.0f, 1.0f };
        c.attack = 0.9f;
        c.release = 1.8f;
        c.bright = 0.3f;
        Builder p("Cloud Pad", "Airy", "Haze");
        p.add(a).add(b).add(c).air(1.5f).chorus(0.4f).trim(0.0f);
        put(p);
    }
    {   // Windswept Isle: a breathy wind flute.
        auto a = wind(0.15f, 0.05f, 0.0f, 0.0f, 0.55f, 0.5f);
        a.attack = 0.12f;
        a.release = 0.35f;
        a.vibrato = 0.1f;
        a.vibratoHz = 4.6f;
        a.lowpass = 6000.0f;
        auto b = character(shift(wind(0.1f, 0.03f, 0.0f, 0.0f, 0.7f, 0.4f, -12.0f), 12.0f, 5.0f, 0.3f));
        b.attack = 0.2f;
        Builder p("Breathy Wind Flute", "Airy", "Wind");
        p.add(a).add(b).air(1.0f).poly(10).trim(0.0f);
        put(p);
    }
    {   // Sky Harbour: a shimmering pluck.
        auto a = layer(Engine::Analog);
        a.p = { 500.0f, 4.5f, 0.22f, 9.0f, 0.15f, 0.9f };
        a.attack = 0.002f;
        a.decay = 1.1f;
        a.sustain = 0.0f;
        a.release = 0.5f;
        a.bright = 0.3f;
        a.velocityTone = 0.6f;
        auto b = shift(layer(Engine::Fm, -10.0f), 12.0f, 0.0f, 0.2f);
        b.p = { 2.0f, 1.2f, 0.35f, 0.0f, 0.0f, 0.0f };
        b.attack = 0.002f;
        b.decay = 0.8f;
        b.sustain = 0.0f;
        b.release = 0.4f;
        Builder p("Shimmering Pluck", "Airy", "Shimmer");
        p.add(a).add(b).air(3.0f).chorus(0.3f).shimmer(0.3f).mode(Character::Shimmer).struck().trim(0.0f);
        put(p);
    }

    // ---- Royal Gardens: courtly ---------------------------------------------------------
    {   // Rose Garden: a concert harp (its octave harmonics as the character).
        auto a = pluck(BodyHarp, 0.22f, 0.55f, 5.0f, 0.45f);
        a.release = 2.5f;
        auto b = character(shift(pluck(BodyHarp, 0.5f, 0.4f, 3.0f, 0.4f, -12.0f), 12.0f, 0.0f, 0.15f));
        b.release = 2.0f;
        Builder p("Concert Harp", "Courtly", "Harmonics");
        p.add(a).add(b).air(1.5f).struck().trim(0.0f);
        put(p);
    }
    {   // Fountain Court: a music box (celesta bars over it).
        auto a = shift(layer(Engine::Tine), 12.0f);
        a.ring = 2.5f;
        a.bright = 0.85f;
        a.release = 1.0f;
        a.velocityGain = 0.5f;
        auto b = character(shift(bell(ModesCelesta, 0.7f, 1.8f, -10.0f), 12.0f, 3.0f, 0.2f));
        b.release = 1.0f;
        Builder p("Music Box", "Courtly", "Celesta");
        p.add(a).add(b).air(2.0f).struck().trim(0.0f);
        put(p);
    }
    {   // Marble Colonnade: a harpsichord (two 8' choirs; a 4' as the character).
        auto a = pluck(BodyHarpsichord, 0.12f, 0.22f, 6.0f, 0.9f);
        a.p[2] = 0.2f;
        a.p[3] = 0.9f;
        a.p[4] = 0.5f;
        a.release = 0.25f;
        a.velocityGain = 0.15f;
        a.velocityTone = 0.1f;
        auto b = character(shift(pluck(BodyHarpsichord, 0.1f, 0.25f, 4.0f, 0.9f, -6.0f), 12.0f));
        b.release = 0.2f;
        b.velocityGain = 0.15f;
        Builder p("Harpsichord", "Courtly", "4' stop");
        p.add(a).add(b).air(1.0f).struck().trim(0.0f);
        put(p);
    }

    // ---- Fairy Glade: twinkly -----------------------------------------------------------
    {   // Mushroom Ring: bouncy pizzicato (the developer's violin pizzicato).
        auto a = sample("violin-pizz");
        a.highpass = 90.0f;
        a.release = 0.3f;
        auto b = standIn(pluck(BodyNone, 0.2f, 0.65f, 0.6f, 0.5f));
        b.release = 0.25f;
        auto c = character(shift(pluck(BodyNone, 0.2f, 0.55f, 0.5f, 0.6f, -10.0f), 12.0f, 0.0f, 0.3f));
        c.release = 0.25f;
        Builder p("Bouncy Pizzicato", "Twinkly", "Bounce");
        p.add(a).add(b).add(c).air(2.5f).struck().trim(0.0f);
        put(p);
    }
    {   // Firefly Hollow: a twinkling glockenspiel.
        auto a = shift(bell(ModesGlock, 0.95f, 3.5f), 12.0f);
        a.release = 3.0f;
        Builder p("Twinkling Glockenspiel", "Twinkly", "Twinkle");
        p.add(a).air(1.0f).shimmer(0.25f).mode(Character::Shimmer).struck().trim(0.0f);
        put(p);
    }
    {   // Fairy Court: a tiny fairy harp.
        auto a = shift(pluck(BodyNone, 0.25f, 0.45f, 1.2f, 0.6f), 12.0f);
        a.release = 0.6f;
        auto b = shift(bell(ModesHarmonic, 0.7f, 1.0f, -14.0f), 12.0f, 0.0f, 0.3f);
        b.release = 1.0f;
        Builder p("Fairy Harp", "Twinkly", "Magic");
        p.add(a).add(b).air(2.5f).shimmer(0.3f).mode(Character::Shimmer).struck().trim(0.0f);
        put(p);
    }

    // ---- Dragon's Peak: epic, heroic ----------------------------------------------------
    {   // Eyrie: heroic horns (a section of three, an octave below as the character).
        auto a = shift(brass(800.0f, 0.62f), 0.0f, -4.0f, -0.35f);
        a.p[3] = 5.0f;
        a.attack = 0.03f;
        a.release = 0.3f;
        a.lowpass = 9000.0f;
        auto b = shift(brass(780.0f, 0.62f, -2.0f), 0.0f, 7.0f, 0.35f);
        b.attack = 0.035f;
        b.release = 0.3f;
        b.lowpass = 9000.0f;
        auto c = character(shift(brass(600.0f, 0.45f, -6.0f), -12.0f));
        c.attack = 0.04f;
        c.lowKey = 24;
        Builder p("Heroic Horns", "Epic, heroic", "Depth");
        p.add(a).add(b).add(c).air(2.0f).poly(12).trim(0.0f);
        put(p);
    }
    {   // Golden Nest: warm strings (the developer's violin with a section beneath it).
        auto a = sample("violin-sustain");
        a.attack = 0.12f;
        a.release = 0.6f;
        a.vibrato = 0.0f;
        a.lowpass = 9000.0f;
        auto b = strings(Viola, 3, 0.42f, -3.0f);
        b.attack = 0.2f;
        b.release = 0.6f;
        auto c = standIn(shift(strings(Violin, 3, 0.45f, -2.0f), 0.0f, 5.0f, 0.2f));
        c.attack = 0.2f;
        auto d = character(shift(strings(Cello, 3, 0.4f, -7.0f), -12.0f));
        d.attack = 0.25f;
        d.lowKey = 24;
        Builder p("Warm Strings", "Epic, heroic", "Cellos");
        p.add(a).add(b).add(c).add(d).air(0.5f).chorus(0.12f).trim(0.0f);
        put(p);
    }
    {   // Summit of Winds: epic choir and brass together.
        auto a = choir(2.0f, 0.6f);
        a.attack = 0.25f;
        auto b = shift(choir(2.5f, 0.6f, -9.0f), 12.0f, 3.0f, 0.2f);
        b.attack = 0.3f;
        auto c = brass(800.0f, 0.5f, -6.0f);
        c.attack = 0.15f;
        c.p[3] = 7.0f;
        c.release = 0.6f;
        auto d = character(shift(brass(650.0f, 0.4f, -9.0f), -12.0f));
        d.attack = 0.15f;
        d.lowKey = 24;
        Builder p("Epic Choir and Brass", "Epic, heroic", "Brass");
        p.add(a).add(b).add(c).add(d).air(2.5f).chorus(0.1f).trim(0.0f);
        put(p);
    }
    for (std::size_t i = 0; i < all.size(); ++i) {
        all[i].trimDb = instrumentTrims[i];
        all[i].standInTrimDb = standInTrims[i];
    }
    for (auto& p : all) {
        p.sustaining = !p.tremoloMod;
        for (int k = 0; k < p.layerCount; ++k)
            if (p.layers[std::size_t(k)].vibratoHz <= 0.0f) p.layers[std::size_t(k)].vibratoHz = 5.0f;
    }
    return all;
}
} // namespace

const Patch& patch(int index) noexcept {
    static const std::array<Patch, instrumentCount> patches = build();
    return patches[std::size_t(std::clamp(index, 0, instrumentCount - 1))];
}

int instrumentIndex(int location, int sub) noexcept {
    location = std::clamp(location, 0, dc::LocationCount - 1);
    int index = 0;
    for (int l = 0; l < location; ++l) index += dc::subLocationCount(l) + 1;
    return std::clamp(index + dc::clampSubLocation(location, sub), 0, instrumentCount - 1);
}

void instrumentPlace(int index, int& location, int& sub) noexcept {
    index = std::clamp(index, 0, instrumentCount - 1);
    for (int l = 0; l < dc::LocationCount; ++l) {
        const int count = dc::subLocationCount(l) + 1;
        if (index < count) { location = l; sub = index; return; }
        index -= count;
    }
    location = dc::LocationCount - 1;
    sub = dc::subLocationCount(location);
}

} // namespace ls::inst
