#include "Synth.h"
#include "../DSP/Common.h"
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
    int wait = 0;              // samples of silence before it speaks (an ensemble singer's timing)
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
        wait = 0;
        update();
        return on = true;
    }
    // Joins a note already sounding (a vowel the Vowel macro turns to): in its sustain
    // loop, faded in, so it never restarts the attack.
    bool startInLoop(const lswb::samples::Instrument& inst, float note, float velocity, float pick, float sampleRate, float fadeSeconds) {
        if (!start(inst, note, velocity, pick, fadeSeconds, sampleRate, 0.0f, 0.0f, 0.0f)) return false;
        if (zone->loops()) pos = double(zone->loopStart);
        return true;
    }
    // Levels the take to a loudness (the ensemble's singers to the lead, the vowels to each other).
    void levelTo(float targetDb) { if (zone != nullptr) gain = dbToGain(std::clamp(targetDb - zone->loudnessDb, -12.0f, 12.0f)); }
    // Moves on without reading (a vowel at zero weight keeps its place).
    void skip(int n) {
        if (!on) return;
        if (wait > 0) { wait = std::max(0, wait - n); return; }
        const auto& z = *zone;
        pos += double(inc) * double(n);
        if (z.loops() && !releasing) while (pos >= double(z.loopEnd)) pos -= double(z.loopEnd - z.loopStart);
        fade *= std::pow(fadeMul * releaseMul, float(n));
        if (amp < 1.0f) amp = std::min(1.0f, amp + attackStep * float(n));
        if (pos >= double(z.length - 2) || fade < 1.0e-4f) on = false;
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
        if (wait > 0) { --wait; return 0.0f; }
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

// The developer's voice as a choir (the recordings update). Up to eight singers; each
// sings every vowel's sweep (mmm, ooh, oh, aah, ee: the Vowel macro crossfades between
// neighbours, equal power) and, from the second singer on, the 5-take choir stacks where
// one lies within a fifth of the note (each singer a different take: a real ensemble),
// else the sweeps again. Every singer has its own detune, a slow drift, a timing offset
// and a place in the stereo field; the Ensemble macro brings them in one by one (one
// voice to a full choir), the level held near the lead's (power sum, a little growth).
// Vowels and singers at zero weight keep their place without being read (cheap); a vowel
// turned to while a note sounds joins in its sustain loop.
struct VoiceStack {
    static constexpr int singers = 8;
    std::array<std::array<SampleVoice, vowelCount>, singers> sweep;
    std::array<std::array<SampleVoice, stackCount>, singers> stack;
    std::array<bool, singers> stackOk {};
    std::array<std::array<bool, stackCount>, singers> stackNear {};
    // Targets (from the macros) and what sounds now, ramped to them per sample over ~30 ms
    // (a macro turned while notes sound never steps).
    std::array<std::array<float, vowelCount + stackCount>, singers> weight {}, weightNow {}, weightStep {};
    std::array<float, singers> singerGain {}, singerNow {}, singerStep {}, detune {}, driftPhase {}, driftHz {};
    float normNow = 1, normStep = 0;
    std::array<PanGains, singers> pan {};
    std::array<float, vowelCount> vowelLevelDb {};
    float note = 60, velocity = 0.8f, vowel = 3, size = 1, bend = 0, norm = 1, rate = 48000, leadDb = -20;
    bool on = false, releasing = false;

    static constexpr std::array<float, singers> detuneCents { 0.0f, 7.0f, -9.0f, 4.0f, -5.0f, 11.0f, -12.0f, 2.0f };
    static constexpr std::array<float, singers> pans { 0.0f, -0.7f, 0.7f, -0.45f, 0.45f, -0.95f, 0.95f, -0.2f };
    static constexpr std::array<float, singers> delays { 0.0f, 0.45f, 0.8f, 0.2f, 0.62f, 1.0f, 0.33f, 0.9f };   // of the timing spread

    static const lswb::samples::Instrument* sweepSet(int v) { return lswb::samples::Bank::get().instrument(vowelSweeps[std::size_t(v)]); }
    static const lswb::samples::Instrument* stackSet(int k) { return lswb::samples::Bank::get().instrument(vowelStacks[std::size_t(k)]); }
    static bool available() {
        for (int v = 0; v < vowelCount; ++v) if (sweepSet(v) != nullptr) return true;
        return false;
    }
    // The vowels' weights (equal power between neighbours) at a position 0..4.
    static std::array<float, vowelCount> vowelWeights(float position) {
        std::array<float, vowelCount> w {};
        position = std::clamp(position, 0.0f, float(vowelCount - 1));
        for (int v = 0; v < vowelCount; ++v) w[std::size_t(v)] = std::sqrt(std::max(0.0f, 1.0f - std::abs(position - float(v))));
        return w;
    }
    void reset() {
        on = false;
        for (auto& s : sweep) for (auto& v : s) v.on = false;
        for (auto& s : stack) for (auto& v : s) v.on = false;
    }
    // The loudness a vowel's take is levelled to: the lead's layer median for that vowel
    // set, so every vowel sounds as loud as the others.
    float layerDb(const lswb::samples::Instrument& inst, const lswb::samples::Zone& z) const { return inst.medianDb(z.layer); }

    void start(float midi, float vel, float velocityCap, float position, float singerCount, float detuneScale, float width, float spreadMs,
               float attack, float sampleRate, uint64_t seed) {
        rate = sampleRate;
        note = midi;
        velocity = velocityCap > 0.0f ? std::min(vel, velocityCap) : vel;
        vowel = position;
        size = singerCount;
        releasing = false;
        on = true;
        // The reference level: the aah sweep's median for this velocity's layer (or the
        // first vowel there is), so all vowels and singers sit at one loudness.
        leadDb = -20;
        for (int v : { 3, 1, 4, 2, 0 })
            if (const auto* inst = sweepSet(v)) {
                if (const auto* z = inst->find(int(std::lround(midi)), velocity, 0.0f)) { leadDb = inst->medianDb(z->layer); break; }
            }
        for (int k = 0; k < singers; ++k) {
            detune[std::size_t(k)] = detuneCents[std::size_t(k)] * detuneScale;
            pan[std::size_t(k)] = panGains(std::clamp(pans[std::size_t(k)] * width, -1.0f, 1.0f));
            driftHz[std::size_t(k)] = 0.11f + 0.05f * float(k);
            driftPhase[std::size_t(k)] = unipolar(mix64(seed + uint64_t(k) * 31));
            const int wait = k == 0 ? 0 : int(delays[std::size_t(k)] * spreadMs * 0.001f * rate);
            const float pick = k == 0 ? 0.0f : (float(k) + 0.5f) / float(singers);
            for (int v = 0; v < vowelCount; ++v) sweep[std::size_t(k)][std::size_t(v)].on = false;
            for (int t = 0; t < stackCount; ++t) {
                auto& sv = stack[std::size_t(k)][std::size_t(t)];
                sv.on = false;
                stackNear[std::size_t(k)][std::size_t(t)] = false;
                const auto* inst = stackSet(t);
                if (k == 0 || inst == nullptr) continue;
                const auto* z = inst->find(int(std::lround(midi)), velocity, 0.0f);
                stackNear[std::size_t(k)][std::size_t(t)] = z != nullptr && std::abs(z->root - midi) <= 7.01f;
            }
            stackOk[std::size_t(k)] = false;
            for (bool near : stackNear[std::size_t(k)]) stackOk[std::size_t(k)] = stackOk[std::size_t(k)] || near;
            computeWeights(k);
            for (int v = 0; v < vowelCount; ++v)
                if (weight[std::size_t(k)][std::size_t(v)] > 0.0f) startSweep(k, v, attack, pick, wait, false);
            for (int t = 0; t < stackCount; ++t)
                if (weight[std::size_t(k)][std::size_t(vowelCount + t)] > 0.0f) startStack(k, t, attack, wait, false);
        }
        updateGains();
        weightNow = weight;
        singerNow = singerGain;
        normNow = norm;
        for (auto& w : weightStep) w = {};
        singerStep = {};
        normStep = 0;
    }
    void startSweep(int k, int v, float attack, float pick, int wait, bool inLoop) {
        const auto* inst = sweepSet(v);
        auto& sv = sweep[std::size_t(k)][std::size_t(v)];
        if (inst == nullptr) return;
        const bool ok = inLoop ? sv.startInLoop(*inst, note, velocity, pick, rate, 0.25f)
                               : sv.start(*inst, note, velocity, pick, attack, rate, 0.0f, 0.0f, 0.0f);
        if (!ok) return;
        sv.vibrato = 0;
        sv.levelTo(leadDb);
        sv.wait = inLoop ? 0 : wait;
    }
    void startStack(int k, int t, float attack, int wait, bool inLoop) {
        const auto* inst = stackSet(t);
        auto& sv = stack[std::size_t(k)][std::size_t(t)];
        if (inst == nullptr) return;
        const float pick = (float(k - 1) + 0.5f) / 7.0f;   // a different take per singer
        const bool ok = inLoop ? sv.startInLoop(*inst, note, velocity, pick, rate, 0.25f)
                               : sv.start(*inst, note, velocity, pick, attack, rate, 0.0f, 0.0f, 0.0f);
        if (!ok) return;
        sv.vibrato = 0;
        sv.levelTo(leadDb);
        sv.wait = inLoop ? 0 : wait;
    }
    void computeWeights(int k) {
        auto& w = weight[std::size_t(k)];
        w = {};
        const auto vw = vowelWeights(vowel);
        if (k > 0 && stackOk[std::size_t(k)]) {
            // The stacks' vowels: mmm, ooh (with half of oh), aah (with half of oh, and ee).
            std::array<float, stackCount> sw { vw[0], std::sqrt(vw[1] * vw[1] + 0.5f * vw[2] * vw[2]),
                                               std::sqrt(0.5f * vw[2] * vw[2] + vw[3] * vw[3] + vw[4] * vw[4]) };
            float lost = 0;
            for (int t = 0; t < stackCount; ++t)
                if (!stackNear[std::size_t(k)][std::size_t(t)]) { lost += sw[std::size_t(t)] * sw[std::size_t(t)]; sw[std::size_t(t)] = 0; }
            // A vowel with no stack near this note: its weight goes to the nearest one that has.
            if (lost > 0.0f) {
                const int order[stackCount][stackCount] = { { 0, 1, 2 }, { 1, 0, 2 }, { 2, 1, 0 } };
                int from = vowel < 0.5f ? 0 : (vowel < 2.0f ? 1 : 2);
                for (int t : order[from])
                    if (stackNear[std::size_t(k)][std::size_t(t)]) { sw[std::size_t(t)] = std::sqrt(sw[std::size_t(t)] * sw[std::size_t(t)] + lost); break; }
            }
            for (int t = 0; t < stackCount; ++t) w[std::size_t(vowelCount + t)] = sw[std::size_t(t)];
        } else {
            for (int v = 0; v < vowelCount; ++v) w[std::size_t(v)] = sweepSet(v) != nullptr ? vw[std::size_t(v)] : 0.0f;
        }
        float power = 0;
        for (float x : w) power += x * x;
        if (power <= 1.0e-9f) {   // nothing recorded for the vowel: the nearest vowel that is
            for (int d = 0; d < vowelCount && power <= 1.0e-9f; ++d)
                for (int v : { int(std::lround(vowel)) - d, int(std::lround(vowel)) + d })
                    if (v >= 0 && v < vowelCount && sweepSet(v) != nullptr) { w[std::size_t(v)] = 1; power = 1; break; }
        }
        const float n = power > 0.0f ? 1.0f / std::sqrt(power) : 0.0f;
        for (auto& x : w) x *= n;
    }
    void updateGains() {
        float power = 0;
        for (int k = 0; k < singers; ++k) {
            const float g = k == 0 ? 1.0f : std::clamp(size - float(k), 0.0f, 1.0f);
            singerGain[std::size_t(k)] = g;
            power += g * g;
        }
        // A bigger choir is a little louder (power ^ 0.3, not ^ 0.5): +3 dB for eight.
        norm = std::pow(std::max(power, 1.0f), -0.35f);
    }
    // The macros, live (per chunk): new weights; a vowel or singer that now sounds and
    // had not started joins in its loop.
    void retarget(float position, float singerCount) {
        const bool moved = std::abs(position - vowel) > 1.0e-4f || std::abs(singerCount - size) > 1.0e-4f;
        vowel = position;
        size = singerCount;
        if (!moved || releasing) return;
        for (int k = 0; k < singers; ++k) {
            computeWeights(k);
            const bool audible = k == 0 || size > float(k);
            if (!audible) continue;
            const float pick = k == 0 ? 0.0f : (float(k) + 0.5f) / float(singers);
            for (int v = 0; v < vowelCount; ++v)
                if (weight[std::size_t(k)][std::size_t(v)] > 0.0f && !sweep[std::size_t(k)][std::size_t(v)].on) startSweep(k, v, 0.0f, pick, 0, true);
            for (int t = 0; t < stackCount; ++t)
                if (weight[std::size_t(k)][std::size_t(vowelCount + t)] > 0.0f && !stack[std::size_t(k)][std::size_t(t)].on) startStack(k, t, 0.0f, 0, true);
        }
        updateGains();
    }
    void update(float bendSemis, int chunkSamples) {
        bend = bendSemis;
        for (int k = 0; k < singers; ++k) {
            driftPhase[std::size_t(k)] += driftHz[std::size_t(k)] * float(chunkSamples) / rate;
            driftPhase[std::size_t(k)] -= std::floor(driftPhase[std::size_t(k)]);
            const float cents = detune[std::size_t(k)] + (k == 0 ? 0.0f : 3.0f * sinCycles(driftPhase[std::size_t(k)]));
            // The ramps for this chunk (one-pole, ~30 ms, as a straight line within it).
            const float a = chunkSamples > 0 ? 1.0f - std::exp(-float(chunkSamples) / (0.03f * rate)) : 1.0f;
            const float per = chunkSamples > 0 ? 1.0f / float(chunkSamples) : 0.0f;
            {
                float& now = singerNow[std::size_t(k)];
                const float next = std::abs(singerGain[std::size_t(k)] - now) < 1.0e-4f ? singerGain[std::size_t(k)] : now + a * (singerGain[std::size_t(k)] - now);
                singerStep[std::size_t(k)] = (next - now) * per;
                if (chunkSamples == 0) now = next;
            }
            const bool silent = singerNow[std::size_t(k)] <= 1.0e-5f && singerGain[std::size_t(k)] <= 0.0f;
            for (int i = 0; i < vowelCount + stackCount; ++i) {
                float& now = weightNow[std::size_t(k)][std::size_t(i)];
                const float target = weight[std::size_t(k)][std::size_t(i)];
                const float next = std::abs(target - now) < 1.0e-4f ? target : now + a * (target - now);
                weightStep[std::size_t(k)][std::size_t(i)] = (next - now) * per;
                if (chunkSamples == 0) now = next;
                auto& sv = i < vowelCount ? sweep[std::size_t(k)][std::size_t(i)] : stack[std::size_t(k)][std::size_t(i - vowelCount)];
                if (!sv.on) continue;
                sv.bend = bend + cents * 0.01f;
                sv.update();
                // Not heard this chunk: it moves on without being read.
                if (silent || (target <= 0.0f && now <= 1.0e-5f)) sv.skip(chunkSamples);
            }
        }
        {
            const float a = chunkSamples > 0 ? 1.0f - std::exp(-float(chunkSamples) / (0.03f * rate)) : 1.0f;
            const float next = normNow + a * (norm - normNow);
            normStep = chunkSamples > 0 ? (next - normNow) / float(chunkSamples) : 0.0f;
            if (chunkSamples == 0) normNow = next;
        }
        bool any = false;
        for (auto& s : sweep) for (auto& v : s) any = any || v.on;
        for (auto& s : stack) for (auto& v : s) any = any || v.on;
        on = any;
    }
    void release(float seconds) {
        releasing = true;
        for (auto& s : sweep) for (auto& v : s) v.release(seconds);
        for (auto& s : stack) for (auto& v : s) v.release(seconds);
    }
    void tick(float& l, float& r) {
        l = r = 0;
        normNow += normStep;
        for (int k = 0; k < singers; ++k) {
            singerNow[std::size_t(k)] = std::max(0.0f, singerNow[std::size_t(k)] + singerStep[std::size_t(k)]);
            const float g = singerNow[std::size_t(k)] * normNow;
            float y = 0;
            auto& w = weightNow[std::size_t(k)];
            const auto& dw = weightStep[std::size_t(k)];
            for (int i = 0; i < vowelCount + stackCount; ++i) w[std::size_t(i)] = std::max(0.0f, w[std::size_t(i)] + dw[std::size_t(i)]);
            if (g <= 1.0e-6f) continue;
            for (int v = 0; v < vowelCount; ++v) {
                auto& sv = sweep[std::size_t(k)][std::size_t(v)];
                if (sv.on && w[std::size_t(v)] > 1.0e-5f) y += w[std::size_t(v)] * sv.tick();
            }
            for (int t = 0; t < stackCount; ++t) {
                auto& sv = stack[std::size_t(k)][std::size_t(t)];
                if (sv.on && w[std::size_t(vowelCount + t)] > 1.0e-5f) y += w[std::size_t(vowelCount + t)] * sv.tick();
            }
            y *= g;
            l += y * pan[std::size_t(k)].l;
            r += y * pan[std::size_t(k)].r;
        }
        l *= 1.4142f;
        r *= 1.4142f;
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

// Cloud Steps' glissando (Direction Update 2): the most grace notes a run has (Character
// at 100 %), and how close together notes must start to share one sweep (a chord).
constexpr int maxRun = 8;
constexpr float chordWindowSeconds = 0.035f;
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
        VoiceStack voice;
        Body body;
        Svf lowpass, lowpassR;
        OnePole highpass, highpassR;
        PanGains pan { 0.7071f, 0.7071f };
        float gain = 1, cutoff = 18000, release = 0.3f, fade = 1, fadeMul = 1, vibrato = 0, hz = 440;
        bool character = false, stretchRelease = true;
        float bent = 0, vowelBase = 3, singersBase = 1, choirVowelBase = 0;
    };
    bool on = false, keyDown = false, sustained = false, released = false, grace = false, moved = false;
    // Punch: the gain on hit (dB at its start), where it is in its settling, the gain now
    // and its step per sample (set once per chunk).
    float punchDb = 0, punchGain = 1, punchStep = 0;
    int punchPos = 0, punchLength = 0;
    int note = -1, patch = 0, delay = 0, layerMask = 0;
    int sweep = -1, sweepIndex = 0, sweepCount = 0;   // a grace note: its sweep, place in it
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
    // Punch's safety limiter: the punched sum never goes above the ceiling, or above what
    // it would be without Punch (so it only ever takes back what Punch added).
    float limiterGain = 1, limiterRelease = 0.001f, limiterLowest = 1;
    bool punching = false;
};

namespace {
bool layerAvailable(const Layer& l) noexcept {
    if (l.engine == Engine::Voice) return VoiceStack::available();
    return l.engine == Engine::Sample && l.sample != nullptr && lswb::samples::Bank::get().instrument(l.sample) != nullptr;
}
// Equal-power key crossfades (Layer::fadeKey).
float keyFade(const Layer& l, float midi) noexcept {
    if (l.fadeKey <= 0.0f) return 1.0f;
    const float t = std::clamp((midi - l.fadeKey) / std::max(0.5f, l.fadeSpan), 0.0f, 1.0f);
    return l.fadeIn ? std::sin(t * 1.5707963f) : std::cos(t * 1.5707963f);
}
bool recorded(Engine e) noexcept { return e == Engine::Sample || e == Engine::Voice; }
// The macros' reach: Vowel moves two vowels either way, Ensemble from one singer to eight.
float vowelPosition(float base, float macro) noexcept { return std::clamp(base + (macro - 0.5f) * 4.0f, 0.0f, 4.0f); }
float singerCount(float base, float macro) noexcept {
    return macro < 0.5f ? 1.0f + (base - 1.0f) * macro * 2.0f : base + (8.0f - base) * (macro - 0.5f) * 2.0f;
}
} // namespace

bool patchUsesRecordings(const Patch& p) noexcept {
    for (int k = 0; k < p.layerCount; ++k)
        if (recorded(p.layers[std::size_t(k)].engine)) return true;
    return false;
}

bool patchHasVoice(const Patch& p) noexcept {
    for (int k = 0; k < p.layerCount; ++k)
        if (p.layers[std::size_t(k)].engine == Engine::Voice || p.layers[std::size_t(k)].engine == Engine::Choir) return true;
    return false;
}

bool patchSampled(const Patch& p) noexcept {
    bool any = false;
    for (int k = 0; k < p.layerCount; ++k) {
        const auto& l = p.layers[std::size_t(k)];
        if (!recorded(l.engine)) continue;
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
    bus->limiterRelease = 1.0f - std::exp(-1.0f / (0.06f * r));
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
            l.voice.reset();
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
    bus->shimmerSend.snap(p.shimmer * dc::dsp::shimmerBusBase * dc::dsp::shimmerControl(macros.shimmer));
    bus->trim.snap(dbToGain(macros.levelDb));
    bus->tremolo.snap(0);
    bus->limiterGain = 1;
    bus->limiterLowest = 1;
    bus->punching = false;
}

float Synth::punchLimiterGain() const noexcept { return bus->limiterGain; }
float Synth::punchLimiterLowest() const noexcept { return bus->limiterLowest; }

void Synth::setMacros(const Macros& m) noexcept {
    macros = m;
    macros.tone = clamp01(m.tone);
    macros.attack = clamp01(m.attack);
    macros.release = clamp01(m.release);
    macros.character = clamp01(m.character);
    macros.vowel = clamp01(m.vowel);
    macros.ensemble = clamp01(m.ensemble);
    macros.octave = std::clamp(m.octave, -3, 3);
    macros.levelDb = std::isfinite(m.levelDb) ? std::clamp(m.levelDb, -48.0f, 12.0f) : 0.0f;
    macros.modulation = std::isfinite(m.modulation) ? std::clamp(m.modulation, 0.0f, 1.5f) : 1.0f;
    macros.punch = clamp01(std::isfinite(m.punch) ? m.punch : 0.5f);
    macros.punchSeconds = std::isfinite(m.punchSeconds) ? std::clamp(m.punchSeconds, 0.02f, 0.5f) : 0.12f;
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
                s.bite = L.bite;
                s.presence = L.presence;
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
                // The Vowel macro moves the formant choir too (its 0..4: oo .. ee), the
                // Ensemble macro its spread.
                lv.choirVowelBase = L.p[0];
                c.vowel = std::clamp(L.p[0] + (macros.vowel - 0.5f) * 3.2f, 0.0f, 4.0f);
                c.breath = L.p[1];
                c.detuneCents = L.p[2] * (0.4f + 1.2f * macros.ensemble);
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
            case Engine::Voice:
                if (!VoiceStack::available()) continue;
                lv.vowelBase = L.p[0];
                lv.singersBase = std::clamp(L.p[1], 1.0f, 8.0f);
                lv.voice.bend = bendNow * p.bendRange;
                lv.voice.start(midi, vel, L.p[2], vowelPosition(lv.vowelBase, macros.vowel), singerCount(lv.singersBase, macros.ensemble),
                               L.p[3] / 10.0f, L.p[4], L.p[5], attack, r, noiseAt);
                lv.voice.update(bendNow * p.bendRange, 0);
                if (!lv.voice.on) continue;
                break;
            case Engine::None: continue;
        }
        // Level, tone, pan.
        const float velGain = (1.0f - L.velocityGain) + L.velocityGain * std::pow(vel, 1.4f);
        lv.gain = dbToGain(L.level + voice.levelDb + trimDb) * velGain * keyFade(L, midi - L.cents * 0.01f);
        lv.cutoff = L.lowpass * std::exp2(-L.velocityTone * (1.0f - vel));
        lv.lowpass.reset();
        lv.lowpass.set(std::min(lv.cutoff * toneScale, 0.45f * r), 0.6f, r);
        lv.highpass.reset();
        lv.highpass.setCutoff(L.highpass, r);
        lv.lowpassR.reset();
        lv.lowpassR.set(std::min(lv.cutoff * toneScale, 0.45f * r), 0.6f, r);
        lv.highpassR.reset();
        lv.highpassR.setCutoff(L.highpass, r);
        lv.pan = panGains(std::clamp(L.pan + L.keyPan * (midi - 60.0f) / 30.0f, -1.0f, 1.0f));
        lv.on = true;
    }
    // Punch: the note's level overshoots (by up to +12 dB, less on softer notes) and settles
    // back to its own level over Punch Time (a raised cosine: smooth at both ends). The note
    // starts from silence, so the overshoot never clicks. Notes moved to a new instrument by
    // travel are not struck again.
    const float db = voice.moved ? 0.0f : dc::dsp::punchDb(macros.punch, p.punchDb) * (0.35f + 0.65f * std::pow(clamp01(velocity), 1.3f));
    voice.punchDb = db;
    voice.punchPos = 0;
    voice.punchStep = 0;
    voice.punchLength = db > 0.05f ? std::max(1, int(macros.punchSeconds * r)) : 0;
    voice.punchGain = voice.punchLength > 0 ? dbToGain(db) : 1.0f;
}

