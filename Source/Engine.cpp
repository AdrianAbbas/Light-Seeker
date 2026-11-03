#include "Engine.h"
#include <cmath>
#include <cstring>

namespace dc {
namespace {
// Choices whose change cannot be morphed, and the amount that fades their layer.
struct DipGroup { P amount; std::array<P, 3> choices; int count; };
constexpr DipGroup dipGroups[] = {
    { ChoirBlend, { Voices, Interval, Interval }, 2 },
    { Tomb, { TombNote, TombOctave, TombOctave }, 2 },
    { Metal, { MetalKind, MetalNote, MetalOctave }, 3 },
};
bool choicesDiffer(const Frame& a, const Frame& b) noexcept {
    for (std::size_t i = 0; i < Count; ++i)
        if (moduleSpecs[i].glide == Glide::Step && a[i] != b[i]) return true;
    return false;
}
// Whether two frames describe different rooms (anything but the reverb's mix and width).
bool roomsDiffer(const Frame& a, const Frame& b) noexcept {
    for (P id : { Space, Decay, PreDelay, Damping, ReverbLowCut, Shimmer, Character, CryptMode, RiseTime, Eternal })
        if (std::abs(a[id] - b[id]) > 1.0e-3f * std::max(1.0f, std::abs(a[id]))) return true;
    return false;
}
bool dippedChoicesDiffer(const Frame& a, const Frame& b) noexcept {
    for (const auto& g : dipGroups)
        for (int k = 0; k < g.count; ++k)
            if (a[g.choices[static_cast<std::size_t>(k)]] != b[g.choices[static_cast<std::size_t>(k)]]) return true;
    return false;
}
}

Frame Engine::targetFrame(const Controls& c) noexcept {
    const int location = std::clamp(c.location, 0, static_cast<int>(LocationCount) - 1);
    auto place = placeFrame(location, c.sub);
    applyMacros(place, location, c.macros);
    for (std::size_t i = 0; i < Count; ++i) {
        const auto& spec = moduleSpecs[i];
        const float a = c.advanced[i];
        if (a == 0.0f || !std::isfinite(a)) continue;
        switch (spec.glide) {
            case Glide::Lin: place[i] += std::clamp(a, -1.0f, 1.0f) * (spec.max - spec.min); break;
            case Glide::Log: place[i] *= std::pow(spec.max / spec.min, std::clamp(a, -1.0f, 1.0f)); break;
            case Glide::Step: place[i] = spec.min + std::round(a) - 1.0f; break;
        }
        place[i] = std::clamp(place[i], spec.min, spec.max);
    }
    Frame f = interpolate(neutralFrame(), place, dsp::clamp01(c.intensity));
    // Choices keep the place's value at any intensity (with the mixes at zero they are
    // inaudible), so Intensity never flips a chamber or an interval half-way.
    for (std::size_t i = 0; i < Count; ++i)
        if (moduleSpecs[i].glide == Glide::Step) f[i] = place[i];
    applyMix(f, c.mix);
    return f;
}

void Engine::applyMix(Frame& f, const Controls::Mix& m) noexcept {
    const auto scale = [](float s) { return std::clamp(std::isfinite(s) ? s : 1.0f, 0.0f, 1.5f); };
    // A crossfaded wet layer (equal power: wet sqrt(m), dry sqrt(1 - m)): its wet-to-dry
    // amplitude ratio times s, so the total power, and the level, stay.
    const auto ratio = [&f](P id, float s) {
        if (s == 1.0f) return;
        const float v = std::clamp(f[id], 0.0f, 0.9999f);
        const float a = s * s * v;
        f[id] = a + (1.0f - v) > 0.0f ? a / (a + (1.0f - v)) : 0.0f;
    };
    // An added layer or a modulation depth: its amount times s (within its range).
    const auto linear = [&f](P id, float s) {
        if (s == 1.0f) return;
        f[id] = std::clamp(f[id] * s, moduleSpecs[id].min, moduleSpecs[id].max);
    };
    const float space = scale(m.reverb), echoes = scale(m.echoes), mod = scale(m.modulation), character = scale(m.character);
    // Reverb / Space: the halls, the Radiance spaces (generated and captured), the aura.
    for (P id : { ReverbMix, VaultMix, WraithMix }) ratio(id, space);
    if (m.preDelayMs != 0.0f && std::isfinite(m.preDelayMs))
        f[PreDelay] = std::clamp(f[PreDelay] + std::clamp(m.preDelayMs, -50.0f, 150.0f), 0.0f, 250.0f);
    // Echoes: the delays (and the tape echo).
    ratio(DelayMix, echoes);
    linear(TapeMix, echoes);
    // Modulation: chorus, the rising phaser, flutter, the filter's movement, pitch waver
    // and the tape's wow (not the shimmer).
    for (P id : { EnsMix, Spiral, Flicker, FilterMovement, Instability, TapeWow }) linear(id, mod);
    // Character: the resonator, the chimes, the warmth, the choir and drone layers, wind
    // and water beads (the places' own sounds).
    for (P id : { Tomb, Metal, BloodMix, ChoirBlend, Drone, Wind, Drips, Drops }) linear(id, character);
}

Frame Engine::targetFrame(int location, float intensity) noexcept {
    Controls c;
    c.location = location;
    c.intensity = intensity;
    return targetFrame(c);
}

Frame Engine::interpolate(const Frame& a, const Frame& b, float t) noexcept {
    Frame f {};
    for (std::size_t i = 0; i < Count; ++i) {
        switch (moduleSpecs[i].glide) {
            case Glide::Lin: f[i] = dsp::lerp(a[i], b[i], t); break;
            case Glide::Log: f[i] = a[i] * std::pow(b[i] / a[i], t); break;   // all Log ranges are > 0
            case Glide::Step: f[i] = t < 0.5f ? a[i] : b[i]; break;
        }
    }
    return f;
}

void Engine::applyDips(Frame& f, const Frame& a, const Frame& b, float t, double seconds) noexcept {
    // Silent for ~80 ms on each side of the switch (the modules smooth their amounts
    // over 40 ms), fading over ~120 ms around that.
    const float plateau = static_cast<float>(std::min(0.2, 0.08 / std::max(1.0e-3, seconds)));
    const float width = std::max(1.0e-3f, std::min(0.5f - plateau, static_cast<float>(0.12 / std::max(1.0e-3, seconds))));
    for (const auto& g : dipGroups) {
        bool differ = false;
        for (int k = 0; k < g.count; ++k) differ = differ || a[g.choices[static_cast<std::size_t>(k)]] != b[g.choices[static_cast<std::size_t>(k)]];
        if (!differ) continue;
        float d = std::clamp((std::abs(t - 0.5f) - plateau) / width, 0.0f, 1.0f);
        d = d * d * (3.0f - 2.0f * d);
        f[g.amount] *= d;
    }
}

void Engine::prepare(double sampleRate) {
    sr = sampleRate;
    context.sampleRate = sr;
    abyss.prepare(sr); blood.prepare(sr); relics.prepare(sr); choir.prepare(sr);
    wraith.prepare(sr); echoes.prepare(sr); ensemble.prepare(sr);
    for (auto& v : vaults) v.prepare(sr);
    for (auto& v : vaultSend) v.setTime(0.03, sr);
    vaultDry.setTime(0.04, sr);
    for (auto& sig : signatures) sig.prepare(sr);
    for (auto& room : rooms) { room.prepare(sr); room.setWetOnly(true); }
    roomDry.setTime(0.04, sr);
    levelMatcher.prepare(sr);
    for (auto& slot : slots) slot.gain.prepare(0.02, sr);
    for (auto& slot : sigSlots) slot.gain.prepare(0.02, sr);
    trimGain.setTime(0.05, sr);
    // Mix: the duck follows the playing in 10 ms and lets go over 150 ms; its gain glides
    // over 8 ms; the low cut's corner glides (per chunk) over about 100 ms.
    duckAttack = dsp::timeCoefficient(0.01, sr);
    duckRelease = dsp::timeCoefficient(0.15, sr);
    duckGain.setTime(0.008, sr);
    lowCutGlide = 1.0f - std::exp(-static_cast<float>(maxChunk) / static_cast<float>(0.1 * sr));
    for (auto& d : vaultPre) d.prepare(static_cast<std::size_t>(0.16 * sr) + 8);
    vaultPreSamples.setTime(0.12, sr);
    warmUpSamples = std::max(1, static_cast<int>(0.005 * sr));
    settleSamples = static_cast<int>(0.5 * sr);
    reset();
}

void Engine::reset() {
    abyss.reset(); blood.reset(); relics.reset(); choir.reset(); wraith.reset(); echoes.reset(); ensemble.reset();
    for (auto& v : vaults) v.reset();
    vaultIdle.fill(true);
    for (auto& v : vaultSend) v.snap(0.0f);
    activeVault = 0; departingVault = -1; vaultCrossfade = false;
    vaultDry.snap(1.0f);
    for (auto& sig : signatures) sig.reset();
    for (auto& slot : sigSlots) { slot.gain.value = 0; slot.idle = true; slot.warmUp = 0; }
    activeSig = 0; latestDeparting = -1; sigCrossfade = false; sigDeparting.fill(false);
    for (auto& room : rooms) room.reset();
    roomIdle.fill(true);
    activeRoom = 0; departingRoom = -1; roomCrossfade = false;
    roomDry.snap(1.0f);
    levelMatcher.reset();
    for (auto& ch : reverbHp) for (auto& f : ch) f.clear();
    lowCutOn = false; lowCutHz = 20;
    duckEnv = 0; duckGain.snap(1.0f); duckActive = false;
    for (auto& d : vaultPre) d.clear();
    vaultPreSamples.snap(0.0f);
    for (auto& slot : slots) { slot.gain.value = 0; slot.idle = true; slot.warmUp = 0; }
    place = {};   // the next setControls() arrives at its place at once
    travel = 1;
}

void Engine::swapSignature() noexcept {
    // The old kind rings out in its instance; the new one starts clean in an idle one
    // (or, if all ring, the quietest).
    sigFromAmount = current[SigAmount];
    const auto A = static_cast<std::size_t>(activeSig);
    latestDeparting = -1;
    if (!sigSlots[A].idle) {
        sigFrames[A] = current;
        signatures[A].depart();
        sigDeparting[A] = true;
        latestDeparting = activeSig;
    }
    int best = -1;
    for (int k = 0; k < sigCount && best < 0; ++k)
        if (k != activeSig && sigSlots[static_cast<std::size_t>(k)].idle) best = k;
    if (best < 0)
        for (int k = 0; k < sigCount; ++k)
            if (k != activeSig && (best < 0 || signatures[static_cast<std::size_t>(k)].tailLevel() < signatures[static_cast<std::size_t>(best)].tailLevel())) best = k;
    activeSig = best;
    const auto B = static_cast<std::size_t>(best);
    sigDeparting[B] = false;
    sigSlots[B].idle = true;   // starts clean (runSignatures resets it)
    sigSlots[B].gain.value = 0;
    sigCrossfade = true;
}

void Engine::startTravel(double seconds) noexcept {
    if (current[SigKind] != to[SigKind]) swapSignature();
    if (current[VaultKind] != to[VaultKind] && to[VaultKind] > 0.5f) {
        // The new space starts clean in an idle instance of the pool (else the quietest);
        // the one we leave fades its send and rings out, so does every other.
        vaultFromSend = vaultSend[static_cast<std::size_t>(activeVault)].target;
        departingVault = activeVault;
        int best = -1;
        float quietest = 1.0e9f;
        for (int k = 0; k < vaultCount; ++k) {
            if (k == activeVault) continue;
            const float level = vaultIdle[static_cast<std::size_t>(k)] ? -1.0f : vaults[static_cast<std::size_t>(k)].level();
            if (level < quietest) { quietest = level; best = k; }
        }
        activeVault = best;
        vaultIdle[static_cast<std::size_t>(activeVault)] = true;
        vaultCrossfade = true;
    }
    from = current;
    trimFrom = trimDb;
    duration = std::max(minTravelSeconds, seconds);
    if (dippedChoicesDiffer(from, to)) duration = std::max(duration, crossfadeSeconds);
    travel = 0;
}

void Engine::setControls(const Controls& c) noexcept {
    context.bpm = c.bpm;
    // Shimmer: every module smooths its own shimmer amounts, so moving it never clicks.
    const float shimmer = dsp::shimmerControl(c.shimmer);
    // The Moonwell keeps its reverb's shimmer as designed at 100 % (its glassy signature
    // shimmer is 15 % lower like every other): with less, it drifts towards its neighbour
    // Starlit Canopy (the distinctness gate; see docs/DEVLOG.md, final touches).
    const bool moonwell = c.location == ElvenSanctuary && clampSubLocation(c.location, c.sub) == 0;
    for (auto& sig : signatures) sig.setShimmer(shimmer * dsp::shimmerLayerBase);
    for (auto& room : rooms) room.setShimmer(shimmer * (moonwell ? 1.0f : dsp::shimmerLoopBase));
    for (auto& v : vaults) v.setShimmer(shimmer * dsp::shimmerLoopBase);
    // Mix: the signatures' own stages per group (smoothed in each instance).
    {
        const auto scale = [](float v) { return std::clamp(std::isfinite(v) ? v : 1.0f, 0.0f, 1.5f); };
        for (auto& sig : signatures) sig.setMix(scale(c.mix.reverb), scale(c.mix.echoes), scale(c.mix.modulation), scale(c.mix.character));
        mixNow = c.mix;
        mixNow.duck = std::isfinite(c.mix.duck) ? dsp::clamp01(c.mix.duck) : 0.0f;
        mixNow.lowCutHz = std::isfinite(c.mix.lowCutHz) && c.mix.lowCutHz >= 20.0f ? std::clamp(c.mix.lowCutHz, 20.0f, 1000.0f) : 0.0f;
        mixNow.preDelayMs = std::isfinite(c.mix.preDelayMs) ? std::clamp(c.mix.preDelayMs, -50.0f, 150.0f) : 0.0f;
    }
    const int location = std::clamp(c.location, 0, static_cast<int>(LocationCount) - 1);
    const Place wanted { location, clampSubLocation(location, c.sub) };
    const Frame target = targetFrame(c);
    const float trim = placeTrimDb(wanted.location, wanted.sub) * dsp::clamp01(c.intensity);
    if (place.location < 0) {
        // First block after prepare: be there already.
        place = wanted;
        current = from = to = target;
        trimDb = trimFrom = trimTo = trim;
        trimGain.snap(dsp::dbToGain(trim));
        travel = 1;
        return;
    }
    const bool frameChanged = std::memcmp(target.data(), to.data(), sizeof(Frame)) != 0;
    if (wanted != place) {
        // Travel from wherever we are (possibly mid-way) to the new place.
        place = wanted;
        to = target;
        trimTo = trim;
        startTravel(c.travelSeconds);
        if (roomsDiffer(current, to) && to[ReverbMix] > 1.0e-4f) {
            // A new room for the new place, in the quietest other instance.
            int quietest = -1;
            for (int k = 0; k < roomCount; ++k)
                if (k != activeRoom && (quietest < 0 || rooms[static_cast<std::size_t>(k)].level() < rooms[static_cast<std::size_t>(quietest)].level())) quietest = k;
            departingRoom = activeRoom;
            activeRoom = quietest;
            roomIdle[static_cast<std::size_t>(activeRoom)] = true;   // starts clean
            roomCrossfade = true;
        }
    } else if (frameChanged || trim != trimTo) {
        // Macros, Advanced or Intensity: the matcher follows quickly; continuous values
        // are smoothed by the modules, choices switch through a short crossfade.
        settle = settleSamples;
        to = target;
        trimTo = trim;
        if (travel >= 1.0f) {
            if (choicesDiffer(current, to)) startTravel(0.05);
            else { current = to; trimDb = trimTo; }
        }
    }
}

void Engine::runRooms(float* l, float* r, int n, float eased) noexcept {
    const float m = current[ReverbMix];
    if (roomCrossfade && travel >= 1.0f) { roomCrossfade = false; departingRoom = -1; }
    bool any = false;
    std::fill(roomSum[0].begin(), roomSum[0].begin() + n, 0.0f);
    std::fill(roomSum[1].begin(), roomSum[1].begin() + n, 0.0f);
    std::copy(l, l + n, roomIn[0].begin());
    std::copy(r, r + n, roomIn[1].begin());
    for (int k = 0; k < roomCount; ++k) {
        const auto K = static_cast<std::size_t>(k);
        Frame& f = roomFrames[K];
        if (k == activeRoom) {
            if (roomCrossfade) { f = to; f[ReverbMix] = m * eased; }   // the destination's room, fading in
            else f = current;
        } else if (k == departingRoom && roomCrossfade) {
            f[ReverbMix] = m * (1.0f - eased);                         // the old room, fading out
        } else {
            f[ReverbMix] = 0.0f;                                       // ringing out, no new input
        }
        auto& room = rooms[K];
        if (f[ReverbMix] <= 1.0e-4f && !room.ringing() && !roomIdle[K]) { roomIdle[K] = true; continue; }
        if (f[ReverbMix] <= 1.0e-4f && roomIdle[K]) continue;
        if (roomIdle[K]) { room.reset(); roomIdle[K] = false; roomWarmUp[K] = 0; }
        std::copy(roomIn[0].begin(), roomIn[0].begin() + n, roomWet[0].begin());
        std::copy(roomIn[1].begin(), roomIn[1].begin() + n, roomWet[1].begin());
        // A room that starts again starts from cleared lines: its input fades in over
        // 5 ms, or the hard onset comes back later as a click (early reflections, the
        // first loop pass, Rising mode's reversed segments). As runSlot does.
        auto& warm = roomWarmUp[K];
        for (int i = 0; i < n && warm < warmUpSamples; ++i, ++warm) {
            const float ramp = 0.5f - 0.5f * std::cos(dsp::twoPi * 0.5f * static_cast<float>(warm) / static_cast<float>(warmUpSamples));
            roomWet[0][static_cast<std::size_t>(i)] *= ramp;
            roomWet[1][static_cast<std::size_t>(i)] *= ramp;
        }
        room.process(roomWet[0].data(), roomWet[1].data(), n, f, context);
        for (int i = 0; i < n; ++i) {
            roomSum[0][static_cast<std::size_t>(i)] += roomWet[0][static_cast<std::size_t>(i)];
            roomSum[1][static_cast<std::size_t>(i)] += roomWet[1][static_cast<std::size_t>(i)];
        }
        any = true;
    }
    roomDry.set(std::sqrt(1.0f - dsp::clamp01(m)));
    if (!any && roomDry.value == 1.0f && roomDry.target == 1.0f) return;
    if (any) shapeReverb(roomSum, n, 0);
    for (int i = 0; i < n; ++i) {
        const auto k = static_cast<std::size_t>(i);
        float g = roomDry.next();
        if (std::abs(g - roomDry.target) < 1.0e-6f) g = roomDry.value = roomDry.target;
        l[i] = roomIn[0][k] * g + roomSum[0][k];
        r[i] = roomIn[1][k] * g + roomSum[1][k];
    }
}

void Engine::runVaults(float* l, float* r, int n, float eased) noexcept {
    if (vaultCrossfade && travel >= 1.0f) vaultCrossfade = false;
    const float m = dsp::clamp01(current[VaultMix]);
    const int kindNow = static_cast<int>(std::lround(vaultCrossfade ? to[VaultKind] : current[VaultKind]));
    bool any = false;
    std::copy(l, l + n, vaultIn[0].begin());
    std::copy(r, r + n, vaultIn[1].begin());
    // Mix: the Reverb Pre-delay offset (later only: the spaces' own pre-delay is in their
    // impulse responses) delays what is sent into the spaces; the dry stays.
    std::array<std::array<float, maxChunk>, 2> sendIn;
    const float* sendL = vaultIn[0].data();
    const float* sendR = vaultIn[1].data();
    vaultPreSamples.set(std::max(0.0f, mixNow.preDelayMs) * 0.001f * static_cast<float>(sr));
    if (vaultPreSamples.target > 0.0f || vaultPreSamples.value > 0.0f) {
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            vaultPre[0].push(vaultIn[0][k]);
            vaultPre[1].push(vaultIn[1][k]);
            float d = vaultPreSamples.next();
            if (vaultPreSamples.target == 0.0f && d < 1.0f) d = vaultPreSamples.value = 0.0f;
            sendIn[0][k] = d < 1.0f ? vaultIn[0][k] : vaultPre[0].read(d);
            sendIn[1][k] = d < 1.0f ? vaultIn[1][k] : vaultPre[1].read(d);
        }
        sendL = sendIn[0].data();
        sendR = sendIn[1].data();
    } else {
        // Kept filled, so turning the offset on starts from the recent signal.
        for (int i = 0; i < n; ++i) { vaultPre[0].push(vaultIn[0][static_cast<std::size_t>(i)]); vaultPre[1].push(vaultIn[1][static_cast<std::size_t>(i)]); }
    }
    std::fill(vaultSum[0].begin(), vaultSum[0].begin() + n, 0.0f);
    std::fill(vaultSum[1].begin(), vaultSum[1].begin() + n, 0.0f);
    for (int k = 0; k < vaultCount; ++k) {
        const auto K = static_cast<std::size_t>(k);
        auto& v = vaults[K];
        float send = 0;
        if (k == activeVault) send = std::sqrt(vaultCrossfade ? dsp::clamp01(to[VaultMix]) * eased : m);
        else if (vaultCrossfade && k == departingVault) send = vaultFromSend * (1.0f - eased);
        vaultSend[K].set(send);
        if (send <= 1.0e-4f && vaultSend[K].value <= 1.0e-4f && (vaultIdle[K] || !v.ringing())) { vaultIdle[K] = true; continue; }
        if (vaultIdle[K]) {
            v.reset();
            v.setKind(k == activeVault ? kindNow : v.kindNow());
            vaultIdle[K] = false;
            vaultWarmUp[K] = 0;
            vaultSend[K].snap(0.0f);
        }
        if (k == activeVault && v.kindNow() != kindNow && kindNow > 0) { v.reset(); v.setKind(kindNow); }
        auto& warm = vaultWarmUp[K];
        for (int i = 0; i < n; ++i) {
            float g = vaultSend[K].next();
            if (warm < warmUpSamples) { g *= 0.5f - 0.5f * std::cos(dsp::twoPi * 0.5f * static_cast<float>(warm) / static_cast<float>(warmUpSamples)); ++warm; }
            vaultWet[0][static_cast<std::size_t>(i)] = sendL[i] * g;
            vaultWet[1][static_cast<std::size_t>(i)] = sendR[i] * g;
        }
        v.process(vaultWet[0].data(), vaultWet[1].data(), vaultWet[0].data(), vaultWet[1].data(), n, k == activeVault ? (vaultCrossfade ? to : current) : current);
        for (int i = 0; i < n; ++i) {
            vaultSum[0][static_cast<std::size_t>(i)] += vaultWet[0][static_cast<std::size_t>(i)];
            vaultSum[1][static_cast<std::size_t>(i)] += vaultWet[1][static_cast<std::size_t>(i)];
        }
        any = true;
    }
    vaultDry.set(std::sqrt(1.0f - m));
    if (!any && vaultDry.value == 1.0f && vaultDry.target == 1.0f) return;
    if (any) shapeReverb(vaultSum, n, 1);
    for (int i = 0; i < n; ++i) {
        const auto k = static_cast<std::size_t>(i);
        float g = vaultDry.next();
        if (std::abs(g - vaultDry.target) < 1.0e-6f) g = vaultDry.value = vaultDry.target;
        l[i] = vaultIn[0][k] * g + vaultSum[0][k];
        r[i] = vaultIn[1][k] * g + vaultSum[1][k];
    }
}

