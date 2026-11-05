// The 31 instruments (Direction Update 1, section 3): one per sub-location, each location
// a family. Uplifting, radiant, majestic; each distinct from its siblings; good dry and in
// its own space. The developer's recordings (AdrianAbbas/Samples) play where they fit:
// the classical guitar (Sunlit Meadow, Leaf Halls), the kalimba (Butterfly Glade), the
// violin's pizzicato with its col legno (Mushroom Ring) and sustain (Golden Nest), the
// Minilogue's soft pad (Cloud Bridge, and its short hits in Sky Harbour) and pulse (Cloud
// Bridge); since the recordings update also his voice (Angelic Choir, Radiant Choir, Epic
// Choir and Brass, the Orchestral Chord Pad's Voices), the banjo (Shimmering Pluck) and
// the piano (under the Celesta). Each has a synthesised stand-in for builds without them.
// trimDb comes from `LightSeekerTests --calibrate-instruments` (a held chord's loudness).
#include "Synth.h"
#include <string_view>
#include "InstrumentTrims.h"
#include "LibraryTrims.h"
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

// The developer's voice (Engine::Voice): a vowel (0 mmm .. 4 ee), how many singers at
// the Ensemble macro's middle, and a velocity cap (0.45: the soft takes only).
Layer voice(float vowel, float singers, float velocityCap, float level = 0.0f) {
    auto l = layer(Engine::Voice, level);
    l.needs = Needs::Samples;
    l.p = { vowel, singers, velocityCap, 10.0f, 1.0f, 45.0f };
    l.attack = 0.06f;
    l.release = 0.9f;
    l.velocityGain = 0.4f;
    l.velocityTone = 0.25f;
    l.keyPan = 0.1f;
    return l;
}

// A synthesised layer that plays only with the recordings (it blends with them, e.g.
// above the voice's highest recorded note).
Layer withRecordings(Layer l) {
    l.needs = Needs::Samples;
    return l;
}

Layer fadeOutAbove(Layer l, float key, float span) { l.fadeKey = key; l.fadeSpan = span; l.fadeIn = false; return l; }
Layer fadeInAbove(Layer l, float key, float span) { l.fadeKey = key; l.fadeSpan = span; l.fadeIn = true; return l; }

Layer standIn(Layer l) {
    l.needs = Needs::NoSamples;
    return l;
}

Layer character(Layer l) {
    l.character = true;
    return l;
}

// Mix and Punch: a brass (or reed) layer that cuts through: the attack's bite, 2-4 kHz
// presence, and more velocity response (soft notes softer, hard ones harder and brighter).
Layer hit(Layer l, float bite, float presence) {
    l.bite = bite;
    l.presence = presence;
    l.velocityGain = std::max(l.velocityGain, 0.9f);
    return l;
}

