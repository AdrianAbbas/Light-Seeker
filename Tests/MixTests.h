// Mix and Punch. Included by LightSeekerTests.cpp (it uses its Suite, Host,
// InstrumentHost, signals and helpers).
//   mix   the MIX parameters (both plug-ins: range, default, automatable, saved, older
//         sessions at the defaults, presets), each MIX group scales its own effects and
//         only those (0 / 100 / 150 %), level-matched, click-free while moving; Shimmer
//         where it was; the reverb helpers (Low Cut, Pre-delay, Duck); the Atmosphere
//         control and its on/off; Punch (the overshoot it is set to, settled in its time,
//         velocity, no clicks, no clipping, saved); the brass's attack (louder, brighter
//         and more present hit, the same sustain); the instruments' reverb defaults; the
//         MIX section in both windows and the Atmosphere in Advanced.

namespace mixtest {
using Render = std::pair<std::vector<float>, std::vector<float>>;

// The engine alone (no matcher, no trims' adaptation): the chain's raw output.
Render rawRender(int location, int sub, const dc::Controls::Mix& mix, const Signal& signal, double seconds = 3.0) {
    constexpr double sr = 48000;
    dc::Engine e;
    e.prepare(sr);
    e.setLevelMatching(false);
    dc::Controls c;
    c.location = location;
    c.sub = sub;
    c.mix = mix;
    const auto total = static_cast<long long>(seconds * sr);
    Render out;
    std::array<float, dc::maxChunk> l {}, r {}, dl {}, dr {};
    for (long long pos = 0; pos < total; pos += dc::maxChunk) {
        const int n = static_cast<int>(std::min<long long>(dc::maxChunk, total - pos));
        e.setControls(c);
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            l[k] = dl[k] = signal(0, pos + i, sr);
            r[k] = dr[k] = signal(1, pos + i, sr);
        }
        e.process(l.data(), r.data(), n, dl.data(), dr.data());
        out.first.insert(out.first.end(), l.begin(), l.begin() + n);
        out.second.insert(out.second.end(), r.begin(), r.begin() + n);
    }
    return out;
}
double differenceRms(const Render& a, const Render& b, std::size_t from, std::size_t to = SIZE_MAX) {
    double sum = 0;
    const auto n = std::min({ a.first.size(), b.first.size(), to });
    for (std::size_t i = from; i < n; ++i) {
        const double dl = a.first[i] - b.first[i], dr = a.second[i] - b.second[i];
        sum += 0.5 * (dl * dl + dr * dr);
    }
    return std::sqrt(sum / double(std::max<std::size_t>(1, n - from)));
}
// The same difference through a band (IIR high- and low-pass, mono).
double bandDifference(const Render& a, const Render& b, std::size_t from, double low, double high) {
    juce::IIRFilter hp, lp, hp2, lp2;
    if (low > 0) { hp.setCoefficients(juce::IIRCoefficients::makeHighPass(48000, low)); hp2.setCoefficients(juce::IIRCoefficients::makeHighPass(48000, low)); }
    else { hp.makeInactive(); hp2.makeInactive(); }
    lp.setCoefficients(juce::IIRCoefficients::makeLowPass(48000, high));
    lp2.setCoefficients(juce::IIRCoefficients::makeLowPass(48000, high));
    double sum = 0;
    const auto n = std::min(a.first.size(), b.first.size());
    for (std::size_t i = 0; i < n; ++i) {
        float d = 0.5f * ((a.first[i] - b.first[i]) + (a.second[i] - b.second[i]));
        if (low > 0) d = hp2.processSingleSampleRaw(hp.processSingleSampleRaw(d));
        d = lp2.processSingleSampleRaw(lp.processSingleSampleRaw(d));
        if (i >= from) sum += double(d) * d;
    }
    return std::sqrt(sum / double(std::max<std::size_t>(1, n - from)));
}

