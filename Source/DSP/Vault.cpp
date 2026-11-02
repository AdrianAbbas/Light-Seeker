#include "Modules.h"
#include "Convolver.h"
#if LS_HAVE_CAPTURED_IRS
#include "LightSeekerIrs.h"
#endif
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <mutex>

// IX. RADIANCE (the Vault class): convolution spaces. Each space is generated from a
// recipe:
// three-band exponentially decaying noise (its own RT60 per band), a slow bloom,
// pre-delay, early reflections, far discrete echoes, bright glints (coins, crystals,
// stars), a low-mid body, and optionally reversed (a swell drawn in towards the note).
// Deterministic (fixed seeds). Each IR is normalised to unit energy per channel.
//
// Spaces 32..34 are captured: impulse responses measured from the developer's own effect
// captures (tools/captures: analysed, the direct sound removed, the tail continued at the
// measured RT60 per octave, 48 kHz) and compiled in. They are derived from private
// recordings, so they are not in the repository: CI makes them from the Samples
// repository (tools/captures/ci-build-irs.sh, CMake's LS_IRS_DIR). A build without them
// (LS_HAVE_CAPTURED_IRS 0) uses generated spaces of the same character instead
// (fallbackRecipes). They are resampled to the host's rate when the set is built.
namespace dc {
using namespace dsp;

namespace {
struct IrRecipe {
    float seconds = 2, preMs = 0, bloomMs = 10;
    float rtLow = 2, rtMid = 2, rtHigh = 1.5f;     // RT60 below 300 Hz, 300 Hz-3 kHz, above 3 kHz
    float lowCut = 60, highCut = 12000;            // on the whole IR
    float bodyHz = 250, bodyDb = 0;                // low-mid body
    float width = 1;                               // 0 = same tail both sides, 1 = decorrelated
    int early = 0; float earlyMs = 60, earlyGain = .25f, earlyLp = 8000;
    int echoes = 0; float echoMs[4] {}, echoGain[4] {}; float echoBlurMs = 12, echoLp = 4000;
    float glints = 0, glintHz = 5000, glintGain = .15f;   // per second
    bool reverse = false;
    float tailGain = 1;                            // the diffuse tail against reflections/echoes
};

// In kind order (vaultChoices in ModuleParams.h): bright, luminous spaces.
const IrRecipe recipes[Vault::kinds] = {
    {},
    // 1 Dawn Hall (Gates of Dawn): a bright marble hall, brassy early reflections.
    { .seconds = 3.6f, .preMs = 22, .bloomMs = 40, .rtLow = 3, .rtMid = 3.4f, .rtHigh = 3, .lowCut = 70, .highCut = 16000, .bodyHz = 900, .bodyDb = 2, .width = 1, .early = 6, .earlyMs = 70, .earlyGain = 0.24f, .earlyLp = 14000 },
    // 2 Mirror Gallery (Hall of Mirrors): dense, wide, glassy early reflections.
    { .seconds = 2.6f, .preMs = 5, .bloomMs = 10, .rtLow = 1.8f, .rtMid = 2.4f, .rtHigh = 2.4f, .lowCut = 110, .highCut = 17000, .width = 1, .early = 6, .earlyMs = 120, .earlyGain = 0.3f, .earlyLp = 15000, .glints = 10, .glintHz = 4500, .glintGain = 0.12f },
    // 3 Throne Hall (Throne of Light): rich, regal, golden glints.
    { .seconds = 4.6f, .preMs = 38, .bloomMs = 120, .rtLow = 4, .rtMid = 4.4f, .rtHigh = 3.6f, .lowCut = 55, .highCut = 14000, .bodyHz = 420, .bodyDb = 2, .width = 1, .early = 5, .earlyMs = 80, .earlyGain = 0.14f, .earlyLp = 10000, .glints = 25, .glintHz = 4800, .glintGain = 0.22f },
    // 4 Open Heights (Spire Summit): thin open air, far airy echoes.
    { .seconds = 4, .preMs = 40, .bloomMs = 80, .rtLow = 1.4f, .rtMid = 3, .rtHigh = 3, .lowCut = 250, .highCut = 15000, .width = 1, .echoes = 3, .echoMs = { 560, 1210, 1960 }, .echoGain = { 0.32f, 0.22f, 0.14f }, .echoBlurMs = 25, .echoLp = 9000, .tailGain = 0.6f },
    // 5 Temple Air (Cloud Steps): soft, airy and sacred, slow bloom.
    { .seconds = 5.5f, .preMs = 45, .bloomMs = 300, .rtLow = 3.5f, .rtMid = 5, .rtHigh = 4.5f, .lowCut = 120, .highCut = 15000, .width = 1, .glints = 6, .glintHz = 5000, .glintGain = 0.1f },
    // 6 Sanctum Bloom (Sanctum): warm, close, softly blooming.
    { .seconds = 4.5f, .preMs = 25, .bloomMs = 350, .rtLow = 4.2f, .rtMid = 4.2f, .rtHigh = 2.4f, .lowCut = 60, .highCut = 9000, .bodyHz = 320, .bodyDb = 2, .width = 0.9f, .early = 4, .earlyMs = 40, .earlyGain = 0.1f, .earlyLp = 6000 },
    // 7 Bell Garden: an open garden of stone, bright and ringing.
    { .seconds = 3, .preMs = 15, .bloomMs = 20, .rtLow = 1.6f, .rtMid = 2.8f, .rtHigh = 3, .lowCut = 150, .highCut = 16000, .width = 1, .early = 4, .earlyMs = 60, .earlyGain = 0.14f, .earlyLp = 12000, .glints = 14, .glintHz = 3500, .glintGain = 0.18f },
    // 8 Moon Pool (Moonwell): a silver night air over water.
    { .seconds = 5, .preMs = 30, .bloomMs = 150, .rtLow = 3.5f, .rtMid = 4.6f, .rtHigh = 4, .lowCut = 80, .highCut = 13000, .width = 1, .early = 3, .earlyMs = 90, .earlyGain = 0.1f, .earlyLp = 9000, .glints = 10, .glintHz = 4200, .glintGain = 0.14f },
    // 9 Wooden Hall (Leaf Halls): warm living wood.
    { .seconds = 2.2f, .preMs = 8, .bloomMs = 30, .rtLow = 2, .rtMid = 1.8f, .rtHigh = 1, .lowCut = 70, .highCut = 8000, .bodyHz = 260, .bodyDb = 3, .width = 0.85f, .early = 6, .earlyMs = 45, .earlyGain = 0.2f, .earlyLp = 6000 },
    // 10 Star Canopy (Starlit Canopy): wide and sparkling.
    { .seconds = 6.5f, .preMs = 50, .bloomMs = 250, .rtLow = 3, .rtMid = 6, .rtHigh = 6.5f, .lowCut = 140, .highCut = 18000, .width = 1, .glints = 30, .glintHz = 5500, .glintGain = 0.24f },
    // 11 Prism Chamber (Prism Hall): bright glass, the highs ring longest.
    { .seconds = 4, .preMs = 12, .bloomMs = 30, .rtLow = 1.4f, .rtMid = 3.6f, .rtHigh = 4.2f, .lowCut = 250, .highCut = 18000, .width = 1, .early = 5, .earlyMs = 50, .earlyGain = 0.18f, .earlyLp = 16000, .glints = 35, .glintHz = 3800, .glintGain = 0.26f },
    // 12 Still Lake (Mirror Lake): calm, reflective, a long smooth tail.
    { .seconds = 8, .preMs = 70, .bloomMs = 400, .rtLow = 7, .rtMid = 8, .rtHigh = 5, .lowCut = 50, .highCut = 10000, .width = 1, .echoes = 1, .echoMs = { 420 }, .echoGain = { 0.2f }, .echoBlurMs = 40, .echoLp = 6000 },
    // 13 Diamond Room (Diamond Vault): small, hard, glittering.
    { .seconds = 2.2f, .preMs = 4, .bloomMs = 5, .rtLow = 0.8f, .rtMid = 1.8f, .rtHigh = 2.2f, .lowCut = 250, .highCut = 18000, .width = 1, .early = 6, .earlyMs = 25, .earlyGain = 0.24f, .earlyLp = 16000, .glints = 40, .glintHz = 3500, .glintGain = 0.3f },
    // 14 Meadow Air (Sunlit Meadow): soft warm open air.
    { .seconds = 2, .preMs = 18, .bloomMs = 60, .rtLow = 1.2f, .rtMid = 1.8f, .rtHigh = 1.2f, .lowCut = 90, .highCut = 9000, .width = 1, .early = 3, .earlyMs = 60, .earlyGain = 0.08f, .earlyLp = 6000, .tailGain = 0.8f },
    // 15 Glade Air (Butterfly Glade): light, airy, bright.
    { .seconds = 2.4f, .preMs = 12, .bloomMs = 40, .rtLow = 1.2f, .rtMid = 2.2f, .rtHigh = 2, .lowCut = 140, .highCut = 14000, .width = 1, .early = 4, .earlyMs = 50, .earlyGain = 0.1f, .earlyLp = 10000 },
    // 16 Valley (Wildflower Hill): open hills, echoes from the far side.
    { .seconds = 3.5f, .preMs = 25, .bloomMs = 60, .rtLow = 1.6f, .rtMid = 2.6f, .rtHigh = 1.8f, .lowCut = 100, .highCut = 9000, .width = 1, .echoes = 2, .echoMs = { 620, 1320 }, .echoGain = { 0.3f, 0.18f }, .echoBlurMs = 30, .echoLp = 5000, .tailGain = 0.7f },
    // 17 Radiant Nave (Nave of Light): the longest, most radiant space.
    { .seconds = 11, .preMs = 100, .bloomMs = 600, .rtLow = 11, .rtMid = 12, .rtHigh = 8, .lowCut = 40, .highCut = 14000, .bodyHz = 600, .bodyDb = 1, .width = 1, .early = 3, .earlyMs = 180, .earlyGain = 0.06f, .earlyLp = 8000, .glints = 8, .glintHz = 5000, .glintGain = 0.12f },
    // 18 Choir Loft: high under the vault, soft and luminous.
    { .seconds = 5, .preMs = 35, .bloomMs = 250, .rtLow = 4.5f, .rtMid = 5, .rtHigh = 3.6f, .lowCut = 80, .highCut = 12000, .width = 1, .early = 4, .earlyMs = 60, .earlyGain = 0.1f, .earlyLp = 8000 },
    // 19 Glass Chapel (Stained Glass Chapel): bright glass and stone.
    { .seconds = 3.8f, .preMs = 20, .bloomMs = 60, .rtLow = 2.6f, .rtMid = 3.6f, .rtHigh = 3.6f, .lowCut = 100, .highCut = 16000, .width = 1, .early = 5, .earlyMs = 50, .earlyGain = 0.14f, .earlyLp = 12000, .glints = 20, .glintHz = 4000, .glintGain = 0.2f },
    // 20 Sky Bridge (Cloud Bridge): wide open sky, swirling.
    { .seconds = 4.5f, .preMs = 35, .bloomMs = 150, .rtLow = 2, .rtMid = 4, .rtHigh = 3.6f, .lowCut = 180, .highCut = 15000, .width = 1, .early = 4, .earlyMs = 150, .earlyGain = 0.1f, .earlyLp = 10000 },
    // 21 Wind Isle (Windswept Isle): thin, airy, wind-filtered.
    { .seconds = 3, .preMs = 20, .bloomMs = 60, .rtLow = 1, .rtMid = 2.4f, .rtHigh = 1.6f, .lowCut = 350, .highCut = 8000, .width = 1, .tailGain = 0.8f },
    // 22 Harbour (Sky Harbour): open water and sky, long and clear.
    { .seconds = 4, .preMs = 30, .bloomMs = 80, .rtLow = 2.4f, .rtMid = 3.4f, .rtHigh = 3, .lowCut = 100, .highCut = 14000, .width = 1, .echoes = 1, .echoMs = { 540 }, .echoGain = { 0.2f }, .echoBlurMs = 30, .echoLp = 8000 },
    // 23 Rose Bower (Rose Garden): soft, lush and close.
    { .seconds = 2.6f, .preMs = 15, .bloomMs = 120, .rtLow = 2.2f, .rtMid = 2.4f, .rtHigh = 1.6f, .lowCut = 80, .highCut = 11000, .bodyHz = 500, .bodyDb = 1.5f, .width = 1, .early = 4, .earlyMs = 40, .earlyGain = 0.1f, .earlyLp = 8000 },
    // 24 Fountain Court: a bright stone court with water.
    { .seconds = 2.6f, .preMs = 10, .bloomMs = 20, .rtLow = 1.6f, .rtMid = 2.4f, .rtHigh = 2.4f, .lowCut = 120, .highCut = 16000, .width = 1, .early = 5, .earlyMs = 80, .earlyGain = 0.16f, .earlyLp = 12000, .glints = 20, .glintHz = 5000, .glintGain = 0.16f },
    // 25 Colonnade (Marble Colonnade): polished marble, hard and clear.
    { .seconds = 2.6f, .preMs = 8, .bloomMs = 10, .rtLow = 1.8f, .rtMid = 2.4f, .rtHigh = 2.2f, .lowCut = 90, .highCut = 15000, .width = 1, .early = 6, .earlyMs = 100, .earlyGain = 0.2f, .earlyLp = 12000 },
    // 26 Mushroom Ring: a small bright glade.
    { .seconds = 1.8f, .preMs = 6, .bloomMs = 15, .rtLow = 0.9f, .rtMid = 1.6f, .rtHigh = 1.8f, .lowCut = 180, .highCut = 16000, .width = 1, .early = 4, .earlyMs = 30, .earlyGain = 0.14f, .earlyLp = 12000, .glints = 12, .glintHz = 4500, .glintGain = 0.16f },
    // 27 Firefly Hollow: a soft hollow full of little lights.
    { .seconds = 3.4f, .preMs = 20, .bloomMs = 120, .rtLow = 2, .rtMid = 3.2f, .rtHigh = 3.2f, .lowCut = 120, .highCut = 17000, .width = 1, .glints = 40, .glintHz = 5200, .glintGain = 0.26f },
    // 28 Fairy Room (Fairy Court): a tiny, very bright room.
    { .seconds = 1.1f, .preMs = 2, .bloomMs = 4, .rtLow = 0.5f, .rtMid = 0.9f, .rtHigh = 1.1f, .lowCut = 220, .highCut = 18000, .width = 1, .early = 6, .earlyMs = 14, .earlyGain = 0.26f, .earlyLp = 16000 },
    // 29 Cliff Eyrie (Eyrie): vast cliffs, distinct far echoes.
    { .seconds = 5, .preMs = 45, .bloomMs = 100, .rtLow = 3, .rtMid = 4, .rtHigh = 2.8f, .lowCut = 60, .highCut = 11000, .width = 1, .early = 3, .earlyMs = 200, .earlyGain = 0.12f, .earlyLp = 7000, .echoes = 3, .echoMs = { 480, 1050, 1700 }, .echoGain = { 0.36f, 0.26f, 0.17f }, .echoBlurMs = 20, .echoLp = 7000, .tailGain = 0.75f },
    // 30 Golden Nest: warm and golden, a cave of gold.
    { .seconds = 3.2f, .preMs = 18, .bloomMs = 60, .rtLow = 3, .rtMid = 3, .rtHigh = 2, .lowCut = 50, .highCut = 11000, .bodyHz = 300, .bodyDb = 3, .width = 1, .early = 5, .earlyMs = 50, .earlyGain = 0.16f, .earlyLp = 8000, .glints = 18, .glintHz = 3800, .glintGain = 0.2f },
    // 31 Heroic Summit (Summit of Winds): huge, heroic and wide.
    { .seconds = 8, .preMs = 70, .bloomMs = 350, .rtLow = 7, .rtMid = 8, .rtHigh = 6, .lowCut = 45, .highCut = 15000, .bodyHz = 400, .bodyDb = 1, .width = 1, .early = 3, .earlyMs = 150, .earlyGain = 0.08f, .earlyLp = 9000 },
};

// The captured spaces' stand-ins, for builds without the captures: generated spaces in
// their spirit (a long bright shimmering hall; a bright hall with far echoes; a lush hall).
const IrRecipe fallbackRecipes[3] = {
    { .seconds = 10.5f, .preMs = 20, .bloomMs = 200, .rtLow = 4, .rtMid = 9.5f, .rtHigh = 8, .lowCut = 110, .highCut = 18000, .width = 1, .early = 3, .earlyMs = 60, .earlyGain = 0.1f, .earlyLp = 14000, .glints = 30, .glintHz = 5000, .glintGain = 0.22f },
    { .seconds = 6, .preMs = 15, .bloomMs = 60, .rtLow = 1, .rtMid = 3.4f, .rtHigh = 2.2f, .lowCut = 140, .highCut = 15000, .width = 1, .echoes = 3, .echoMs = { 380, 760, 1140 }, .echoGain = { 0.3f, 0.2f, 0.12f }, .echoBlurMs = 18, .echoLp = 8000, .tailGain = 0.8f },
    { .seconds = 3.6f, .preMs = 14, .bloomMs = 40, .rtLow = 2.6f, .rtMid = 2.8f, .rtHigh = 1.8f, .lowCut = 80, .highCut = 12000, .bodyHz = 400, .bodyDb = 1, .width = 1, .early = 5, .earlyMs = 35, .earlyGain = 0.16f, .earlyLp = 10000, .tailGain = 0.75f },
};

double lengthScale(double sampleRate) { return sampleRate > 50000.0 ? 0.6 : 1.0; }   // memory at 88.2/96 kHz

struct OnePole {
    float a = 0, s = 0;
    void set(double hz, double sr) { a = poleForHz(hz, sr); }
    float lp(float x) { s += a * (x - s); return s; }
    float hp(float x) { s += a * (x - s); return x - s; }
};

std::array<std::vector<float>, 2> generate(const IrRecipe& r, double sr, std::uint32_t seed) {
    const auto total = static_cast<std::size_t>(r.seconds * lengthScale(sr) * sr);
    const auto pre = static_cast<std::size_t>(r.preMs * 0.001 * sr);
    std::array<std::vector<float>, 2> out;
    Random random; random.seed(seed);
    std::array<std::vector<float>, 2> noise;
    for (auto& n : noise) { n.resize(total); for (auto& v : n) v = random.bipolar(); }
    for (std::size_t i = 0; i < total; ++i) noise[1][i] = r.width * noise[1][i] + (1.0f - r.width) * noise[0][i];
    for (int ch = 0; ch < 2; ++ch) {
        auto& h = out[static_cast<std::size_t>(ch)];
        h.assign(total + pre, 0.0f);
        OnePole lowBand, highBand; lowBand.set(300, sr); highBand.set(3000, sr);
        const double kL = 6.9078 / (r.rtLow * sr), kM = 6.9078 / (r.rtMid * sr), kH = 6.9078 / (r.rtHigh * sr);
        const double bloom = std::max(0.002, r.bloomMs * 0.001) * sr;
        double tailEnergy = 0;
        std::vector<float> tail(total);
        for (std::size_t i = 0; i < total; ++i) {
            const float n = noise[static_cast<std::size_t>(ch)][i];
            const float low = lowBand.lp(n), high = n - highBand.lp(n), mid = n - low - high;
            const double t = static_cast<double>(i);
            const double v = low * std::exp(-kL * t) + mid * std::exp(-kM * t) + high * std::exp(-kH * t);
            tail[i] = static_cast<float>(v * (1.0 - std::exp(-t / bloom)));
            tailEnergy += double(tail[i]) * tail[i];
        }
        const float tailScale = r.tailGain / static_cast<float>(std::sqrt(std::max(1.0e-20, tailEnergy)));
        // Early reflections and far echoes: discrete taps (echoes as short clusters).
        std::vector<float> taps(total, 0.0f), echoes(total, 0.0f), glints(total, 0.0f);
        Random tapRandom; tapRandom.seed(seed * 7u + 11u);
        for (int k = 0; k < r.early; ++k) {
            const float frac = (static_cast<float>(k) + 0.3f + 0.6f * tapRandom.unit()) / static_cast<float>(std::max(1, r.early));
            const auto at = static_cast<std::size_t>(frac * r.earlyMs * 0.001f * sr);
            const float pan = (k & 1) ? 0.7f : -0.7f;
            const float g = r.earlyGain * (1.0f - 0.55f * static_cast<float>(k) / static_cast<float>(std::max(1, r.early)))
                          * (1.0f + (ch == 0 ? -pan : pan) * 0.8f) * (tapRandom.unit() < 0.5f ? -1.0f : 1.0f);
            if (at < total) taps[at] += g;
        }
        for (int e = 0; e < r.echoes; ++e) {
            const auto at = static_cast<std::size_t>(r.echoMs[e] * 0.001f * sr);
            const float pan = (e & 1) ? 0.6f : -0.6f;
            for (int c = 0; c < 5; ++c) {
                const auto pos = at + static_cast<std::size_t>(tapRandom.unit() * r.echoBlurMs * 0.001f * sr);
                if (pos < total) echoes[pos] += r.echoGain[e] / std::sqrt(5.0f) * (1.0f + (ch == 0 ? -pan : pan) * 0.7f)
                                                * (c == 0 ? 1.0f : (tapRandom.unit() < 0.5f ? -0.7f : 0.7f));
            }
        }
        // Glints: short decaying sines high up, following the tail's envelope.
        if (r.glints > 0) {
            Random glintRandom; glintRandom.seed(seed * 13u + static_cast<std::uint32_t>(ch) * 101u + 3u);
            const double perSample = r.glints / sr;
            for (std::size_t i = 0; i < total; ++i) {
                if (glintRandom.unit() >= perSample) continue;
                const double hz = r.glintHz * (1.0 + 1.2 * glintRandom.unit());
                const double life = (0.006 + 0.02 * glintRandom.unit()) * sr;
                const double env0 = std::exp(-kM * static_cast<double>(i)) * (1.0 - std::exp(-static_cast<double>(i) / bloom));
                const float amp = static_cast<float>(r.glintGain * env0 * (0.4 + 0.6 * glintRandom.unit()));
                for (std::size_t j = 0; j < static_cast<std::size_t>(4 * life) && i + j < total; ++j)
                    glints[i + j] += amp * static_cast<float>(std::sin(2.0 * pi * hz * j / sr) * std::exp(-static_cast<double>(j) / life));
            }
        }
        OnePole earlyLp, echoLp; earlyLp.set(r.earlyLp, sr); echoLp.set(r.echoLp, sr);
        OnePole lc, hc1, hc2; lc.set(r.lowCut, sr); hc1.set(r.highCut, sr); hc2.set(r.highCut, sr);
        Biquad body; body.peaking(sr, r.bodyHz, 0.8, r.bodyDb);
        for (std::size_t i = 0; i < total; ++i) {
            float v = tail[i] * tailScale + earlyLp.lp(taps[i]) + echoLp.lp(echoes[i]);
            v = hc2.lp(hc1.lp(lc.hp(v)));
            v = body.process(v) + glints[i] * tailScale * 40.0f;
            h[pre + i] = v;
        }
        if (r.reverse) {
            std::reverse(h.begin() + static_cast<long>(pre), h.end());
            const auto fadeIn = static_cast<std::size_t>(0.01 * sr), fadeOut = static_cast<std::size_t>(0.025 * sr);
            for (std::size_t i = 0; i < fadeIn && pre + i < h.size(); ++i) h[pre + i] *= static_cast<float>(i) / static_cast<float>(fadeIn);
            for (std::size_t i = 0; i < fadeOut && i < h.size(); ++i) h[h.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(fadeOut);
        } else {
            // A short fade at the very end (no truncation click).
            const auto fade = static_cast<std::size_t>(0.05 * sr);
            for (std::size_t i = 0; i < fade && i < h.size(); ++i) h[h.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);
        }
    }
    double energy = 0;
    for (const auto& h : out) for (float v : h) energy += double(v) * v;
    const float norm = static_cast<float>(std::sqrt(2.0 / std::max(1.0e-20, energy)));
    for (auto& h : out) for (auto& v : h) v *= norm;
    return out;
}

// The captured spaces, in kind order from Vault::firstCaptured (made in CI: LS_IRS_DIR).
constexpr const char* capturedFiles[] = { "radiant-shimmer-hall.wav", "cliff-hall-echo.wav", "chorus-hall.wav" };
constexpr double capturedSeconds[] = { 9.5, 6.0, 5.0 };   // as exported (export_irs.py)

std::array<std::vector<float>, 2> captured(int index, double sr) {
    std::array<std::vector<float>, 2> out;
    const char* file = capturedFiles[std::clamp(index, 0, 2)];
    int size = 0;
    const char* data = nullptr;
#if LS_HAVE_CAPTURED_IRS
    for (int i = 0; i < LightSeekerIrs::namedResourceListSize; ++i)
        if (juce::String(LightSeekerIrs::originalFilenames[i]) == file)
            data = LightSeekerIrs::getNamedResource(LightSeekerIrs::namedResourceList[i], size);
#else
    juce::ignoreUnused(file);
#endif
    if (data != nullptr) {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::MemoryInputStream(data, static_cast<std::size_t>(size), false), true));
        if (reader != nullptr && reader->lengthInSamples > 0) {
            const int n = static_cast<int>(reader->lengthInSamples);
            juce::AudioBuffer<float> buffer(2, n);
            reader->read(&buffer, 0, n, 0, true, true);
            const double ratio = reader->sampleRate / sr;
            // The interpolator reads a little ahead: stop 64 input samples before the end.
            const int outLength = std::max(1, static_cast<int>(std::floor((n - 64) / ratio * lengthScale(sr))));
            for (int ch = 0; ch < 2; ++ch) {
                auto& h = out[static_cast<std::size_t>(ch)];
                h.assign(static_cast<std::size_t>(outLength), 0.0f);
                if (std::abs(ratio - 1.0) < 1.0e-9) std::copy(buffer.getReadPointer(ch), buffer.getReadPointer(ch) + std::min(outLength, n), h.begin());
                else {
                    juce::WindowedSincInterpolator interpolator;
                    interpolator.process(ratio, buffer.getReadPointer(ch), h.data(), outLength);
                }
                // A short fade at the very end (lengthScale may shorten it).
                const auto fade = std::min<std::size_t>(h.size() / 4, static_cast<std::size_t>(0.05 * sr));
                for (std::size_t i = 0; i < fade; ++i) h[h.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);
            }
        }
    }
    if (out[0].empty())   // built without the captures: the generated stand-in
        return generate(fallbackRecipes[std::clamp(index, 0, 2)], sr, 0x5eedca70u + static_cast<std::uint32_t>(index) * 977u);
    double energy = 0;
    for (const auto& h : out) for (float v : h) energy += double(v) * v;
    const float norm = static_cast<float>(std::sqrt(2.0 / std::max(1.0e-20, energy)));
    for (auto& h : out) for (auto& v : h) v *= norm;
    return out;
}

// The shared set: every kind's spectra for one sample rate, built on first use.
struct IrSet {
    std::vector<std::shared_ptr<const conv::IrSpectra>> owned;
    std::vector<std::array<const conv::IrSpectra*, 2>> table;
    int maxLength = conv::minLength;
};
std::shared_ptr<const IrSet> irSetFor(double sr) {
    static std::mutex mutex;
    static std::map<long, std::weak_ptr<const IrSet>> sets;
    std::lock_guard<std::mutex> lock(mutex);
    const long key = std::lround(sr);
    if (auto existing = sets[key].lock()) return existing;
    auto set = std::make_shared<IrSet>();
    set->table.resize(Vault::kinds, { nullptr, nullptr });
    for (int k = 1; k < Vault::kinds; ++k) {
        const auto ir = k >= Vault::firstCaptured ? captured(k - Vault::firstCaptured, sr)
                                                  : generate(recipes[k], sr, 0x5eed0000u + static_cast<std::uint32_t>(k) * 977u);
        for (int ch = 0; ch < 2; ++ch) {
            auto spectra = conv::buildSpectra(ir[static_cast<std::size_t>(ch)].data(), static_cast<int>(ir[static_cast<std::size_t>(ch)].size()));
            set->maxLength = std::max(set->maxLength, spectra->length);
            set->table[static_cast<std::size_t>(k)][static_cast<std::size_t>(ch)] = spectra.get();
            set->owned.push_back(std::move(spectra));
        }
    }
    std::shared_ptr<const IrSet> result = set;
    sets[key] = result;
    return result;
}
}

