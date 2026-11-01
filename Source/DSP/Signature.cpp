#include "Modules.h"

// VIII. SIGNATURE: one effect per place. Every place
// of the world uses a different kind, on top of its room, so that any two places are
// tellable apart blind. A kind is a recipe of small primitives:
//
//   serial (crossfaded by Amount):  saturation flavour -> tone -> attack softening ->
//                                   wobble (modulated delay) -> tremolo
//   parallel (added, x Amount):     tuned feedback combs, a modal bank (optionally struck
//                                   only by attacks), pitch layers (continuous, on
//                                   attacks, or twinkling; optional vowel formants and
//                                   a slow bloom), a tap pattern (fixed, scattering,
//                                   crackling, circling, diffused), smear, reverse swell
//
// Tone (-1..1, 0 = as designed) transposes resonances by up to an octave in semitone
// steps and tilts filters; Rate (x0.25..x4) scales every modulation rate.
namespace dc {
using namespace dsp;

namespace sig {
enum Sat { SatNone, SatWarm, SatGrowl, SatMenace, SatGrit };
enum Tone { ToneNone, ToneLow, ToneHigh, ToneBell, ToneShelf };
enum Shape { Sine, SmoothRandom, Bubbling, Slosh, Crooked };
enum TapMode { TapsFixed, TapsScatter, TapsCrackle, TapsCircle, TapsDiffuse };
enum PitchMode { PitchContinuous, PitchTransient, PitchTwinkle, PitchGlitter };
enum Formant { FormantNone, FormantAh, FormantOh };

// Fields in declaration order (designated initialisers must follow it).
struct Recipe {
    Sat sat = SatNone; float drive = 0;                                     // dB
    Tone tone = ToneNone; float toneHz = 1000, toneQ = .7f, toneDb = 0;
    float swellMs = 0;                                                      // attack softening
    float wobbleMs = 0, wobbleHz = 0; Shape wobbleShape = Sine; float wobbleMix = 1;
    float tremDepth = 0, tremHz = 0; Shape tremShape = Sine; bool tremLayersOnly = false;
    int combs = 0; float combHz[Signature::maxCombs] {}; float combFeedback = 0, combDamp = 0, combGain = 0;
    int modes = 0; float modeHz[Signature::maxModes] {}; float modeT60 = 1, modeGain = 0; bool modeStruck = false;
    int pitches = 0; float pitchRatio[2] {}; float pitchGain = 0; PitchMode pitchMode = PitchContinuous;
    float pitchHp = 20, pitchLp = 20000; Formant formant = FormantNone; float bloomMs = 0;
    int taps = 0; float tapMs[Signature::maxTaps] {}; float tapGain[Signature::maxTaps] {}; float tapPan[Signature::maxTaps] {};
    float tapLp = 20000, tapDarken = 1; TapMode tapMode = TapsFixed; float tapRate = .1f;
    float smear = 0, smearMs = 30;
    float reverseMs = 0, reverseGain = 0, reverseLp = 20000;
    // LIGHT SEEKER's primitives (Milestone 2).
    float spiralGain = 0, spiralHz = .08f, spiralHp = 400;  // an endlessly rising pitch spiral (three voices, an octave each)
    float prismGain = 0, prismHz = .05f;                     // an octave shimmer split into colour bands across the stereo field
    float hueGain = 0, hueHz = .06f;                         // a shimmer seen through a slowly sweeping coloured filter pair
    bool autoPan = false;                                    // the tremolo moves the sound from side to side
    float swirlDepth = 0, swirlHz = .2f;                     // the whole stereo image rotating around the listener
    bool tapsFromPitch = false;                              // the taps echo the pitch layer (bouncing sparkles)
};

// The 31 signatures, in kind order (Locations.cpp assigns one to each place).
const Recipe recipes[Signature::kinds] = {
    {},   // 0 None
    // ---- Celestial Citadel
    // 1 Gates of Dawn, Fanfare Reflections: bright, brassy first reflections thrown
    // left and right like a fanfare from the gate towers.
    { .tone = ToneShelf, .toneHz = 3200, .toneDb = 3.5f,
      .taps = 6, .tapMs = { 11, 19, 29, 41, 57, 76 }, .tapGain = { .55f, .5f, .45f, .4f, .34f, .28f }, .tapPan = { -.9f, .9f, -.7f, .7f, -.5f, .5f },
      .tapLp = 16000, .tapDarken = .96f },
    // 2 Hall of Mirrors, Mirror Multiplication: reflections multiplying outwards across
    // the stereo field, each mirror farther than the last.
    { .taps = 6, .tapMs = { 31, 47, 71, 107, 160, 240 }, .tapGain = { .4f, .45f, .45f, .4f, .34f, .28f }, .tapPan = { -.3f, .4f, -.6f, .75f, -.9f, 1 },
      .tapLp = 14000, .tapDarken = .97f, .tapMode = TapsScatter, .tapRate = .2f },
    // 3 Throne of Light, Golden Chorus: a regal doubling with a halo of gold an octave up.
    { .wobbleMs = 4.5f, .wobbleHz = .55f, .wobbleShape = Sine, .wobbleMix = .5f,
      .pitches = 1, .pitchRatio = { 2.0f }, .pitchGain = .3f, .pitchHp = 1500, .pitchLp = 11000 },
    // 4 Spire Summit, Sky Echoes: long airy echoes falling away from the spire.
    { .taps = 3, .tapMs = { 690, 1380, 2070 }, .tapGain = { .45f, .3f, .2f }, .tapPan = { .8f, -.8f, .5f }, .tapLp = 9000, .tapDarken = .85f,
      .tapMode = TapsScatter, .tapRate = .3f },
    // ---- Sky Temple
    // 5 Cloud Steps, Rising Spiral: an endless spiral of shimmer climbing the steps,
    // three voices each rising through an octave, drifting from left to right.
    { .spiralGain = .6f, .spiralHz = .07f, .spiralHp = 450 },
    // 6 Sanctum, Choir Bloom: a soft sung "ah" blooming under the instrument.
    { .pitches = 2, .pitchRatio = { 1.0035f, 2.0f }, .pitchGain = .45f, .pitchLp = 4200, .formant = FormantAh, .bloomMs = 800,
      .smear = .5f, .smearMs = 45 },
    // 7 Bell Garden, Wind Chimes: tuned chimes (a pentatonic set) struck by the playing.
    { .modes = 6, .modeHz = { 1046.5f, 1174.7f, 1396.9f, 1568.0f, 1760.0f, 2093.0f }, .modeT60 = 3.5f, .modeGain = .35f, .modeStruck = true },
    // ---- Elven Sanctuary
    // 8 Moonwell, Water Glass: a glassy shimmer moving on the water's surface.
    { .wobbleMs = 1.2f, .wobbleHz = .4f, .wobbleShape = Slosh, .wobbleMix = .45f,
      .pitches = 1, .pitchRatio = { 2.0f }, .pitchGain = .3f, .pitchMode = PitchTwinkle, .pitchHp = 2500, .pitchLp = 12000 },
    // 9 Leaf Halls, Living Wood: warm wooden body resonances and a gentle doubling.
    { .wobbleMs = 3, .wobbleHz = .35f, .wobbleShape = Sine, .wobbleMix = .35f,
      .modes = 4, .modeHz = { 196, 277, 392, 554 }, .modeT60 = .5f, .modeGain = .5f },
    // 10 Starlit Canopy, Star Harmonics: sparkling high harmonics, twinkling.
    { .pitches = 2, .pitchRatio = { 3.0f, 4.0f }, .pitchGain = .35f, .pitchMode = PitchTwinkle, .pitchHp = 4000, .pitchLp = 13000 },
    // ---- Crystal Palace
    // 11 Prism Hall, Spectral Prism: an octave shimmer split into colour bands, each
    // band at its own place in the stereo field, the bands slowly turning.
    { .prismGain = .7f, .prismHz = .05f },
    // 12 Mirror Lake, Still Reflection: a calm reflection across the water, softened.
    { .taps = 2, .tapMs = { 380, 760 }, .tapGain = { .4f, .25f }, .tapPan = { -.5f, .5f }, .tapLp = 6000, .tapDarken = .8f,
      .smear = .4f, .smearMs = 60 },
    // 13 Diamond Vault, Crystal Modes: bright, inharmonic crystal modes ringing.
    { .modes = 5, .modeHz = { 1568.0f, 2349.3f, 3136.0f, 3951.1f, 4698.6f }, .modeT60 = 4, .modeGain = .4f, .modeStruck = true },
    // ---- Blossom Meadow
    // 14 Sunlit Meadow, Sun Warmth: a sunny, gentle saturation, highs softened.
    { .sat = SatWarm, .drive = 8, .tone = ToneShelf, .toneHz = 7000, .toneDb = -2 },
    // 15 Butterfly Glade, Butterfly Flutter: the sound flutters lightly from side to
    // side, like wings (an irregular auto-pan).
    { .tremDepth = .8f, .tremHz = 4.5f, .tremShape = SmoothRandom, .autoPan = true },
    // 16 Wildflower Hill, Valley Echoes: gentle echoes coming back over the valley.
    { .taps = 3, .tapMs = { 520, 1040, 1560 }, .tapGain = { .45f, .3f, .2f }, .tapPan = { -.7f, .7f, -.4f }, .tapLp = 5000, .tapDarken = .8f },
    // ---- Golden Cathedral
    // 17 Nave of Light, Radiant Cascade: long diffused cascades from the golden vault.
    { .taps = 4, .tapMs = { 210, 430, 660, 900 }, .tapGain = { .35f, .27f, .2f, .14f }, .tapPan = { -.9f, .9f, -.6f, .6f },
      .tapLp = 9000, .tapDarken = .9f, .tapMode = TapsDiffuse },
    // 18 Choir Loft, Angel Choir: an angelic choir an octave and a fifth above.
    { .pitches = 2, .pitchRatio = { 2.0f, 1.5f }, .pitchGain = .5f, .pitchLp = 6000, .formant = FormantAh, .bloomMs = 400 },
    // 19 Stained Glass Chapel, Colour Shift: a shimmer seen through coloured glass, its
    // colour changing slowly (two band-passes sweeping in opposite directions).
    { .smear = .3f, .smearMs = 40, .hueGain = .75f, .hueHz = .05f },
    // ---- Floating Isles
    // 20 Cloud Bridge, Cloud Swirl: echoes swirling around the listener.
    { .taps = 6, .tapMs = { 60, 120, 180, 240, 300, 360 }, .tapGain = { .45f, .4f, .34f, .28f, .23f, .18f },
      .tapLp = 10000, .tapDarken = .92f, .tapMode = TapsCircle, .tapRate = .2f, .swirlDepth = .85f, .swirlHz = .13f },
    // 21 Windswept Isle, Wind Veil: the sound thinned and blown about by the wind.
    { .tone = ToneHigh, .toneHz = 380, .toneQ = .7f, .wobbleMs = 1.5f, .wobbleHz = .5f, .wobbleShape = SmoothRandom, .wobbleMix = .5f,
      .tremDepth = .3f, .tremHz = .4f, .tremShape = SmoothRandom },
    // 22 Sky Harbour, Soaring Echoes: echoes that soar a fifth higher as they sail away
    // (the pitch layer echoed), over the harbour's rising tape echoes.
    { .pitches = 1, .pitchRatio = { 1.4983f }, .pitchGain = .3f, .pitchHp = 300,
      .taps = 3, .tapMs = { 470, 940, 1410 }, .tapGain = { .55f, .4f, .28f }, .tapPan = { .7f, -.7f, .3f }, .tapLp = 12000, .tapDarken = .95f,
      .tapsFromPitch = true },
    // ---- Royal Gardens
    // 23 Rose Garden, Lush Chorus: a soft, lush, wide chorus.
    { .wobbleMs = 5, .wobbleHz = .45f, .wobbleShape = Sine, .wobbleMix = .5f },
    // 24 Fountain Court, Fountain Sparkle: sparkling, water-like modulation.
    { .wobbleMs = .6f, .wobbleHz = 5, .wobbleShape = Bubbling, .wobbleMix = .4f,
      .pitches = 1, .pitchRatio = { 2.0f }, .pitchGain = .25f, .pitchMode = PitchTwinkle, .pitchHp = 3000 },
    // 25 Marble Colonnade, Column Slaps: rhythmic slaps from column to column.
    { .taps = 6, .tapMs = { 105, 210, 315, 420, 525, 630 }, .tapGain = { .5f, .42f, .35f, .28f, .22f, .17f }, .tapPan = { -.85f, .85f, -.85f, .85f, -.85f, .85f },
      .tapLp = 12000, .tapDarken = .93f },
    // ---- Fairy Glade
    // 26 Mushroom Ring, Bouncy Sparkle: every attack bounces up an octave and a fifth,
    // the sparkle bouncing left and right.
    { .pitches = 2, .pitchRatio = { 2.0f, 3.0f }, .pitchGain = .5f, .pitchMode = PitchTransient, .pitchHp = 1200, .pitchLp = 12000,
      .taps = 4, .tapMs = { 125, 250, 375, 500 }, .tapGain = { .6f, .45f, .32f, .22f }, .tapPan = { -.9f, .9f, -.9f, .9f }, .tapLp = 14000,
      .tapsFromPitch = true },
    // 27 Firefly Hollow, Firefly Glitter: tiny grains of light two octaves up, each a
    // firefly somewhere in the stereo field, blinking on and off.
    { .pitches = 2, .pitchRatio = { 4.0f, 2.0f }, .pitchGain = .6f, .pitchMode = PitchGlitter, .pitchHp = 2500, .pitchLp = 14000,
      .smear = .3f, .smearMs = 25 },
    // 28 Fairy Court, Harp Resonance: harp strings (tuned combs) ringing in sympathy.
    { .combs = 3, .combHz = { 523.3f, 659.3f, 784.0f }, .combFeedback = .92f, .combDamp = .1f, .combGain = .5f },
    // ---- Dragon's Peak
    // 29 Eyrie, Cliff Echoes: echoes thrown back off the cliffs, far and scattered.
    { .taps = 4, .tapMs = { 310, 640, 1020, 1490 }, .tapGain = { .45f, .34f, .25f, .17f }, .tapPan = { -.8f, .6f, -.4f, .9f },
      .tapLp = 7500, .tapDarken = .85f, .tapMode = TapsScatter, .tapRate = .15f },
    // 30 Golden Nest, Golden Harmonics: a warm golden harmonic richness.
    { .sat = SatWarm, .drive = 7,
      .modes = 6, .modeHz = { 110, 220, 330, 440, 550, 660 }, .modeT60 = .8f, .modeGain = .45f },
    // 31 Summit of Winds, Soaring Octave: a heroic octave soaring above, blooming.
    { .pitches = 1, .pitchRatio = { 2.0f }, .pitchGain = .45f, .pitchHp = 500, .bloomMs = 300 },
};
}

const char* Signature::kindName(int k) noexcept {
    static const auto names = [] {
        std::array<std::array<char, 32>, kinds> out {};
        const char* s = signatureChoices;
        for (auto& name : out) {
            std::size_t i = 0;
            while (*s != 0 && *s != '|') { if (i + 1 < name.size()) name[i++] = *s; ++s; }
            if (*s == '|') ++s;
        }
        return out;
    }();
    return names[static_cast<std::size_t>(std::clamp(k, 0, kinds - 1))].data();
}

float Signature::Lfo::next(int shape, float hz, double rate, Random& random) noexcept {
    phase += hz / rate;
    phase -= std::floor(phase);
    const float p = static_cast<float>(phase);
    switch (shape) {
        case sig::Sine: return std::sin(twoPi * p);
        case sig::Slosh: return 0.6f * std::sin(twoPi * p) + 0.4f * std::sin(twoPi * (1.618f * p) + 1.0f);
        case sig::SmoothRandom:
        case sig::Bubbling:
        case sig::Crooked: {
            if (--countdown <= 0) {
                const float period = shape == sig::Bubbling ? 0.5f + random.unit() : 0.7f + 0.6f * random.unit();
                countdown = std::max(1, static_cast<int>(rate * period / std::max(0.01f, hz)));
                target = random.bipolar();
            }
            if (hz != followHz || shape != followShape) {   // exp() only when the rate changes
                followHz = hz; followShape = shape;
                follow = timeCoefficient(shape == sig::Bubbling ? 0.022 : 0.6 / std::max(0.01f, hz), rate);
            }
            value += follow * (target - value);
            return shape == sig::Crooked ? 0.6f * std::sin(twoPi * p) + 0.5f * value : value;
        }
        default: return 0;
    }
}

void Signature::prepare(double sampleRate) {
    sr = sampleRate;
    amount.setTime(0.04, sr);
    for (auto& d : wobbleLine) d.prepare(static_cast<std::size_t>(0.03 * sr));
    for (auto& ch : combLines) for (auto& d : ch) d.prepare(static_cast<std::size_t>(0.07 * sr));
    for (auto& ch : shifters) for (auto& s : ch) s.prepare(sr, 0.045);
    for (auto& d : tapLine) d.prepare(static_cast<std::size_t>(2.3 * sr));
    for (auto& ch : smearLines) for (auto& d : ch) d.prepare(static_cast<std::size_t>(0.1 * sr));
    for (auto& ch : diffuserLines) for (auto& d : ch) d.prepare(static_cast<std::size_t>(0.05 * sr));
    for (auto& d : reverseLine) d.prepare(static_cast<std::size_t>(2.1 * sr));
    for (auto& sh : spiralShifters) sh.prepare(sr, 0.05);
    prismShifter.prepare(sr, 0.045);
    for (auto& sh : hueShifters) sh.prepare(sr, 0.05);
    for (auto& d : dc) d.prepare(sr);
    fastA = timeCoefficient(0.001, sr); fastR = timeCoefficient(0.06, sr);
    slowA = timeCoefficient(0.03, sr); slowR = timeCoefficient(0.3, sr);
    gateR = timeCoefficient(0.25, sr);
    bloomR = timeCoefficient(2.5, sr);
    tailDecay = 1.0f - timeCoefficient(0.05, sr);
    reset();
}

void Signature::reset() {
    for (auto& d : wobbleLine) d.clear();
    for (auto& ch : combLines) for (auto& d : ch) d.clear();
    for (auto& ch : combState) ch.fill(0);
    for (auto& ch : modes) for (auto& f : ch) f.clear();
    for (auto& ch : shifters) for (auto& s : ch) { s.clear(); s.setPhase(0); }
    for (auto& f : pitchHp) f.clear();
    for (auto& f : pitchLp) f.clear();
    for (auto& ch : formants) for (auto& f : ch) f.clear();
    for (auto& d : tapLine) d.clear();
    tapLpState0.fill(0); tapLpState1.fill(0); tapJitter.fill(1);
    for (auto& ch : smearLines) for (auto& d : ch) d.clear();
    for (auto& ch : diffuserLines) for (auto& d : ch) d.clear();
    for (auto& d : reverseLine) d.clear();
    for (auto& f : reverseLp) f.clear();
    for (std::size_t k = 0; k < spiralShifters.size(); ++k) { spiralShifters[k].clear(); spiralShifters[k].setPhase(0.37 * static_cast<double>(k)); }
    prismShifter.clear(); prismShifter.setPhase(0);
    for (auto& sh : hueShifters) { sh.clear(); sh.setPhase(0); }
    for (auto* f : { &spiralHpf, &prismSplit }) for (auto& v : *f) v.clear();
    for (auto& ch : hueFilters) for (auto& f : ch) f.clear();
    spiralPhase = prismPhase = huePhase = swirlPhase = 0; hueCountdown = 0;
    glitterGain.fill(0); pitchVoice.fill(0); serialBeforeTrem.fill(0); glitterTarget = 0; glitterPan = 0;
    for (auto* f : { &toneSvf, &growlLow, &growlSub }) for (auto& s : *f) s.clear();
    for (auto& b : toneEq) b.clear();
    for (auto& b : bite) b.clear();
    for (auto& d : dc) d.clear();
    growlPrev.fill(0); held.fill(0); holdCount.fill(0);
    growlFlip = 1;
    envFast = envSlow = envSwell = envBloom = gateEnv = 0;
    wobbleLfo = {}; wobbleLfoR = {}; wobbleLfoR.phase = 0.25; tremLfo = {};
    for (std::size_t k = 0; k < tapLfo.size(); ++k) { tapLfo[k] = {}; tapLfo[k].phase = 0.17 * static_cast<double>(k); }
    twinkle = { 1, 1 }; twinkleTarget = { 1, 1 }; twinkleCountdown = 0; crackleCountdown = 0;
    circlePhase = 0; reversePhase = 0;
    random.seed(0x5349474eu);
    kind = -1;
    amount.value = -1;   // not primed
}

void Signature::depart() noexcept {
    departing = true;
    heldOut = std::max(0.0f, amount.value);
}

void Signature::configure(int newKind, float newTone) noexcept {
    const bool kindChanged = newKind != kind;
    if (!kindChanged && std::abs(newTone - tone) < 1.0e-4f) return;
    if (kindChanged && kind >= 0) {
        // The engine dips the layer out for a kind change: start the new one clean.
        const float a = amount.value, t = amount.target;
        reset();
        amount.value = a; amount.target = t;
    }
    kind = newKind;
    tone = newTone;
    const auto& rc = sig::recipes[static_cast<std::size_t>(std::clamp(kind, 0, kinds - 1))];
    recipe = &rc;
    // Resonances move in semitones (no glide between pitches), filters tilt smoothly.
    toneShift = static_cast<float>(std::pow(2.0, std::round(tone * 12.0f) / 12.0));
    const double tilt = std::pow(2.0, static_cast<double>(tone));
    const auto keepSvf = [this](Svf& f, double hz, float damping) { const float s1 = f.s1, s2 = f.s2; f.set(hz, sr, damping); f.s1 = s1; f.s2 = s2; };
    for (int ch = 0; ch < 2; ++ch) {
        const auto C = static_cast<std::size_t>(ch);
        keepSvf(toneSvf[C], rc.toneHz * tilt, 1.0f / std::max(0.3f, rc.toneQ));
        if (rc.tone == sig::ToneBell) toneEq[C].peaking(sr, rc.toneHz * tilt, rc.toneQ, rc.toneDb);
        else if (rc.tone == sig::ToneShelf) toneEq[C].highShelf(sr, rc.toneHz * tilt, rc.toneDb);
        bite[C].peaking(sr, 1300.0 * tilt, 0.8, 4.0);
        keepSvf(growlLow[C], 280.0, 1.0f);
        keepSvf(growlSub[C], 420.0, 1.2f);
        keepSvf(pitchHp[C], rc.pitchHp * tilt, 1.0f);
        keepSvf(pitchLp[C], std::min(19000.0, rc.pitchLp * tilt), 1.0f);
        keepSvf(reverseLp[C], std::min(19000.0, rc.reverseLp * tilt), 1.0f);
        keepSvf(spiralHpf[C], rc.spiralHp * tilt, 1.0f);
        const double f1 = rc.formant == sig::FormantOh ? 450 : 750, f2 = rc.formant == sig::FormantOh ? 800 : 1200;
        formants[C][0].bandPass(sr, f1, 4.0);
        formants[C][1].bandPass(sr, f2, 5.0);
        for (int k = 0; k < rc.modes; ++k) {
            const auto K = static_cast<std::size_t>(k);
            const double hz = std::min(static_cast<double>(rc.modeHz[k] * toneShift), sr * 0.42);
            const double t60 = std::max(0.05, rc.modeT60 * (1.0 - 0.08 * k));
            const float damping = static_cast<float>(std::clamp(6.9078 / (pi * t60 * hz), 0.0005, 1.0));
            { auto& m = modes[C][K]; const float s1 = m.s1, s2 = m.s2; m.setResonant(hz * (ch == 0 ? 1.0 : 1.0015), sr, damping); m.s1 = s1; m.s2 = s2; }
            // Between unity peak gain (damping) and unity energy: narrow, long modes still
            // ring audibly from ordinary playing; the soft limit catches exact hits.
            modeGain[K] = std::sqrt(damping) * (rc.modeStruck ? 0.6f : 0.15f) / (1.0f + 0.25f * static_cast<float>(k)) * (hz < sr * 0.41 ? 1.0f : 0.0f);
        }
    }
    // The prism's two crossovers (its colour bands): low | mid | high of the shimmer.
    keepSvf(prismSplit[0], 1400.0 * tilt, 1.2f);
    keepSvf(prismSplit[1], 4200.0 * tilt, 1.2f);
    for (int k = 0; k < rc.combs; ++k)
        combSamples[static_cast<std::size_t>(k)] = static_cast<float>(std::clamp(sr / (rc.combHz[k] * toneShift), 2.0, 0.065 * sr));
    combCoefficient = 1.0f - rc.combDamp * 0.9f;
    for (int k = 0; k < maxTaps; ++k)
        tapCoefficient[static_cast<std::size_t>(k)] = poleForHz(std::min(19000.0, rc.tapLp * tilt * std::pow(rc.tapDarken, k)), sr);
    static constexpr float smearRatios[2][smearStages] = { { 1.0f, .73f, .53f, .37f }, { .91f, .67f, .47f, .41f } };
    static constexpr float diffuserMs[2][diffuserStages] = { { 13.7f, 7.3f, 4.1f }, { 11.9f, 8.3f, 3.7f } };
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < smearStages; ++k) {
            smearSamples[static_cast<std::size_t>(ch)][static_cast<std::size_t>(k)] = static_cast<float>(rc.smearMs * 0.001 * smearRatios[ch][k] * sr);
            if (k < diffuserStages) diffuserSamples[static_cast<std::size_t>(ch)][static_cast<std::size_t>(k)] = static_cast<float>(diffuserMs[ch][k] * 0.001 * sr);
        }
    swellA = timeCoefficient(std::max(0.001f, rc.swellMs * 0.001f), sr);
    bloomA = timeCoefficient(std::max(0.001f, rc.bloomMs * 0.001f), sr);
    const double driveDb = rc.drive;
    driveGain = dbToGain(static_cast<float>(driveDb));
    // Compensate the drive for a typical -12 dBFS signal so the serial blend stays level.
    driveComp = rc.sat == sig::SatNone ? 1.0f : 0.25f / std::max(1.0e-4f, std::tanh(driveGain * 0.25f));
}