enum Group { Reverb, Echoes, Modulation, Character, groupCount };
const char* groupNames[] = { "Reverb", "Echoes", "Modulation", "Character" };
const char* groupIds[] = { dc::ids::mixReverb, dc::ids::mixEchoes, dc::ids::mixModulation, dc::ids::mixCharacter };
// The signature kinds with a stage of each group (Signature.cpp's recipes): smear;
// taps or reverse swell; wobble, tremolo, auto-pan or swirl; combs, modes, a sung or
// doubling pitch layer, saturation.
const std::set<int> sigKinds[] = {
    { 6, 12, 19, 27 },
    { 1, 2, 4, 12, 16, 17, 20, 22, 25, 26, 29 },
    { 3, 8, 9, 15, 20, 21, 23, 24 },
    { 1, 6, 7, 9, 13, 14, 18, 28, 30 },
};
bool hasGroup(int location, int sub, int g) {
    dc::Controls c;
    c.location = location;
    c.sub = sub;
    const auto f = dc::Engine::targetFrame(c);
    const auto any = [&f](std::initializer_list<dc::P> ids) { for (auto id : ids) if (f[id] > 1.0e-4f) return true; return false; };
    bool frame = false;
    switch (g) {
        case Reverb: frame = any({ dc::ReverbMix, dc::VaultMix, dc::WraithMix }); break;
        case Echoes: frame = any({ dc::DelayMix, dc::TapeMix }); break;
        case Modulation: frame = any({ dc::EnsMix, dc::Spiral, dc::Flicker, dc::FilterMovement, dc::Instability }) || (f[dc::TapeMix] > 1.0e-4f && f[dc::TapeWow] > 1.0e-4f); break;
        case Character: frame = any({ dc::Tomb, dc::Metal, dc::BloodMix, dc::ChoirBlend, dc::Drone, dc::Wind, dc::Drips, dc::Drops }); break;
        default: break;
    }
    return frame || sigKinds[g].count(static_cast<int>(std::lround(f[dc::SigKind]))) > 0;
}
dc::Controls::Mix withGroup(int g, float v) {
    dc::Controls::Mix m;
    if (g == Reverb) m.reverb = v;
    if (g == Echoes) m.echoes = v;
    if (g == Modulation) m.modulation = v;
    if (g == Character) m.character = v;
    return m;
}
// Every group off but one (the place's other effects out of the way).
dc::Controls::Mix only(int g, float v) {
    dc::Controls::Mix m;
    m.reverb = m.echoes = m.modulation = m.character = 0.0f;
    if (g == Reverb) m.reverb = v;
    if (g == Echoes) m.echoes = v;
    if (g == Modulation) m.modulation = v;
    if (g == Character) m.character = v;
    return m;
}
// Short bursts of a bright chord: the playing (0.6 s) and the gaps (1.4 s).
Signal bursts(float amplitude) {
    const auto chord = sawChord(amplitude);
    return [chord](int ch, long long n, double sr) {
        const double t = std::fmod(static_cast<double>(n) / sr, 2.0);
        const double edge = std::min({ 1.0, t / 0.005, std::max(0.0, (0.6 - t) / 0.005) });
        return t < 0.6 ? chord(ch, n, sr) * static_cast<float>(edge) : 0.0f;
    };
}
// An instrument's dry output (Intensity 0), a held chord or single note.
struct Played { std::vector<float> l, r; double sr = 48000; std::size_t onset = 0; float limiterLowest = 1; };
Played play(const char* name, float punch, float velocity, const std::vector<int>& notes, double hold = 1.2, float punchMs = -1.0f, float levelDb = 0.0f, bool dry = true) {
    InstrumentHost h;
    h.instrument(mixsong::instrumentNamed(name));
    if (dry) h.dry();
    h.set(dc::ids::punch, punch);
    if (punchMs > 0) h.set(dc::ids::punchTime, punchMs);
    if (levelDb != 0.0f) h.set(dc::ids::instLevel, levelDb);
    h.render(0.4);   // the Level trim glides (80 ms): settled before the first note
    for (int n : notes) h.noteOn(n, velocity);
    h.render(hold);
    for (int n : notes) h.noteOff(n);
    h.render(0.4);
    Played p;
    p.l = h.outL;
    p.r = h.outR;
    p.sr = h.sr;
    p.onset = h.at(0.4);
    p.limiterLowest = h.p->synthForTest().punchLimiterLowest();
    return p;
}
// RMS in frames (ms) from the onset.
std::vector<double> envelope(const Played& p, double frameMs, double seconds) {
    const auto frame = static_cast<std::size_t>(frameMs * 0.001 * p.sr);
    std::vector<double> env;
    for (std::size_t i = p.onset; i + frame <= std::min(p.l.size(), p.onset + static_cast<std::size_t>(seconds * p.sr)); i += frame) {
        double sum = 0;
        for (std::size_t k = i; k < i + frame; ++k) sum += 0.5 * (double(p.l[k]) * p.l[k] + double(p.r[k]) * p.r[k]);
        env.push_back(std::sqrt(sum / double(frame)));
    }
    return env;
}
double rmsBetween(const Played& p, double from, double to) {
    double sum = 0;
    const auto a = p.onset + static_cast<std::size_t>(from * p.sr), b = std::min(p.l.size(), p.onset + static_cast<std::size_t>(to * p.sr));
    for (std::size_t k = a; k < b; ++k) sum += 0.5 * (double(p.l[k]) * p.l[k] + double(p.r[k]) * p.r[k]);
    return std::sqrt(sum / double(std::max<std::size_t>(1, b - a)));
}
double peakOf(const Played& p) {
    double m = 0;
    for (std::size_t k = 0; k < p.l.size(); ++k) m = std::max({ m, std::abs(double(p.l[k])), std::abs(double(p.r[k])) });
    return m;
}
double maxSecondDifference(const Played& p, std::size_t from, std::size_t to) {
    double m = 0;
    for (std::size_t i = std::max<std::size_t>(from, 2); i < std::min(to, p.l.size()); ++i)
        m = std::max(m, std::abs(double(p.l[i]) - 2.0 * p.l[i - 1] + p.l[i - 2]));
    return m;
}
// A band's share of an instrument's energy (dB) in a window after the onset.
double bandShare(const Played& p, double from, double to, double low, double high) {
    juce::IIRFilter hp, lp, hp2, lp2;
    hp.setCoefficients(juce::IIRCoefficients::makeHighPass(p.sr, low));
    hp2.setCoefficients(juce::IIRCoefficients::makeHighPass(p.sr, low));
    lp.setCoefficients(juce::IIRCoefficients::makeLowPass(p.sr, high));
    lp2.setCoefficients(juce::IIRCoefficients::makeLowPass(p.sr, high));
    double band = 0, all = 0;
    const auto a = p.onset + static_cast<std::size_t>(from * p.sr), b = std::min(p.l.size(), p.onset + static_cast<std::size_t>(to * p.sr));
    for (std::size_t k = p.onset; k < b; ++k) {
        const float x = 0.5f * (p.l[k] + p.r[k]);
        const float y = lp2.processSingleSampleRaw(lp.processSingleSampleRaw(hp2.processSingleSampleRaw(hp.processSingleSampleRaw(x))));
        if (k >= a) { band += double(y) * y; all += double(x) * x; }
    }
    return 10.0 * std::log10(std::max(1.0e-20, band) / std::max(1.0e-20, all));
}
}

