// The real woodwinds (VSCO 2 Community Edition, CC0; the real woodwinds update): included by
// LightSeekerTests.cpp after LoudnessTests.h (it uses InstrumentHost, the brass group's pitch
// and click measures, mixtest::play, the loudness test's melody and meter, writeWav).
//   woodwinds                the library's woodwinds loaded; every woodwind instrument with them in
//                            tune across its range (a cents report), its dynamics smooth (velocity
//                            layers, the mod wheel's expression), no clicks at its loops, note-offs
//                            and legato joins, level-matched with its synthesised self on the
//                            loudness test's melody in its place (the chord pad on its chords), still
//                            distinct from its siblings, no clipping; the stand-ins without the library
//   --calibrate-woodwinds f  writes the woodwind trims (Source/Synth/WoodwindTrims.h) and prints the
//                            chord pad's own trims with its woodwinds (Source/Synth/InstrumentTrims.h)
//   --listen-woodwinds dir   each changed instrument before (synthesised) vs after (real), dry and
//                            in its place; the cents and level report

namespace woodtest {
// The woodwind instruments: name, the range played (MIDI), a note for the dynamics, solo (a
// melody instrument: legato, its melody matched) or the chord pad's woodwind choir.
struct WoodInstrument { const char* name; int low, high, sweepNote; bool solo; };
const std::array<WoodInstrument, 4> woodwinds { {
    { "Airy Flute", 62, 95, 79, true },
    { "Breathy Wind Flute", 62, 95, 79, true },
    { "Pastoral Oboe", 59, 89, 72, true },
    { "Orchestral Chord Pad", 48, 76, 60, false },
} };

bool libraryLoaded() {
    samplesLoaded();
    return ls::inst::patchLibraryLoaded(ls::inst::patch(mixsong::instrumentNamed("Airy Flute")));
}

// "Before": the same build with the library's woodwinds left out (their synthesised stand-ins
// play; the chord pad's real brass stays), or the whole library (`all`).
struct Before {
    bool all;
    explicit Before(bool on = true, bool everything = false) : all(everything) {
        if (all) lswb::samples::Bank::bypassLibraryForTests(on);
        else lswb::samples::Bank::bypassWoodwindsForTests(on);
    }
    ~Before() {
        if (all) lswb::samples::Bank::bypassLibraryForTests(false);
        else lswb::samples::Bank::bypassWoodwindsForTests(false);
    }
};

// The level of a mono signal in windows (dB).
std::vector<double> windows(const std::vector<float>& x, double sr, double from, double to, double ms) {
    std::vector<double> out;
    const auto w = std::max<std::size_t>(1, std::size_t(ms * 0.001 * sr));
    for (auto k = std::size_t(from * sr); k + w <= std::min(x.size(), std::size_t(to * sr)); k += w) {
        double sum = 0;
        for (std::size_t q = k; q < k + w; ++q) sum += double(x[q]) * x[q];
        out.push_back(toDb(std::sqrt(sum / double(w))));
    }
    return out;
}

// The level in overlapping windows (dB): `ms` long, every `hop` ms (a vibrato's pulse averaged out).
std::vector<double> sliding(const std::vector<float>& x, double sr, double from, double to, double ms, double hop) {
    std::vector<double> out;
    const auto w = std::max<std::size_t>(1, std::size_t(ms * 0.001 * sr)), h = std::max<std::size_t>(1, std::size_t(hop * 0.001 * sr));
    for (auto k = std::size_t(from * sr); k + w <= std::min(x.size(), std::size_t(to * sr)); k += h) {
        double sum = 0;
        for (std::size_t q = k; q < k + w; ++q) sum += double(x[q]) * x[q];
        out.push_back(toDb(std::sqrt(sum / double(w))));
    }
    return out;
}

// The mod wheel swept from 0 to full over `seconds` while a note is held (CC 1 in 1/127 steps),
// then held there: a mono render from the note's start.
// (`sweep` false: the same note with the wheel left at 0, the recording's own movement.)
std::vector<float> wheelSweep(int instrument, int note, double seconds, bool dry = true, bool sweep = true) {
    InstrumentHost h;
    h.instrument(instrument);
    if (dry) h.dry();
    h.cc(1, 0);
    h.render(0.1);
    h.noteOn(note, 0.7f);
    h.render(0.8);
    for (int v = 1; v <= 127; ++v) {
        if (sweep) h.cc(1, v);
        h.render(seconds / 127.0);
    }
    h.render(0.8);
    h.noteOff(note);
    h.render(0.8);
    h.cc(1, 0);
    h.render(0.05);
    return h.mono(h.at(0.1), h.end());
}

// A legato line: each note starts `overlap` seconds before the last one lets go (a real legato
// join), and a detached one (a gap between the notes): mono, from the first note. `omit`: that
// note is left out (its key never pressed; everything else the same), so the difference of the
// two renders is that note alone.
std::vector<float> line(int instrument, bool legato, bool dry = true, double* secondAt = nullptr, int omit = -1) {
    InstrumentHost h;
    h.instrument(instrument);
    if (dry) h.dry();
    h.render(0.1);
    const int notes[] { 72, 74, 76, 79, 77, 76, 74, 72 };
    const double length = 0.45, overlap = 0.04, gap = 0.06;
    const auto on = [&](int k) { if (k != omit) h.noteOn(notes[k], 0.75f); };
    const auto off = [&](int k) { if (k != omit) h.noteOff(notes[k]); };
    for (int k = 0; k < 8; ++k) {
        if (k == 0) on(0);
        h.render(length - (legato ? overlap : 0.0));
        if (k + 1 < 8 && legato) {
            if (k == 0 && secondAt != nullptr) *secondAt = double(h.end()) / h.sr - 0.1;
            on(k + 1);
            h.render(overlap);
            off(k);
        } else {
            off(k);
            if (k + 1 < 8) {
                h.render(gap);
                if (k == 0 && secondAt != nullptr) *secondAt = double(h.end()) / h.sr - 0.1;
                on(k + 1);
            }
        }
    }
    h.render(1.2);
    return h.mono(h.at(0.1), h.end());
}

// The instrument's own loudness test part (its melody; the chord pad's chords) in its place,
// at the shipped defaults: BS.1770 integrated.
double ownLoudness(int instrument) {
    using namespace loudness;
    return loudnessOf(playPart(instrument, roleOf(instrument).part)).integrated;
}

// The chord pad's woodwinds alone (its loudness test chords with the Woodwinds macro as
// designed, against the same with it at 0; their difference in power).
double padWoodwindsLufs(int instrument) {
    using namespace loudness;
    const auto withMacro = [&](float layer) {
        Options o;
        auto notes = notesOf(Chords, shiftOf(instrument));
        InstrumentHost h;
        h.instrument(instrument);
        h.set(dc::ids::instLayer, layer);
        h.render(settleSeconds);
        struct Event { double t; bool on; int note; };
        std::vector<Event> ev;
        double last = 0;
        for (const auto& n : notes) {
            ev.push_back({ n.at * beat, true, n.note });
            ev.push_back({ (n.at + n.length) * beat, false, n.note });
            last = std::max(last, (n.at + n.length) * beat);
        }
        std::stable_sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) { return a.t < b.t || (a.t == b.t && !a.on && b.on); });
        double now = 0;
        for (const auto& e : ev) {
            if (e.t > now) { h.render(e.t - now); now = e.t; }
            if (e.on) h.noteOn(e.note, velocity);
            else h.noteOff(e.note);
        }
        h.render(last + o.tail - now);
        const auto skip = std::ptrdiff_t(h.at(settleSeconds));
        return std::make_pair(std::vector<float>(h.outL.begin() + skip, h.outL.end()), std::vector<float>(h.outR.begin() + skip, h.outR.end()));
    };
    const auto [l1, r1] = withMacro(0.5f);
    const auto [l0, r0] = withMacro(0.0f);
    std::vector<float> dl(l1.size()), dr(r1.size());
    for (std::size_t k = 0; k < l1.size(); ++k) { dl[k] = l1[k] - (k < l0.size() ? l0[k] : 0.0f); dr[k] = r1[k] - (k < r0.size() ? r0[k] : 0.0f); }
    return measure(dl, dr, 48000.0).integrated;
}

