// Final touches: the Shimmer control. Included by LightSeekerTests.cpp (it uses its
// Suite, Host, InstrumentHost and signals).
//   shimmer   the parameter (range, default, automatable, saved, older sessions at the
//             default), shimmer energy scaling with the control (0 / 100 / 150 %), the
//             default about 15-25 % below the original design, level-matched at every
//             setting, click-free while moving, places without shimmer untouched, and the
//             instrument's shimmer scaled too.

namespace shimmertest {
// Places whose sound has a shimmer, octave-shimmer or sparkle stage (each kind of stage
// at least once): the spiral, the prism, the hue shimmer, a reverb with shimmer and a
// halo, the glitter, the twinkling harmonics, a soaring octave.
const std::vector<std::pair<int, int>> shimmerPlaces {
    { dc::SkyTemple, 0 },          // Cloud Steps: the rising spiral (+ the reverb's shimmer)
    { dc::CrystalPalace, 0 },      // Prism Hall: the spectral prism
    { dc::GoldenCathedral, 2 },    // Stained Glass Chapel: the hue shimmer
    { dc::CelestialCitadel, 2 },   // Throne of Light: reverb and Vault shimmer, the gold halo
    { dc::FairyGlade, 1 },         // Firefly Hollow: the glitter
    { dc::ElvenSanctuary, 2 },     // Starlit Canopy: the twinkling harmonics
    { dc::DragonsPeak, 2 },        // Summit of Winds: the soaring octave
};

// The engine alone (no level matcher, no trims' adaptation): the chain's raw output for a
// place at a Shimmer setting, a bright chord for `seconds`.
std::pair<std::vector<float>, std::vector<float>> rawRender(int location, int sub, float shimmer, double seconds = 4.0) {
    constexpr double sr = 48000;
    dc::Engine e;
    e.prepare(sr);
    e.setLevelMatching(false);
    dc::Controls c;
    c.location = location;
    c.sub = sub;
    c.shimmer = shimmer;
    const auto chord = sawChord(0.25f);
    const auto total = static_cast<long long>(seconds * sr);
    std::vector<float> outL, outR;
    std::array<float, dc::maxChunk> l {}, r {}, dl {}, dr {};
    for (long long pos = 0; pos < total; pos += dc::maxChunk) {
        const int n = static_cast<int>(std::min<long long>(dc::maxChunk, total - pos));
        e.setControls(c);
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            l[k] = dl[k] = chord(0, pos + i, sr);
            r[k] = dr[k] = chord(1, pos + i, sr);
        }
        e.process(l.data(), r.data(), n, dl.data(), dr.data());
        outL.insert(outL.end(), l.begin(), l.begin() + n);
        outR.insert(outR.end(), r.begin(), r.begin() + n);
    }
    return { outL, outR };
}
// RMS of the difference between two renders (the shimmer's own energy when `b` has none).
double differenceRms(const std::pair<std::vector<float>, std::vector<float>>& a, const std::pair<std::vector<float>, std::vector<float>>& b, std::size_t from) {
    double sum = 0;
    const auto n = std::min(a.first.size(), b.first.size());
    for (std::size_t i = from; i < n; ++i) {
        const double dl = a.first[i] - b.first[i], dr = a.second[i] - b.second[i];
        sum += 0.5 * (dl * dl + dr * dr);
    }
    return std::sqrt(sum / double(std::max<std::size_t>(1, n - from)));
}
}