void testMixPunch(Suite& s);
void testMix(Suite& s) {
    using namespace mixtest;
    s.section("mix: the MIX section, the reverb helpers, Atmosphere, Punch and the brass");

    // ---- The parameters, in both plug-ins.
    struct Spec { const char* id; float min, max, def; };
    const std::vector<Spec> specs {
        { dc::ids::mixReverb, 0, 1.5f, 1 }, { dc::ids::mixEchoes, 0, 1.5f, 1 }, { dc::ids::mixModulation, 0, 1.5f, 1 },
        { dc::ids::mixCharacter, 0, 1.5f, 1 }, { dc::ids::mixAtmosphere, 0, 1.5f, 1 }, { dc::ids::shimmer, 0, 1.5f, 1 },
        { dc::ids::mixLowCut, dc::lowCutOff - 5.0f, dc::lowCutMax, dc::lowCutOff - 5.0f }, { dc::ids::mixPreDelay, dc::preDelayMin, dc::preDelayMax, 0 },
        { dc::ids::mixDuck, 0, 1, 0 },
    };
    for (const bool instrument : { false, true }) {
        LightSeekerProcessor p(instrument);
        const std::string who = instrument ? "instrument: " : "FX: ";
        for (const auto& sp : specs) {
            auto* f = dynamic_cast<juce::AudioParameterFloat*>(p.parameters.getParameter(sp.id));
            if (!s.check(f != nullptr, who + sp.id + " exists")) continue;
            s.check(f->isAutomatable() && std::abs(f->range.start - sp.min) < 1.0e-4f && std::abs(f->range.end - sp.max) < 1.0e-4f && std::abs(f->get() - sp.def) < 1.0e-5f,
                    who + sp.id + ": automatable, its range and default", f->getCurrentValueAsText().toStdString());
        }
        s.check(p.parameters.getParameter(dc::ids::mixLowCut)->getCurrentValueAsText() == "Off", who + "Reverb Low Cut reads Off by default");
        s.check(p.parameters.getParameter(dc::ids::mixDuck)->getCurrentValueAsText() == "0 %", who + "Reverb Duck reads 0 % (off) by default");
        auto* on = p.parameters.getParameter(dc::ids::atmosOn);
        s.check(on != nullptr && on->isAutomatable() && on->getValue() > 0.5f, who + "Atmosphere On exists, automatable, on by default (its level, 0 %, keeps it silent)");
        if (instrument) {
            auto* punch = dynamic_cast<juce::AudioParameterFloat*>(p.parameters.getParameter(dc::ids::punch));
            auto* time = dynamic_cast<juce::AudioParameterFloat*>(p.parameters.getParameter(dc::ids::punchTime));
            s.check(punch != nullptr && punch->isAutomatable() && std::abs(punch->get() - 0.5f) < 1.0e-6f, "instrument: Punch is automatable, 50 % = the instrument's own");
            s.check(time != nullptr && time->isAutomatable() && time->range.start == 20.0f && time->range.end == 500.0f, "instrument: Punch Time is automatable, 20-500 ms");
        }
        // Saved, restored; a session saved before Mix and Punch opens at the defaults.
        const std::vector<std::pair<const char*, float>> moved { { dc::ids::mixReverb, 0.4f }, { dc::ids::mixEchoes, 1.3f }, { dc::ids::mixModulation, 0.2f },
                                                                   { dc::ids::mixCharacter, 1.5f }, { dc::ids::mixAtmosphere, 0.7f }, { dc::ids::mixLowCut, 250.0f },
                                                                   { dc::ids::mixPreDelay, 60.0f }, { dc::ids::mixDuck, 0.5f }, { dc::ids::atmosOn, 0.0f } };
        for (const auto& [id, v] : moved) p.parameters.getParameter(id)->setValueNotifyingHost(p.parameters.getParameter(id)->convertTo0to1(v));
        if (instrument) { p.parameters.getParameter(dc::ids::punch)->setValueNotifyingHost(0.9f); }
        juce::MemoryBlock state;
        p.getStateInformation(state);
        LightSeekerProcessor q(instrument);
        q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        bool same = true;
        for (const auto& [id, v] : moved) same &= std::abs(q.parameters.getRawParameterValue(id)->load() - v) < 1.0e-3f;
        if (instrument) same &= std::abs(q.parameters.getParameter(dc::ids::punch)->getValue() - 0.9f) < 1.0e-4f;
        s.check(same, who + "every MIX control (and Punch) is saved with the session");
        auto tree = p.parameters.copyState();
        for (int k = tree.getNumChildren(); --k >= 0;) {
            const auto id = tree.getChild(k).getProperty("id").toString();
            if (id.startsWith("mix") || id == dc::ids::atmosOn || id.startsWith("punch")) tree.removeChild(k, nullptr);
        }
        juce::MemoryBlock older;
        if (auto xml = tree.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, older);
        LightSeekerProcessor o(instrument);
        for (const auto& [id, v] : moved) o.parameters.getParameter(id)->setValueNotifyingHost(o.parameters.getParameter(id)->convertTo0to1(v));
        o.setStateInformation(older.getData(), static_cast<int>(older.getSize()));
        bool defaults = true;
        for (const auto& sp : specs)
            if (std::string(sp.id) != dc::ids::shimmer) defaults &= std::abs(o.parameters.getRawParameterValue(sp.id)->load() - sp.def) < 1.0e-5f;
        defaults &= o.parameters.getRawParameterValue(dc::ids::atmosOn)->load() > 0.5f;
        if (instrument) defaults &= std::abs(o.parameters.getParameter(dc::ids::punch)->getValue() - 0.5f) < 1.0e-6f;
        s.check(defaults, who + "a session saved before Mix and Punch opens with every MIX control as designed");
    }
    {   // Presets: a user preset carries them, a factory preset sets them as designed.
        Host a;
        a.set(dc::ids::mixReverb, 0.5f);
        a.set(dc::ids::mixDuck, 0.4f);
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ls-mix-preset-test.lspreset");
        juce::MemoryBlock data;
        a.p->getStateInformation(data);
        auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), int(data.getSize()));
        s.check(xml != nullptr && xml->writeTo(file), "a user preset with MIX Reverb 50 % and Duck 40 % is written");
        Host b;
        s.check(b.p->loadUserPreset(file) && std::abs(b.p->parameters.getRawParameterValue(dc::ids::mixReverb)->load() - 0.5f) < 1.0e-4f
                    && std::abs(b.p->parameters.getRawParameterValue(dc::ids::mixDuck)->load() - 0.4f) < 1.0e-4f, "... and loads with them");
        file.deleteFile();
        b.p->applyFactoryPreset(0);
        s.check(std::abs(b.p->parameters.getRawParameterValue(dc::ids::mixReverb)->load() - 1.0f) < 1.0e-6f && !b.p->presetModified(),
                "a factory preset sets the MIX as designed (unmodified)");
        b.set(dc::ids::mixEchoes, 0.6f);
        s.check(b.p->presetModified(), "moving a MIX control marks the preset modified");
    }
    {   // An older session sounds as it did: the same samples as a new instance at the defaults.
        Host a, b;
        auto tree = a.p->parameters.copyState();
        for (int k = tree.getNumChildren(); --k >= 0;) {
            const auto id = tree.getChild(k).getProperty("id").toString();
            if (id.startsWith("mix") || id == dc::ids::atmosOn) tree.removeChild(k, nullptr);
        }
        juce::MemoryBlock older;
        if (auto xml = tree.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, older);
        b.set(dc::ids::mixReverb, 0.3f);
        b.p->setStateInformation(older.getData(), static_cast<int>(older.getSize()));
        a.go(dc::GoldenCathedral, 0);
        b.go(dc::GoldenCathedral, 0);
        a.render(3.0, sawChord(0.3f));
        b.render(3.0, sawChord(0.3f));
        double diff = 0;
        for (std::size_t i = 0; i < a.end(); ++i) diff = std::max(diff, std::abs(double(a.outL[i]) - b.outL[i]));
        s.check(diff == 0.0, "an older session (no MIX in it) plays sample for sample as the defaults");
        // At the defaults the MIX changes no place's frame.
        int changed = 0;
        for (const auto& [l, k] : allPlaces()) {
            dc::Controls c;
            c.location = l;
            c.sub = k;
            const auto f = dc::Engine::targetFrame(c);
            const auto g = dc::Engine::targetFrame(l, 1.0f);
            if (k == 0 && std::memcmp(f.data(), g.data(), sizeof(dc::Frame)) != 0) ++changed;
            dc::Frame m = f;
            dc::Engine::applyMix(m, dc::Controls::Mix {});
            if (std::memcmp(f.data(), m.data(), sizeof(dc::Frame)) != 0) ++changed;
        }
        s.check(changed == 0, "the MIX at 100 % leaves every place's frame exactly as designed");
    }

    // ---- Each group scales its own effects (0 / 100 / 150 %), and only those.
    const auto from = static_cast<std::size_t>(1.0 * 48000);
    for (int g = 0; g < groupCount; ++g) {
        int members = 0, untouched = 0;
        std::string moved, weak, capped;
        int more = 0, measured = 0;
        for (const auto& [l, k] : allPlaces()) {
            const bool member = hasGroup(l, k, g);
            if (member) {
                ++members;
                if (members > 5) continue;   // five of them, each kind of stage at least once in the set
                const auto none = rawRender(l, k, withGroup(g, 0.0f), sawChord(0.25f)), def = rawRender(l, k, withGroup(g, 1.0f), sawChord(0.25f)),
                           most = rawRender(l, k, withGroup(g, 1.5f), sawChord(0.25f));
                const double eDef = differenceRms(def, none, from), eMost = differenceRms(most, none, from);
                std::cout << "    " << groupNames[g] << " " << placeName(l, k) << ": 100 % " << fmt(toDb(eDef)) << " dB from 0 %, 150 % "
                          << fmt(toDb(eMost) - toDb(eDef), 2) << " dB more\n";
                ++measured;
                // A depth already near its end (a flutter at 90 % depth) cannot grow much more:
                // 150 % must not be less, and most places must clearly have more.
                if (!(eDef > 1.0e-4 && eMost >= 0.98 * eDef)) weak += placeName(l, k) + " (" + fmt(eMost / std::max(1.0e-9, eDef), 2) + "x) ";
                if (eMost > 1.12 * eDef) ++more;
                else capped += placeName(l, k) + " (" + fmt(eMost / std::max(1.0e-9, eDef), 2) + "x) ";
            } else {
                const auto a = rawRender(l, k, withGroup(g, 0.0f), sawChord(0.25f), 1.5), b = rawRender(l, k, withGroup(g, 1.5f), sawChord(0.25f), 1.5);
                if (differenceRms(a, b, 0) > 0.0) moved += placeName(l, k) + " ";
                else ++untouched;
            }
        }
        s.check(weak.empty(), std::string(groupNames[g]) + ": 0 % takes its effects out, 150 % never less than 100 % (five places with them)", weak);
        s.check(more * 5 >= measured * 3, std::string(groupNames[g]) + ": 150 % clearly more than 100 % on most of them", std::to_string(more) + " of " + std::to_string(measured));
        if (!capped.empty()) std::cout << "  [report] " << groupNames[g] << " near its full depth at 100 %: " << capped << "\n";
        s.check(moved.empty(), std::string(groupNames[g]) + ": places without its effects sound the same at 0 % and 150 %", moved);
        s.metric(std::string(groupNames[g]) + ": places with its effects / without (checked identical)", members, (" / " + std::to_string(untouched)).c_str());
    }
    {   // The reverb at 0 % is the dry place: no tail after the playing stops.
        // (The Nave's Radiant Cascade is an echo: the other groups are off here.)
        const auto none = rawRender(dc::GoldenCathedral, 0, only(Reverb, 0.0f), bursts(0.3f), 2.0);
        const auto def = rawRender(dc::GoldenCathedral, 0, only(Reverb, 1.0f), bursts(0.3f), 2.0);
        const auto tail = [](const Render& r) { double sum = 0; for (std::size_t i = 48000; i < 96000; ++i) sum += double(r.first[i]) * r.first[i]; return std::sqrt(sum / 48000.0); };
        s.check(toDb(tail(def)) - toDb(tail(none)) > 20.0, "Nave of Light: MIX Reverb 0 % leaves no reverb tail in the gaps",
                fmt(toDb(tail(none))) + " vs " + fmt(toDb(tail(def))) + " dBFS");
    }
    // Level-matched through the processor (trims and matcher) at 0 / 100 / 150 %.
    const std::vector<std::tuple<int, int, int>> levelPlaces {
        { Reverb, dc::GoldenCathedral, 0 }, { Reverb, dc::CelestialCitadel, 3 }, { Echoes, dc::BlossomMeadow, 2 }, { Echoes, dc::DragonsPeak, 0 },
        { Modulation, dc::RoyalGardens, 0 }, { Modulation, dc::BlossomMeadow, 1 }, { Character, dc::SkyTemple, 2 }, { Character, dc::DragonsPeak, 1 },
    };
    for (const auto& [g, l, k] : levelPlaces) {
        std::array<double, 3> level {};
        const float settings[] = { 0.0f, 1.0f, 1.5f };
        for (int i = 0; i < 3; ++i) {
            Host h;
            h.set(groupIds[g], settings[i]);
            h.go(l, k);
            h.render(7.0, sawChord(0.3f));
            level[static_cast<std::size_t>(i)] = toDb(h.rms(h.at(4.0), h.end()));
        }
        const double spread = std::max({ level[0], level[1], level[2] }) - std::min({ level[0], level[1], level[2] });
        s.check(spread < 1.5, placeName(l, k) + ": level-matched at MIX " + groupNames[g] + " 0 / 100 / 150 % (within 1.5 dB)",
                fmt(level[0]) + " / " + fmt(level[1]) + " / " + fmt(level[2]) + " dBFS");
    }
    // Click-free while moving: every MIX control swept every block (64 samples) and jumped,
    // against the same timeline held at its largest setting.
    const std::vector<std::tuple<const char*, float, float, int, int>> sweeps {
        { dc::ids::mixReverb, 0.0f, 1.5f, dc::GoldenCathedral, 0 }, { dc::ids::mixEchoes, 0.0f, 1.5f, dc::BlossomMeadow, 2 },
        { dc::ids::mixModulation, 0.0f, 1.5f, dc::BlossomMeadow, 1 }, { dc::ids::mixCharacter, 0.0f, 1.5f, dc::SkyTemple, 2 },
        { dc::ids::mixLowCut, 70.0f, 600.0f, dc::GoldenCathedral, 0 }, { dc::ids::mixPreDelay, -50.0f, 150.0f, dc::CelestialCitadel, 3 },
        { dc::ids::mixDuck, 0.0f, 1.0f, dc::GoldenCathedral, 0 }, { dc::ids::mixReverb, 0.0f, 1.5f, dc::CrystalPalace, 1 },
    };
    for (const auto& [id, low, high, l, k] : sweeps) {
        const auto run = [&, id = id, low = low, high = high, l = l, k = k](bool moving) {
            Host h(48000, 64);
            if (!moving) h.set(id, high);
            h.go(l, k);
            h.render(3.0, sine(220.0, 0.25f));
            for (int step = 0; step <= 300; ++step) {
                const float t = step <= 150 ? float(step) / 150.0f : float(300 - step) / 150.0f;
                if (moving) h.set(id, low + (high - low) * t);
                h.render(64.0 / 48000.0, sine(220.0, 0.25f));
            }
            if (moving) h.set(id, high);
            h.render(0.5, sine(220.0, 0.25f));
            if (moving) h.set(id, low);
            h.render(0.5, sine(220.0, 0.25f));
            return h;
        };
        const Host h = run(true), held = run(false);
        const double steady = std::max(h.maxStep(h.at(1.5), h.at(3.0)), held.maxStep(held.at(1.5), held.end()));
        const double moving = h.maxStep(h.at(3.0), h.end());
        s.check(h.finite() && moving < 2.5 * steady, placeName(l, k) + ": " + id + " moves without clicks", fmt(moving / steady, 2) + "x the steady step");
    }

    // ---- The reverb helpers.
    {   // Low Cut: the reverbs' lows go, their highs stay (their tails in the gaps, the other
        // groups off so the tail is the reverbs alone).
        dc::Controls::Mix off = only(Reverb, 1.0f), cut = only(Reverb, 1.0f), dry = only(Reverb, 0.0f);
        cut.lowCutHz = 400.0f;
        const auto silent = rawRender(dc::GoldenCathedral, 0, dry, bursts(0.3f), 4.0);
        const auto a = rawRender(dc::GoldenCathedral, 0, off, bursts(0.3f), 4.0), b = rawRender(dc::GoldenCathedral, 0, cut, bursts(0.3f), 4.0);
        const auto gap = [](const Render& r) { Render g; g.first.assign(r.first.begin() + 2 * 48000 + 38400, r.first.begin() + 4 * 48000); g.second.assign(r.second.begin() + 2 * 48000 + 38400, r.second.begin() + 4 * 48000); return g; };
        const auto ga = gap(a), gb = gap(b), gz = gap(silent);
        const double lowOff = bandDifference(ga, gz, 2400, 0, 150), lowCut = bandDifference(gb, gz, 2400, 0, 150);
        const double highOff = bandDifference(ga, gz, 2400, 2000, 8000), highCut = bandDifference(gb, gz, 2400, 2000, 8000);
        s.check(toDb(lowOff) - toDb(lowCut) > 8.0, "Reverb Low Cut 400 Hz: the reverb's lows (below 150 Hz) much lower", fmt(toDb(lowOff) - toDb(lowCut)) + " dB less");
        s.check(std::abs(toDb(highOff) - toDb(highCut)) < 1.5, "... its highs (2-8 kHz) as they were", fmt(toDb(highCut) - toDb(highOff)) + " dB");
    }
    {   // Pre-delay: the reverbs start later by the offset (a short burst; the reverbs alone).
        const Signal burst = [](int, long long n, double sr) {
            const double t = static_cast<double>(n) / sr - 0.5;
            return t >= 0 && t < 0.004 ? static_cast<float>(0.5 * std::sin(2.0 * 3.14159265 * 900.0 * t)) : 0.0f;
        };
        const auto onset = [&](float offset) {
            dc::Controls::Mix m = only(Reverb, 1.0f), none = only(Reverb, 0.0f);
            m.preDelayMs = offset;
            none.preDelayMs = offset;
            const auto a = rawRender(dc::CelestialCitadel, 3, m, burst, 1.5), z = rawRender(dc::CelestialCitadel, 3, none, burst, 1.5);
            double peak = 0;
            std::vector<double> env;
            const std::size_t frame = 48;   // 1 ms
            for (std::size_t i = 24000; i + frame < a.first.size(); i += frame) {
                double sum = 0;
                for (std::size_t q = i; q < i + frame; ++q) { const double d = (a.first[q] - z.first[q]) + (a.second[q] - z.second[q]); sum += d * d; }
                env.push_back(std::sqrt(sum / frame));
                peak = std::max(peak, env.back());
            }
            // After the burst itself (4 ms; the dry's level differs in it): where the reverb rises.
            double late = 0;
            for (std::size_t q = 8; q < env.size(); ++q) late = std::max(late, env[q]);
            for (std::size_t q = 8; q < env.size(); ++q) if (env[q] > 0.1 * late) return double(q);
            return -1.0;
        };
        const double at0 = onset(0.0f), at100 = onset(100.0f);
        s.check(at0 >= 0 && std::abs(at100 - at0 - 100.0) < 25.0, "Reverb Pre-delay +100 ms: the reverbs start about 100 ms later",
                fmt(at0, 0) + " ms -> " + fmt(at100, 0) + " ms after the burst");
    }
    {   // Duck: the reverbs dip while the playing sounds, recover in the gaps; at 0 % nothing.
        dc::Engine e;
        e.prepare(48000);
        e.setLevelMatching(false);
        dc::Controls c;
        c.location = dc::GoldenCathedral;
        c.mix.duck = 1.0f;
        const auto signal = bursts(0.3f);
        std::array<float, dc::maxChunk> l {}, r {}, dl {}, dr {};
        double playing = 1, gap = 0;
        for (long long pos = 0; pos < 6 * 48000; pos += dc::maxChunk) {
            e.setControls(c);
            for (int i = 0; i < dc::maxChunk; ++i) { l[std::size_t(i)] = dl[std::size_t(i)] = signal(0, pos + i, 48000); r[std::size_t(i)] = dr[std::size_t(i)] = signal(1, pos + i, 48000); }
            e.process(l.data(), r.data(), dc::maxChunk, dl.data(), dr.data());
            const double t = std::fmod(double(pos) / 48000.0, 2.0);
            if (pos > 2 * 48000 && t > 0.2 && t < 0.55) playing = std::min(playing, double(e.duckGainNow()));
            if (pos > 2 * 48000 && t > 1.6 && t < 1.95) gap = std::max(gap, double(e.duckGainNow()));
        }
        s.check(toDb(playing) < -12.0, "Reverb Duck 100 %: the reverbs dip under the playing (to about -15 dB)", fmt(toDb(playing)) + " dB");
        s.check(gap > 0.97, "... and come back in the gaps", fmt(toDb(gap), 2) + " dB");
        dc::Controls::Mix on, off;
        on.duck = 1.0f;
        const auto a = rawRender(dc::GoldenCathedral, 0, off, bursts(0.3f), 6.0), b = rawRender(dc::GoldenCathedral, 0, on, bursts(0.3f), 6.0);
        // With and without the duck only the reverbs' return differs: by (1 - duck gain).
        const auto windowed = [&](double lo, double hi) {
            double sum = 0; std::size_t count = 0;
            for (std::size_t i = 4 * 48000; i < 6 * 48000; ++i) {
                const double t = std::fmod(double(i) / 48000.0, 2.0);
                if (t < lo || t > hi) continue;
                const double d = 0.5 * ((a.first[i] - b.first[i]) + (a.second[i] - b.second[i]));
                sum += d * d; ++count;
            }
            return std::sqrt(sum / double(std::max<std::size_t>(1, count)));
        };
        const double underPlaying = windowed(0.25, 0.55), inGaps = windowed(1.6, 1.95);
        // (Not quite nothing in the gaps: the Radiance space after the Halls reverberates the
        // Halls' ducked return, so a little of the dip carries into its long tail.)
        s.check(underPlaying > 1.0e-3 && inGaps < 0.2 * underPlaying, "... the duck takes the reverb down under the playing, hardly in the gaps",
                fmt(toDb(underPlaying)) + " vs " + fmt(toDb(inGaps)) + " dBFS of difference");
        dc::Controls::Mix offAgain;
        (void) offAgain;
        s.check(differenceRms(rawRender(dc::GoldenCathedral, 0, off, bursts(0.3f), 2.0), rawRender(dc::GoldenCathedral, 0, dc::Controls::Mix {}, bursts(0.3f), 2.0), 0) == 0.0,
                "Reverb Duck 0 % (the default) changes nothing");
    }

    // ---- The Atmosphere: its MIX control scales it; Atmosphere On turns it off.
    {
        const auto run = [](float mixLevel, bool on, float level) {
            Host h;
            h.set(dc::ids::atmosLevel, level);
            h.set(dc::ids::atmosDensity, 0.9f);
            h.set(dc::ids::mixAtmosphere, mixLevel);
            h.set(dc::ids::atmosOn, on ? 1.0f : 0.0f);
            h.go(dc::SkyTemple, 2);
            h.render(8.0, sine(330.0, 0.02f));
            return h;
        };
        const Host none = run(1.0f, true, 0.0f), zero = run(0.0f, true, 0.6f), def = run(1.0f, true, 0.6f), most = run(1.5f, true, 0.6f), off = run(1.0f, false, 0.6f);
        const auto diff = [&none](const Host& h) {
            double sum = 0;
            for (std::size_t i = h.at(1.0); i < h.end(); ++i) { const double d = h.outL[i] - none.outL[i]; sum += d * d; }
            return std::sqrt(sum / double(h.end() - h.at(1.0)));
        };
        const double eDef = diff(def), eMost = diff(most);
        s.check(eDef > 1.0e-5 && eMost / eDef > 1.3 && eMost / eDef < 1.7, "MIX Atmosphere 150 % is about 1.5x the Atmosphere of 100 %", fmt(eMost / std::max(1.0e-12, eDef), 2) + "x");
        s.check(diff(zero) < 0.02 * eDef, "MIX Atmosphere 0 %: no Atmosphere", fmt(toDb(diff(zero))) + " dB");
        s.check(diff(off) == 0.0, "Atmosphere On off: no Atmosphere (as at Level 0 %)");
    }

    testMixPunch(s);
}