struct Tuning { double worst = 0; int at = 0; std::string list; };
Tuning tuning(int i, int low, int high) {
    Tuning t;
    for (int m = low; m <= high; m += 3) {
        const auto x = brasstest::heldNote(i, m, 0.75f, 1.3);
        const auto from = std::size_t(0.3 * 48000.0), to = std::size_t(1.25 * 48000.0);
        const double c = brasstest::brassCents(std::vector<float>(x.begin() + std::ptrdiff_t(from), x.begin() + std::ptrdiff_t(std::min(to, x.size()))), 48000.0, m);
        t.list += std::to_string(m) + ":" + fmt(c, 1) + " ";
        if (std::abs(c) > std::abs(t.worst)) { t.worst = c; t.at = m; }
    }
    return t;
}

std::vector<double> velocityCurve(int i, int note) {
    std::vector<double> level;
    for (int k = 2; k <= 20; ++k) {
        const auto x = brasstest::heldNote(i, note, 0.05f * float(k), 0.8);
        double sum = 0;
        const auto a = std::size_t(0.02 * 48000.0), z = std::min(x.size(), std::size_t(0.8 * 48000.0));
        for (std::size_t q = a; q < z; ++q) sum += double(x[q]) * x[q];
        level.push_back(toDb(std::sqrt(sum / double(z - a))));
    }
    return level;
}

// A held note through its loops and its release, then fast repeats: the worst click ratio of each.
void heldAndRepeats(int i, int note, double& held, double& repeats, double& peakDb) {
    InstrumentHost h;
    h.instrument(i);
    h.dry();
    h.render(0.1);
    h.noteOn(note, 0.8f);
    h.noteOn(note + 7, 0.7f);
    h.render(5.0);
    h.noteOff(note);
    h.noteOff(note + 7);
    h.render(1.5);
    for (int k = 0; k < 12; ++k) {
        h.noteOn(note + (k % 3) * 4, 0.5f + 0.04f * float(k));
        h.render(0.09);
        h.noteOff(note + (k % 3) * 4);
        h.render(0.03);
    }
    h.render(1.0);
    const auto x = h.mono(0, h.end());
    held = brasstest::clickRatio(x, h.sr, h.at(0.4), h.at(6.6));
    repeats = brasstest::clickRatio(x, h.sr, h.at(6.6), h.end());
    peakDb = h.finite() ? toDb(h.peak(0, h.end())) : 99.0;
}
}