void Signature::process(float* l, float* r, int n, const Frame& p, const Context&) noexcept {
    configure(std::clamp(static_cast<int>(std::lround(p[SigKind])), 0, kinds - 1), std::clamp(p[SigTone], -1.0f, 1.0f));
    amount.set(clamp01(p[SigAmount]));
    if (amount.value < 0) amount.snap(amount.target);
    const auto& rc = *recipe;
    if (kind == 0) return;
    const float rate = std::clamp(p[SigRate], 0.25f, 4.0f);
    const float wobbleCentre = rc.wobbleMs * 0.001f * static_cast<float>(sr) + 2.0f;
    const float wobbleDepth = rc.wobbleMs * 0.001f * static_cast<float>(sr);
    float* io[] = { l, r };

    for (int i = 0; i < n; ++i) {
        const float a = amount.next();
        // Departing (ringing out after a journey): the serial blend and the layers' input
        // fade with the amount, the layers' output stays, so their tails ring on.
        const float feed = departing ? (heldOut > 1.0e-5f ? std::min(1.0f, a / heldOut) : 0.0f) : 1.0f;
        const float layerAmount = departing ? heldOut : a;
        const float x[2] = { finiteOr(l[i]), finiteOr(r[i]) };
        const float level = std::max(std::abs(x[0]), std::abs(x[1]));
        envFast += (level > envFast ? fastA : fastR) * (level - envFast);
        envSlow += (level > envSlow ? slowA : slowR) * (level - envSlow);
        envSwell += (level > envSwell ? swellA : slowR) * (level - envSwell);
        envBloom += (level > envBloom ? bloomA : bloomR) * (level - envBloom);
        // Attack detector: the fast envelope well above the slow one.
        const float attack = clamp01((envFast - 1.4f * envSlow) / std::max(1.0e-5f, envFast));
        gateEnv = attack > gateEnv ? attack : gateEnv + gateR * (attack - gateEnv);

        const float wobble = rc.wobbleMs > 0 ? wobbleLfo.next(rc.wobbleShape, rc.wobbleHz * rate, sr, random) : 0.0f;
        const float wobbleR = rc.wobbleMs > 0 ? (rc.wobbleShape == sig::Sine || rc.wobbleShape == sig::Slosh
                                                     ? wobbleLfoR.next(rc.wobbleShape, rc.wobbleHz * rate, sr, random) : wobble) : 0.0f;
        float trem = 1;
        if (rc.tremDepth > 0) trem = 1.0f - rc.tremDepth * (0.5f + 0.5f * tremLfo.next(rc.tremShape, rc.tremHz * rate, sr, random));
        // Only real onsets (the peak well above the slower envelope) are softened, not the
        // beating of a sustained chord.
        const float swellGain = rc.swellMs > 0 ? std::min(1.0f, 1.4f * envSwell / std::max(1.0e-5f, envFast)) : 1.0f;

        // Pitch twinkle: each voice's level wanders (stars, ice).
        if (rc.pitchMode == sig::PitchTwinkle && --twinkleCountdown <= 0) {
            twinkleCountdown = static_cast<int>(sr * (0.05 + 0.12 * random.unit()) / rate);
            for (auto& t : twinkleTarget) t = random.unit() < 0.35f ? 1.0f : 0.15f * random.unit();
        }
        // Glitter: short grains of the pitch layer (15-60 ms), each at its own place in
        // the stereo field, scattered in time (fireflies).
        if (rc.pitchMode == sig::PitchGlitter) {
            if (--twinkleCountdown <= 0) {
                twinkleCountdown = static_cast<int>(sr * (0.015 + 0.045 * random.unit()) / rate);
                glitterTarget = random.unit() < 0.45f ? 1.0f : 0.0f;
                if (glitterTarget > 0) glitterPan = random.bipolar() * 0.85f;
            }
            for (auto& gg : glitterGain) gg += 0.02f * (glitterTarget - gg);
        }
        if (rc.tapMode == sig::TapsCrackle && --crackleCountdown <= 0) {
            crackleCountdown = static_cast<int>(sr * (0.008 + 0.03 * random.unit()));
            for (auto& j : tapJitter) j = 0.6f + 0.55f * random.unit();
        }
        for (int k = 0; k < 2; ++k) twinkle[static_cast<std::size_t>(k)] += 0.004f * (twinkleTarget[static_cast<std::size_t>(k)] - twinkle[static_cast<std::size_t>(k)]);

        float serial[2], layers[2] = { 0, 0 }, pre[2];
        for (int ch = 0; ch < 2; ++ch) {
            const auto C = static_cast<std::size_t>(ch);
            float y = x[ch];
            // Saturation flavours.
            switch (rc.sat) {
                case sig::SatWarm: {
                    const float b = 0.18f;
                    y = (std::tanh(driveGain * y + b) - std::tanh(b)) * driveComp;
                    y = dc[C].process(y);
                    break;
                }
                case sig::SatGrowl: {
                    const float low = growlLow[C].low(y);
                    if (low > 0 && growlPrev[C] <= 0 && ch == 0) growlFlip = -growlFlip;   // octave down: flip on every other cycle
                    growlPrev[C] = low;
                    const float sub = growlSub[C].low(growlFlip * std::min(1.0f, envFast * 1.5f) * 0.5f);
                    y = std::tanh(driveGain * (y + 0.2f * sub)) * driveComp;
                    y = dc[C].process(y);
                    break;
                }
                case sig::SatMenace: {
                    const float e = bite[C].process(y) * driveGain;
                    y = (e > 0 ? std::tanh(e) : std::tanh(1.5f * e) / 1.5f) * driveComp * 1.1f;
                    y = dc[C].process(y);
                    break;
                }
                case sig::SatGrit: {
                    y = std::tanh(driveGain * y) * driveComp;
                    if (++holdCount[C] >= 3) { holdCount[C] = 0; held[C] = y; }
                    y = 0.7f * y + 0.3f * held[C];
                    break;
                }
                default: break;
            }
            switch (rc.tone) {
                case sig::ToneLow: y = toneSvf[C].low(y); break;
                case sig::ToneHigh: y = toneSvf[C].highPass(y); break;
                case sig::ToneBell: case sig::ToneShelf: y = toneEq[C].process(y); break;
                default: break;
            }
            y *= swellGain;
            if (rc.wobbleMs > 0) {
                auto& line = wobbleLine[C];
                line.push(y);
                const float moved = line.read(wobbleCentre + wobbleDepth * (ch == 0 ? wobble : wobbleR));
                y = y + rc.wobbleMix * (moved - y);
            }
            serialBeforeTrem[ch] = y;
            if (!rc.tremLayersOnly && !rc.autoPan) y *= trem;
            serial[ch] = y;
            pre[ch] = y * feed;
        }

        // Parallel layers, fed by the serial result.
        for (int ch = 0; ch < 2; ++ch) {
            const auto C = static_cast<std::size_t>(ch);
            float add = 0;
            for (int k = 0; k < rc.combs; ++k) {
                const auto K = static_cast<std::size_t>(k);
                auto& line = combLines[C][K];
                const float out = line.read(combSamples[K] * (ch == 0 ? 1.0f : 1.003f));
                combState[C][K] += combCoefficient * (out - combState[C][K]);
                line.push(pre[ch] + rc.combFeedback * combState[C][K]);
                add += out * rc.combGain * (1.0f - rc.combFeedback) * 2.5f;
            }
            if (rc.modes > 0) {
                // Struck modes: the attack itself plus a short noise "mallet" scaled by
                // the playing, so they ring whatever notes are played.
                const float excite = rc.modeStruck ? (pre[ch] + random.bipolar() * envFast * 0.7f) * gateEnv * gateEnv : pre[ch];
                float ring = 0;
                for (int k = 0; k < rc.modes; ++k) ring += modeGain[static_cast<std::size_t>(k)] * modes[C][static_cast<std::size_t>(k)].bandPass(excite);
                add += rc.modeGain * 2.2f * softLimit(ring, 1.5f);
            }
            if (rc.pitches > 0) {
                float voice = 0;
                for (int k = 0; k < rc.pitches; ++k) {
                    const float v = shifters[C][static_cast<std::size_t>(k)].process(pre[ch], rc.pitchRatio[k] * (ch == 0 ? 1.0f : 1.002f));
                    voice += rc.pitchMode == sig::PitchTwinkle ? v * twinkle[static_cast<std::size_t>(k)] : v;
                }
                if (rc.pitchHp > 25) voice = pitchHp[C].highPass(voice);
                if (rc.pitchLp < 19000) voice = pitchLp[C].low(voice);
                if (rc.formant != sig::FormantNone) voice = 2.2f * (formants[C][0].process(voice) + 0.7f * formants[C][1].process(voice));
                if (rc.pitchMode == sig::PitchTransient) voice *= gateEnv;
                if (rc.pitchMode == sig::PitchGlitter) voice *= glitterGain[C] * (ch == 0 ? 1.0f - glitterPan : 1.0f + glitterPan);
                if (rc.bloomMs > 0) voice *= std::min(1.5f, envBloom / std::max(1.0e-5f, envFast));
                add += voice * rc.pitchGain;
                pitchVoice[C] = voice * rc.pitchGain;
            }
            if (rc.smear > 0) {
                // Allpass smear: blurs attacks into a wash.
                float s = pre[ch] + 0.35f * add;
                for (int k = 0; k < smearStages; ++k) {
                    auto& line = smearLines[C][static_cast<std::size_t>(k)];
                    const float d = line.read(smearSamples[C][static_cast<std::size_t>(k)]);
                    const float v = s + 0.62f * d;
                    line.push(v);
                    s = d - 0.62f * v;
                }
                add += rc.smear * s;
            }
            if (rc.reverseMs > 0) {
                // Two reverse heads, sin^2 windows half a segment apart.
                auto& line = reverseLine[C];
                line.push(pre[ch]);
                const float length = rc.reverseMs * 0.001f * static_cast<float>(sr);
                float rev = 0;
                for (int head = 0; head < 2; ++head) {
                    double ph = reversePhase + 0.5 * head;
                    ph -= std::floor(ph);
                    const float w = std::sin(dsp::twoPi * 0.5f * static_cast<float>(ph));
                    rev += line.read(1.0f + 2.0f * length * static_cast<float>(ph)) * w * w;
                }
                add += reverseLp[C].low(rev) * rc.reverseGain;
            }
            layers[ch] = add;
        }
        // ---- LIGHT SEEKER primitives (mono-fed, stereo out).
        const float mono = 0.5f * (pre[0] + pre[1]);
        if (rc.spiralGain > 0) {
            // Three voices a third of a cycle apart, each rising through an octave
            // (ratio 1 -> 2) and faded by a sin^2 window over its cycle: an endless climb.
            spiralPhase += rc.spiralHz * rate / sr;
            spiralPhase -= std::floor(spiralPhase);
            float sl = 0, sr2 = 0;
            for (int k = 0; k < spiralVoices; ++k) {
                double ph = spiralPhase + static_cast<double>(k) / spiralVoices;
                ph -= std::floor(ph);
                const float ratio = static_cast<float>(std::exp2(ph)) * 1.0f;
                const float w = std::sin(dsp::pi * static_cast<float>(ph));
                const float v = spiralShifters[static_cast<std::size_t>(k)].process(mono, ratio * 1.4983f) * w * w;   // starts a fifth up
                const float pan = -0.8f + 1.6f * static_cast<float>(ph);   // climbs from left to right
                sl += v * (0.5f - 0.5f * pan);
                sr2 += v * (0.5f + 0.5f * pan);
            }
            sl = spiralHpf[0].highPass(sl); sr2 = spiralHpf[1].highPass(sr2);
            layers[0] += rc.spiralGain * 1.6f * sl;
            layers[1] += rc.spiralGain * 1.6f * sr2;
        }
        if (rc.prismGain > 0) {
            // One octave-up shimmer split into three colour bands, each band at its own
            // place in the stereo field, the bands slowly turning.
            const float up = prismShifter.process(mono, 2.0f);
            float bandLow = 0, bandHigh = 0;
            const float low = prismSplit[0].process(up, bandLow, bandHigh);
            float b2 = 0, h2 = 0;
            const float mid = prismSplit[1].process(bandHigh, b2, h2) ;
            const float bands[3] = { low, mid, h2 };
            prismPhase += rc.prismHz * rate / sr;
            prismPhase -= std::floor(prismPhase);
            for (int k = 0; k < 3; ++k) {
                const float pan = std::sin(twoPi * static_cast<float>(prismPhase + k / 3.0));
                const float angle = (pan + 1.0f) * 0.25f * static_cast<float>(pi);
                layers[0] += rc.prismGain * 1.3f * bands[k] * std::cos(angle);
                layers[1] += rc.prismGain * 1.3f * bands[k] * std::sin(angle);
            }
        }
        if (rc.hueGain > 0) {
            // A shimmer an octave up seen through two resonant band-passes that sweep
            // slowly through the spectrum in opposite directions: its colour changes.
            if (--hueCountdown <= 0) {
                hueCountdown = 32;
                huePhase += 32.0 * rc.hueHz * rate / sr;
                huePhase -= std::floor(huePhase);
                const double s0 = 0.5 + 0.5 * std::sin(dsp::twoPi * huePhase);
                const double c1 = 700.0 * std::pow(9.0, s0), c2 = 700.0 * std::pow(9.0, 1.0 - s0);
                for (int ch = 0; ch < 2; ++ch) {
                    auto keep = [this](Svf& f, double hz) { const float s1 = f.s1, s2 = f.s2; f.set(hz, sr, 0.35f); f.s1 = s1; f.s2 = s2; };
                    keep(hueFilters[static_cast<std::size_t>(ch)][0], ch == 0 ? c1 : c2);
                    keep(hueFilters[static_cast<std::size_t>(ch)][1], ch == 0 ? c2 * 1.5 : c1 * 1.5);
                }
            }
            for (int ch = 0; ch < 2; ++ch) {
                const auto C = static_cast<std::size_t>(ch);
                const float up = hueShifters[C].process(pre[ch], ch == 0 ? 2.0f : 2.003f);
                float b1 = 0, h1 = 0, b2 = 0, h2 = 0;
                hueFilters[C][0].process(up, b1, h1);
                hueFilters[C][1].process(up, b2, h2);
                layers[ch] += rc.hueGain * 1.8f * (b1 + 0.7f * b2);
            }
        }
        if (rc.reverseMs > 0) {
            reversePhase += 1.0 / (rc.reverseMs * 0.001 * sr);
            reversePhase -= std::floor(reversePhase);
        }

        if (rc.taps > 0) {
            tapLine[0].push(rc.tapsFromPitch ? 2.0f * pitchVoice[0] : pre[0]);
            tapLine[1].push(rc.tapsFromPitch ? 2.0f * pitchVoice[1] : pre[1]);
            if (rc.tapMode == sig::TapsCircle) { circlePhase += rc.tapRate * rate / sr; circlePhase -= std::floor(circlePhase); }
            float tl = 0, tr = 0;
            for (int k = 0; k < rc.taps; ++k) {
                const auto K = static_cast<std::size_t>(k);
                float samples = rc.tapMs[k] * 0.001f * static_cast<float>(sr);
                if (rc.tapMode == sig::TapsScatter) samples *= 1.0f + 0.012f * tapLfo[K].next(sig::SmoothRandom, rc.tapRate * rate, sr, random);
                // Each channel's echo comes from the same channel (mixing left and right
                // would make stereo-detuned material beat); the pan is a balance.
                tapLpState0[K] += tapCoefficient[K] * (tapLine[0].read(samples) - tapLpState0[K]);
                tapLpState1[K] += tapCoefficient[K] * (tapLine[1].read(samples) - tapLpState1[K]);
                float g = rc.tapGain[k];
                if (rc.tapMode == sig::TapsCrackle) g *= tapJitter[K];
                float pan = rc.tapPan[k];
                if (rc.tapMode == sig::TapsCircle) pan = std::sin(twoPi * static_cast<float>(circlePhase + k / 6.0));
                const float angle = (pan + 1.0f) * 0.25f * static_cast<float>(pi);
                tl += tapLpState0[K] * g * std::cos(angle) * 1.41f;
                tr += tapLpState1[K] * g * std::sin(angle) * 1.41f;
            }
            if (rc.tapMode == sig::TapsDiffuse) {
                float* both[2] = { &tl, &tr };
                for (int ch = 0; ch < 2; ++ch)
                    for (int k = 0; k < diffuserStages; ++k) {
                        auto& line = diffuserLines[static_cast<std::size_t>(ch)][static_cast<std::size_t>(k)];
                        const float d = line.read(diffuserSamples[static_cast<std::size_t>(ch)][static_cast<std::size_t>(k)]);
                        const float v = *both[ch] + 0.6f * d;
                        line.push(v);
                        *both[ch] = d - 0.6f * v;
                    }
            }
            layers[0] += tl;
            layers[1] += tr;
        }
        const float layerGain = rc.tremLayersOnly ? trem : 1.0f;
        float out[2];
        for (int ch = 0; ch < 2; ++ch)
            out[ch] = x[ch] + a * (serial[ch] - x[ch]) + layerAmount * layers[ch] * layerGain;
        if (rc.autoPan && rc.tremDepth > 0) {
            // The tremolo as a pan: what leaves one side arrives on the other (equal power).
            const float m = 1.0f - trem;   // 0 .. depth
            const float side = rc.tremDepth > 0 ? std::clamp(m / rc.tremDepth * 2.0f - 1.0f, -1.0f, 1.0f) : 0.0f;
            const float angle = (1.0f + a * rc.tremDepth * side) * 0.25f * static_cast<float>(pi);
            const float dryL = serialBeforeTrem[0], dryR = serialBeforeTrem[1];
            const float panned = 0.5f * (dryL + dryR);
            out[0] += a * (panned * std::cos(angle) * 1.41f - serial[0]);
            out[1] += a * (panned * std::sin(angle) * 1.41f - serial[1]);
        }
        if (rc.swirlDepth > 0) {
            // The stereo image turns around the listener (a rotation of left/right).
            swirlPhase += rc.swirlHz * rate / sr;
            swirlPhase -= std::floor(swirlPhase);
            const float theta = a * rc.swirlDepth * 0.9f * std::sin(twoPi * static_cast<float>(swirlPhase));
            const float c = std::cos(theta), sn = std::sin(theta);
            const float l0 = out[0], r0 = out[1];
            out[0] = c * l0 - sn * r0;
            out[1] = sn * l0 + c * r0;
        }
        for (int ch = 0; ch < 2; ++ch) io[ch][i] = finiteOr(out[ch]);
        tail = std::max(std::abs(layers[0]), std::abs(layers[1])) * layerAmount + tailDecay * tail;
    }
}
}