void Engine::prepareMixChunk(const float* dryL, const float* dryR, int n) noexcept {
    // The low cut's corner: towards the wanted frequency (or back down to 20 Hz, then off).
    const float wanted = mixNow.lowCutHz > 0.0f ? mixNow.lowCutHz : 20.0f;
    if (mixNow.lowCutHz > 0.0f && !lowCutOn) {
        lowCutOn = true;
        lowCutHz = 20.0f;
        for (auto& ch : reverbHp) for (auto& f : ch) f.clear();
    }
    if (lowCutOn) {
        lowCutHz += lowCutGlide * (wanted - lowCutHz);
        if (std::abs(wanted - lowCutHz) < 0.05f) lowCutHz = wanted;
        if (mixNow.lowCutHz <= 0.0f && lowCutHz <= 20.5f) lowCutOn = false;
        else for (auto& ch : reverbHp) for (auto& f : ch) { const float s1 = f.s1, s2 = f.s2; f.set(lowCutHz, sr, 1.4142f); f.s1 = s1; f.s2 = s2; }
    }
    // The duck: follows the playing (the engine's input), from -48 dBFS (none) to -24 dBFS
    // (all of it), the reverbs' return down to -15 dB at Duck 100 %.
    duckActive = mixNow.duck > 0.0f || duckGain.value != 1.0f;
    for (int i = 0; i < n; ++i) {
        const float level = std::max(std::abs(dryL[i]), std::abs(dryR[i]));
        duckEnv += (level > duckEnv ? duckAttack : duckRelease) * (level - duckEnv);
        if (!duckActive) continue;
        float amount = std::clamp((duckEnv - 0.004f) / (0.063f - 0.004f), 0.0f, 1.0f);
        amount = amount * amount * (3.0f - 2.0f * amount);
        duckGain.set(1.0f - mixNow.duck * (1.0f - 0.178f) * amount);
        float g = duckGain.next();
        if (mixNow.duck <= 0.0f && std::abs(g - 1.0f) < 1.0e-5f) g = duckGain.value = 1.0f;
        duckRow[static_cast<std::size_t>(i)] = g;
    }
}