void Synth::startNote(int note, float velocity, bool sustained, bool moved) noexcept {
    const auto& p = patch(current);
    const bool sampled = patchSampled(p);
    const auto start = [&](int delaySamples, float semis, float levelDb, bool grace) -> Voice* {
        auto* v = freeVoice(current, !grace);
        if (v == nullptr) return nullptr;
        v->on = true;
        v->keyDown = !sustained;
        v->sustained = sustained;
        v->released = false;
        v->grace = grace;
        v->moved = moved;
        v->punchLength = 0;
        v->punchGain = 1;
        v->punchStep = 0;
        v->note = note;
        v->patch = current;
        v->velocity = velocity;
        v->semis = semis;
        v->levelDb = levelDb;
        v->delay = delaySamples;
        v->sweep = -1;
        v->kill = 1;
        v->killStep = 0;
        v->serial = ++serial;
        v->layerMask = 0;
        for (auto& l : v->layer) l.on = false;
        for (int k = 0; k < p.layerCount; ++k) {
            const auto& L = p.layers[std::size_t(k)];
            if (L.engine == Engine::None) continue;
            if (grace && L.run <= 0) continue;
            // A recorded layer needs its recordings; a synthesised one marked Samples plays
            // with the recordings (blending with them), never instead of them.
            if (L.needs == Needs::Samples && !(recorded(L.engine) ? layerAvailable(L) : sampled)) continue;
            if (L.needs == Needs::NoSamples && sampled) continue;
            if (recorded(L.engine) && !layerAvailable(L)) continue;
            v->layerMask |= 1 << k;
            v->layer[std::size_t(k)].on = false;
        }
        if (v->layerMask == 0) { v->on = false; return nullptr; }
        if (delaySamples <= 0) startLayers(*v);   // else updateChunk starts it when its time comes
        return v;
    };
    // A glissando (layers with `run`): grace notes sweep up through the notes held and the
    // played note lands at the top. The Character macro sets how long the sweep is (as
    // designed at 0.5, none at 0). Notes moved to this instrument by travel just sound.
    int run = 0;
    float runMs = 0;
    for (int k = 0; k < p.layerCount; ++k)
        if (p.layers[std::size_t(k)].run > 0) { run = p.layers[std::size_t(k)].run; runMs = p.layers[std::size_t(k)].runMs; }
    const int count = run > 0 && !moved ? std::clamp(int(std::lround(float(run) * 2.0f * macros.character)), 0, maxRun) : 0;
    if (count == 0) { start(0, 0.0f, 0.0f, false); return; }
    // A note starting with (or just after) another one joins its sweep: one sweep per chord,
    // rising into its highest note, every note of the chord landing together.
    const auto window = uint64_t(chordWindowSeconds * float(rate));
    for (int s = 0; s < maxSweeps; ++s) {
        auto& sw = sweeps[std::size_t(s)];
        if (!sw.active || clock - sw.start > window || clock >= sw.land) continue;
        sw.members[std::size_t(note)] = true;
        sw.top = std::max(sw.top, note);
        start(int(sw.land - clock), 0.0f, 0.0f, false);
        return;
    }
    const int s = nextSweep;
    nextSweep = (nextSweep + 1) % maxSweeps;
    auto& sw = sweeps[std::size_t(s)];
    sw = {};
    sw.active = true;
    sw.start = clock;
    const auto step = std::max<uint64_t>(1, uint64_t(runMs * 0.001f * float(rate)));
    sw.land = clock + step * uint64_t(count);
    sw.top = note;
    sw.members[std::size_t(note)] = true;
    // The grace notes rise in level into the note (a crescendo); each one's pitch is chosen
    // when it sounds, from what is held then (so a chord's later notes count). The first
    // waits one sample: notes of the same chord arrive at the same moment.
    for (int i = 0; i < count; ++i) {
        auto* v = start(int(uint64_t(i) * step) + 1, 0.0f, -3.0f - 1.6f * float(count - 1 - i), true);
        if (v == nullptr) continue;
        v->sweep = s;
        v->sweepIndex = i;
        v->sweepCount = count;
    }
    start(int(sw.land - clock), 0.0f, 0.0f, false);
}