// The real brass (VSCO 2 Community Edition, CC0; the real brass update): a synthesised brass
// layer that plays the library's sustain with its staccato as the attack's front when the
// library is loaded, a little of its own synthesis under them for weight and shine
// (synthDb), shaped to cut through: presence (2-4 kHz), bite (a bright transient band, more
// with velocity) and drive (density, not level). Without the library: the synthesis alone,
// exactly as before. Levels relative to the synthesised layer's own.
struct RealSpec {
    const char* sustain;
    const char* front;
    float frontDb, synthDb, presence, bite, drive;
};
Layer real(Layer l, const RealSpec& r) {
    l.real.sustain = r.sustain;
    l.real.front = r.front;
    l.real.frontDb = r.frontDb;
    l.real.synthDb = r.synthDb;
    l.real.presence = r.presence;
    l.real.bite = r.bite;
    l.real.drive = r.drive;
    return l;
}
// A second player: a neighbouring note's take (`offset` semitones), a little later.
Layer secondPlayer(Layer l, float offset, float frontOffset, float delayMs) {
    l.real.offset = offset;
    l.real.frontOffset = frontOffset;
    l.real.delayMs = delayMs;
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
        // Mix and Punch: a quicker rise (6-8 ms), the bite of a hard-blown attack and 2-4 kHz
        // presence, more velocity response: they cut through a mix and hit.
        auto a = hit(brass(1650.0f, 0.88f), 0.8f, 0.35f);
        a.p[3] = 6.0f;
        a.pan = -0.2f;
        a.attack = 0.006f;
        a.lowpass = 14000;
        auto b = hit(shift(brass(1600.0f, 0.82f, -4.0f), 0.0f, 9.0f, 0.3f), 0.8f, 0.35f);
        b.attack = 0.008f;
        auto c = hit(character(shift(brass(1700.0f, 0.7f, -9.0f), 12.0f, -4.0f, 0.1f)), 0.6f, 0.25f);
        c.attack = 0.01f;
        // The real brass: the library's trumpet (sustain, staccato front) in both players
        // (the second a neighbouring note's take, 8 ms later), bright and biting; the
        // synthesis a little under them, and the piccolo above stays synthesised (shine).
        a = real(a, { "vsco-trumpet-sus", "vsco-trumpet-stac", 5.5f, -12.0f, 0.6f, 0.8f, 0.35f });
        a.real.lowpass = 15000.0f;
        a.real.release = 0.3f;
        a.real.strikeDb = 9.0f;
        a.real.strikeMs = 25.0f;
        a.real.frontSkipMs = 25.0f;
        b = secondPlayer(real(b, { "vsco-trumpet-sus", "vsco-trumpet-stac", 3.5f, -12.0f, 0.55f, 0.7f, 0.35f }), 2.0f, -2.0f, 8.0f);
        b.real.lowpass = 14000.0f;
        b.real.release = 0.3f;
        b.real.strikeDb = 8.0f;
        b.real.strikeMs = 25.0f;
        b.real.frontSkipMs = 25.0f;
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
        // The real brass: the library's horn sustain swelling in with the pad (no front: a
        // pad), warm, the synthesis under it.
        b = real(b, { "vsco-horn-sus", nullptr, 0.0f, -9.0f, 0.1f, 0.0f, 0.15f });
        b.real.attack = 0.7f;
        b.real.release = 1.2f;
        b.real.lowpass = 7000.0f;
        b.real.velocityGain = 0.6f;
        auto c = standIn(character(choir(0.8f, 0.4f, -3.0f)));
        c.attack = 0.9f;
        c.release = 1.8f;
        auto d = shift(strings(Cello, 2, 0.4f, -7.0f), -12.0f);
        d.attack = 0.6f;
        d.release = 1.3f;
        d.lowKey = 24;
        // The Voices: the developer's soft choir, between "ooh" and "aah" (the soft takes, four singers).
        auto e = character(voice(2.0f, 4.0f, 0.45f, -4.0f));
        e.attack = 0.9f;
        e.release = 1.8f;
        e.highpass = 110.0f;
        e.lowpass = 9000.0f;
        Builder p("Orchestral Chord Pad", "Radiant brass and strings", "Voices");
        p.add(a).add(b).add(c).add(d).add(e).air(2.0f).chorus(0.15f).trim(0.0f);
        put(p);
    }
    {   // Spire Summit: a soaring solo horn.
        // Mix and Punch: it speaks in 35-45 ms (was 90-110), with bite and presence (the
        // low-pass opened to let the 3 kHz band through), still round and majestic.
        auto a = hit(brass(480.0f, 0.22f), 0.6f, 0.3f);
        a.p[1] = 1.0f;
        a.p[2] = 0.05f;
        a.attack = 0.035f;
        a.release = 0.6f;
        a.vibrato = 0.07f;
        a.vibratoDelay = 0.45f;
        a.vibratoHz = 5.0f;
        a.lowpass = 4800;
        auto b = hit(character(shift(brass(470.0f, 0.2f, -7.0f), 0.0f, 7.0f, 0.35f)), 0.5f, 0.25f);
        b.attack = 0.045f;
        b.release = 0.6f;
        b.lowpass = 4200;
        // The real brass: the library's horn, round and majestic, its staccato under the
        // attack (softer than the trumpets': it speaks, it does not bark), presence for the
        // mix; the second horn a neighbouring note's take, 12 ms later.
        a = real(a, { "vsco-horn-sus", "vsco-horn-stac", 3.5f, -13.0f, 0.5f, 0.6f, 0.25f });
        a.real.lowpass = 11000.0f;
        a.real.release = 0.5f;
        a.real.strikeDb = 11.0f;
        a.real.strikeMs = 30.0f;
        a.real.frontSkipMs = 35.0f;
        b = secondPlayer(real(b, { "vsco-horn-sus", "vsco-horn-stac", 0.5f, -14.0f, 0.4f, 0.45f, 0.25f }), 2.0f, 1.0f, 12.0f);
        b.real.lowpass = 10000.0f;
        b.real.release = 0.5f;
        b.real.strikeDb = 7.0f;
        b.real.strikeMs = 30.0f;
        b.real.frontSkipMs = 35.0f;
        Builder p("Soaring Horn", "Radiant brass and strings", "Second horn");
        p.add(a).add(b).air(1.5f).poly(10).trim(0.0f);
        put(p);
    }

    // ---- Sky Temple: bells and voices --------------------------------------------------
    {   // Cloud Steps: a harp whose every note arrives on a rising glissando: a quick sweep
        // up through the notes held (chord tones and their octaves; one note alone, its
        // octaves and fifth) into the note played (Direction Update 2).
        auto a = pluck(BodyHarp, 0.3f, 0.45f, 3.5f, 0.68f);
        a.run = 4;
        a.runMs = 24.0f;
        a.release = 1.2f;
        a.keyPan = 0.35f;
        Builder p("Rising Harp Glissando", "Bells and voices", "Glissando");
        p.add(a).air(3.0f).chorus(0.2f).struck().trim(0.0f);
        put(p);
    }
    {   // Sanctum: an angelic choir. The developer's voice: the soft takes and the falsetto
        // ("ooh"), four singers, bright and airy with a light shimmer. His falsetto ends at
        // E5: above it the recordings go on pitched up (their formants rise with them, so
        // the voice grows younger, not lower) and fade over an octave into the formant
        // choir, which carries the top. Without the recordings the formant choir alone.
        auto v = fadeOutAbove(voice(1.0f, 4.0f, 0.45f), 76.0f, 12.0f);
        v.attack = 0.3f;
        v.release = 0.6f;   // the recorded tail plays on release (twice this to fall away)
        v.highpass = 170.0f;
        v.lowpass = 15000.0f;
        auto a = standIn(choir(0.6f, 0.35f));
        a.p[1] = 0.12f;
        a.attack = 0.45f;
        a.release = 1.0f;
        a.vibrato = 0.1f;
        auto top = withRecordings(fadeInAbove(choir(0.5f, 0.45f, -1.0f), 74.0f, 12.0f));
        top.p[1] = 0.14f;
        top.attack = 0.4f;
        top.release = 1.2f;
        top.vibrato = 0.08f;
        auto b = character(shift(choir(1.0f, 0.3f, -9.0f), 12.0f, 4.0f));
        b.attack = 0.6f;
        b.release = 1.2f;
        Builder p("Angelic Choir", "Bells and voices", "Descant");
        p.add(v).add(a).add(top).add(b).air(3.5f).chorus(0.14f).shimmer(0.07f).trim(0.0f);
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
        // The developer's piano (its long notes) under the bars: the hammered body of a
        // celesta's keyboard action, high-passed so the bars stay on top.
        auto d = sample("piano-long", -7.0f);
        d.highpass = 280.0f;
        d.p[1] = 2.2f;
        d.release = 0.45f;
        Builder p("Celesta", "Crystal", "Sparkle");
        p.add(a).add(b).add(c).add(d).air(2.5f).struck().trim(0.0f);
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
        // The trumpet stop (Mix and Punch): it speaks first (25 ms), with bite and presence.
        auto b = hit(character(layer(Engine::Reed, -9.0f)), 0.6f, 0.3f);
        b.p = { 3500.0f, 1400.0f, 0.02f, 4.0f, 0, 0 };
        b.attack = 0.025f;
        b.release = 0.25f;
        b.velocityGain = 0.0f;
        b.velocityTone = 0.0f;
        Builder p("Grand Organ", "Sacred, golden", "Trumpet stop");
        p.add(a).add(b).air(2.0f).trim(0.0f);
        put(p);
    }
    {   // Choir Loft: a radiant choir, bright "ah", basses below, sopranos above. The
        // developer's voice: the full chest takes and the falsetto, the "aah" stacks as the
        // choir (five singers), warm and majestic; the formant choir carries on above his
        // falsetto and stands in without the recordings.
        auto v = fadeOutAbove(voice(3.0f, 5.0f, 0.0f), 78.0f, 10.0f);
        v.attack = 0.95f;   // a choir swelling in (the organ beside it speaks at once)
        v.release = 0.8f;   // and its recorded tail dies away (the organ stops)
        v.highpass = 75.0f;
        v.lowpass = 9000.0f;   // warm
        auto top = withRecordings(fadeInAbove(choir(2.6f, 0.6f, -2.0f), 76.0f, 10.0f));
        top.attack = 0.15f;
        top.vibrato = 0.12f;
        auto a = standIn(choir(2.2f, 0.7f));
        a.p[1] = 0.07f;
        a.p[2] = 11.0f;
        a.attack = 0.15f;
        a.release = 0.7f;
        a.vibrato = 0.14f;
        auto b = shift(choir(2.0f, 0.55f, -4.0f), -12.0f, 0.0f, -0.25f);   // the basses
        b.attack = 0.18f;
        auto c = character(shift(choir(3.0f, 0.65f, -10.0f), 12.0f, 3.0f, 0.25f));
        c.attack = 0.2f;
        Builder p("Radiant Choir", "Sacred, golden", "Sopranos");
        p.add(v).add(top).add(a).add(b).add(c).air(3.5f).chorus(0.12f).trim(0.0f);
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
        // The Minilogue's pulsating patch, held, quietly inside the cloud (a slow inner motion).
        auto d = sample("mlg-pulse-long", -14.0f);
        d.attack = 0.6f;
        d.release = 1.6f;
        d.highpass = 250.0f;
        d.lowpass = 7000.0f;
        d.velocityGain = 0.3f;
        Builder p("Cloud Pad", "Airy", "Haze");
        p.add(a).add(b).add(c).add(d).air(1.5f).chorus(0.4f).trim(0.0f);
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
    {   // Sky Harbour: a shimmering pluck. The developer's banjo (open G, three dynamics,
        // the 5th string's three hits per dynamic as round-robins), made to shimmer rather
        // than twang: its drum's boom and the low strings high-passed away, the ring
        // shortened, the Minilogue's soft pad (short hits) as a synth body under it, an FM
        // sparkle above, chorus and a rising shimmer around it. Its stand-in is the
        // analog pluck it had before.
        auto bj = sample("banjo");
        bj.highpass = 300.0f;
        bj.lowpass = 15000.0f;
        bj.p[1] = 2.0f;
        bj.release = 0.5f;
        bj.velocityGain = 0.45f;
        auto pad = sample("mlg-softpad-short", -10.0f);
        pad.highpass = 180.0f;
        pad.lowpass = 9000.0f;
        pad.p[1] = 1.4f;
        pad.release = 0.5f;
        auto a = layer(Engine::Analog);
        a.needs = Needs::NoSamples;
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
        p.add(bj).add(pad).add(a).add(b).air(3.5f).chorus(0.35f).shimmer(0.35f).mode(Character::Shimmer).struck().trim(0.0f);
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
        // The violin's col legno (the bow's wood on the string): a woody tick on each note.
        auto d = sample("violin-collegno", -9.0f);
        d.highpass = 400.0f;
        d.p[1] = 0.25f;
        d.release = 0.1f;
        d.velocityGain = 0.6f;
        Builder p("Bouncy Pizzicato", "Twinkly", "Bounce");
        p.add(a).add(b).add(c).add(d).air(2.5f).struck().trim(0.0f);
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
        // GarageBand hotfix: its strike stood 14-15 dB over its own ring (the sharpest of
        // all) and its chords reached the safety limiter even before Punch: the string rings
        // a little longer (1.2 -> 1.8: more body, so its level trim came down 1.5 dB) and its
        // chords' plucks no longer add their first peaks (alternatePolarity).
        auto a = shift(pluck(BodyNone, 0.25f, 0.45f, 1.8f, 0.6f), 12.0f);
        a.release = 0.6f;
        a.alternatePolarity = true;
        auto b = shift(bell(ModesHarmonic, 0.7f, 1.0f, -14.0f), 12.0f, 0.0f, 0.3f);
        b.release = 1.0f;
        Builder p("Fairy Harp", "Twinkly", "Magic");
        p.add(a).add(b).air(2.5f).shimmer(0.3f).mode(Character::Shimmer).struck().trim(0.0f);
        put(p);
    }

    // ---- Dragon's Peak: epic, heroic ----------------------------------------------------
    {   // Eyrie: heroic horns (a section of three, an octave below as the character).
        // Mix and Punch: a quicker rise (12-20 ms), bite and presence.
        auto a = hit(shift(brass(800.0f, 0.62f), 0.0f, -4.0f, -0.35f), 0.7f, 0.3f);
        a.p[3] = 5.0f;
        a.attack = 0.012f;
        a.release = 0.3f;
        a.lowpass = 9000.0f;
        auto b = hit(shift(brass(780.0f, 0.62f, -2.0f), 0.0f, 7.0f, 0.35f), 0.7f, 0.3f);
        b.attack = 0.015f;
        b.release = 0.3f;
        b.lowpass = 9000.0f;
        auto c = hit(character(shift(brass(600.0f, 0.45f, -6.0f), -12.0f)), 0.4f, 0.0f);
        c.attack = 0.02f;
        c.lowKey = 24;
        // The real brass: a horn section from the library (two players: a neighbouring note's
        // take 7 ms later), hard-tongued staccato fronts, bright and biting; the depth an
        // octave below on the library's trombone.
        a = real(a, { "vsco-horn-sus", "vsco-horn-stac", 5.5f, -12.0f, 0.6f, 0.8f, 0.35f });
        a.real.lowpass = 13000.0f;
        a.real.release = 0.35f;
        a.real.strikeDb = 9.0f;
        a.real.strikeMs = 28.0f;
        a.real.frontSkipMs = 35.0f;
        b = secondPlayer(real(b, { "vsco-horn-sus", "vsco-horn-stac", 4.0f, -12.0f, 0.55f, 0.75f, 0.35f }), -2.0f, 2.0f, 7.0f);
        b.real.lowpass = 12000.0f;
        b.real.release = 0.35f;
        b.real.strikeDb = 8.0f;
        b.real.strikeMs = 28.0f;
        b.real.frontSkipMs = 35.0f;
        c = real(c, { "vsco-trombone-sus", "vsco-trombone-stac", 2.5f, -12.0f, 0.2f, 0.4f, 0.25f });
        c.real.strikeDb = 4.0f;
        c.real.frontSkipMs = 30.0f;
        c.real.lowpass = 9000.0f;
        c.real.release = 0.4f;
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
    {   // Summit of Winds: epic choir and brass together. The developer's voice: the full
        // chest takes, seven singers from the "aah" stacks, and his low drones (G2, A3) an
        // octave under the brass. The formant choir stands in without the recordings.
        auto v = voice(3.0f, 7.0f, 0.0f);
        v.attack = 0.7f;    // the choir swells in under the brass's attack
        v.release = 0.8f;
        v.highpass = 70.0f;
        v.lowpass = 16000.0f;
        // The choir in two octaves: the upper voices an octave up (his falsetto "ooh" and
        // the soft takes, three singers).
        auto up = shift(voice(1.5f, 3.0f, 0.45f, -7.0f), 12.0f, 3.0f, 0.2f);
        up.attack = 0.4f;
        up.release = 0.6f;
        up.highpass = 200.0f;
        auto drone = shift(sample("voice-drone", -9.0f), -12.0f);
        drone.attack = 0.35f;
        drone.release = 1.0f;
        drone.lowKey = 28;
        drone.p[2] = 1.0f;
        drone.lowpass = 4000.0f;
        auto a = standIn(choir(2.0f, 0.6f));
        a.attack = 0.25f;
        auto b = standIn(shift(choir(2.5f, 0.6f, -9.0f), 12.0f, 3.0f, 0.2f));
        b.attack = 0.3f;
        // Mix and Punch: the brass strikes (50-70 ms, with bite and presence; it used to swell
        // with the choir, 350 ms) as a bright low section an octave under the choir (a brighter
        // formant: trombones), the warm brass an octave below with it, and the choir swells in
        // over them. (Striking at the choir's pitch it sounded like the Eyrie's horn section:
        // the siblings check, see docs/DEVLOG.md.)
        // In the lowest notes (played below D3-A#3, where they cross over) the striking brass
        // plays at the choir's pitch instead, as before (an octave lower it would blur them).
        auto c = fadeInAbove(hit(shift(brass(1300.0f, 0.8f, -9.0f), -12.0f), 0.6f, 0.3f), 38.0f, 8.0f);
        auto low = fadeOutAbove(hit(brass(800.0f, 0.5f, -9.0f), 0.6f, 0.3f), 50.0f, 8.0f);
        low.attack = 0.05f;
        low.p[3] = 7.0f;
        low.release = 0.6f;
        c.attack = 0.05f;
        c.p[3] = 7.0f;
        c.release = 0.6f;
        auto d = hit(character(shift(brass(650.0f, 0.4f, -9.0f), -12.0f)), 0.4f, 0.0f);
        d.attack = 0.07f;
        d.lowKey = 24;
        // The real brass: the striking low section is the library's trombone (sustain and its
        // staccato front, bright and biting; at the choir's pitch in the lowest notes), the
        // warm brass an octave below its tuba, the synthesis a little under them.
        c = real(c, { "vsco-trombone-sus", "vsco-trombone-stac", 5.5f, -12.0f, 0.55f, 0.75f, 0.35f });
        c.real.lowpass = 12000.0f;
        c.real.release = 0.5f;
        c.real.strikeDb = 11.0f;
        c.real.strikeMs = 28.0f;
        c.real.frontSkipMs = 30.0f;
        // The lowest notes keep more of the synthesis (-6 dB): its strong fundamental carries their
        // pitch (the real trombone's is in its upper harmonics there, under the choir's).
        low = real(low, { "vsco-trombone-sus", "vsco-trombone-stac", 5.5f, -6.0f, 0.5f, 0.7f, 0.35f });
        low.real.lowpass = 11000.0f;
        low.real.release = 0.5f;
        low.real.strikeDb = 11.0f;
        low.real.strikeMs = 28.0f;
        low.real.frontSkipMs = 30.0f;
        d = real(d, { "vsco-tuba-sus", "vsco-tuba-stac", 1.5f, -12.0f, 0.1f, 0.3f, 0.25f });
        d.real.strikeDb = 4.0f;
        d.real.frontSkipMs = 30.0f;
        d.real.lowpass = 6000.0f;
        d.real.release = 0.5f;
        Builder p("Epic Choir and Brass", "Epic, heroic", "Brass");
        p.add(v).add(up).add(drone).add(a).add(b).add(c).add(d).add(low).air(4.5f).chorus(0.1f).trim(0.0f);
        put(p);
    }
    for (std::size_t i = 0; i < all.size(); ++i) {
        all[i].trimDb = instrumentTrims[i];
        all[i].standInTrimDb = standInTrims[i];
        all[i].libraryTrimDb = libraryTrims[i];
        all[i].libraryStandInTrimDb = libraryStandInTrims[i];
        all[i].libraryTiltDb = libraryTilts[i];
    }
    // Mix and Punch. Punch (dB of gain on hit at Punch 50 %): high on the brass, low on bowed
    // strings and winds, none on pads and choirs. GarageBand hotfix: none on the struck and
    // plucked instruments (bells, mallets, celesta, harpsichord, harps, plucks): their strike
    // is already the sharpest part of the note, and the overshoot on top of it drove chords
    // into the safety limiters (a clang). Turned up, their Punch rises in after the strike
    // (Synth::startLayers). "was" is the Mix and Punch default, kept as punchLegacyDb. Reverb (how much of the place's reverbs the instrument takes): the
    // instruments mostly played in a mix (brass, strings, plucks, harps, guitars) sit a
    // little drier by default; MIX Reverb at 150 % brings back the place's whole space. The
    // brass also puts its dry hit first: the reverbs start 20 ms later and dip under its
    // attacks.
    struct MixDefaults { const char* name; float punchDb, space, preDelayMs, duck; };
    static constexpr MixDefaults defaults[] = {
        { "Fanfare Trumpets", 8.0f, 0.7f, 20.0f, 0.3f },
        { "Shimmering Strings", 1.5f, 0.75f, 0.0f, 0.0f },
        { "Orchestral Chord Pad", 0.0f, 1.0f, 0.0f, 0.0f },
        { "Soaring Horn", 6.0f, 0.75f, 20.0f, 0.25f },
        { "Rising Harp Glissando", 0.0f, 0.75f, 0.0f, 0.0f },   // was 3.5
        { "Angelic Choir", 0.0f, 1.0f, 0.0f, 0.0f },
        { "Wind Chimes and Bells", 0.0f, 1.0f, 0.0f, 0.0f },   // was 2.0
        { "Glass Harp", 1.0f, 1.0f, 0.0f, 0.0f },
        { "Elven Lyre", 0.0f, 0.75f, 0.0f, 0.0f },   // was 3.5
        { "Airy Flute", 1.5f, 1.0f, 0.0f, 0.0f },
        { "Crystal Bells", 0.0f, 1.0f, 0.0f, 0.0f },   // was 2.0
        { "Glass Harmonica Pad", 0.0f, 1.0f, 0.0f, 0.0f },
        { "Celesta", 0.0f, 1.0f, 0.0f, 0.0f },   // was 2.5
        { "Warm Acoustic Guitar", 0.0f, 0.7f, 0.0f, 0.0f },   // was 3.5
        { "Bouncy Kalimba", 0.0f, 0.8f, 0.0f, 0.0f },   // was 3.5
        { "Pastoral Oboe", 1.5f, 1.0f, 0.0f, 0.0f },
        { "Grand Organ", 2.5f, 1.0f, 10.0f, 0.0f },
        { "Radiant Choir", 0.0f, 1.0f, 0.0f, 0.0f },
        { "Celesta and Harp", 0.0f, 0.8f, 0.0f, 0.0f },   // was 3.0
        { "Cloud Pad", 0.0f, 1.0f, 0.0f, 0.0f },
        { "Breathy Wind Flute", 1.0f, 1.0f, 0.0f, 0.0f },
        { "Shimmering Pluck", 0.0f, 0.75f, 0.0f, 0.0f },   // was 3.5
        { "Concert Harp", 0.0f, 0.75f, 0.0f, 0.0f },   // was 3.5
        { "Music Box", 0.0f, 1.0f, 0.0f, 0.0f },   // was 2.0
        { "Harpsichord", 0.0f, 0.8f, 0.0f, 0.0f },   // was 3.0
        { "Bouncy Pizzicato", 0.0f, 0.75f, 0.0f, 0.0f },   // was 3.5
        { "Twinkling Glockenspiel", 0.0f, 1.0f, 0.0f, 0.0f },   // was 2.0
        { "Fairy Harp", 0.0f, 0.75f, 0.0f, 0.0f },   // was 3.5
        { "Heroic Horns", 8.0f, 0.7f, 20.0f, 0.3f },
        { "Warm Strings", 1.5f, 0.75f, 0.0f, 0.0f },
        { "Epic Choir and Brass", 4.0f, 0.85f, 15.0f, 0.2f },
    };
    // The Punch of the Mix and Punch release where the hotfix changed it.
    static constexpr std::pair<const char*, float> punchBefore[] = {
        { "Rising Harp Glissando", 3.5f },
        { "Wind Chimes and Bells", 2.0f },
        { "Elven Lyre", 3.5f },
        { "Crystal Bells", 2.0f },
        { "Celesta", 2.5f },
        { "Warm Acoustic Guitar", 3.5f },
        { "Bouncy Kalimba", 3.5f },
        { "Celesta and Harp", 3.0f },
        { "Shimmering Pluck", 3.5f },
        { "Concert Harp", 3.5f },
        { "Music Box", 2.0f },
        { "Harpsichord", 3.0f },
        { "Bouncy Pizzicato", 3.5f },
        { "Twinkling Glockenspiel", 2.0f },
        { "Fairy Harp", 3.5f },
    };
    for (auto& p : all)
        for (const auto& d : defaults)
            if (std::string_view(p.name) == d.name) { p.punchDb = p.punchLegacyDb = d.punchDb; p.space = d.space; p.spacePreDelayMs = d.preDelayMs; p.spaceDuck = d.duck; }
    for (auto& p : all)
        for (const auto& [name, db] : punchBefore)
            if (std::string_view(p.name) == name) p.punchLegacyDb = db;
    for (auto& p : all) {
        p.sustaining = !p.tremoloMod;
        for (int k = 0; k < p.layerCount; ++k)
            if (p.layers[std::size_t(k)].vibratoHz <= 0.0f) p.layers[std::size_t(k)].vibratoHz = 5.0f;
    }
    return all;
}
} // namespace

namespace {
std::array<Patch, instrumentCount>& patches() {
    static std::array<Patch, instrumentCount> all = build();
    return all;
}
} // namespace

const Patch& patch(int index) noexcept {
    return patches()[std::size_t(std::clamp(index, 0, instrumentCount - 1))];
}

void setLibraryTiltForTests(int index, float dbPerOctave) noexcept {
    patches()[std::size_t(std::clamp(index, 0, instrumentCount - 1))].libraryTiltDb = dbPerOctave;
}

void setLibraryTrimForTests(int index, float db, bool standIn) noexcept {
    auto& p = patches()[std::size_t(std::clamp(index, 0, instrumentCount - 1))];
    (standIn ? p.libraryStandInTrimDb : p.libraryTrimDb) = db;
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
