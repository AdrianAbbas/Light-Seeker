#include "Atmosphere.h"
#include "../World/Journey.h"
#include "../Synth/wb/SampleBank.h"

namespace dc {
using namespace dsp;

namespace {
using S = AtmosSound;
// The sounds of every place, in place order (up to three; the first is the most often).
constexpr AtmosSet sets[LocationCount][placesPerLocation] = {
    { { { S::Birdsong, S::DistantBell, S::SoftWind }, 3 }, { { S::Chimes, S::HarpGlint, S::DistantBell }, 3 },
      { { S::HarpGlint, S::DistantBell, S::Chimes }, 3 }, { { S::SoftWind, S::Birdsong, S::DistantBell }, 3 } },                 // Celestial Citadel
    { { { S::SoftWind, S::Chimes, S::DistantBell }, 3 }, { { S::DistantBell, S::HarpGlint, S::SoftWind }, 3 },
      { { S::Chimes, S::DistantBell, S::Birdsong }, 3 }, {} },                                                                      // Sky Temple
    { { { S::FountainShimmer, S::HarpGlint, S::Leaves }, 3 }, { { S::Leaves, S::Birdsong, S::HarpGlint }, 3 },
      { { S::Chimes, S::Leaves, S::Birdsong }, 3 }, {} },                                                                           // Elven Sanctuary
    { { { S::Chimes, S::HarpGlint, S::FountainShimmer }, 3 }, { { S::Brook, S::Chimes, S::SoftWind }, 3 },
      { { S::Chimes, S::HarpGlint, S::FountainShimmer }, 3 }, {} },                                                                 // Crystal Palace
    { { { S::Birdsong, S::Leaves, S::SoftWind }, 3 }, { { S::Birdsong, S::Leaves, S::HarpGlint }, 3 },
      { { S::SoftWind, S::Birdsong, S::DistantBell }, 3 }, {} },                                                                    // Blossom Meadow
    { { { S::DistantBell, S::HarpGlint, S::SoftWind }, 3 }, { { S::HarpGlint, S::DistantBell, S::Chimes }, 3 },
      { { S::Chimes, S::DistantBell, S::HarpGlint }, 3 }, {} },                                                                     // Golden Cathedral
    { { { S::SoftWind, S::Birdsong, S::Chimes }, 3 }, { { S::SoftWind, S::Breath, S::Birdsong }, 3 },
      { { S::SoftWind, S::DistantBell, S::Brook }, 3 }, {} },                                                                       // Floating Isles
    { { { S::Birdsong, S::FountainShimmer, S::Leaves }, 3 }, { { S::FountainShimmer, S::Birdsong, S::Brook }, 3 },
      { { S::Birdsong, S::SoftWind, S::FountainShimmer }, 3 }, {} },                                                                // Royal Gardens
    { { { S::HarpGlint, S::Chimes, S::Birdsong }, 3 }, { { S::Chimes, S::Leaves, S::HarpGlint }, 3 },
      { { S::HarpGlint, S::Chimes, S::Leaves }, 3 }, {} },                                                                          // Fairy Glade
    { { { S::SoftWind, S::DistantBell, S::Birdsong }, 3 }, { { S::SoftWind, S::Chimes, S::DistantBell }, 3 },
      { { S::SoftWind, S::DistantBell, S::Birdsong }, 3 }, {} },                                                                    // Dragon's Peak
};
constexpr const char* names[] = { "Soft wind", "Distant bell", "Birdsong", "Fountain shimmer", "Chimes", "Harp glint", "Leaves", "Brook", "Breath of air" };
// A major pentatonic (C D E G A), the notes the bells, chimes and harp choose from.
constexpr float pentatonic[] = { 1.0f, 1.1225f, 1.2599f, 1.4983f, 1.6818f };
inline float unit(std::uint32_t h, int shift) noexcept { return static_cast<float>((h >> shift) & 0xffu) / 255.0f; }
}

bool Atmosphere::breathRecorded() noexcept { return lswb::samples::Bank::get().instrument("breath") != nullptr; }

const char* atmosSoundName(AtmosSound s) noexcept { return names[std::clamp(static_cast<int>(s), 0, static_cast<int>(AtmosSound::Count) - 1)]; }

AtmosSet atmosphereOf(int location, int sub) noexcept {
    location = std::clamp(location, 0, LocationCount - 1);
    sub = clampSubLocation(location, sub);
    return sets[location][sub];
}

void Atmosphere::prepare(double sampleRate) {
    sr = sampleRate;
    attack = timeCoefficient(0.01, sr);
    release = timeCoefficient(0.4, sr);
    reset();
}

void Atmosphere::reset() {
    for (auto& v : voices) v = Voice {};
    lastSlot = -1;
    started = 0;
    duck = 1; envelope = 0; smoothLevel = 0;
}

int Atmosphere::activeVoices() const noexcept {
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void Atmosphere::start(AtmosSound sound, std::uint32_t h, float) noexcept {
    Voice* slot = nullptr;
    for (auto& v : voices) if (!v.active) { slot = &v; break; }
    if (slot == nullptr) return;   // all busy: skip (sparse by design)
    ++started;
    Voice& v = *slot;
    v = Voice {};
    v.active = true;
    v.sound = sound;
    v.random.seed(h | 1u);
    v.pan = (unit(h, 0) - 0.5f) * 1.4f;
    v.pitch = 0.85f + 0.3f * unit(h, 8);
    v.gain = 0.7f + 0.3f * unit(h, 16);
    const auto bp = [&](Svf& f, double hz, float q) { f.set(hz, sr, 1.0f / q); };
    const float note = pentatonic[static_cast<std::size_t>(std::min(4.0f, unit(h, 24) * 5.0f))];
    switch (sound) {
        case S::SoftWind: v.length = 3.5 + 2.0 * unit(h, 24); bp(v.filter, 650 * v.pitch, 1.2f); v.gain *= 0.8f; break;
        case S::DistantBell: v.length = 5.0; v.state = 523.25f * 0.5f * note; bp(v.filter, 2600, 0.7f); v.gain *= 0.8f; break;
        case S::Birdsong: v.length = 0.9 + 0.8 * unit(h, 24); v.count = 3 + static_cast<int>(unit(h, 24) * 4.0f); v.state = 2800.0f * v.pitch; break;
        case S::FountainShimmer: v.length = 1.8; bp(v.filter, 5200, 0.6f); break;
        case S::Chimes: v.length = 2.8; v.count = 3 + static_cast<int>(unit(h, 24) * 3.0f); v.state = 1046.5f * note; break;
        case S::HarpGlint: v.length = 2.2; v.count = 3 + static_cast<int>(unit(h, 24) * 2.0f); v.state = 523.25f * note; break;
        case S::Leaves: v.length = 0.8 + 0.6 * unit(h, 24); bp(v.filter, 3800 * v.pitch, 0.6f); v.gain *= 0.8f; break;
        case S::Brook: v.length = 1.6; bp(v.filter, 900 * v.pitch, 0.9f); break;
        case S::Breath: {
            // A recorded breath (one of the takes, by the hash), read a little faster (a
            // few semitones up: lighter, airier), high-passed to its air.
            v.data = nullptr;
            if (const auto* inst = lswb::samples::Bank::get().instrument("breath"))
                if (const auto* z = inst->find(60, 0.5f, unit(h, 24))) {
                    v.data = z->data;
                    v.dataLength = z->length;
                    v.readPos = 0;
                    v.readStep = double(inst->rate) / sr * (1.15 + 0.2 * unit(h, 8));
                    v.length = double(z->length) / (v.readStep * sr);
                }
            if (v.data == nullptr) v.length = 2.0 + unit(h, 24);   // the synthesised gust
            bp(v.filter, 1800, 0.7f);
            bp(v.filter2, 7000, 0.7f);
            v.gain *= 0.55f;
            break;
        }
        default: v.active = false; break;
    }
}

float Atmosphere::render(Voice& v) noexcept {
    const float t = static_cast<float>(v.age), len = static_cast<float>(v.length);
    const float dt = static_cast<float>(1.0 / sr);
    const float u = t / len;
    const float bell = std::sin(static_cast<float>(pi) * clamp01(u));   // a gentle in-out envelope
    float y = 0;
    switch (v.sound) {
        case S::SoftWind: {
            // An airy breath: band-passed noise whose centre drifts slowly.
            v.state2 += dt * 0.3f;
            v.filter.set(550.0 + 260.0 * std::sin(twoPi * v.state2), sr, 0.7f);
            y = v.filter.bandPass(v.random.bipolar()) * 0.45f * bell * bell;
            break;
        }
        case S::DistantBell: {
            // A far, gentle bell (a major chord of partials), softened by distance.
            static constexpr float ratios[] = { 0.5f, 1.0f, 1.26f, 1.5f, 2.0f, 2.52f };
            static constexpr float decays[] = { 0.45f, 0.6f, 0.85f, 1.0f, 1.3f, 1.8f };
            float sum = 0;
            for (int k = 0; k < 6; ++k) sum += std::sin(twoPi * v.state * ratios[k] * t) * std::exp(-decays[k] * t) / (1.0f + 0.5f * static_cast<float>(k));
            y = v.filter.low(sum) * 0.28f * std::min(1.0f, t * 120.0f);
            break;
        }
        case S::Birdsong: case S::Chimes: case S::HarpGlint: {
            // A short phrase of notes: chirps, chime strikes or plucked harp strings.
            if (v.next <= 0 && v.count > 0) {
                --v.count;
                v.env = 1;
                v.next = static_cast<int>(sr * (v.sound == S::Birdsong ? 0.07 + 0.09 * v.random.unit() : v.sound == S::Chimes ? 0.18 + 0.25 * v.random.unit() : 0.13));
                v.phase = v.phase2 = 0;
                if (v.sound == S::HarpGlint) v.state2 = v.state * pentatonic[static_cast<std::size_t>(v.count % 5)] * (v.count >= 5 ? 2.0f : 1.0f);
                else if (v.sound == S::Chimes) v.state2 = v.state * pentatonic[static_cast<std::size_t>(std::min(4.0f, v.random.unit() * 5.0f))];
                else v.state2 = v.state * (0.85f + 0.4f * v.random.unit());
            }
            --v.next;
            if (v.sound == S::Birdsong) {
                // A chirp: a quick downward sweep with a trill, 40-90 ms.
                const float local = 1.0f - v.env;
                const float f = v.state2 * (1.25f - 0.45f * local) * (1.0f + 0.06f * std::sin(twoPi * 38.0f * t));
                v.phase += f * dt; v.phase -= std::floor(v.phase);
                y = std::sin(twoPi * v.phase) * std::sin(static_cast<float>(pi) * clamp01(local)) * 0.22f;
                v.env = std::max(0.0f, v.env - dt / 0.065f);
            } else if (v.sound == S::Chimes) {
                // A chime: a fundamental and two inharmonic partials, ringing out.
                v.phase += v.state2 * dt; v.phase -= std::floor(v.phase);
                v.phase2 += v.state2 * 2.76f * dt; v.phase2 -= std::floor(v.phase2);
                y = (std::sin(twoPi * v.phase) + 0.35f * std::sin(twoPi * v.phase2)) * v.env * 0.16f;
                v.env *= 1.0f - 2.4f * dt;
            } else {
                // A harp string: a soft pluck (fundamental and octave), up the scale.
                v.phase += v.state2 * dt; v.phase -= std::floor(v.phase);
                const float s1 = std::sin(twoPi * v.phase), s2 = std::sin(2.0f * twoPi * v.phase);
                y = (s1 + 0.3f * s2 * v.env) * v.env * 0.2f * std::min(1.0f, (1.0f - v.env) * 400.0f + 0.05f);
                v.env *= 1.0f - 3.2f * dt;
            }
            break;
        }
        case S::FountainShimmer: {
            // Water falling in the basin: tiny bright droplets over a soft hiss.
            float drops = 0;
            if (v.random.unit() < 0.004f) { v.state2 = 2000.0f + 4000.0f * v.random.unit(); v.env = 1; }
            v.phase += v.state2 * dt; v.phase -= std::floor(v.phase);
            drops = std::sin(twoPi * v.phase) * v.env;
            v.env *= 1.0f - 160.0f * dt;
            y = (drops * 0.12f + v.filter.bandPass(v.random.bipolar()) * 0.12f) * bell;
            break;
        }
        case S::Leaves: y = v.filter.bandPass(v.random.bipolar()) * bell * bell * 0.3f; break;
        case S::Breath: {
            float x = 0;
            if (v.data != nullptr) {
                const auto i = static_cast<int>(v.readPos);
                if (i + 1 < v.dataLength) {
                    const float f = static_cast<float>(v.readPos - i);
                    x = (v.data[i] * (1.0f - f) + v.data[i + 1] * f) * (1.0f / 32768.0f) * 1.6f;
                }
                v.readPos += v.readStep;
            } else {
                x = v.random.bipolar() * 0.35f * bell * bell;   // a gust of air
            }
            // Only the air: above ~1.8 kHz, gently rolled off above 7 kHz.
            y = v.filter2.low(v.filter.highPass(x));
            break;
        }
        case S::Brook: {
            v.filter.set(700.0 + 500.0 * std::sin(twoPi * 3.0f * t + v.pan), sr, 0.8f);
            y = v.filter.bandPass(v.random.bipolar()) * bell * 0.35f;
            break;
        }
        default: break;
    }
    v.age += 1.0 / sr;
    if (v.age >= v.length) v.active = false;
    return finiteOr(y) * v.gain;
}

void Atmosphere::process(float* l, float* r, int n, double beats, double beatsPerSample, int location, int sub,
                         float level, float density, int seed, const float* dryL, const float* dryR, float mixGain) noexcept {
    level = clamp01(level);
    mixGain = std::clamp(std::isfinite(mixGain) ? mixGain : 1.0f, 0.0f, 1.5f);
    // Schedule: one chance per eighth note, decided by a hash of (seed, slot, place).
    const auto slotAt = [](double b) { return static_cast<long long>(std::floor(std::max(0.0, b) * 2.0)); };
    const long long first = slotAt(beats), last = slotAt(beats + beatsPerSample * n);
    if (lastSlot < 0 || first < lastSlot || first - lastSlot > 64) lastSlot = first - 1;   // a jump in position
    const float chance = clamp01(density) * 0.3f;
    const auto set = atmosphereOf(location, sub);
    for (long long slot = lastSlot + 1; slot <= last && level > 1.0e-4f && set.count > 0; ++slot) {
        const auto h = roamHash(static_cast<std::uint32_t>(seed) * 977u + 13u, slot, static_cast<std::uint32_t>(location * 8 + sub + 101));
        if (static_cast<float>(h & 0xffffu) / 65535.0f >= chance) continue;
        // Weighted choice: the first sound most often.
        const float pick = unit(h, 16);
        const int index = set.count == 1 ? 0 : (pick < 0.55f ? 0 : (pick < 0.85f || set.count == 2 ? 1 : 2));
        start(set.sounds[static_cast<std::size_t>(index)], h, level);
    }
    lastSlot = std::max(lastSlot, last);

    const float levelStep = timeCoefficient(0.05, sr);
    for (int i = 0; i < n; ++i) {
        smoothLevel += levelStep * (level - smoothLevel);
        smoothMix += levelStep * (mixGain - smoothMix);
        if (std::abs(mixGain - smoothMix) < 1.0e-6f) smoothMix = mixGain;
        // Ducking: under the instrument the layer steps back (-12 dB when it is loud).
        const float in = std::max(std::abs(dryL[i]), std::abs(dryR[i]));
        envelope += (in > envelope ? attack : release) * (in - envelope);
        duck = 1.0f / (1.0f + 6.0f * envelope);
        float sl = 0, sr2 = 0;
        for (auto& v : voices) {
            if (!v.active) continue;
            const float y = render(v);
            sl += y * (1.0f - v.pan) * 0.7f;
            sr2 += y * (1.0f + v.pan) * 0.7f;
        }
        // At Level 100 % an event peaks around -20 dBFS: a background, never a feature.
        const float g = smoothLevel * smoothLevel * duck * 0.18f * smoothMix;
        l[i] += sl * g;
        r[i] += sr2 * g;
    }
}
}