bool Vault::capturedSpacesBuiltIn() noexcept { return LS_HAVE_CAPTURED_IRS != 0; }

double Vault::kindSeconds(int k) noexcept {
    if (k >= firstCaptured && k < kinds)
        return LS_HAVE_CAPTURED_IRS ? capturedSeconds[k - firstCaptured]
                                    : fallbackRecipes[k - firstCaptured].seconds + fallbackRecipes[k - firstCaptured].preMs * 0.001;
    return k > 0 && k < kinds ? recipes[k].seconds + recipes[k].preMs * 0.001 : 0.0;
}
const char* Vault::kindName(int k) noexcept {
    static const auto names = [] {
        std::array<std::array<char, 32>, kinds> out {};
        const char* s = vaultChoices;
        for (auto& name : out) {
            std::size_t i = 0;
            while (*s != 0 && *s != '|') { if (i + 1 < name.size()) name[i++] = *s; ++s; }
            if (*s == '|') ++s;
        }
        return out;
    }();
    return names[static_cast<std::size_t>(std::clamp(k, 0, kinds - 1))].data();
}

Vault::Vault() : left(std::make_unique<conv::Convolver>()), right(std::make_unique<conv::Convolver>()) {}
Vault::~Vault() = default;

void Vault::prepare(double sampleRate) {
    sr = sampleRate;
    auto set = irSetFor(sr);
    irs = &set->table;
    left->prepare(set->maxLength);
    right->prepare(set->maxLength);
    cache = set;
    for (auto& s : shifters) s.prepare(sr, 0.06);
    for (auto* s : { &shimmer, &toneHz, &duck }) s->setTime(0.05, sr);
    envAttack = timeCoefficient(0.01, sr);
    envRelease = timeCoefficient(0.35, sr);
    tailDecay = static_cast<float>(std::exp(-1.0 / (0.25 * sr)));
    reset();
}