int Synth::sweepPitch(Voice& v) noexcept {
    // The notes the sweep may use: the pitch classes held now (keys down, pedalled, and the
    // sweep's own notes); one pitch class alone adds its fifth. Below the sweep's top note,
    // nearest first, the run takes the count-th nearest for its first note and climbs.
    auto& sw = sweeps[std::size_t(v.sweep)];
    std::array<bool, 12> classes {};
    int distinct = 0;
    for (int n = 0; n < 128; ++n) {
        const auto& k = keys[std::size_t(n)];
        if (!(k.down || k.sustained || sw.members[std::size_t(n)])) continue;
        if (!classes[std::size_t(n % 12)]) ++distinct;
        classes[std::size_t(n % 12)] = true;
    }
    if (distinct == 0) return -1;
    if (distinct == 1)
        for (int c = 0; c < 12; ++c)
            if (classes[std::size_t(c)]) { classes[std::size_t((c + 7) % 12)] = true; break; }
    std::array<int, maxRun> below {};
    int found = 0;
    for (int m = sw.top - 1; m >= 0 && m >= sw.top - 36 && found < maxRun; --m)
        if (classes[std::size_t(m % 12)]) below[std::size_t(found++)] = m;
    if (found == 0) return -1;
    const int want = std::min(v.sweepCount - 1 - v.sweepIndex, found - 1);
    int pitch = below[std::size_t(want)];
    // Always upwards: if the chord changed under the sweep, the next note above the last.
    if (pitch <= sw.last) {
        pitch = -1;
        for (int k = found - 1; k >= 0; --k)
            if (below[std::size_t(k)] > sw.last) { pitch = below[std::size_t(k)]; break; }
        if (pitch < 0) return -1;
    }
    sw.last = pitch;
    return pitch;
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
            case Engine::Voice: lv.voice.release(s); break;
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
        if (k.down || k.sustained) startNote(note, k.velocity, !k.down, true);
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
                if (v.sweep >= 0) {
                    // A glissando's grace note: its pitch from what is held now.
                    const int pitch = sweepPitch(v);
                    if (pitch < 0) { v.on = false; continue; }
                    v.semis = float(pitch - v.note);
                    runLog[std::size_t(runLogged % runLogSize)] = { pitch, clock };
                    ++runLogged;
                }
                startLayers(v);
                if (v.released) {   // its key let go before it sounded: it plays and lets go
                    v.released = false;
                    releaseVoice(v);
                }
            }
            continue;
        }
        if (v.punchLength > 0) {
            // Punch: the gain at the end of this chunk, reached linearly over it.
            if (v.punchPos >= v.punchLength) { v.punchLength = 0; v.punchGain = 1; v.punchStep = 0; }
            else {
                v.punchPos += chunk;
                const float t = std::min(1.0f, float(v.punchPos) / float(v.punchLength));
                const float next = t >= 1.0f ? 1.0f : dbToGain(v.punchDb * 0.5f * (1.0f + std::cos(3.14159265f * t)));
                v.punchStep = (next - v.punchGain) / float(chunk);
            }
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
                    // A released string ends once it is below -80 dB at the output: a damped
                    // waveguide can keep a tiny residue near DC ringing for ever (inaudible,
                    // but it held a voice).
                    alive = lv.pluck.active && !(v.released && lv.pluck.peak * lv.gain < 1.0e-4f);
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
                case Engine::Voice:
                    lv.voice.retarget(vowelPosition(lv.vowelBase, macros.vowel), singerCount(lv.singersBase, macros.ensemble));
                    lv.voice.update(bend, chunk);
                    alive = lv.voice.on;
                    break;
                case Engine::None: alive = false; break;
            }
            if (!alive) { lv.on = false; continue; }
            if (lv.engine == Engine::Choir) {
                lv.choir.vowel = std::clamp(lv.choirVowelBase + (macros.vowel - 0.5f) * 3.2f, 0.0f, 4.0f);
            }
            lv.lowpass.set(std::min(lv.cutoff * toneScale, 0.45f * r), 0.6f, r);
            lv.lowpassR.set(std::min(lv.cutoff * toneScale, 0.45f * r), 0.6f, r);
            any = true;
        }
        if (!any || v.kill <= 0.0f) {
            v.on = false;
            for (auto& lv : v.layer) lv.on = false;
        }
    }
    // The bus follows the instrument.
    bus->shelfGain.next(dbToGain(p.air + (macros.tone - 0.5f) * 8.0f));
    const float chorusTarget = (p.characterMode == Character::Chorus ? p.chorus * characterGain : p.chorus) * macros.modulation;
    const float shimmerTarget = (p.characterMode == Character::Shimmer ? p.shimmer * characterGain : p.shimmer) * dc::dsp::shimmerBusBase * dc::dsp::shimmerControl(macros.shimmer);
    bus->chorusMix.next(chorusTarget);
    bus->shimmerSend.next(shimmerTarget);
    bus->trim.next(dbToGain(macros.levelDb));
    bus->tremolo.next(p.tremoloMod ? modNow * p.modDepth : 0.0f);
    bus->chorus.set(0.33f, 0.0032f, std::min(bus->chorusMix.value, 1.0f));
    bus->shimmer.set(0.85f, 5.0f, 0.75f);
}