// Punch, the brass, the instruments' defaults and the windows (also alone: `--only mix-punch`).
void testMixPunch(Suite& s) {
    using namespace mixtest;
    // ---- Punch.
    {
        // The overshoot it is set to, settled in its time (envelope ratio to Punch 0, 5 ms frames).
        const auto ratio = [](const Played& a, const Played& b, double seconds) {
            const auto ea = envelope(a, 5.0, seconds), eb = envelope(b, 5.0, seconds);
            std::vector<double> r;
            for (std::size_t k = 0; k < std::min(ea.size(), eb.size()); ++k) r.push_back(eb[k] > 1.0e-6 ? toDb(ea[k]) - toDb(eb[k]) : 0.0);
            return r;
        };
        const std::vector<int> chord { 60, 64, 67 };
        // At Level -12 dB: well below the safety limiter (it is checked on its own below).
        constexpr float quiet = -12.0f;
        const auto base = play("Warm Acoustic Guitar", 0.0f, 1.0f, chord, 1.0, 200.0f, quiet);
        for (const float control : { 0.5f, 1.0f }) {
            const auto punched = play("Warm Acoustic Guitar", control, 1.0f, chord, 1.0, 200.0f, quiet);
            const double want = dc::dsp::punchDb(control, ls::inst::patch(mixsong::instrumentNamed("Warm Acoustic Guitar")).punchDb);
            const auto r = ratio(punched, base, 0.6);
            double most = 0;
            for (std::size_t k = 0; k < 8; ++k) most = std::max(most, r[k]);
            double late = 0;
            for (std::size_t k = 44; k < r.size(); ++k) late = std::max(late, std::abs(r[k]));
            s.check(std::abs(most - want) < 1.0, "Punch " + fmt(control * 100.0, 0) + " %: the hit overshoots by its +" + fmt(want, 1) + " dB",
                    fmt(most, 2) + " dB");
            s.check(late < 0.3, "... and has settled after its Punch Time (200 ms)", fmt(late, 2) + " dB after 220 ms");
            const double mid = r.size() > 20 ? r[20] : 0.0;   // 100 ms: half way
            s.check(mid > 0.3 * want && mid < 0.7 * want, "... half of it half way (raised cosine)", fmt(mid, 2) + " dB at 100 ms");
        }
        {   // Punch Time sets the settling.
            const auto slow = play("Warm Acoustic Guitar", 1.0f, 1.0f, chord, 1.0, 400.0f, quiet), slowBase = play("Warm Acoustic Guitar", 0.0f, 1.0f, chord, 1.0, 400.0f, quiet);
            const auto r = ratio(slow, slowBase, 0.6);
            s.check(r.size() > 90 && r[60] > 1.0 && std::abs(r[88]) < 0.3, "Punch Time 400 ms: still punching at 300 ms, settled at 440 ms",
                    fmt(r.size() > 60 ? r[60] : 0.0, 2) + " / " + fmt(r.size() > 88 ? r[88] : 0.0, 2) + " dB");
        }
        {   // Velocity-sensitive: a soft note punches less.
            const auto soft = play("Warm Acoustic Guitar", 1.0f, 0.4f, chord, 1.0, 200.0f, quiet), softBase = play("Warm Acoustic Guitar", 0.0f, 0.4f, chord, 1.0, 200.0f, quiet);
            const auto hard = play("Warm Acoustic Guitar", 1.0f, 1.0f, chord, 1.0, 200.0f, quiet);
            const auto rs = ratio(soft, softBase, 0.1), rh = ratio(hard, base, 0.1);
            double ms = 0, mh = 0;
            for (std::size_t k = 0; k < 8; ++k) { ms = std::max(ms, rs[k]); mh = std::max(mh, rh[k]); }
            s.check(ms < mh - 3.0, "Punch is velocity-sensitive (a soft note punches less)", fmt(ms, 1) + " vs " + fmt(mh, 1) + " dB");
        }
        {   // Click-free: no step beyond the gain it adds; Punch 0 is the instrument exactly.
            const auto punched = play("Warm Acoustic Guitar", 1.0f, 1.0f, chord, 1.0, 20.0f, quiet);
            const auto plain = play("Warm Acoustic Guitar", 0.0f, 1.0f, chord, 1.0, 20.0f, quiet);
            const double gain = dc::dsp::dbToGain(dc::dsp::punchMaxDb);
            const double stepPunched = maxSecondDifference(punched, punched.onset, punched.onset + 9600), stepPlain = maxSecondDifference(plain, plain.onset, plain.onset + 9600);
            s.check(stepPunched < 1.2 * gain * stepPlain, "Punch 100 % at 20 ms: no clicks (no step beyond the gain it adds)",
                    fmt(stepPunched / std::max(1.0e-9, stepPlain), 2) + "x for " + fmt(gain, 2) + "x gain");
            const auto again = play("Warm Acoustic Guitar", 0.0f, 1.0f, chord, 1.0, 120.0f, quiet);
            double diff = 0;
            for (std::size_t i = 0; i < std::min(again.l.size(), plain.l.size()); ++i) diff = std::max(diff, std::abs(double(again.l[i]) - plain.l[i]));
            s.check(diff == 0.0, "Punch 0 %: Punch Time changes nothing (no Punch at all)");
        }
        {   // Never clips: the most Punch, loud brass chords, Level +12 dB, in its place.
            const auto loud = play("Heroic Horns", 1.0f, 1.0f, { 48, 55, 60, 64, 67, 72 }, 1.0, 300.0f, 12.0f, false);
            s.check(peakOf(loud) <= 0.9661, "Punch 100 % on loud brass at +12 dB: no clipping (below -0.3 dBFS)", fmt(toDb(peakOf(loud)), 2) + " dBFS");
            s.check(loud.limiterLowest < 1.0f, "... the Punch limiter took the overshoot back", fmt(toDb(loud.limiterLowest), 1) + " dB");
            const auto below = play("Warm Acoustic Guitar", 1.0f, 1.0f, chord, 1.0, 120.0f, -18.0f);
            s.check(below.limiterLowest == 1.0f && peakOf(below) < 0.9, "below the ceiling (Level -18 dB, Punch 100 %) the limiter does nothing",
                    fmt(toDb(peakOf(below)), 1) + " dBFS peak");
            const auto normal = play("Warm Acoustic Guitar", 0.5f, 0.8f, chord, 1.0);
            std::cout << "  [report] Warm Acoustic Guitar, a chord at velocity 0.8, its own Punch, Level 0 dB: the limiter took "
                      << fmt(-toDb(normal.limiterLowest), 1) << " dB at most\n";
        }
    }
    // Punch defaults: high on the brass, medium on plucks and harps, off or low on pads and choirs.
    {
        const auto design = [](const char* name) { return ls::inst::patch(mixsong::instrumentNamed(name)).punchDb; };
        bool brass = true, plucks = true, pads = true;
        for (const char* n : { "Fanfare Trumpets", "Heroic Horns", "Soaring Horn" }) brass &= design(n) >= 6.0f;
        for (const char* n : { "Concert Harp", "Fairy Harp", "Rising Harp Glissando", "Warm Acoustic Guitar", "Shimmering Pluck", "Bouncy Kalimba", "Elven Lyre" })
            plucks &= design(n) >= 2.5f && design(n) <= 5.0f;
        for (const char* n : { "Angelic Choir", "Radiant Choir", "Cloud Pad", "Orchestral Chord Pad", "Glass Harmonica Pad" }) pads &= design(n) <= 1.0f;
        s.check(brass, "Punch defaults high on the brass (6 dB or more)");
        s.check(plucks, "... medium on the plucks and harps (2.5-5 dB)");
        s.check(pads, "... off or low on the pads and choirs (1 dB or less)");
    }

    // ---- The brass: a louder, brighter, more present hit; the same sustain. The design before
    // Mix and Punch, measured with `--brass-report` on that code (a held C major chord at
    // velocity 0.95, dry, synthesised layers): attack peak over sustain (dB, 5 ms frames),
    // the 2-4 kHz share of the first 150 ms (dB of the total) and the sustain (dBFS).
    struct BrassBefore { const char* name; double attackToSustain, presence, sustain; };
    const BrassBefore before[] = {
        { "Fanfare Trumpets", 6.1, -15.6, -18.6 }, { "Soaring Horn", 4.9, -26.2, -17.3 }, { "Heroic Horns", 5.5, -19.5, -17.1 },
        { "Epic Choir and Brass", -1.9, -24.4, -17.6 }, { "Grand Organ", 3.5, -19.2, -19.8 },
    };
    for (const auto& b : before) {
        const std::vector<int> chord { 60, 64, 67 };
        const auto plain = play(b.name, 0.0f, 0.95f, chord, 1.4), hit = play(b.name, 0.5f, 0.95f, chord, 1.4);
        const double sustainPlain = rmsBetween(plain, 0.8, 1.3), sustainHit = rmsBetween(hit, 0.8, 1.3);
        const auto e = envelope(hit, 5.0, 0.2), ep = envelope(plain, 5.0, 0.2);
        double peak = 0, gain = 0;
        for (std::size_t k = 0; k < e.size(); ++k) {
            peak = std::max(peak, e[k]);
            if (k < ep.size() && ep[k] > 1.0e-6) gain = std::max(gain, toDb(e[k]) - toDb(ep[k]));
        }
        const std::string n = b.name;
        const auto& patch = ls::inst::patch(mixsong::instrumentNamed(b.name));
        // Measured against the synthesised design; with the recordings (the choir's voice)
        // the numbers are reported only.
        const bool comparable = !ls::inst::patchUsesRecordings(patch) || !ls::inst::patchSampled(patch);
        const double attackToSustain = toDb(peak) - toDb(sustainHit), presence = bandShare(hit, 0.0, 0.15, 2000, 4000);
        std::cout << "    " << n << ": attack-to-sustain " << fmt(b.attackToSustain, 1) << " -> " << fmt(attackToSustain, 1) << " dB, presence "
                  << fmt(b.presence, 1) << " -> " << fmt(presence, 1) << " dB, sustain " << fmt(b.sustain, 1) << " -> " << fmt(toDb(sustainHit), 1) << " dBFS\n";
        s.check(std::abs(toDb(sustainHit) - toDb(sustainPlain)) < 0.3, n + ": the same sustain level with its Punch", fmt(toDb(sustainHit) - toDb(sustainPlain), 2) + " dB");
        s.check(gain > 1.5, n + ": its Punch makes the hit louder", "+" + fmt(gain, 1) + " dB at most");
        if (!comparable) continue;
        const bool organ = n == "Grand Organ";   // the trumpet stop is one layer of the organ
        s.check(attackToSustain > b.attackToSustain + (organ ? 0.8 : 1.5), n + ": a louder hit over the sustain than before",
                fmt(b.attackToSustain, 1) + " -> " + fmt(attackToSustain, 1) + " dB");
        s.check(presence > b.presence + (organ ? 0.5 : 1.0), n + ": more 2-4 kHz presence on the attack than before", fmt(b.presence, 1) + " -> " + fmt(presence, 1) + " dB");
        s.check(std::abs(toDb(sustainHit) - b.sustain) < 0.5, n + ": the same sustain as before", fmt(b.sustain, 1) + " -> " + fmt(toDb(sustainHit), 1) + " dBFS");
    }
    {   // The fanfare's rise: quicker than 25 ms from 10 to 90 %.
        const auto p = play("Fanfare Trumpets", 0.0f, 0.9f, { 60, 64, 67 }, 0.6);
        const auto e = envelope(p, 2.0, 0.3);
        double peak = 0;
        for (double v : e) peak = std::max(peak, v);
        std::size_t a = 0, b = 0;
        while (a < e.size() && e[a] < 0.1 * peak) ++a;
        b = a;
        while (b < e.size() && e[b] < 0.9 * peak) ++b;
        s.check(double(b - a) * 2.0 < 25.0, "Fanfare Trumpets: rises in under 25 ms (10-90 %)", fmt(double(b - a) * 2.0, 0) + " ms");
    }
    // The instruments mostly played in a mix sit a little drier; the huge spaces stay (150 %).
    {
        bool drier = true, reachable = true;
        for (const char* n : { "Fanfare Trumpets", "Heroic Horns", "Soaring Horn", "Warm Strings", "Shimmering Strings", "Concert Harp", "Fairy Harp",
                               "Warm Acoustic Guitar", "Shimmering Pluck", "Rising Harp Glissando", "Bouncy Pizzicato" }) {
            const auto& p = ls::inst::patch(mixsong::instrumentNamed(n));
            drier &= p.space < 1.0f && p.space >= 0.66f;
            reachable &= p.space * dc::mixMax >= 1.0f;
        }
        s.check(drier, "brass, strings, plucks, harps and guitars take a drier share of the place's reverbs by default (66-90 %)");
        s.check(reachable, "... MIX Reverb 150 % brings back the place's whole space");
        bool padsFull = true;
        for (const char* n : { "Angelic Choir", "Radiant Choir", "Cloud Pad", "Orchestral Chord Pad" }) padsFull &= ls::inst::patch(mixsong::instrumentNamed(n)).space == 1.0f;
        s.check(padsFull, "the pads and choirs keep the place's whole reverb");
    }
    {   // The instrument's chorus follows MIX Modulation (Shimmering Strings, its own chorus).
        std::vector<Played> r;
        for (const float m : { 0.0f, 1.0f, 1.5f }) {
            InstrumentHost h;
            h.instrument(mixsong::instrumentNamed("Shimmering Strings"));
            h.dry();
            h.set(dc::ids::mixModulation, m);
            h.render(0.1);
            for (int n : { 60, 64, 67 }) h.noteOn(n);
            h.render(2.0);
            Played p; p.l = h.outL; p.r = h.outR; p.onset = h.at(0.1);
            r.push_back(p);
        }
        const auto diff = [](const Played& a, const Played& b) { double sum = 0; for (std::size_t i = 48000; i < a.l.size(); ++i) { const double d = a.l[i] - b.l[i]; sum += d * d; } return std::sqrt(sum); };
        s.check(diff(r[1], r[0]) > 0 && diff(r[2], r[0]) > 1.1 * diff(r[1], r[0]), "instrument: its own chorus follows MIX Modulation (0 < 100 < 150 %)");
    }

    // ---- The windows: the MIX section in both, the Atmosphere in Advanced.
    for (const bool instrument : { false, true }) {
        const std::string who = instrument ? "instrument: " : "FX: ";
        LightSeekerProcessor p(instrument);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = dynamic_cast<LightSeekerEditor*>(editor.get());
        if (!s.check(e != nullptr, who + "the window opens")) continue;
        auto& v = e->view();
        s.check(v.mixButton().isVisible() && !v.mixOpen(), who + "a MIX button with the levels; the section folded away at first");
        v.mixButton().setToggleState(true, juce::dontSendNotification);
        v.mixButton().onClick();
        auto& panel = v.mixPanelForTest();
        s.check(v.mixOpen() && panel.isVisible() && panel.getBottom() <= v.getHeight() - 140 && panel.getX() >= 0 && panel.getRight() <= v.getWidth(),
                who + "MIX opens above the controls, inside the window");
        bool labelled = true;
        const char* want[] = { "Reverb", "Echoes", "Modulation", "Shimmer", "Character", "Atmosphere", "Low Cut", "Pre-delay", "Duck" };
        for (int k = 0; k < dc::ui::MixPanel::count; ++k) labelled &= panel.label(k) == want[k] && panel.slider(k).isVisible();
        s.check(labelled, who + "its nine controls, labelled");
        panel.slider(0).setValue(0.4, juce::sendNotificationSync);
        panel.slider(3).setValue(1.2, juce::sendNotificationSync);
        s.check(std::abs(p.parameters.getRawParameterValue(dc::ids::mixReverb)->load() - 0.4f) < 1.0e-3f
                    && std::abs(p.parameters.getRawParameterValue(dc::ids::shimmer)->load() - 1.2f) < 1.0e-3f,
                who + "its sliders move the parameters (Shimmer is the same Shimmer control)");
        panel.resetButton().onClick();
        s.check(std::abs(p.parameters.getRawParameterValue(dc::ids::mixReverb)->load() - 1.0f) < 1.0e-6f
                    && std::abs(p.parameters.getRawParameterValue(dc::ids::shimmer)->load() - 1.0f) < 1.0e-6f, who + "As designed puts every MIX control back");
        s.check(p.viewState().mix, who + "the window remembers MIX open");
        {
            std::unique_ptr<juce::AudioProcessorEditor> again(p.createEditor());
            auto* e2 = dynamic_cast<LightSeekerEditor*>(again.get());
            s.check(e2 != nullptr && e2->view().mixOpen(), who + "a reopened window has MIX open, as it was left");
        }
        v.advancedButton().setToggleState(true, juce::dontSendNotification);
        v.advancedButton().onClick();
        s.check(!v.mixOpen() && v.advancedOpen(), who + "Advanced and MIX share the space: opening one folds the other");
        panel.closeButton().onClick();
        if (instrument && e->instrumentPanel() != nullptr) {
            auto* ip = e->instrumentPanel();
            ip->mixButton().onClick();
            s.check(v.mixOpen() && ip->mixButton().getToggleState(), "instrument: the panel's MIX button opens the MIX section");
            panel.closeButton().onClick();
            s.check(!v.mixOpen() && !ip->mixButton().getToggleState(), "... and both buttons follow it closing");
            s.check(ip->macroSlider(8).isVisible() && ip->macroSlider(9).isVisible(), "instrument: Punch and Punch Time on the panel");
            ip->macroSlider(8).setValue(0.8, juce::sendNotificationSync);
            s.check(std::abs(p.parameters.getParameter(dc::ids::punch)->getValue() - 0.8f) < 1.0e-3f, "... the Punch slider moves Punch");
            ip->chooseForTest(mixsong::instrumentNamed("Fanfare Trumpets"));
            s.check(p.parameters.getParameter(dc::ids::punch)->getCurrentValueAsText().endsWith("dB"), "... reading in dB of the instrument's own",
                    p.parameters.getParameter(dc::ids::punch)->getCurrentValueAsText().toStdString());
        }
    }
    {   // The Atmosphere's settings in Advanced: On, Level, Density (on at Level 0 starts it).
        LightSeekerProcessor p(false);
        p.parameters.getParameter(dc::ids::atmosOn)->setValueNotifyingHost(0.0f);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = dynamic_cast<LightSeekerEditor*>(editor.get());
        if (e != nullptr) {
            auto& v = e->view();
            v.advancedButton().setToggleState(true, juce::dontSendNotification);
            v.advancedButton().onClick();
            auto* drawer = dynamic_cast<juce::Component*>(&v.drawerForTest());
            juce::ToggleButton* on = nullptr;
            int sliders = 0;
            std::function<void(juce::Component&)> find = [&](juce::Component& c) {
                for (auto* child : c.getChildren()) {
                    if (auto* t = dynamic_cast<juce::ToggleButton*>(child); t != nullptr && t->getButtonText() == "On") on = t;
                    if (auto* sl = dynamic_cast<juce::Slider*>(child); sl != nullptr && sl->getTooltip().isEmpty()) ++sliders;
                    find(*child);
                }
            };
            find(*drawer);
            s.check(on != nullptr, "Advanced: the Atmosphere's On switch");
            if (on != nullptr) {
                on->setToggleState(true, juce::sendNotificationSync);
                on->onClick();
                s.check(p.parameters.getRawParameterValue(dc::ids::atmosOn)->load() > 0.5f && p.parameters.getRawParameterValue(dc::ids::atmosLevel)->load() > 0.3f,
                        "... turning it on (at Level 0 %) starts it at 35 %");
            }
        }
    }
}