void Vault::reset() {
    left->reset(); right->reset();
    for (auto& s : shifters) s.clear();
    shimmerLp = toneState1 = toneState2 = {};
    for (auto& f : back) f.fill(0.0f);
    envelope = 0; tail = 0;
    shimmer.snap(0); duck.snap(0); toneHz.value = -1;
}

void Vault::setKind(int k) noexcept {
    kind = std::clamp(k, 0, kinds - 1);
    const bool ok = irs != nullptr && kind > 0;
    left->setIr(ok ? (*irs)[static_cast<std::size_t>(kind)][0] : nullptr);
    right->setIr(ok ? (*irs)[static_cast<std::size_t>(kind)][1] : nullptr);
}

void Vault::process(const float* inL, const float* inR, float* wetL, float* wetR, int n, const Frame& p) noexcept {
    if (kind <= 0 || !left->hasIr()) {
        std::fill(wetL, wetL + n, 0.0f); std::fill(wetR, wetR + n, 0.0f);
        tail = 0;
        return;
    }
    shimmer.set(clamp01(p[VaultShimmer]));
    duck.set(clamp01(p[VaultDuck]));
    toneHz.set(std::clamp(p[VaultTone], 300.0f, 20000.0f));
    if (toneHz.value < 0) toneHz.snap(toneHz.target);
    const float toneCoefficient = poleForHz(toneHz.skip(n), sr);
    const bool toneOn = toneHz.value < 19000.0f;
    const auto N = static_cast<std::size_t>(n);
    // Input: the dry plus last chunk's wet an octave up (the shimmer cascade), each
    // side into its own decorrelated IR.
    for (std::size_t i = 0; i < N; ++i) {
        const float sh = shimmer.next();
        float fl = finiteOr(inL[i]), fr = finiteOr(inR[i]);
        if (sh > 1.0e-4f) {
            for (int ch = 0; ch < 2; ++ch) {
                const auto C = static_cast<std::size_t>(ch);
                float up = shifters[C].process(back[C][i], 2.0f);
                shimmerLp[C] += 0.25f * (up - shimmerLp[C]);   // tame the grain buzz
                (ch == 0 ? fl : fr) += 0.55f * sh * softLimit(up - 0.3f * shimmerLp[C], 1.0f);
            }
        }
        feed[0][i] = fl; feed[1][i] = fr;
        const float level = std::max(std::abs(inL[i]), std::abs(inR[i]));
        envelope += (level > envelope ? envAttack : envRelease) * (level - envelope);
    }
    left->process(feed[0].data(), wetL, n);
    right->process(feed[1].data(), wetR, n);
    for (std::size_t i = 0; i < N; ++i) {
        float l = wetL[i], r = wetR[i];
        back[0][i] = l; back[1][i] = r;
        if (toneOn) {
            toneState1[0] += toneCoefficient * (l - toneState1[0]); toneState2[0] += toneCoefficient * (toneState1[0] - toneState2[0]); l = toneState2[0];
            toneState1[1] += toneCoefficient * (r - toneState1[1]); toneState2[1] += toneCoefficient * (toneState1[1] - toneState2[1]); r = toneState2[1];
        }
        const float g = 1.0f - duck.next() * std::min(0.75f, envelope * 6.0f);
        l = finiteOr(l * g); r = finiteOr(r * g);
        wetL[i] = l; wetR[i] = r;
        tail = std::max(std::max(std::abs(l), std::abs(r)), tail * tailDecay);
    }
}
}