void testWoodwinds(Suite& s) {
    using namespace woodtest;
    s.section("woodwinds: the real woodwinds (VSCO 2 Community Edition, CC0) in every woodwind instrument");
    {   // The licence lists the woodwinds (the same entry as the brass).
        auto dir = juce::File::getCurrentWorkingDirectory();
        juce::File licence;
        for (int up = 0; up < 6 && !licence.existsAsFile(); ++up, dir = dir.getParentDirectory())
            licence = dir.getChildFile("docs/LICENSES/VSCO-2-CE.txt");
        const auto text = licence.loadFileAsString();
        s.check(text.contains("CC0 1.0 Universal") && text.contains("28092772094b2d9f1148d84cea97f4545b8c687d") && text.contains("woodwinds")
                    && text.contains("flute") && text.contains("oboe") && text.contains("piccolo") && text.contains("clarinet") && text.contains("bassoon"),
                "the library's licence entry lists the woodwinds used (docs/LICENSES/VSCO-2-CE.txt)", licence.getFullPathName().toStdString());
    }
    for (const auto& w : woodwinds)
        s.check(ls::inst::patchUsesWoodwinds(ls::inst::patch(mixsong::instrumentNamed(w.name))), std::string(w.name) + " has real woodwind layers");
    {
        const auto& pad = ls::inst::patch(mixsong::instrumentNamed("Orchestral Chord Pad"));
        s.check(pad.layerMacro != nullptr && std::string(pad.layerMacro) == "Woodwinds", "the chord pad's woodwinds are on their own macro (Woodwinds)");
        int others = 0;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) others += ls::inst::patch(i).layerMacro != nullptr && i != mixsong::instrumentNamed("Orchestral Chord Pad") ? 1 : 0;
        s.check(others == 0, "... no other instrument has a second macro (dimmed there)");
    }
    {   // The panel: the second macro beside Character, named Woodwinds on the chord pad, it moves
        // its parameter; dimmed (and named Layer) on the other instruments.
        LightSeekerProcessor p(true);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = dynamic_cast<LightSeekerEditor*>(editor.get());
        if (e != nullptr && e->instrumentPanel() != nullptr) {
            auto* ip = e->instrumentPanel();
            ip->chooseForTest(mixsong::instrumentNamed("Orchestral Chord Pad"));
            auto& slider = ip->macroSlider(10);
            s.check(slider.isVisible() && slider.isEnabled() && ip->macroLabel(10).getText() == "Woodwinds",
                    "the panel: the chord pad's Woodwinds macro, enabled", ip->macroLabel(10).getText().toStdString());
            s.check(slider.getX() > ip->macroSlider(3).getX() && slider.getX() < ip->macroSlider(4).getX(), "... beside Character (before Vowel)");
            slider.setValue(0.8, juce::sendNotificationSync);
            s.check(std::abs(p.parameters.getParameter(dc::ids::instLayer)->getValue() - 0.8f) < 1.0e-3f, "... the slider moves the Layer parameter");
            ip->chooseForTest(mixsong::instrumentNamed("Airy Flute"));
            s.check(!ip->macroSlider(10).isEnabled() && ip->macroLabel(10).getText() == "Layer", "... dimmed on an instrument without a second macro");
        } else
            s.check(false, "the panel: the instrument's editor opens");
    }
    const bool loaded = libraryLoaded();
    std::cout << "  library woodwinds: " << (loaded ? "loaded" : "not found (only the stand-ins are tested)") << "; " << lswb::samples::Bank::get().message() << "\n";
    if (!loaded) return;
    const char* only = std::getenv("LS_WOODWINDS_ONLY");
    for (const auto& w : woodwinds) {
        const int i = mixsong::instrumentNamed(w.name);
        const std::string n = w.name;
        if (only != nullptr && n != only) continue;
        const auto& p = ls::inst::patch(i);
        s.check(ls::inst::patchLibraryLoaded(p), n + ": plays the library woodwinds");
        {   // In tune across its range: a note every 3 semitones (harmonic sum over 0.3-1.25 s).
            const auto t = tuning(i, w.low, w.high);
            std::cout << "    " << n << " tuning (cents, MIDI:cents): " << t.list << "\n";
            s.check(std::abs(t.worst) <= 15.0, n + ": in tune across its range (MIDI " + std::to_string(w.low) + "-" + std::to_string(w.high) + ", within 15 cents)",
                    "worst " + fmt(t.worst, 1) + " cents at " + std::to_string(t.at));
        }
        {   // Dynamics: velocity layers smooth (0.1-1 in 0.05 steps: rising, no step).
            const auto level = velocityCurve(i, w.sweepNote);
            double worstUp = 0, worstDown = 0;
            for (std::size_t k = 1; k < level.size(); ++k) {
                worstUp = std::max(worstUp, level[k] - level[k - 1]);
                worstDown = std::min(worstDown, level[k] - level[k - 1]);
            }
            std::string curve;
            for (double v : level) curve += fmt(v, 1) + " ";
            const std::string detail = "steps " + fmt(worstDown, 2) + " .. +" + fmt(worstUp, 2) + " dB, range " + fmt(level.back() - level.front(), 1) + " dB: " + curve;
            std::cout << "    " << n << " velocity curve: " << detail << "\n";
            s.check(worstDown > -0.75 && worstUp < 3.5 && level.back() > level.front() + 4.0,
                    n + ": dynamics smooth with velocity (0.1-1 in 0.05 steps: rising over 4 dB, no dip, no step over 3.5 dB)", detail);
        }
        {   // The mod wheel's expression: a swell, smooth (no step), no clicks, against the stand-in's sweep.
            const auto x = wheelSweep(i, w.sweepNote, 2.5);
            // Smooth: no step beyond the recording's own movement (the same note with the wheel
            // left at 0: the vibrato's and the player's own swell), by more than 1 dB.
            const auto steps = [](const std::vector<double>& e, double& up, double& down) {
                up = 0;
                down = 0;
                for (std::size_t k = 1; k < e.size(); ++k) { up = std::max(up, e[k] - e[k - 1]); down = std::min(down, e[k] - e[k - 1]); }
            };
            double up = 0, down = 0, up0 = 0, down0 = 0;
            steps(sliding(x, 48000.0, 0.5, 3.6, 400.0, 100.0), up, down);
            steps(sliding(wheelSweep(i, w.sweepNote, 2.5, true, false), 48000.0, 0.5, 3.6, 400.0, 100.0), up0, down0);
            const double rise = windows(x, 48000.0, 3.4, 4.0, 600.0).front() - windows(x, 48000.0, 0.2, 0.8, 600.0).front();
            double click0 = 0;
            {
                Before off;
                const auto x0 = wheelSweep(i, w.sweepNote, 2.5);
                click0 = brasstest::clickRatio(x0, 48000.0, std::size_t(0.3 * 48000), x0.size());
            }
            const double click = brasstest::clickRatio(x, 48000.0, std::size_t(0.3 * 48000), x.size());
            std::cout << "    " << n << " mod wheel 0 -> 100 %: " << fmt(rise, 1) << " dB louder, steps per 100 ms " << fmt(down, 2) << " .. +" << fmt(up, 2)
                      << " dB (the wheel at 0: " << fmt(down0, 2) << " .. +" << fmt(up0, 2) << "), click ratio " << fmt(click, 1) << " (stand-in " << fmt(click0, 1) << ")\n";
            // (The chord pad: only its woodwinds swell, a soft layer in it: a little.)
            const double wanted = w.solo ? 2.0 : 0.3;
            s.check(rise > wanted && up < std::max(1.0, up0) + 1.0 && down > std::min(-1.0, down0) - 1.0,
                    n + ": the mod wheel is its expression: a smooth swell (over " + fmt(wanted, 1)
                        + " dB; 400 ms windows every 100 ms: no step 1 dB beyond the note's own movement)",
                    fmt(rise, 1) + " dB, steps " + fmt(down, 2) + " .. +" + fmt(up, 2) + " (own " + fmt(down0, 2) + " .. +" + fmt(up0, 2) + ")");
            s.check(click < std::max(14.0, 1.25 * click0), n + ": ... with no clicks", "ratio " + fmt(click, 1) + " (stand-in " + fmt(click0, 1) + ")");
        }
        {   // No clicks held through its loops and its release, in fast repeats; no clipping.
            double held = 0, repeats = 0, peak = 0, held0 = 0, repeats0 = 0, peak0 = 0;
            {
                Before off;
                heldAndRepeats(i, w.sweepNote, held0, repeats0, peak0);
            }
            heldAndRepeats(i, w.sweepNote, held, repeats, peak);
            const double limit = std::max(14.0, 1.25 * std::max(held0, repeats0));
            s.check(held < limit && repeats < limit, n + ": no clicks (held through its loops and its note-off; fast repeats)",
                    "worst ratio " + fmt(held, 1) + " / " + fmt(repeats, 1) + " (stand-in " + fmt(held0, 1) + " / " + fmt(repeats0, 1) + ")");
            s.check(peak < 0.0, n + ": no clipping at its defaults", fmt(peak, 1) + " dBFS");
        }
        if (w.solo) {
            // Legato: a note started while the last one is held joins it (no new attack), with no click;
            // detached notes start afresh.
            // Legato: a note started while the last one is held joins it (no front, the last note
            // fading under it instead of its recorded release): the line's level runs on through the
            // join (10 ms frames, its lowest from 30 ms before to 150 ms after the join against the
            // notes either side) no deeper than the note's own movement (its vibrato and the
            // player's: the same statistic on a note held alone) plus 1.5 dB; no clicks there.
            const auto dipAround = [](const std::vector<float>& x, double at) {
                const auto e = windows(x, 48000.0, at - 0.1, at + 0.3, 10.0);
                const auto mean = [&](std::size_t a, std::size_t b) {
                    double sum = 0;
                    for (std::size_t k = a; k < b; ++k) sum += e[k];
                    return sum / double(std::max<std::size_t>(1, b - a));
                };
                double lowest = e[7];
                for (std::size_t k = 7; k < 25; ++k) lowest = std::min(lowest, e[k]);
                return lowest - std::min(mean(0, 7), mean(25, 40));
            };
            double at = 0, at0 = 0;
            const auto joined = line(i, true, true, &at);
            ls::inst::setLegatoForTests(i, false);
            const auto fresh = line(i, true, true, &at0);
            ls::inst::setLegatoForTests(i, true);
            const double dip = dipAround(joined, at), dipFresh = dipAround(fresh, at0);
            if (const char* dump = std::getenv("LS_WOODWIND_DUMP")) {
                writeWav(juce::File(juce::String(dump) + "/legato-" + juce::String(n).replaceCharacter(' ', '-') + ".wav"), joined, joined, 48000.0);
                std::cout << "    (the join at " << fmt(at, 3) << " s)\n";
            }
            // (The note's own movement: the deepest of the statistic over a held note's sustain, at
            // positions 50 ms apart: a recorded vibrato pulses by several dB in 10 ms frames.)
            double own = 0;
            {
                const auto held = brasstest::heldNote(i, 74, 0.75f, 1.8);
                for (double t = 0.6; t <= 1.3; t += 0.05) own = std::min(own, dipAround(held, t));
            }
            double click0 = 0;
            {
                Before off;
                const auto x0 = line(i, true);
                click0 = brasstest::clickRatio(x0, 48000.0, std::size_t(0.05 * 48000), x0.size());
            }
            const double click = brasstest::clickRatio(joined, 48000.0, std::size_t(0.05 * 48000), joined.size());
            const auto detached = line(i, false);
            const double clickDetached = brasstest::clickRatio(detached, 48000.0, std::size_t(0.05 * 48000), detached.size());
            std::cout << "    " << n << " legato: the line through the join dips " << fmt(dip, 1) << " dB (a note held alone " << fmt(own, 1) << ", fresh attacks "
                      << fmt(dipFresh, 1) << "); click ratio legato " << fmt(click, 1) << ", detached " << fmt(clickDetached, 1) << " (stand-in legato " << fmt(click0, 1) << ")\n";
            s.check(dip > std::min(own, 0.0) - 1.5, n + ": legato joins the line smoothly (no dip through the join beyond the note's own movement + 1.5 dB)",
                    fmt(dip, 1) + " dB (held alone " + fmt(own, 1) + ", fresh attacks " + fmt(dipFresh, 1) + ")");
            s.check(click < std::max(14.0, 1.25 * click0) && clickDetached < std::max(14.0, 1.25 * click0), n + ": no clicks at legato transitions (or detached ones)",
                    fmt(click, 1) + " / " + fmt(clickDetached, 1) + " (stand-in " + fmt(click0, 1) + ")");
        }
        {   // Level-matched with its synthesised self on the realistic loudness test: its melody (the
            // chord pad: its chords, and its woodwinds alone) in its own place at the defaults.
            double before = 0, after = 0;
            {
                Before off(true, !w.solo);
                before = ownLoudness(i);
            }
            after = ownLoudness(i);
            std::cout << "    " << n << ": " << loudness::partName(loudness::roleOf(i).part) << " in its place " << fmt(before, 2) << " -> " << fmt(after, 2) << " LUFS\n";
            s.check(std::abs(after - before) <= 1.0, n + ": level-matched with its synthesised self (its " + loudness::partName(loudness::roleOf(i).part)
                        + " in its place, BS.1770, within 1 LU)", fmt(before, 2) + " -> " + fmt(after, 2) + " LUFS");
            s.check(std::abs(after - loudness::targetLufs) <= loudness::tolerance, n + ": ... at the instruments' level (-20 LUFS +-1.5)", fmt(after, 2) + " LUFS");
            if (!w.solo) {
                double ww0 = 0;
                {
                    Before off;
                    ww0 = padWoodwindsLufs(i);
                }
                const double ww = padWoodwindsLufs(i);
                std::cout << "    " << n << ": its woodwinds alone " << fmt(ww0, 2) << " -> " << fmt(ww, 2) << " LUFS\n";
                s.check(std::abs(ww - ww0) <= 1.5, n + ": its woodwinds level-matched with their stand-ins (within 1.5 LU)", fmt(ww0, 2) + " -> " + fmt(ww, 2) + " LUFS");
                s.check(ww < after - 6.0, n + ": ... a soft layer in the pad (6 LU under it or more)", fmt(ww, 2) + " vs " + fmt(after, 2) + " LUFS");
            }
        }
        {   // Loud: full velocity, +12 dB Level in its place: never clips.
            const auto loud = mixtest::play(w.name, 0.5f, 1.0f, w.solo ? std::vector<int> { 72, 76 } : std::vector<int> { 48, 55, 60, 64, 67, 72 }, 1.5, -1.0f, 12.0f, false);
            s.check(mixtest::peakOf(loud) <= 0.9661, n + ": full velocity, +12 dB in its place: no clipping (below -0.3 dBFS)", fmt(toDb(mixtest::peakOf(loud)), 2) + " dBFS");
        }
    }
    {   // Still distinct from their siblings (the instruments group's fingerprint and gate), and the
        // two flutes from each other.
        for (const auto& pair : { std::pair<const char*, const char*> { "Airy Flute", "Glass Harp" }, { "Airy Flute", "Elven Lyre" },
                                  { "Breathy Wind Flute", "Cloud Pad" }, { "Breathy Wind Flute", "Shimmering Pluck" },
                                  { "Pastoral Oboe", "Warm Acoustic Guitar" }, { "Pastoral Oboe", "Bouncy Kalimba" },
                                  { "Orchestral Chord Pad", "Shimmering Strings" }, { "Orchestral Chord Pad", "Fanfare Trumpets" },
                                  { "Airy Flute", "Breathy Wind Flute" } }) {
            const int a = mixsong::instrumentNamed(pair.first), c = mixsong::instrumentNamed(pair.second);
            const double d = printDistance(instrumentPrint(renderInstrument(a)), instrumentPrint(renderInstrument(c)));
            double d0 = 0;
            {
                Before off(true, true);
                d0 = printDistance(instrumentPrint(renderInstrument(a)), instrumentPrint(renderInstrument(c)));
            }
            std::cout << "    " << pair.first << " / " << pair.second << ": distance " << fmt(d, 2) << " (the stand-ins " << fmt(d0, 2) << ")\n";
            // The two flutes are not siblings (different locations); their stand-ins were 1.5 apart:
            // there they must be more distinct than before (the siblings' gate is 3.0).
            if (std::string(pair.second) == "Breathy Wind Flute")
                s.check(d > d0, std::string(pair.first) + " is more distinct from " + pair.second + " than their stand-ins (not siblings: different locations)",
                        "distance " + fmt(d0, 2) + " -> " + fmt(d, 2));
            else
                s.check(d > 3.0, std::string(pair.first) + " is distinct from its sibling " + pair.second, "distance " + fmt(d, 2));
        }
    }
    {   // The chord pad's Woodwinds macro: none at 0, as designed at 50 %, more at 100 %.
        const int i = mixsong::instrumentNamed("Orchestral Chord Pad");
        const auto level = [&](float layer) {
            InstrumentHost h;
            h.instrument(i);
            h.dry();
            h.set(dc::ids::instLayer, layer);
            h.render(0.1);
            for (int m : { 60, 64, 67 }) h.noteOn(m, 0.8f);
            h.render(2.5);
            return toDb(h.rms(h.at(1.5), h.at(2.5)));
        };
        const double l0 = level(0.0f), l5 = level(0.5f), l1 = level(1.0f);
        s.check(l0 < l5 - 0.2 && l5 < l1 - 0.2, "the chord pad's Woodwinds macro: more woodwinds as it rises (0 / 50 / 100 %)",
                fmt(l0, 2) + " / " + fmt(l5, 2) + " / " + fmt(l1, 2) + " dB");
    }
    {   // Without the library: the synthesised stand-ins play.
        Before off(true, true);
        for (const auto& w : woodwinds) {
            const int i = mixsong::instrumentNamed(w.name);
            s.check(!ls::inst::patchLibraryLoaded(ls::inst::patch(i)), std::string(w.name) + ": without the library its synthesised stand-in plays");
            InstrumentHost h;
            h.instrument(i);
            h.dry();
            h.render(0.1);
            h.noteOn(w.sweepNote, 0.8f);
            h.render(1.0);
            s.check(h.rms(h.at(0.5), h.at(1.1)) > 1.0e-4, std::string(w.name) + ": ... and sounds");
        }
    }
}

