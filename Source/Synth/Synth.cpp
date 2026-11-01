#include "Synth.h"
#include <algorithm>
#include <cmath>

namespace ls::inst {
using namespace lswb::dsp;

namespace {
constexpr float lowestHz = 24.0f;

// Mode sets (ratio to the note, decay relative to the note's T60, gain).
struct ModeSet { int count; std::array<float, 8> ratio, decay, gain; };
const std::array<ModeSet, ModesCount>& modeSets() {
    static const std::array<ModeSet, ModesCount> sets { {
        // Wind chime: free tubes (uniform free bar), long and pure.
        { 4, { 1.0f, 2.756f, 5.404f, 8.933f }, { 1.0f, 0.55f, 0.3f, 0.15f }, { 1.0f, 0.45f, 0.22f, 0.1f } },
        // Wine glass.
        { 4, { 1.0f, 2.32f, 4.25f, 6.63f }, { 1.0f, 0.6f, 0.35f, 0.2f }, { 1.0f, 0.35f, 0.18f, 0.08f } },
        // Glockenspiel: steel bars (free bar), bright upper modes dying fast.
        { 4, { 1.0f, 2.756f, 5.404f, 8.933f }, { 1.0f, 0.35f, 0.15f, 0.07f }, { 1.0f, 0.5f, 0.3f, 0.12f } },
        // Celesta: steel bars over tuned resonators (the octave and the bar's tuned overtone).
        { 4, { 1.0f, 2.0f, 3.98f, 10.6f }, { 1.0f, 0.55f, 0.3f, 0.08f }, { 1.0f, 0.22f, 0.16f, 0.04f } },
        // A tuned bell with a major tierce (an uplifting bell): hum, prime, tierce, quint,
        // nominal and the upper partials.
        { 8, { 0.5f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f }, { 1.3f, 1.0f, 0.75f, 0.6f, 0.5f, 0.35f, 0.25f, 0.18f },
             { 0.35f, 1.0f, 0.4f, 0.3f, 0.45f, 0.18f, 0.12f, 0.06f } },
        // Glass harmonica (rubbed bowls): almost pure, a faint octave and twelfth.
        { 4, { 1.0f, 2.0f, 3.0f, 4.25f }, { 1.0f, 0.8f, 0.6f, 0.4f }, { 1.0f, 0.16f, 0.06f, 0.03f } },
        // A soft harmonic chime.
        { 6, { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f }, { 1.0f, 0.7f, 0.5f, 0.38f, 0.28f, 0.2f }, { 1.0f, 0.45f, 0.25f, 0.15f, 0.08f, 0.05f } },
        // Crystal: bright, sweet, a little inharmonic.
        { 6, { 1.0f, 2.0f, 2.756f, 4.0f, 5.404f, 6.63f }, { 1.0f, 0.8f, 0.55f, 0.45f, 0.3f, 0.22f }, { 1.0f, 0.55f, 0.3f, 0.28f, 0.14f, 0.08f } },
    } };
    return sets;
}

// A plucked or bowed instrument's body: a few resonant modes and the direct sound.
struct Body {
    std::array<Svf, 5> mode;
    std::array<float, 5> gain {};
    int count = 0;
    float direct = 1;
    void setPluck(int type, float rate) {
        count = 0;
        direct = 1;
        if (type <= BodyNone || type >= BodyCount) return;
        const auto& spec = bodies[std::size_t(type)];
        direct = spec.direct;
        for (const auto& m : spec.mode) {
            mode[std::size_t(count)].set(m.hz, m.q, rate);
            mode[std::size_t(count)].reset();
            gain[std::size_t(count)] = m.gain;
            ++count;
        }
    }
    void setViolin(int size, float rate) {
        const float scale = stringBodyScale[std::size_t(std::clamp(size, 0, int(StringSizeCount) - 1))];
        count = 0;
        direct = 0.45f;
        for (const auto& m : violinBody) {
            mode[std::size_t(count)].set(m.hz * (m.hz > 2000.0f ? 1.0f : scale), m.q, rate);
            mode[std::size_t(count)].reset();
            gain[std::size_t(count)] = m.gain;
            ++count;
        }
    }
    float process(float x) {
        if (count == 0) return x;
        float y = x * direct;
        for (int k = 0; k < count; ++k) y += gain[std::size_t(k)] * mode[std::size_t(k)].bandpass(x);
        return y;
    }
};

// The developer's recordings, played as WORLDBUILDER's SamplePart plays them: a cubic
// read of the nearest recorded note (velocity layer, round-robin take), tuned to the key,
// looping its sustain, crossing into its recorded release when let go.
struct SampleVoice {
    const lswb::samples::Zone* zone = nullptr;
    double pos = 0, tailPos = -1;
    float rateRatio = 1, inc = 1, midi = 60, bend = 0, vibrato = 0, vibratoHz = 5, vibratoDelay = 0.3f, vibratoPhase = 0, age = 0;
    float amp = 1, attackStep = 1, fade = 1, fadeMul = 1, releaseMul = 1, tailMix = 0, tailStep = 0, gain = 1, rate = 48000;
    bool on = false, releasing = false;
    static float read(const int16_t* d, double p) {
        const auto i = int(p);
        const float t = float(p - double(i));
        const float xm1 = d[i - 1], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return (((c3 * t + c2) * t + c1) * t + x0) * (1.0f / 32768.0f);
    }
    bool start(const lswb::samples::Instrument& inst, float note, float velocity, float pick, float attack, float sampleRate, float skip,
               float decayT60, float even) {
        rate = sampleRate;
        zone = inst.find(int(std::lround(note)), velocity, pick);
        if (zone == nullptr || zone->length < 16) return on = false;
        midi = note;
        rateRatio = inst.rate / rate;
        pos = 1.0 + double(std::min(skip * inst.rate, float(zone->length) * 0.25f));
        if (zone->loops() && pos >= double(zone->loopStart)) pos = double(zone->loopStart);
        tailPos = -1;
        tailMix = 0;
        gain = dbToGain(std::clamp(even * (inst.medianDb(zone->layer) - zone->loudnessDb), -12.0f, 12.0f));
        amp = attack > 0.0005f ? 0.0f : 1.0f;
        attackStep = attack > 0.0005f ? 1.0f / (attack * rate) : 1.0f;
        fade = 1;
        fadeMul = decayT60 > 0.0f ? std::exp(-6.9078f / (decayT60 * rate)) : 1.0f;
        releaseMul = 1;
        releasing = false;
        age = 0;
        vibratoPhase = 0;
        update();
        return on = true;
    }
    void release(float seconds) {
        if (!on || releasing) return;
        releasing = true;
        const auto& z = *zone;
        if (z.release > 0 && pos < double(z.release)) {
            // The bow lifts: the recorded tail, faded in over 40 ms, a slower fade on top.
            tailPos = double(z.release);
            tailMix = 0;
            tailStep = 1.0f / (0.04f * rate);
            releaseMul = std::exp(-6.9078f / (std::max(seconds * 2.0f, 0.05f) * rate));
        } else {
            releaseMul = std::exp(-6.9078f / (std::max(seconds, 0.01f) * rate));
        }
    }
    void update() {
        if (!on) return;
        age += float(chunk) / rate;
        float semis = midi - zone->root + bend;
        if (vibrato > 0.0f) {
            vibratoPhase += vibratoHz * float(chunk) / rate;
            vibratoPhase -= std::floor(vibratoPhase);
            semis += vibrato * smoothstep(vibratoDelay, vibratoDelay + 0.4f, age) * sinCycles(vibratoPhase);
        }
        inc = rateRatio * std::exp2(semis / 12.0f);
    }
    float tick() {
        if (!on) return 0.0f;
        const auto& z = *zone;
        float y = read(z.data, pos);
        if (tailPos >= 0.0) {
            y = y * (1.0f - tailMix) + read(z.data, tailPos) * tailMix;
            tailPos += double(inc);
            tailMix += tailStep;
            if (tailMix >= 1.0f) {
                pos = tailPos;
                tailPos = -1;
            }
        }
        pos += double(inc);
        if (z.loops() && !releasing && pos >= double(z.loopEnd)) pos -= double(z.loopEnd - z.loopStart);
        if (tailPos >= double(z.length - 2)) tailPos = -1, tailMix = 0, pos = double(z.length);
        if (amp < 1.0f) amp = std::min(1.0f, amp + attackStep);
        fade *= fadeMul * releaseMul;
        if (pos >= double(z.length - 2) || fade < 1.0e-4f) on = false;
        return y * amp * fade * gain;
    }
};

// A pipe organ: its ranks (16', 8', 4', 2 2/3', 2' and a mixture of the 1 1/3' and 1')
// as sines, each pipe a few cents from the others (a big organ never beats exactly), a
// chiff of wind as each pipe speaks, a breath of wind under the tone. Pipes speak in
// about 30-60 ms (the lower ones later) and stop as quickly.
struct OrganVoice {
    static constexpr int ranks = 7;
    static constexpr std::array<float, ranks> ratio { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };
    std::array<Phasor, ranks> pipe;
    std::array<float, ranks> level {}, detune {};
    Env env;
    Svf chiffBand;
    OnePole windLp;
    float hz = 220, bend = 0, chiff = 0.3f, chiffLevel = 0, chiffDecay = 0.99f, rate = 48000, gain = 1;
    uint64_t noiseIndex = 0;
    bool active() const { return env.active(); }
    void reset() { env.stop(); chiffBand.reset(); windLp.reset(); }
    void start(float noteHz, const std::array<float, 6>& stops, float chiffAmount, float attack, float sampleRate, uint64_t noiseAt) {
        rate = sampleRate;
        hz = noteHz;
        float sum = 0;
        for (int k = 0; k < ranks; ++k) {
            const float stop = k < 5 ? stops[std::size_t(k)] : stops[5] * (k == 5 ? 0.7f : 0.5f);
            const float f = hz * ratio[std::size_t(k)];
            level[std::size_t(k)] = f < 0.42f * rate && f > 20.0f ? stop : 0.0f;
            sum += level[std::size_t(k)];
            detune[std::size_t(k)] = 1.0f + 0.0006f * float((k * 7) % 5 - 2);
            if (!env.active()) pipe[std::size_t(k)].setPhase(0.9f * float(k));
        }
        const float norm = 1.0f / std::max(0.6f, std::sqrt(sum));
        for (auto& l : level) l *= norm;
        chiff = chiffAmount;
        chiffLevel = chiff * 0.5f;
        chiffDecay = std::exp(-1.0f / (0.035f * rate));
        chiffBand.set(std::min(hz * 3.0f, 0.4f * rate), 3.0f, rate);
        windLp.setCutoff(2500.0f, rate);
        noiseIndex = noiseAt;
        // Low pipes speak a little later.
        env.trigger(attack * std::clamp(std::pow(262.0f / hz, 0.3f), 0.7f, 1.6f), 1.0f, 1.0f, rate);
        update();
    }
    void release(float seconds) { env.release(seconds, rate); }
    void update() {
        const float b = std::exp2(bend / 12.0f);
        for (int k = 0; k < ranks; ++k) {
            pipe[std::size_t(k)].setFrequency(std::min(hz * ratio[std::size_t(k)] * detune[std::size_t(k)] * b, 0.45f * rate), rate);
            pipe[std::size_t(k)].normalise();
        }
    }
    float tick(const Stream& noise) {
        const float e = env.tick();
        if (e <= 0.0f) return 0.0f;
        float y = 0;
        for (int k = 0; k < ranks; ++k) {
            pipe[std::size_t(k)].step();
            y += level[std::size_t(k)] * pipe[std::size_t(k)].im;
        }
        const float n = noise.white(noiseIndex++);
        y += chiffBand.bandpass(n) * chiffLevel + windLp.lp(n) * 0.004f;
        chiffLevel *= chiffDecay;
        return y * e * gain;
    }
};

// Grace notes climbing above the note (Cloud Steps' glissando): a major pentatonic.
constexpr std::array<float, 8> runSteps { 2, 4, 7, 9, 12, 14, 16, 19 };
} // namespace

struct Synth::Voice {
    struct LayerVoice {
        Engine engine = Engine::None;
        bool on = false;
        SynthVoice synth;
        Pluck pluck;
        lswb::dsp::Bell bell;
        TineVoice tine;
        FmVoice fm;
        ChoirVoice choir;
        WindVoice wind;
        BowedVoice bowed;
        StringVoice strings;
        SampleVoice sample;
        OrganVoice organ;
        Body body;
        Svf lowpass;
        OnePole highpass;
        PanGains pan { 0.7071f, 0.7071f };
        float gain = 1, cutoff = 18000, release = 0.3f, fade = 1, fadeMul = 1, vibrato = 0, hz = 440;
        bool character = false, stretchRelease = true;
        float bent = 0;
    };
    bool on = false, keyDown = false, sustained = false, released = false, grace = false;
    int note = -1, patch = 0, delay = 0, layerMask = 0;
    float velocity = 0, semis = 0, levelDb = 0, kill = 1, killStep = 0;
    uint32_t serial = 0;
    std::array<LayerVoice, Synth::maxLayers> layer;
};

struct Synth::Bus {
    std::array<OnePole, 2> shelf;
    Chorus chorus;
    lswb::dsp::Shimmer shimmer;
    Smoother shelfGain, chorusMix, shimmerSend, trim, tremolo;
    float tremoloPhase = 0;
};

namespace {
bool layerAvailable(const Layer& l) noexcept {
    return l.engine == Engine::Sample && l.sample != nullptr && lswb::samples::Bank::get().instrument(l.sample) != nullptr;
}
} // namespace

bool patchUsesRecordings(const Patch& p) noexcept {
    for (int k = 0; k < p.layerCount; ++k)
        if (p.layers[std::size_t(k)].engine == Engine::Sample) return true;
    return false;
}

bool patchSampled(const Patch& p) noexcept {
    bool any = false;
    for (int k = 0; k < p.layerCount; ++k) {
        const auto& l = p.layers[std::size_t(k)];
        if (l.engine != Engine::Sample) continue;
        if (!layerAvailable(l)) return false;
        any = true;
    }
    return any;
}

Synth::Synth() : bus(std::make_unique<Bus>()) {
    voices.reserve(maxVoices);
    for (int i = 0; i < maxVoices; ++i) voices.push_back(std::make_unique<Voice>());
    noise.setKey(0x51A7u, 77);
}

Synth::~Synth() = default;

void Synth::prepare(double sampleRate) {
    rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
    const auto r = float(rate);
    for (auto& v : voices)
        for (auto& l : v->layer) {
            l.pluck.prepare(r, lowestHz);
            l.strings.prepare(r, lowestHz);
        }
    bus->chorus.prepare(r);
    bus->shimmer.prepare(r);
    for (auto& s : bus->shelf) s.setCutoff(2800.0f, r);
    const float controlRate = r / float(chunk);
    for (auto* s : { &bus->shelfGain, &bus->chorusMix, &bus->shimmerSend, &bus->trim, &bus->tremolo }) s->setTime(0.08f, controlRate);
    reset();
}

void Synth::reset() noexcept {
    for (auto& v : voices) {
        v->on = false;
        for (auto& l : v->layer) {
            l.on = false;
            l.synth.reset();
            l.pluck.reset();
            l.bell.reset();
            l.tine.reset();
            l.fm.reset();
            l.choir.reset();
            l.wind.reset();
            l.bowed.reset();
            l.strings.reset();
            l.sample.on = false;
            l.organ.reset();
        }
    }
    keys = {};
    pedal = false;
    bendNow = modNow = 0;
    chunkLeft = 0;
    bus->chorus.clear();
    bus->shimmer.clear();
    for (auto& s : bus->shelf) s.reset();
    const auto& p = patch(current);
    bus->shelfGain.snap(dbToGain(p.air));
    bus->chorusMix.snap(p.chorus);
    bus->shimmerSend.snap(p.shimmer);
    bus->trim.snap(dbToGain(macros.levelDb));
    bus->tremolo.snap(0);
}

void Synth::setMacros(const Macros& m) noexcept {
    macros = m;
    macros.tone = clamp01(m.tone);
    macros.attack = clamp01(m.attack);
    macros.release = clamp01(m.release);
    macros.character = clamp01(m.character);
    macros.octave = std::clamp(m.octave, -3, 3);
    macros.levelDb = std::isfinite(m.levelDb) ? std::clamp(m.levelDb, -48.0f, 12.0f) : 0.0f;
}

int Synth::activeVoices() const noexcept {
    int n = 0;
    for (const auto& v : voices) n += v->on ? 1 : 0;
    return n;
}

int Synth::heldKeys() const noexcept {
    int n = 0;
    for (const auto& k : keys) n += (k.down || k.sustained) ? 1 : 0;
    return n;
}

void Synth::kill(Voice& v, float seconds) noexcept {
    if (!v.on) return;
    v.killStep = 1.0f / std::max(1.0f, seconds * float(rate));
    v.keyDown = v.sustained = false;
    v.released = true;
}

Synth::Voice* Synth::freeVoice(int forPatch, bool enforceLimit) noexcept {
    // The patch's polyphony (played notes; grace notes do not count): beyond it, its
    // oldest note (released first) fades out quickly.
    const int limit = enforceLimit ? patch(forPatch).polyphony : maxVoices + 1;
    int count = 0;
    Voice* oldest = nullptr;
    Voice* oldestReleased = nullptr;
    for (auto& v : voices) {
        if (!v->on || v->grace || v->patch != forPatch || v->killStep > 0.0f) continue;
        ++count;
        if (oldest == nullptr || v->serial < oldest->serial) oldest = v.get();
        if (v->released && (oldestReleased == nullptr || v->serial < oldestReleased->serial)) oldestReleased = v.get();
    }
    if (count >= limit) kill(oldestReleased != nullptr ? *oldestReleased : *oldest, 0.012f);
    for (auto& v : voices)
        if (!v->on) return v.get();
    // Every voice busy (rare: long tails and runs): the quietest, oldest released one.
    Voice* best = nullptr;
    float quietest = 1.0e30f;
    for (auto& v : voices) {
        float level = v->released ? 0.0f : 1.0f;
        level += float(v->serial) * 1.0e-9f;
        if (level < quietest) { quietest = level; best = v.get(); }
    }
    return best;
}

void Synth::startLayers(Voice& voice) noexcept {
    const auto& p = patch(voice.patch);
    // The instrument's level trim travels with its notes (the old instrument's notes ring
    // out at their own level while the new one's start at theirs).
    const float trimDb = patchUsesRecordings(p) && !patchSampled(p) ? p.standInTrimDb : p.trimDb;
    const int note = voice.note;
    const float velocity = voice.velocity;
    const float graceRing = voice.grace ? 0.7f : 1.0f;
    const float r = float(rate);
    const float attackScale = std::exp2((macros.attack - 0.5f) * 4.0f);
    const float ringScale = std::exp2((macros.release - 0.5f) * 2.0f);
    const float toneScale = std::exp2((macros.tone - 0.5f) * 3.0f);
    for (int k = 0; k < p.layerCount; ++k) {
        if ((voice.layerMask & (1 << k)) == 0) continue;
        const auto& L = p.layers[std::size_t(k)];
        auto& lv = voice.layer[std::size_t(k)];
        const float midi = float(note + 12 * macros.octave) + L.transpose + L.cents * 0.01f + voice.semis;
        if (midi - L.cents * 0.01f < float(L.lowKey) - 0.01f || midi - L.cents * 0.01f > float(L.highKey) + 0.01f) continue;
        const float hz = 440.0f * std::exp2((midi - 69.0f) / 12.0f);
        if (hz >= 0.45f * r) continue;
        const float vel = clamp01(velocity);
        const float attack = L.attack * attackScale;
        const uint64_t noiseAt = uint64_t(voice.serial) * 0x9E3779B97F4A7C15ull + uint64_t(k) * 0x1000;
        lv.engine = L.engine;
        lv.hz = hz;
        lv.vibrato = L.vibrato;
        lv.character = L.character;
        lv.release = L.release;
        lv.fade = 1;
        lv.fadeMul = 1;
        lv.bent = 0;
        lv.body.count = 0;
        const float hardness = clamp01(L.p[0] * (1.15f - 0.3f * macros.attack) + 0.25f * (vel - 0.6f));
        switch (L.engine) {
            case Engine::Brass:
            case Engine::Analog:
            case Engine::Reed: {
                auto& s = lv.synth;
                if (!s.active()) s.reset();
                s.model = L.engine == Engine::Brass ? SynthBrass : (L.engine == Engine::Reed ? SynthReed : SynthAnalog);
                s.velocity = vel;
                s.glide = 0;
                s.bend = bendNow * p.bendRange;
                s.vibratoDepth = L.vibrato;
                s.vibratoHz = L.vibratoHz;
                s.vibratoDelay = L.vibratoDelay;
                s.gain = 1;
                s.pan = { 0.7071f, 0.7071f };
                s.air.setKey(voice.serial, uint32_t(k));
                s.scoopScale = 1;
                s.plainScoop = 0;
                if (L.engine == Engine::Brass) {
                    s.formantHz = L.p[0];
                    s.formantLevel = L.p[1];
                    s.breath = L.p[2];
                    s.detuneCents = L.p[3];
                    s.cutoff = 1000.0f * std::exp2((L.bright - 0.5f) * 2.0f);
                } else if (L.engine == Engine::Reed) {
                    s.cutoff = L.p[0];
                    s.formantHz = L.p[1];
                    s.formantLevel = 0.5f;
                    s.breath = L.p[2];
                    s.detuneCents = L.p[3];
                    s.envOctaves = 1.0f;
                } else {
                    s.cutoff = L.p[0] * std::exp2(0.8f * (vel - 0.7f));
                    s.filterEnvOctaves = L.p[1];
                    s.filterDecay = L.p[2] > 0.0f ? std::exp(-float(chunk) / (L.p[2] * r)) : 0.0f;
                    s.detuneCents = L.p[3];
                    s.subLevel = L.p[4];
                    s.width = L.p[5];
                    s.envOctaves = L.bright * 2.0f;
                    s.q = 0.7f;
                }
                s.start(hz, attack, L.decay, L.sustain, r);
                s.update(r);
                break;
            }
            case Engine::Pluck: {
                Pluck::Shape shape;
                shape.position = L.p[0];
                shape.loss = L.p[1];
                shape.stiffness = L.p[2];
                shape.course = L.p[3];
                shape.courseCents = 3.0f;
                shape.pickNoise = L.p[4];
                lv.pluck.start(hz, L.ring * ringScale * graceRing, clamp01(L.bright * (1.15f - 0.3f * macros.attack)), vel, 0.0f, r, noiseAt, shape);
                lv.body.setPluck(L.model, r);
                break;
            }
            case Engine::Bell: {
                const auto& set = modeSets()[std::size_t(std::clamp(L.model, 0, int(ModesCount) - 1))];
                lv.bell.extras = {};
                lv.bell.extras.beatHz = L.p[1];
                lv.bell.extras.variation = L.p[2];
                lv.bell.start(hz, L.ring * ringScale * graceRing, hardness, 0.5f, 0.0f, r, set.ratio.data(), set.decay.data(), set.gain.data(), set.count);
                break;
            }
            case Engine::Tine:
                lv.tine.gainLevel = 1;
                lv.tine.start(hz, L.ring * ringScale * graceRing, clamp01(L.bright * (1.15f - 0.3f * macros.attack)), vel, r);
                lv.tine.update();
                break;
            case Engine::Fm: {
                auto& f = lv.fm;
                if (!f.active()) f.reset();
                f.ratio = L.p[0];
                f.index = L.p[1] * (0.5f + 0.7f * vel);
                f.indexFloor = L.p[3];
                f.feedback = L.p[4];
                f.glide = 0;
                f.vibratoDepth = L.vibrato;
                f.vibratoHz = L.vibratoHz;
                f.gain = 1;
                f.bend = bendNow * p.bendRange;
                f.start(hz, attack, L.decay, L.sustain, r, L.p[2]);
                f.update(r);
                break;
            }
            case Engine::Choir: {
                auto& c = lv.choir;
                if (!c.active()) c.reset();
                c.vowel = L.p[0];
                c.breath = L.p[1];
                c.detuneCents = L.p[2];
                c.brightness = L.bright;
                c.velocity = vel;
                c.vibratoDepth = L.vibrato;
                c.vibratoHz = L.vibratoHz;
                c.glide = 0;
                c.gain = 1;
                c.bend = bendNow * p.bendRange;
                c.start(hz, attack, r);
                c.update(r);
                break;
            }
            case Engine::Wind: {
                auto& w = lv.wind;
                if (!w.active()) w.reset();
                w.h2 = L.p[0];
                w.h3 = L.p[1];
                w.h4 = L.p[2];
                w.h5 = L.p[3];
                w.breath = L.p[4];
                w.air = L.p[4] * 0.5f;
                w.chiff = L.p[5];
                w.velocity = vel;
                w.vibratoDepth = L.vibrato;
                w.vibratoHz = L.vibratoHz;
                w.vibratoDelay = L.vibratoDelay;
                w.wander = 2.0f;
                w.gain = 1;
                w.bend = bendNow * p.bendRange;
                w.start(hz, attack, 0.25f, r, noiseAt);
                w.update(r);
                break;
            }
            case Engine::Bowed: {
                auto& b = lv.bowed;
                const auto& set = modeSets()[std::size_t(std::clamp(L.model, 0, int(ModesCount) - 1))];
                b.modes = std::min(set.count, BowedVoice::maxModes);
                for (int m = 0; m < b.modes; ++m) {
                    b.ratio[std::size_t(m)] = set.ratio[std::size_t(m)];
                    b.decay[std::size_t(m)] = set.decay[std::size_t(m)];
                    b.gain[std::size_t(m)] = set.gain[std::size_t(m)];
                }
                b.tunedHz = 0;
                b.rosin = L.p[0];
                b.noisiness = L.p[1];
                b.ringSeconds = L.ring;
                b.vibratoDepth = L.vibrato;
                b.vibratoHz = L.vibratoHz;
                b.glide = 0;
                b.gainLevel = 1;
                b.bend = bendNow * p.bendRange;
                b.start(hz, attack, r, noiseAt);
                b.update(r);
                break;
            }
            case Engine::Strings: {
                auto& s = lv.strings;
                s.players = std::clamp(int(L.p[0]), 1, StringVoice::maxPlayers);
                s.spreadCents = L.p[1];
                s.position = L.p[2];
                s.bowNoise = L.p[3];
                s.brightness = L.bright;
                s.vibratoDepth = L.vibrato;
                s.vibratoDelay = L.vibratoDelay;
                s.glide = 0;
                s.gain = 1;
                s.settleScale = 0.5f;
                s.bend = bendNow * p.bendRange;
                s.start(hz, attack, vel, unipolar(mix64(noiseAt)), noiseAt, false);
                s.update();
                lv.body.setViolin(L.model, r);
                break;
            }
            case Engine::Sample: {
                const auto* inst = lswb::samples::Bank::get().instrument(L.sample);
                lv.sample.vibrato = L.vibrato;
                lv.sample.vibratoHz = L.vibratoHz;
                lv.sample.vibratoDelay = L.vibratoDelay;
                lv.sample.bend = bendNow * p.bendRange;
                if (inst == nullptr
                    || !lv.sample.start(*inst, midi, vel, unipolar(mix64(noiseAt + 3)), attack, r, L.p[0], L.p[1] * ringScale, L.p[2]))
                    continue;
                break;
            }
            case Engine::Organ:
                if (!lv.organ.active()) lv.organ.reset();
                lv.organ.bend = bendNow * p.bendRange;
                lv.organ.start(hz, L.p, L.bright, attack, r, noiseAt);
                break;
            case Engine::None: continue;
        }
        // Level, tone, pan.
        const float velGain = (1.0f - L.velocityGain) + L.velocityGain * std::pow(vel, 1.4f);
        lv.gain = dbToGain(L.level + voice.levelDb + trimDb) * velGain;
        lv.cutoff = L.lowpass * std::exp2(-L.velocityTone * (1.0f - vel));
        lv.lowpass.reset();
        lv.lowpass.set(std::min(lv.cutoff * toneScale, 0.45f * r), 0.6f, r);
        lv.highpass.reset();
        lv.highpass.setCutoff(L.highpass, r);
        lv.pan = panGains(std::clamp(L.pan + L.keyPan * (midi - 60.0f) / 30.0f, -1.0f, 1.0f));
        lv.on = true;
    }
}

void Synth::startNote(int note, float velocity, bool sustained) noexcept {
    const auto& p = patch(current);
    const bool sampled = patchSampled(p);
    const auto start = [&](int delaySamples, float semis, float levelDb, bool grace) {
        auto* v = freeVoice(current, !grace);
        if (v == nullptr) return;
        v->on = true;
        v->keyDown = !sustained;
        v->sustained = sustained;
        v->released = false;
        v->grace = grace;
        v->note = note;
        v->patch = current;
        v->velocity = velocity;
        v->semis = semis;
        v->levelDb = levelDb;
        v->delay = delaySamples;
        v->kill = 1;
        v->killStep = 0;
        v->serial = ++serial;
        v->layerMask = 0;
        for (auto& l : v->layer) l.on = false;
        for (int k = 0; k < p.layerCount; ++k) {
            const auto& L = p.layers[std::size_t(k)];
            if (L.engine == Engine::None) continue;
            if (grace && L.run <= 0) continue;
            if (L.needs == Needs::Samples && !layerAvailable(L)) continue;
            if (L.needs == Needs::NoSamples && sampled) continue;
            if (L.engine == Engine::Sample && !layerAvailable(L)) continue;
            v->layerMask |= 1 << k;
            v->layer[std::size_t(k)].on = false;
        }
        if (v->layerMask == 0) { v->on = false; return; }
        if (delaySamples <= 0) startLayers(*v);   // else updateChunk starts it when its time comes
    };
    start(0, 0.0f, 0.0f, false);
    // A run of grace notes climbing above the note (layers with `run`).
    int run = 0;
    float runMs = 0;
    for (int k = 0; k < p.layerCount; ++k)
        if (p.layers[std::size_t(k)].run > 0) { run = p.layers[std::size_t(k)].run; runMs = p.layers[std::size_t(k)].runMs; }
    if (run > 0) {
        // The Character macro sets how far the run climbs (as designed at 0.5, none at 0).
        const int count = std::clamp(int(std::lround(float(run) * 2.0f * macros.character)), 0, int(runSteps.size()));
        for (int i = 0; i < count; ++i)
            start(int(float(i + 1) * runMs * 0.001f * float(rate)), runSteps[std::size_t(i)], -3.0f - 2.2f * float(i), true);
    }
}

void Synth::releaseVoice(Voice& v) noexcept {
    if (!v.on || v.released) return;
    v.released = true;
    v.keyDown = v.sustained = false;
    if (v.delay > 0) return;   // a grace note not yet played: it plays, then lets go at once
    const float scale = std::exp2((macros.release - 0.5f) * 4.0f);
    for (auto& lv : v.layer) {
        if (!lv.on) continue;
        // A layer's release is the time to fall 60 dB; WORLDBUILDER's envelopes take a
        // time constant (a 60 dB fall takes 6.9 of them).
        const float s = std::max(0.01f, lv.release * scale);
        const float tau = s / 6.9078f;
        switch (lv.engine) {
            case Engine::Brass: case Engine::Analog: case Engine::Reed: lv.synth.release(tau, float(rate)); break;
            case Engine::Pluck: if (lv.release > 0.0f) lv.pluck.damp(s); break;
            case Engine::Bell: if (lv.release > 0.0f) lv.fadeMul = std::exp(-6.9078f / (s * float(rate))); break;
            case Engine::Tine: lv.tine.release(s); break;
            case Engine::Fm: lv.fm.release(tau, float(rate)); break;
            case Engine::Choir: lv.choir.release(tau, float(rate)); break;
            case Engine::Wind: lv.wind.release(tau, float(rate)); break;
            case Engine::Bowed: lv.bowed.release(tau, float(rate)); break;
            case Engine::Strings: lv.strings.release(tau); break;
            case Engine::Sample: lv.sample.release(s); break;
            case Engine::Organ: lv.organ.release(tau); break;
            case Engine::None: break;
        }
    }
}

void Synth::noteOn(int note, float velocity) noexcept {
    if (note < 0 || note > 127) return;
    if (!(velocity > 0.0f)) { noteOff(note); return; }
    velocity = std::clamp(velocity, 0.01f, 1.0f);
    // The same key again: its sounding note lets go (it rings on in its release).
    for (auto& v : voices)
        if (v->on && v->note == note && !v->released) releaseVoice(*v);
    keys[std::size_t(note)] = { true, false, velocity };
    startNote(note, velocity, false);
}

void Synth::noteOff(int note) noexcept {
    if (note < 0 || note > 127) return;
    auto& key = keys[std::size_t(note)];
    if (!key.down) return;
    key.down = false;
    key.sustained = pedal;
    for (auto& v : voices) {
        if (!v->on || v->note != note || !v->keyDown) continue;
        if (pedal) { v->keyDown = false; v->sustained = true; }
        else releaseVoice(*v);
    }
}

void Synth::sustainPedal(bool down) noexcept {
    if (down == pedal) return;
    pedal = down;
    if (down) return;
    for (auto& k : keys) k.sustained = false;
    for (auto& v : voices)
        if (v->on && v->sustained && !v->keyDown) releaseVoice(*v);
}

void Synth::pitchBend(float amount) noexcept { bendNow = std::isfinite(amount) ? std::clamp(amount, -1.0f, 1.0f) : 0.0f; }
void Synth::modWheel(float amount) noexcept { modNow = std::isfinite(amount) ? clamp01(amount) : 0.0f; }

void Synth::allNotesOff() noexcept {
    pedal = false;
    for (auto& k : keys) k = {};
    for (auto& v : voices) releaseVoice(*v);
}

void Synth::allSoundOff() noexcept {
    pedal = false;
    for (auto& k : keys) k = {};
    for (auto& v : voices) kill(*v, 0.008f);
}

void Synth::handle(const juce::MidiMessage& m) noexcept {
    if (m.isNoteOn()) noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) noteOff(m.getNoteNumber());
    else if (m.isSustainPedalOn()) sustainPedal(true);
    else if (m.isSustainPedalOff()) sustainPedal(false);
    else if (m.isPitchWheel()) pitchBend(float(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isControllerOfType(1)) modWheel(float(m.getControllerValue()) / 127.0f);
    else if (m.isAllNotesOff()) allNotesOff();
    else if (m.isAllSoundOff()) allSoundOff();
    else if (m.isResetAllControllers()) { pitchBend(0); modWheel(0); sustainPedal(false); }
}

void Synth::setPatch(int index) noexcept {
    index = std::clamp(index, 0, instrumentCount - 1);
    if (index == current) return;
    // The old instrument lets go of everything it holds; the keys still down (or held
    // by the pedal) start again on the new one.
    for (auto& v : voices)
        if (v->on && !v->released) releaseVoice(*v);
    current = index;
    for (int note = 0; note < 128; ++note) {
        const auto& k = keys[std::size_t(note)];
        if (k.down || k.sustained) startNote(note, k.velocity, !k.down);
    }
}

void Synth::updateChunk() noexcept {
    const auto& p = patch(current);
    const float r = float(rate);
    const float toneScale = std::exp2((macros.tone - 0.5f) * 3.0f);
    const float characterGain = 2.0f * macros.character;
    for (auto& vp : voices) {
        auto& v = *vp;
        if (!v.on) continue;
        if (v.delay > 0) {
            v.delay -= chunk;
            if (v.delay <= 0) {
                v.delay = 0;
                startLayers(v);
                if (v.released) {   // its key let go before it sounded: it plays and lets go
                    v.released = false;
                    releaseVoice(v);
                }
            }
            continue;
        }
        const auto& vpatch = patch(v.patch);
        const float bend = bendNow * vpatch.bendRange;
        const float vibratoAdd = vpatch.tremoloMod ? 0.0f : modNow * vpatch.modDepth;
        bool any = false;
        for (auto& lv : v.layer) {
            if (!lv.on) continue;
            bool alive = true;
            switch (lv.engine) {
                case Engine::Brass: case Engine::Analog: case Engine::Reed:
                    lv.synth.bend = bend;
                    lv.synth.vibratoDepth = lv.vibrato + vibratoAdd;
                    lv.synth.update(r);
                    alive = lv.synth.active();
                    break;
                case Engine::Pluck:
                    lv.pluck.bendTo(std::exp2(bend / 12.0f));
                    alive = lv.pluck.active;
                    break;
                case Engine::Bell:
                    if (std::abs(bend - lv.bent) > 1.0e-4f) {
                        lv.bent = bend;
                        const float ratio = std::exp2(bend / 12.0f);
                        for (int m = 0; m < lv.bell.modes; ++m)
                            lv.bell.mode[std::size_t(m)].retune(std::min(lv.bell.freq[std::size_t(m)] * ratio, 0.45f * r), lv.bell.tau[std::size_t(m)], r);
                    }
                    lv.bell.update();
                    alive = lv.bell.active && lv.fade > 1.0e-4f;
                    break;
                case Engine::Tine:
                    lv.tine.bend = bend;
                    lv.tine.update();
                    alive = lv.tine.active();
                    break;
                case Engine::Fm:
                    lv.fm.bend = bend;
                    lv.fm.vibratoDepth = lv.vibrato + vibratoAdd;
                    lv.fm.update(r);
                    alive = lv.fm.active();
                    break;
                case Engine::Choir:
                    lv.choir.bend = bend;
                    lv.choir.vibratoDepth = lv.vibrato + vibratoAdd;
                    lv.choir.update(r);
                    alive = lv.choir.active();
                    break;
                case Engine::Wind:
                    lv.wind.bend = bend;
                    lv.wind.vibratoDepth = lv.vibrato + vibratoAdd;
                    lv.wind.update(r);
                    alive = lv.wind.active();
                    break;
                case Engine::Bowed:
                    lv.bowed.bend = bend;
                    lv.bowed.vibratoDepth = lv.vibrato + vibratoAdd;
                    lv.bowed.update(r);
                    alive = lv.bowed.active();
                    break;
                case Engine::Strings:
                    lv.strings.bend = bend;
                    lv.strings.vibratoDepth = lv.vibrato + vibratoAdd;
                    lv.strings.update();
                    alive = lv.strings.active();
                    break;
                case Engine::Sample:
                    lv.sample.bend = bend;
                    lv.sample.vibrato = lv.vibrato + vibratoAdd;
                    lv.sample.update();
                    alive = lv.sample.on;
                    break;
                case Engine::Organ:
                    lv.organ.bend = bend;
                    lv.organ.update();
                    alive = lv.organ.active();
                    break;
                case Engine::None: alive = false; break;
            }
            if (!alive) { lv.on = false; continue; }
            lv.lowpass.set(std::min(lv.cutoff * toneScale, 0.45f * r), 0.6f, r);
            any = true;
        }
        if (!any || v.kill <= 0.0f) {
            v.on = false;
            for (auto& lv : v.layer) lv.on = false;
        }
    }
    // The bus follows the instrument.
    bus->shelfGain.next(dbToGain(p.air + (macros.tone - 0.5f) * 8.0f));
    const float chorusTarget = p.characterMode == Character::Chorus ? p.chorus * characterGain : p.chorus;
    const float shimmerTarget = p.characterMode == Character::Shimmer ? p.shimmer * characterGain : p.shimmer;
    bus->chorusMix.next(chorusTarget);
    bus->shimmerSend.next(shimmerTarget);
    bus->trim.next(dbToGain(macros.levelDb));
    bus->tremolo.next(p.tremoloMod ? modNow * p.modDepth : 0.0f);
    bus->chorus.set(0.33f, 0.0032f, std::min(bus->chorusMix.value, 1.0f));
    bus->shimmer.set(0.85f, 5.0f, 0.75f);
}

void Synth::render(float* outL, float* outR, int n) noexcept {
    const float characterGain = 2.0f * macros.character;
    for (int i = 0; i < n; ++i) {
        if (chunkLeft <= 0) {
            updateChunk();
            chunkLeft = chunk;
        }
        --chunkLeft;
        float l = 0, r = 0;
        for (auto& vp : voices) {
            auto& v = *vp;
            if (!v.on || v.delay > 0) continue;
            float vl = 0, vr = 0;
            for (auto& lv : v.layer) {
                if (!lv.on) continue;
                float y = 0;
                switch (lv.engine) {
                    case Engine::Brass: case Engine::Analog: case Engine::Reed: {
                        float a = 0, b = 0;
                        lv.synth.tick(a, b);
                        y = (a + b) * 0.7071f;
                        break;
                    }
                    case Engine::Pluck: y = lv.body.process(lv.pluck.tick(noise)) * 2.0f; break;
                    case Engine::Bell: y = lv.bell.tick() * lv.fade; lv.fade *= lv.fadeMul; break;
                    case Engine::Tine: y = lv.tine.tick(); break;
                    case Engine::Fm: y = lv.fm.tick(); break;
                    case Engine::Choir: y = lv.choir.tick(); break;
                    case Engine::Wind: y = lv.wind.tick(noise); break;
                    case Engine::Bowed: y = lv.bowed.tick(noise); break;
                    case Engine::Strings: y = lv.body.process(lv.strings.tick(noise)); break;
                    case Engine::Sample: y = lv.sample.tick(); break;
                    case Engine::Organ: y = lv.organ.tick(noise); break;
                    case Engine::None: break;
                }
                y = lv.lowpass.tick(lv.highpass.hp(y)).lp * lv.gain * (lv.character ? characterGain : 1.0f);
                vl += y * lv.pan.l;
                vr += y * lv.pan.r;
            }
            if (v.killStep > 0.0f) {
                v.kill -= v.killStep;
                if (v.kill <= 0.0f) { v.kill = 0; v.on = false; for (auto& lv : v.layer) lv.on = false; }
                vl *= v.kill;
                vr *= v.kill;
            }
            l += vl;
            r += vr;
        }
        // The bus: tremolo (mod wheel on struck instruments), the bright shelf, chorus, shimmer.
        auto& b = *bus;
        if (b.tremolo.value > 1.0e-4f) {
            b.tremoloPhase += 5.5f / float(rate);
            b.tremoloPhase -= std::floor(b.tremoloPhase);
            const float t = 1.0f - b.tremolo.value * 0.5f * (1.0f + sinCycles(b.tremoloPhase));
            l *= t;
            r *= t;
        }
        const float shelf = b.shelfGain.value - 1.0f;
        l += shelf * b.shelf[0].hp(l);
        r += shelf * b.shelf[1].hp(r);
        if (b.chorusMix.value > 1.0e-3f) b.chorus.process(l, r);
        if (b.shimmerSend.value > 1.0e-3f) {
            float sl = 0, sr = 0;
            b.shimmer.process(l * b.shimmerSend.value, r * b.shimmerSend.value, sl, sr);
            l += sl * 0.5f;
            r += sr * 0.5f;
        }
        const float g = b.trim.value;
        outL[i] += finiteOr(l * g);
        outR[i] += finiteOr(r * g);
    }
}

} // namespace ls::inst