void Engine::shapeReverb(std::array<std::array<float, maxChunk>, 2>& wet, int n, int which) noexcept {
    if (lowCutOn) {
        auto& hp = reverbHp[static_cast<std::size_t>(which)];
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            wet[0][k] = hp[0].highPass(wet[0][k]);
            wet[1][k] = hp[1].highPass(wet[1][k]);
        }
    }
    if (duckActive)
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            wet[0][k] *= duckRow[k];
            wet[1][k] *= duckRow[k];
        }
}

void Engine::runSignatures(float* l, float* r, int n, float eased) noexcept {
    if (sigCrossfade && travel >= 1.0f) sigCrossfade = false;
    // Departing instances first (their serial part fading out, their tails ringing on),
    // then the active one (fading in during a journey).
    for (int k = 0; k < sigCount; ++k) {
        const auto K = static_cast<std::size_t>(k);
        if (k == activeSig || !sigDeparting[K]) continue;
        auto& f = sigFrames[K];
        f[SigAmount] = sigCrossfade && k == latestDeparting ? sigFromAmount * (1.0f - eased) : 0.0f;
        auto& sig = signatures[K];
        // Skipped only once its own smoothed amount has reached zero too: bypassing a
        // serial kind (a chorus) a few percent in would step.
        if (f[SigAmount] <= 1.0e-4f && !sig.ringing() && sig.amountNow() <= 1.0e-5f) {
            sigSlots[K].idle = true; sigSlots[K].gain.value = 0; sigDeparting[K] = false;
            if (latestDeparting == k) latestDeparting = -1;
            continue;
        }
        sigSlots[K].gain.value = 1.0f;
        runSlot(sig, sigSlots[K], true, l, r, n, &f);
    }
    const auto A = static_cast<std::size_t>(activeSig);
    auto& f = sigFrames[A];
    if (sigCrossfade) { f = to; f[SigAmount] = to[SigAmount] * eased; }
    else f = current;
    runSlot(signatures[A], sigSlots[A], f[SigAmount] > 1.0e-4f, l, r, n, &f);
}