int calibrateWoodwinds(const juce::String& path) {
    using namespace woodtest;
    if (!libraryLoaded()) {
        std::cout << "the library woodwinds are not loaded (LIGHTSEEKER_SAMPLES): nothing to calibrate\n";
        return 1;
    }
    const bool recordings = ls::inst::patchSampled(ls::inst::patch(mixsong::instrumentNamed("Orchestral Chord Pad")));
    std::array<float, 31> trims {}, standIns {};
    for (int pass = 0; pass < 2; ++pass) {
        const bool standIn = pass == 1;
        lswb::samples::Bank::bypassRecordingsForTests(standIn);
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            const auto& p = ls::inst::patch(i);
            auto& out = standIn ? standIns : trims;
            out[std::size_t(i)] = standIn ? p.woodwindStandInTrimDb : p.woodwindTrimDb;
            if (!ls::inst::patchUsesWoodwinds(p)) continue;
            if (standIn && !ls::inst::patchUsesRecordings(p)) { out[std::size_t(i)] = trims[std::size_t(i)]; continue; }
            if (!standIn && !recordings && ls::inst::patchUsesRecordings(p)) continue;
            const bool solo = p.layerMacro == nullptr;
            // The solo woodwinds on their melody in place; the chord pad's woodwinds alone (its
            // chords with its Woodwinds macro against the macro at 0).
            double target = 0;
            {
                Before off;
                target = solo ? ownLoudness(i) : padWoodwindsLufs(i);
            }
            float trim = out[std::size_t(i)];
            for (int step = 0; step < 6; ++step) {
                ls::inst::setWoodwindTrimForTests(i, trim, standIn);
                const double level = solo ? ownLoudness(i) : padWoodwindsLufs(i);
                const double miss = target - level;
                std::cout << (standIn ? "without the recordings, " : "") << instrumentName(i) << ": stand-in " << fmt(target, 2) << " LUFS, real " << fmt(level, 2)
                          << " at trim " << fmt(trim, 2) << "\n";
                if (std::abs(miss) < 0.1) break;
                trim = std::clamp(trim + float(miss) * (solo ? 1.0f : 1.2f), -24.0f, 24.0f);
            }
            out[std::size_t(i)] = float(std::round(trim * 10.0f) / 10.0f);
            ls::inst::setWoodwindTrimForTests(i, out[std::size_t(i)], standIn);
        }
    }
    // The chord pad gained a layer: its own trim brings its chords back to the target (the
    // loudness rebalance's; printed for Source/Synth/InstrumentTrims.h).
    for (int pass = 0; pass < 2; ++pass) {
        lswb::samples::Bank::bypassRecordingsForTests(pass == 1);
        const int i = mixsong::instrumentNamed("Orchestral Chord Pad");
        const auto& p = ls::inst::patch(i);
        float trim = pass == 0 ? p.trimDb : p.standInTrimDb;
        for (int step = 0; step < 4; ++step) {
            ls::inst::setTrimForTests(i, trim, pass == 1);
            const double level = ownLoudness(i);
            std::cout << (pass ? "without the recordings, " : "") << "Orchestral Chord Pad: " << fmt(level, 2) << " LUFS at its trim " << fmt(trim, 1) << "\n";
            if (std::abs(level - loudness::targetLufs) <= 0.1) break;
            trim = float(std::round((trim - (level - loudness::targetLufs)) * 10.0) / 10.0);
        }
        std::cout << "instrument trim (" << (pass ? "standInTrims" : "instrumentTrims") << ") for the Orchestral Chord Pad: " << fmt(trim, 1) << "\n";
    }
    lswb::samples::Bank::bypassRecordingsForTests(false);
    const auto list = [](const std::array<float, 31>& a) {
        juce::String out;
        for (std::size_t i = 0; i < a.size(); ++i) out << (i ? ", " : "") << juce::String(a[i], 1) << "f";
        return out;
    };
    juce::String text;
    text << "#pragma once\n"
         << "// The real woodwinds' level trims (dB, per instrument, on their real part), written by\n"
         << "// `LightSeekerTests --calibrate-woodwinds Source/Synth/WoodwindTrims.h`: with the library\n"
         << "// loaded, each woodwind instrument's melody from the loudness test (Tests/LoudnessTests.h:\n"
         << "// 16 bars of quarters, eighths and long notes, velocity 80, its own register, in its own\n"
         << "// place at the shipped defaults) as loud (BS.1770 integrated) as with the synthesis it\n"
         << "// replaces (the stand-in); the chord pad's woodwinds on its chords, with its Woodwinds macro\n"
         << "// against the macro at 0. `woodwindStandInTrims`: the same without the developer's recordings.\n"
         << "// 0 without the library.\n"
         << "#include <array>\n\nnamespace ls::inst {\n"
         << "inline constexpr std::array<float, 31> woodwindTrims { " << list(trims) << " };\n"
         << "inline constexpr std::array<float, 31> woodwindStandInTrims { " << list(standIns) << " };\n}\n";
    return juce::File::getCurrentWorkingDirectory().getChildFile(path).replaceWithText(text) ? 0 : 1;
}