void testShimmer(Suite& s) {
    using namespace shimmertest;
    s.section("shimmer: the Shimmer control (0-150 %), both plug-ins");
    // The parameter, in both plug-ins.
    for (const bool instrument : { false, true }) {
        LightSeekerProcessor p(instrument);
        const std::string who = instrument ? "instrument" : "FX";
        auto* sh = dynamic_cast<juce::AudioParameterFloat*>(p.parameters.getParameter(dc::ids::shimmer));
        if (!s.check(sh != nullptr, who + ": the Shimmer parameter exists")) continue;
        s.check(sh->isAutomatable(), who + ": Shimmer is automatable");
        s.check(sh->range.start == 0.0f && std::abs(sh->range.end - 1.5f) < 1.0e-6f, who + ": Shimmer ranges 0-150 %");
        s.check(std::abs(sh->get() - 1.0f) < 1.0e-6f, who + ": Shimmer defaults to 100 %");
        s.check(sh->getCurrentValueAsText() == "100 %", who + ": Shimmer reads 100 %", sh->getCurrentValueAsText().toStdString());
        // Saved and restored; a session saved before Shimmer existed opens at the default.
        sh->setValueNotifyingHost(sh->convertTo0to1(1.5f));
        juce::MemoryBlock state;
        p.getStateInformation(state);
        LightSeekerProcessor q(instrument);
        q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        s.check(std::abs(q.parameters.getRawParameterValue(dc::ids::shimmer)->load() - 1.5f) < 1.0e-4f, who + ": Shimmer is saved with the session");
        auto tree = p.parameters.copyState();
        for (int k = tree.getNumChildren(); --k >= 0;)
            if (tree.getChild(k).getProperty("id").toString() == dc::ids::shimmer) tree.removeChild(k, nullptr);
        juce::MemoryBlock older;
        if (auto xml = tree.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, older);
        LightSeekerProcessor o(instrument);
        o.parameters.getParameter(dc::ids::shimmer)->setValueNotifyingHost(0.0f);   // whatever it was
        o.setStateInformation(older.getData(), static_cast<int>(older.getSize()));
        s.check(std::abs(o.parameters.getRawParameterValue(dc::ids::shimmer)->load() - 1.0f) < 1.0e-6f, who + ": a session saved before Shimmer opens at 100 %");
    }
    // User presets carry it; a factory preset sets the default.
    {
        Host a;
        a.set(dc::ids::shimmer, 0.4f);
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ls-shimmer-preset-test.lspreset");
        juce::MemoryBlock data;
        a.p->getStateInformation(data);
        auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), int(data.getSize()));
        s.check(xml != nullptr && xml->writeTo(file), "a user preset with Shimmer 40 % is written");
        Host b;
        b.set(dc::ids::shimmer, 1.2f);
        s.check(b.p->loadUserPreset(file) && std::abs(b.p->parameters.getRawParameterValue(dc::ids::shimmer)->load() - 0.4f) < 1.0e-4f,
                "... and loads with Shimmer 40 %");
        file.deleteFile();
        b.p->applyFactoryPreset(0);
        s.check(std::abs(b.p->parameters.getRawParameterValue(dc::ids::shimmer)->load() - 1.0f) < 1.0e-6f, "a factory preset sets Shimmer to 100 %");
        s.check(!b.p->presetModified(), "a factory preset with Shimmer at 100 % reads unmodified");
        b.set(dc::ids::shimmer, 1.3f);
        s.check(b.p->presetModified(), "moving Shimmer marks the preset modified");
    }
    {
        Host a;
        a.set(dc::ids::shimmer, 0.4f);
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        Host b;
        b.p->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        s.check(std::abs(b.p->parameters.getRawParameterValue(dc::ids::shimmer)->load() - 0.4f) < 1.0e-4f, "Shimmer 40 % survives a state round trip (presets, A/B, sessions)");
    }

    // The shimmer's energy scales with the control, the default sits below the design.
    const auto from = static_cast<std::size_t>(2.0 * 48000);
    for (const auto& [l, k] : shimmerPlaces) {
        const auto none = rawRender(l, k, 0.0f), def = rawRender(l, k, 1.0f), most = rawRender(l, k, 1.5f);
        // "As designed": the base of each stage undone (1 / 0.75 .. 1 / 0.85): about 1.25.
        const auto design = rawRender(l, k, 1.25f);
        const double eDef = differenceRms(def, none, from), eMost = differenceRms(most, none, from), eDesign = differenceRms(design, none, from);
        const std::string name = placeName(l, k);
        s.metric(name + ": shimmer energy at 0 / 100 / 125 / 150 %", toDb(eDef), " dB (100 %)");
        std::cout << "    " << name << ": 150 % vs 100 % " << fmt(toDb(eMost) - toDb(eDef), 2) << " dB, 100 % vs the design "
                  << fmt(toDb(eDef) - toDb(eDesign), 2) << " dB\n";
        s.check(eDef > 1.0e-4, name + ": Shimmer 100 % has shimmer (differs from 0 %)", fmt(toDb(eDef)) + " dBFS");
        s.check(eMost / eDef > 1.3 && eMost / eDef < 1.8, name + ": Shimmer 150 % is about 1.5x the shimmer of 100 %", fmt(eMost / eDef, 2) + "x");
        s.check(eDef / eDesign > 0.7 && eDef / eDesign < 0.92, name + ": the default is about 15-25 % below the original design", fmt(eDef / eDesign, 2) + "x");
    }

    // Places without any shimmer stage are untouched by the control (no other sound
    // changes): the same samples at 0 % and 150 %.
    {
        int checked = 0;
        std::string moved;
        for (const auto& [l, k] : allPlaces()) {
            const auto f = dc::placeFrame(l, k);
            if (f[dc::Shimmer] > 0.0f || f[dc::VaultShimmer] > 0.0f) continue;
            const auto a = rawRender(l, k, 0.0f, 1.5), b = rawRender(l, k, 1.5f, 1.5);
            if (differenceRms(a, b, 0) > 0.0) {
                // A signature with a shimmer layer: skip (it is meant to move).
                const int kind = static_cast<int>(std::lround(f[dc::SigKind]));
                static const std::set<int> shimmerKinds { 3, 5, 8, 10, 11, 13, 19, 22, 24, 26, 27, 31 };
                if (shimmerKinds.count(kind) == 0) moved += placeName(l, k) + " ";
                continue;
            }
            ++checked;
        }
        s.check(moved.empty(), "places without a shimmer stage sound the same at every Shimmer setting", moved);
        s.metric("places without shimmer, checked identical at 0 % and 150 %", checked);
    }

    // Level-matched at 0, 100 and 150 % through the real processor (trims and matcher).
    for (const auto& [l, k] : shimmerPlaces) {
        std::array<double, 3> level {};
        const float settings[] = { 0.0f, 1.0f, 1.5f };
        for (int i = 0; i < 3; ++i) {
            Host h;
            h.set(dc::ids::shimmer, settings[i]);
            h.go(l, k);
            h.render(7.0, sawChord(0.3f));
            level[static_cast<std::size_t>(i)] = toDb(h.rms(h.at(4.0), h.end()));
        }
        const double spread = std::max({ level[0], level[1], level[2] }) - std::min({ level[0], level[1], level[2] });
        s.check(spread < 1.5, placeName(l, k) + ": level-matched at Shimmer 0 / 100 / 150 % (within 1.5 dB)",
                fmt(level[0]) + " / " + fmt(level[1]) + " / " + fmt(level[2]) + " dBFS");
    }

    // Click-free while moving: Shimmer swept 0 -> 150 % -> 0 in steps every block (64
    // samples: as fast as automation gets) on a low sine.
    // The steady reference is the same timeline with Shimmer held at 150 % (the most
    // shimmer: glitter and twinkles are random bursts with steps of their own).
    for (const auto& [l, k] : shimmerPlaces) {
        const auto run = [&](bool moving) {
            Host h(48000, 64);
            if (!moving) h.set(dc::ids::shimmer, 1.5f);
            h.go(l, k);
            h.render(3.0, sine(220.0, 0.25f));
            for (int step = 0; step <= 300; ++step) {
                if (moving) h.set(dc::ids::shimmer, step <= 150 ? 1.5f * float(step) / 150.0f : 1.5f * float(300 - step) / 150.0f);
                h.render(64.0 / 48000.0, sine(220.0, 0.25f));
            }
            // A jump too (a host setting it at once): the modules smooth it.
            if (moving) h.set(dc::ids::shimmer, 1.5f);
            h.render(0.5, sine(220.0, 0.25f));
            if (moving) h.set(dc::ids::shimmer, 0.0f);
            h.render(0.5, sine(220.0, 0.25f));
            return h;
        };
        const Host h = run(true), held = run(false);
        const double steady = std::max(h.maxStep(h.at(1.5), h.at(3.0)), held.maxStep(held.at(1.5), held.end()));
        const double moving = h.maxStep(h.at(3.0), h.end());
        s.check(h.finite(), placeName(l, k) + ": finite while Shimmer moves");
        s.check(moving < 2.5 * steady, placeName(l, k) + ": Shimmer moves without clicks", fmt(moving / steady, 2) + "x the steady step");
    }

    // The instrument: its own shimmer reverb (Crystal Bells at Prism Hall) follows the
    // control too, click-free.
    {
        std::vector<std::vector<float>> outs;
        for (const float v : { 0.0f, 1.0f, 1.5f }) {
            InstrumentHost h;
            h.set(dc::ids::shimmer, v);
            h.go(dc::CrystalPalace, 0);
            h.render(0.2);
            for (int n : { 72, 76, 79 }) h.noteOn(n);
            h.render(2.5);
            std::vector<float> mono(h.outL.size());
            for (std::size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (h.outL[i] + h.outR[i]);
            outs.push_back(mono);
        }
        const auto diff = [](const std::vector<float>& a, const std::vector<float>& b) {
            double sum = 0; const auto n = std::min(a.size(), b.size());
            for (std::size_t i = 0; i < n; ++i) sum += double(a[i] - b[i]) * (a[i] - b[i]);
            return std::sqrt(sum / double(std::max<std::size_t>(1, n)));
        };
        const double eDef = diff(outs[1], outs[0]), eMost = diff(outs[2], outs[0]);
        s.check(eDef > 1.0e-4 && eMost > 1.2 * eDef, "instrument: Crystal Bells' shimmer grows with Shimmer (0 < 100 % < 150 %)",
                fmt(toDb(eDef)) + " / " + fmt(toDb(eMost)) + " dB");
        InstrumentHost h(48000, 64);
        h.go(dc::CrystalPalace, 0);
        h.render(0.2);
        for (int n : { 72, 76, 79 }) h.noteOn(n);
        h.render(1.0);
        const double before = [&] { double m = 0; for (std::size_t i = h.at(0.7); i < h.end(); ++i) m = std::max(m, std::abs(double(h.outL[i]) - 2.0 * h.outL[i - 1] + h.outL[i - 2])); return m; }();
        const auto start = h.end();
        for (int step = 0; step <= 200; ++step) {
            h.set(dc::ids::shimmer, step <= 100 ? 1.5f * float(step) / 100.0f : 1.5f * float(200 - step) / 100.0f);
            h.render(64.0 / 48000.0);
        }
        double during = 0;
        for (std::size_t i = start; i < h.end(); ++i) during = std::max(during, std::abs(double(h.outL[i]) - 2.0 * h.outL[i - 1] + h.outL[i - 2]));
        s.check(h.finite() && during < 2.5 * before, "instrument: Shimmer moves without clicks", fmt(during / std::max(1.0e-9, before), 2) + "x");
    }
}