bool Engine::anyDeparting() const noexcept {
    for (bool d : sigDeparting) if (d) return true;
    return false;
}

template <class Module>
void Engine::runSlot(Module& module, Slot& slot, bool enabled, float* l, float* r, int n, const Frame* frame) noexcept {
    const float wanted = enabled ? 1.0f : 0.0f;
    if (slot.gain.value == 0 && !enabled) { slot.idle = true; return; }
    if (slot.idle) { module.reset(); slot.idle = false; slot.warmUp = 0; }
    std::copy(l, l + n, pre[0].begin());
    std::copy(r, r + n, pre[1].begin());
    // A module that starts again starts from cleared buffers. Its input fades in, or
    // the hard onset would come back later as a click (the first echo, the first
    // reverb pass).
    for (int i = 0; i < n && slot.warmUp < warmUpSamples; ++i, ++slot.warmUp) {
        const float ramp = 0.5f - 0.5f * std::cos(dsp::twoPi * 0.5f * static_cast<float>(slot.warmUp) / static_cast<float>(warmUpSamples));
        l[i] *= ramp;
        r[i] *= ramp;
    }
    module.process(l, r, n, frame != nullptr ? *frame : current, context);
    for (int i = 0; i < n; ++i) {
        const float g = slot.gain.next(wanted);
        const auto k = static_cast<std::size_t>(i);
        l[i] = pre[0][k] + (l[i] - pre[0][k]) * g;
        r[i] = pre[1][k] + (r[i] - pre[1][k]) * g;
    }
}