// Snapshots of the MIX section (FX and instrument) and the Atmosphere in Advanced.
int mixSnapshots(const juce::String& directory) {
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    int written = 0;
    const auto save = [&](juce::Component& c, const juce::String& name) {
        const auto image = c.createComponentSnapshot(c.getLocalBounds(), true, 1.0f);
        const auto file = dir.getChildFile(name);
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat png;
        written += out.openedOk() && png.writeImageToStream(image, out) ? 1 : 0;
    };
    for (const bool instrument : { false, true }) {
        LightSeekerProcessor p(instrument);
        p.setRateAndBufferSizeDetails(48000, 512);
        p.prepareToPlay(48000, 512);
        p.travelTo(dc::CelestialCitadel, 0);
        juce::AudioBuffer<float> b(2, 512);
        juce::MidiBuffer m;
        p.processBlock(b, m);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = dynamic_cast<LightSeekerEditor*>(editor.get());
        if (e == nullptr) return 1;
        e->view().settleForTest();
        e->view().setMixOpen(true);
        if (auto* panel = e->instrumentPanel()) panel->refreshForTest();
        save(*e, instrument ? "mix-section-instrument.png" : "mix-section-fx.png");
        if (!instrument) {
            e->view().advancedButton().setToggleState(true, juce::dontSendNotification);
            e->view().advancedButton().onClick();
            save(*e, "advanced-atmosphere.png");
        }
    }
    std::cout << written << " snapshots written to " << dir.getFullPathName() << "\n";
    return written == 3 ? 0 : 1;
}