// Each changed instrument: a phrase (a detached melody, a legato line, a long note swelled with
// the mod wheel, staccato repeats; the chord pad: its chords), before (the synthesis) and after
// (the real woodwinds), dry and in its place; one A/B file each; the cents and level report.
int listenWoodwinds(const juce::String& directory) {
    using namespace woodtest;
    const bool loaded = libraryLoaded();
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    int written = 0;
    const auto phrase = [](int instrument, bool solo, bool dry) {
        InstrumentHost h;
        h.instrument(instrument);
        if (dry) h.dry();
        h.cc(1, 0);
        h.render(0.2);
        if (solo) {
            const int up = std::string(ls::inst::patch(instrument).name) == "Pastoral Oboe" ? 0 : 12;
            // A detached melody (quarters, eighths, a long note).
            const int melody[] { 64, 67, 69, 67, 64, 72, 71 };
            const double beats[] { 1, 0.5, 0.5, 1, 1, 3, 1 };
            for (int k = 0; k < 7; ++k) {
                h.noteOn(melody[k] + up, 0.7f);
                h.render(beats[k] * 0.5 - 0.05);
                h.noteOff(melody[k] + up);
                h.render(0.05);
            }
            h.render(0.4);
            // A legato line (each note joining the last).
            const int legato[] { 69, 71, 72, 76, 74, 72, 69 };
            for (int k = 0; k < 7; ++k) {
                h.noteOn(legato[k] + up, 0.7f);
                if (k > 0) {
                    h.render(0.04);
                    h.noteOff(legato[k - 1] + up);
                }
                h.render(k == 6 ? 1.6 : 0.36);
            }
            h.noteOff(legato[6] + up);
            h.render(0.6);
            // A long note, swelled with the mod wheel and back.
            h.noteOn(72 + up, 0.6f);
            h.render(0.6);
            for (int v = 0; v <= 127; v += 2) { h.cc(1, v); h.render(0.025); }
            h.render(0.6);
            for (int v = 127; v >= 0; v -= 2) { h.cc(1, v); h.render(0.02); }
            h.render(0.4);
            h.noteOff(72 + up);
            h.render(0.6);
            // Staccato repeats, soft to loud.
            for (int k = 0; k < 8; ++k) {
                h.noteOn(67 + up + (k % 2) * 5, 0.3f + 0.09f * float(k));
                h.render(0.12);
                h.noteOff(67 + up + (k % 2) * 5);
                h.render(0.13);
            }
        } else {
            const std::array<std::array<int, 4>, 4> chords { { { 48, 55, 64, 72 }, { 45, 57, 64, 72 }, { 41, 53, 65, 69 }, { 43, 55, 62, 71 } } };
            for (const auto& c : chords) {
                for (int m : c) h.noteOn(m, 0.75f);
                h.render(2.4);
                for (int m : c) h.noteOff(m);
            }
        }
        h.render(dry ? 1.2 : 3.0);
        return std::make_pair(h.outL, h.outR);
    };
    for (const auto& w : woodwinds) {
        const int i = mixsong::instrumentNamed(w.name);
        const auto slug = juce::String(w.name).toLowerCase().replaceCharacter(' ', '-');
        for (const bool dry : { true, false }) {
            std::vector<float> L, R;
            {
                Before off;
                const auto [l, r] = phrase(i, w.solo, dry);
                L = l;
                R = r;
            }
            const auto [l2, r2] = phrase(i, w.solo, dry);
            const auto tag = slug + (dry ? "-dry" : "-in-place");
            written += writeWav(dir.getChildFile(tag + "-before.wav"), L, R, 48000.0) ? 1 : 0;
            written += writeWav(dir.getChildFile(tag + "-after.wav"), l2, r2, 48000.0) ? 1 : 0;
            std::vector<float> AL = L, AR = R;
            AL.resize(AL.size() + 48000, 0.0f);
            AR.resize(AR.size() + 48000, 0.0f);
            AL.insert(AL.end(), l2.begin(), l2.end());
            AR.insert(AR.end(), r2.begin(), r2.end());
            written += writeWav(dir.getChildFile(tag + "-before-then-after.wav"), AL, AR, 48000.0) ? 1 : 0;
            std::cout << w.name << (dry ? " dry" : " in its place") << ": before " << fmt(loudness::measure(L, R, 48000.0).integrated, 1) << " LUFS, after "
                      << fmt(loudness::measure(l2, r2, 48000.0).integrated, 1) << " LUFS\n";
        }
        if (loaded) {
            const auto t = tuning(i, w.low, w.high);
            std::cout << w.name << " tuning (cents, MIDI:cents): " << t.list << "(worst " << fmt(t.worst, 1) << " at " << t.at << ")\n";
        }
    }
    std::cout << written << " listening files written to " << dir.getFullPathName() << (loaded ? "" : " (the library was not loaded: before and after are the same)") << "\n";
    return written > 0 ? 0 : 1;
}