void Synth::render(float* outL, float* outR, int n) noexcept {
    const float characterGain = 2.0f * macros.character;
    const uint64_t base = clock;
    for (int i = 0; i < n; ++i) {
        if (chunkLeft <= 0) {
            clock = base + uint64_t(i);
            updateChunk();
            chunkLeft = chunk;
        }
        --chunkLeft;
        float l = 0, r = 0, l0 = 0, r0 = 0;
        bool punched = false;
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
                    case Engine::Voice: {
                        // The ensemble is stereo of its own: each side filtered, then the layer's pan.
                        float a = 0, b = 0;
                        lv.voice.tick(a, b);
                        const float g = lv.gain * (lv.character ? characterGain : 1.0f) * 1.4142f;
                        vl += lv.lowpass.tick(lv.highpass.hp(a)).lp * g * lv.pan.l;
                        vr += lv.lowpassR.tick(lv.highpassR.hp(b)).lp * g * lv.pan.r;
                        continue;
                    }
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
            if (v.punchLength > 0) {
                // Punch: the note's own sum (l0, r0) and the punched one.
                l0 += vl;
                r0 += vr;
                const float g = v.punchGain;
                v.punchGain = std::max(1.0f, v.punchGain + v.punchStep);
                vl *= g;
                vr *= g;
                punched = true;
            } else {
                l0 += vl;
                r0 += vr;
            }
            l += vl;
            r += vr;
        }
        // Punch's safety limiter (instant attack, 60 ms release): only what Punch added is
        // ever taken back, and only above the ceiling (-0.4 dBFS at the instrument's output).
        auto& lim = *bus;
        if (punched || lim.limiterGain < 1.0f) {
            // The ceiling at the synth's output: the Level trim comes after, so it is divided out.
            const float ceiling = 0.955f / std::max(1.0e-3f, lim.trim.value);
            const float peak = std::max(std::abs(l), std::abs(r));
            const float floorLevel = std::max(ceiling, std::max(std::abs(l0), std::abs(r0)));
            const float needed = peak > floorLevel ? floorLevel / peak : 1.0f;
            if (needed < lim.limiterGain) lim.limiterGain = needed;
            else {
                lim.limiterGain = std::min(needed, lim.limiterGain + lim.limiterRelease * (1.0f - lim.limiterGain));
                if (lim.limiterGain > 0.99999f) lim.limiterGain = 1.0f;
            }
            lim.limiterLowest = std::min(lim.limiterLowest, lim.limiterGain);
            l *= lim.limiterGain;
            r *= lim.limiterGain;
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
    clock = base + uint64_t(n);
}

} // namespace ls::inst