// Debugging aid: a place's frame (the MIX groups' parameters) and its tail at MIX settings.
int mixDebug(int location, int sub) {
    using namespace mixtest;
    dc::Controls c;
    c.location = location;
    c.sub = sub;
    const auto f = dc::Engine::targetFrame(c);
    for (auto id : { dc::ReverbMix, dc::VaultMix, dc::VaultKind, dc::WraithMix, dc::DelayMix, dc::TapeMix, dc::SigKind, dc::SigAmount, dc::PreDelay, dc::Decay, dc::EnsMix, dc::ChoirBlend })
        std::cout << dc::moduleSpecs[id].id << " = " << f[id] << "\n";
    const auto tail = [](const Render& r) { double sum = 0; for (std::size_t i = 48000; i < 96000; ++i) sum += double(r.first[i]) * r.first[i]; return toDb(std::sqrt(sum / 48000.0)); };
    for (float v : { 0.0f, 1.0f, 1.5f }) std::cout << "reverb " << v << ": tail " << fmt(tail(rawRender(location, sub, withGroup(Reverb, v), bursts(0.3f), 2.0))) << "\n";
    for (float v : { 0.0f, 1.0f }) std::cout << "echoes " << v << ": tail " << fmt(tail(rawRender(location, sub, withGroup(Echoes, v), bursts(0.3f), 2.0))) << "\n";
    dc::Controls::Mix all0;
    all0.reverb = all0.echoes = all0.modulation = all0.character = 0;
    std::cout << "all groups 0: tail " << fmt(tail(rawRender(location, sub, all0, bursts(0.3f), 2.0))) << "\n";
    return 0;
}