void Engine::process(float* l, float* r, int n, const float* dryL, const float* dryR) noexcept {
    if (place.location < 0) { Controls c; setControls(c); }
    if (travel < 1.0f) {
        travel = std::min(1.0f, travel + static_cast<float>(n / (duration * sr)));
        const float eased = travel * travel * (3.0f - 2.0f * travel);
        if (travel >= 1.0f) { current = to; trimDb = trimTo; }
        else {
            current = interpolate(from, to, eased);
            applyDips(current, from, to, travel, duration);
            trimDb = dsp::lerp(trimFrom, trimTo, eased);
        }
    }

    // A module runs while the current frame asks anything of it; at neutral it is
    // skipped (and starts clean when it is needed again). The Echoes and the Crypt
    // keep running while their tails ring after their mix went to zero.
    const auto& p = current;
    const bool abyssActive = p[Cutoff] < 19500.0f || p[HighPass] > 21.0f || p[Darkness] > 1.0e-3f || p[Tomb] > 1.0e-3f;
    const bool relicsActive = p[Flicker] > 1.0e-4f || p[Metal] > 1.0e-4f || p[Wind] > 1.0e-4f || p[Drips] > 1.0e-4f;
    const bool echoesActive = p[DelayMix] > 1.0e-4f || echoes.ringing();
    const bool ensembleActive = p[EnsMix] > 1.0e-4f || p[Spiral] > 1.0e-4f || p[TapeMix] > 1.0e-4f || p[Drops] > 1.0e-4f || ensemble.ringing();
    bool vaultActive = p[VaultMix] > 1.0e-4f || to[VaultMix] > 1.0e-4f;
    for (const auto& v : vaults) vaultActive = vaultActive || v.ringing();
    bool cryptActive = p[ReverbMix] > 1.0e-4f;
    for (const auto& room : rooms) cryptActive = cryptActive || room.ringing();
    const bool wetActive = p[ChoirBlend] > 1.0e-4f || p[Drone] > 1.0e-4f || p[WraithMix] > 1.0e-4f
                        || echoesActive || cryptActive || p[BloodMix] > 1.0e-4f || relicsActive || p[SigAmount] > 1.0e-4f
                        || anyDeparting() || to[SigAmount] > 1.0e-4f || ensembleActive || vaultActive;
    prepareMixChunk(dryL, dryR, n);
    runSlot(abyss, slots[0], abyssActive, l, r, n);
    blood.process(l, r, n, p, context);
    runSlot(relics, slots[1], relicsActive, l, r, n);
    runSlot(choir, slots[2], p[ChoirBlend] > 1.0e-4f || p[Drone] > 1.0e-4f, l, r, n);
    runSlot(wraith, slots[3], p[WraithMix] > 1.0e-4f, l, r, n);
    runSignatures(l, r, n, travel * travel * (3.0f - 2.0f * travel));
    runSlot(ensemble, slots[5], ensembleActive, l, r, n);
    runSlot(echoes, slots[4], echoesActive, l, r, n);
    runRooms(l, r, n, travel * travel * (3.0f - 2.0f * travel));
    if (vaultActive || vaultDry.value != 1.0f) runVaults(l, r, n, travel * travel * (3.0f - 2.0f * travel));
    if (!matching) return;
    trimGain.set(dsp::dbToGain(trimDb));
    for (int i = 0; i < n; ++i) { const float g = trimGain.next(); l[i] *= g; r[i] *= g; }
    // The matcher stays agile while travelling and for half a second after arriving.
    // At neutral nothing changes the level, so the matcher returns to unity.
    settle = travel < 1.0f ? settleSamples : std::max(0, settle - n);
    levelMatcher.process(dryL, dryR, l, r, n, abyssActive || wetActive, settle > 0);
}
}