// Debugging: `--woodwind-level "<instrument>" note`: a held note's level (0.02-0.8 s, dry) per
// velocity: with everything, with Character at 0, and without the library's woodwinds.
int woodwindLevel(const juce::String& name, int note) {
    woodtest::libraryLoaded();
    const int i = mixsong::instrumentNamed(name.toRawUTF8());
    for (int mode = 0; mode < 3; ++mode) {
        lswb::samples::Bank::bypassWoodwindsForTests(mode == 2);
        std::cout << (mode == 0 ? "all:          " : mode == 1 ? "character 0:  " : "stand-in:     ");
        for (int k = 2; k <= 20; ++k) {
            InstrumentHost h;
            h.instrument(i);
            h.dry();
            if (mode == 1) h.set(dc::ids::instCharacter, 0.0f);
            h.render(0.1);
            h.noteOn(note, 0.05f * float(k));
            h.render(0.8);
            std::cout << fmt(toDb(h.rms(h.at(0.12), h.at(0.9))), 1) << " ";
            if (const char* dump = std::getenv("LS_WOODWIND_DUMP"))
                writeWav(juce::File(juce::String(dump) + "/level-" + juce::String(mode) + "-" + juce::String(k) + ".wav"), h.outL, h.outR, h.sr);
        }
        std::cout << "\n";
    }
    lswb::samples::Bank::bypassWoodwindsForTests(false);
    return 0;
}
