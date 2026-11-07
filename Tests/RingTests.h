// The celesta's softer ring (the developer, from GarageBand: the strike is better since the
// hotfix, but the ringing after it is too strong, clangy and metallic). Included by
// LightSeekerTests.cpp after LoudnessTests.h (it uses InstrumentHost, the loudness test's
// player, meter and parts, writeWav, toDb, fmt).
//   ring               the celesta family (Celesta, Celesta and Harp, Music Box) before the
//                      softer ring (`ls::inst::useRingBeforeForTests`) and now, dry and in its
//                      place, a note and a chord at velocities 0.3 / 0.7 / 1.0: the afterring
//                      (the level after the first 100 ms, and against the strike) and the
//                      tail's inharmonic energy clearly lower; the strike's sparkle kept;
//                      level-matched on the loudness test (its arpeggio in its place)
//   --ring-report      the same measurements as a table
//   --calibrate-ring   the family's trims, each as loud as before on the loudness test (printed)
//   --listen-ring dir  the celesta before vs after, alone (dry and in its place) and in the
//                      seven-part test piece

namespace ringtest {

inline const std::array<const char*, 3>& family() {
    static const std::array<const char*, 3> names { "Celesta", "Celesta and Harp", "Music Box" };
    return names;
}

struct Ring {
    double strikeDb = -200;      // RMS of the first 100 ms
    double afterDb = -200;       // RMS from 100 ms to 2 s (the note held): the afterring
    double afterRelDb = 0;       // afterDb - strikeDb
    double inharmonicDb = -200;  // the afterring's energy away from every note's harmonics (dB, as an RMS level)
    double inharmonicShareDb = 0;   // that against the afterring's whole energy
    double sparkleDb = -200;     // the strike's level above 5 kHz (first 60 ms; dB, as an RMS level): its sparkle
    double peakDb = -200;
    double outputLimiterDb = 0;   // the most the output's safety limiter took (dB)
};

constexpr double holdSeconds = 2.0, strikeSeconds = 0.1;

// Energy (power spectrum, summed over Hann frames) of a mono signal: harmonic (within
// max(3 bins, 4 % of the note) of a multiple of one of the notes) and the rest, 0.85 x the
// lowest note to 18 kHz.
inline void spectralSplit(const std::vector<float>& x, double sr, const std::vector<int>& notes, double& harmonic, double& other) {
    constexpr int order = 13, size = 1 << order, hop = size / 2;
    juce::dsp::FFT fft(order);
    std::vector<float> window(size), data(size * 2);
    for (int k = 0; k < size; ++k) window[std::size_t(k)] = float(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * k / size));
    std::vector<double> power(size / 2 + 1, 0.0);
    for (std::size_t at = 0; at + size <= x.size(); at += hop) {
        std::fill(data.begin(), data.end(), 0.0f);
        for (int k = 0; k < size; ++k) data[std::size_t(k)] = x[at + std::size_t(k)] * window[std::size_t(k)];
        fft.performRealOnlyForwardTransform(data.data(), true);
        for (int k = 0; k <= size / 2; ++k) {
            const double re = data[std::size_t(2 * k)], im = data[std::size_t(2 * k + 1)];
            power[std::size_t(k)] += re * re + im * im;
        }
    }
    const double bin = sr / size;
    double lowest = 1.0e9;
    std::vector<double> f0s;
    for (int n : notes) { f0s.push_back(440.0 * std::pow(2.0, (n - 69) / 12.0)); lowest = std::min(lowest, f0s.back()); }
    harmonic = other = 0;
    for (int k = 1; k <= size / 2; ++k) {
        const double f = k * bin;
        if (f < 0.85 * lowest || f > 18000.0) continue;
        bool onHarmonic = false;
        for (double f0 : f0s) {
            const double h = std::max(1.0, std::round(f / f0));
            if (std::abs(f - h * f0) <= std::max(3.0 * bin, 0.04 * f0)) { onHarmonic = true; break; }
        }
        (onHarmonic ? harmonic : other) += power[std::size_t(k)];
    }
}

inline double energyAbove(const std::vector<float>& x, double sr, double hz, double& total) {
    // A 4th-order Butterworth-ish high-pass: two cascaded biquads (RBJ, Q 0.54 / 1.31).
    double above = 0;
    total = 0;
    const double qs[2] = { 0.5412, 1.3066 };
    std::vector<double> y(x.begin(), x.end());
    for (double q : qs) {
        const double w = 2.0 * juce::MathConstants<double>::pi * hz / sr, c = std::cos(w), al = std::sin(w) / (2.0 * q);
        const double a0 = 1 + al, b0 = (1 + c) / 2 / a0, b1 = -(1 + c) / a0, b2 = (1 + c) / 2 / a0, a1 = -2 * c / a0, a2 = (1 - al) / a0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (auto& v : y) {
            const double o = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = v; y2 = y1; y1 = o; v = o;
        }
    }
    for (std::size_t k = 0; k < x.size(); ++k) { above += y[k] * y[k]; total += double(x[k]) * x[k]; }
    return above;
}

// One note or chord held for 2 s, dry or in its place, at the instrument's defaults.
inline Ring measure(const char* name, float velocity, const std::vector<int>& notes, bool dry, std::vector<float>* keep = nullptr) {
    InstrumentHost h;
    h.instrument(attacktest::indexNamed(name));
    if (dry) h.dry();
    const double settle = dry ? 0.4 : loudness::settleSeconds;
    h.render(settle);
    for (int n : notes) h.noteOn(n, velocity);
    h.render(holdSeconds);
    for (int n : notes) h.noteOff(n);
    h.render(0.5);
    const auto onset = h.at(settle);
    Ring r;
    r.strikeDb = toDb(h.rms(onset, onset + h.at(strikeSeconds)));
    r.afterDb = toDb(h.rms(onset + h.at(strikeSeconds), onset + h.at(holdSeconds)));
    r.afterRelDb = r.afterDb - r.strikeDb;
    r.peakDb = toDb(h.peak(onset, h.end()));
    r.outputLimiterDb = toDb(h.p->outputLimiterLowestForTest());
    std::vector<float> tail, strike;
    for (auto k = onset + h.at(strikeSeconds); k < onset + h.at(holdSeconds); ++k) tail.push_back(0.5f * (h.outL[k] + h.outR[k]));
    for (auto k = onset; k < onset + h.at(0.06); ++k) strike.push_back(0.5f * (h.outL[k] + h.outR[k]));
    double harmonic = 0, other = 0;
    spectralSplit(tail, h.sr, notes, harmonic, other);
    r.inharmonicShareDb = 10.0 * std::log10(std::max(other, 1.0e-30) / std::max(harmonic + other, 1.0e-30));
    r.inharmonicDb = r.afterDb + r.inharmonicShareDb;
    double total = 0;
    const double above = energyAbove(strike, h.sr, 5000.0, total);
    r.sparkleDb = 10.0 * std::log10(std::max(above, 1.0e-30) / double(std::max<std::size_t>(1, strike.size())));
    juce::ignoreUnused(total);
    if (const auto dump = juce::SystemStats::getEnvironmentVariable("LS_RING_DUMP", {}); dump.isNotEmpty())
        writeWav(juce::File(dump).getChildFile(juce::String(name).replaceCharacter(' ', '-') + (ls::inst::ringBeforeForTests() ? "-before-" : "-now-") + juce::String(velocity, 1) + "-"
                                                 + juce::String(int(notes.size())) + (dry ? "-dry" : "-place") + ".wav"), h.outL, h.outR, h.sr);
    if (keep != nullptr) {
        keep->assign(h.outL.begin() + std::ptrdiff_t(onset), h.outL.end());
    }
    return r;
}

struct Case { int instrument; float velocity; int voicing; bool dry; };   // voicing 0: C6, 1: C5-E5-G5
inline const std::vector<int>& voicingNotes(int v) {
    static const std::vector<int> note { 84 }, chord { 72, 76, 79 };
    return v == 0 ? note : chord;
}
inline std::string caseName(const Case& c) {
    return std::string(family()[std::size_t(c.instrument)]) + (c.voicing == 0 ? ", C6" : ", C5-E5-G5") + " at " + fmt(c.velocity, 1) + (c.dry ? ", dry" : ", in its place");
}
inline std::vector<Case> cases() {
    std::vector<Case> out;
    for (int i = 0; i < 3; ++i)
        for (bool dry : { true, false })
            for (int v = 0; v < 2; ++v)
                for (float vel : { 0.3f, 0.7f, 1.0f }) out.push_back({ i, vel, v, dry });
    return out;
}

// Every case before (the celesta family as it was) and now, rendered on several threads.
inline void measureAll(std::vector<Ring>& before, std::vector<Ring>& now) {
    const auto all = cases();
    for (int pass = 0; pass < 2; ++pass) {
        ls::inst::useRingBeforeForTests(pass == 0);
        auto& out = pass == 0 ? before : now;
        out.assign(all.size(), {});
        std::atomic<std::size_t> next { 0 };
        std::vector<std::thread> pool;
        const int threads = juce::jlimit(1, 8, int(std::thread::hardware_concurrency()));
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&] {
                for (auto k = next++; k < all.size(); k = next++)
                    out[k] = measure(family()[std::size_t(all[k].instrument)], all[k].velocity, voicingNotes(all[k].voicing), all[k].dry);
            });
        for (auto& t : pool) t.join();
    }
    ls::inst::useRingBeforeForTests(false);
}

// The loudness test's role part (the arpeggio) in its place, before and now.
inline void roleLoudness(std::array<double, 3>& before, std::array<double, 3>& now) {
    for (int pass = 0; pass < 2; ++pass) {
        ls::inst::useRingBeforeForTests(pass == 0);
        auto& out = pass == 0 ? before : now;
        std::vector<std::thread> pool;
        for (int i = 0; i < 3; ++i)
            pool.emplace_back([&, i] {
                const int index = attacktest::indexNamed(family()[std::size_t(i)]);
                out[std::size_t(i)] = loudness::loudnessOf(loudness::playPart(index, loudness::roleOf(index).part)).integrated;
            });
        for (auto& t : pool) t.join();
    }
    ls::inst::useRingBeforeForTests(false);
}

// How much lower each must be now (dB; negative: how much higher it may be). The celesta clearly;
// Celesta and Harp shares its bars (its tail is mostly its harp and the chapel's glints): a little
// lower, never more inharmonic; the Music Box was checked and is unchanged (its tail is its own
// tines, harmonic). The inharmonic share is gated dry only: in its place the share of a chord's
// tail can rise while its inharmonic energy falls (the harmonic ring falls more, the place's
// own glints stay). The sparkle: the strike's level above 5 kHz, at most this much lower.
struct Gate { double after, afterRel, inharmonic, shareDry, sparkleDry, sparklePlace, strike; };
inline Gate gateOf(int instrument) {
    // The celesta in its place: a chord's highs were mostly the vault's crystal resonator driven
    // to the output limiter (-0.3 dBFS: the clang); 30 % of it now (MIX Character brings more).
    if (instrument == 0) return { 1.5, 3.5, 2.0, 2.5, 3.5, 7.5, 3.0 };
    if (instrument == 1) return { 0.3, 0.5, -0.5, -0.6, 1.0, 4.5, 1.0 };
    return { -0.1, -0.1, -0.1, -0.1, 0.1, 0.1, 0.1 };
}
} // namespace ringtest

int ringReport() {
    using namespace ringtest;
    samplesLoaded();
    std::cout << "recordings: " << lswb::samples::Bank::get().message() << "\n";
    std::cout << "captured spaces: " << (dc::Vault::capturedSpacesBuiltIn() ? "yes" : "no (fallbacks)") << "\n";
    std::vector<Ring> before, now;
    measureAll(before, now);
    const auto all = cases();
    std::cout << "case | strike (dB) | afterring after 100 ms (dB) | afterring vs strike (dB) | inharmonic in the afterring (dB) | inharmonic share (dB) | sparkle (dB) | peak (dBFS)   [before -> now]\n";
    for (std::size_t k = 0; k < all.size(); ++k) {
        const auto& b = before[k];
        const auto& n = now[k];
        std::cout << caseName(all[k]) << " | " << fmt(b.strikeDb) << " -> " << fmt(n.strikeDb) << " | " << fmt(b.afterDb) << " -> " << fmt(n.afterDb) << " ("
                  << fmt(n.afterDb - b.afterDb) << ") | " << fmt(b.afterRelDb) << " -> " << fmt(n.afterRelDb) << " | " << fmt(b.inharmonicDb) << " -> " << fmt(n.inharmonicDb)
                  << " (" << fmt(n.inharmonicDb - b.inharmonicDb) << ") | " << fmt(b.inharmonicShareDb) << " -> " << fmt(n.inharmonicShareDb) << " | " << fmt(b.sparkleDb)
                  << " -> " << fmt(n.sparkleDb) << " | " << fmt(b.peakDb) << " -> " << fmt(n.peakDb) << "\n";
    }
    std::array<double, 3> lb {}, ln {};
    roleLoudness(lb, ln);
    for (int i = 0; i < 3; ++i)
        std::cout << family()[std::size_t(i)] << ", the loudness test's arpeggio in its place: " << fmt(lb[std::size_t(i)], 2) << " -> " << fmt(ln[std::size_t(i)], 2) << " LUFS\n";
    return 0;
}

// `--calibrate-ring`: the celesta family's trims (with the recordings and without them: the
// stand-in trims) so that each plays the loudness test's arpeggio in its place exactly as loud as
// before the softer ring; printed for Source/Synth/InstrumentTrims.h.
int calibrateRing() {
    using namespace ringtest;
    samplesLoaded();
    std::cout << "recordings: " << lswb::samples::Bank::get().message() << "\n";
    for (int pass = 0; pass < 2; ++pass) {
        lswb::samples::Bank::bypassRecordingsForTests(pass == 1);
        std::array<double, 3> target {}, unused {};
        roleLoudness(target, unused);
        ls::inst::useRingBeforeForTests(false);
        for (int i = 0; i < 3; ++i) {
            const int index = attacktest::indexNamed(family()[std::size_t(i)]);
            const auto& p = ls::inst::patch(index);
            if (pass == 1 && !ls::inst::patchUsesRecordings(p)) continue;
            float trim = pass == 0 ? p.trimDb : p.standInTrimDb;
            for (int round = 0; round < 5; ++round) {
                ls::inst::setTrimForTests(index, trim, pass == 1);
                const double level = loudness::loudnessOf(loudness::playPart(index, loudness::roleOf(index).part)).integrated;
                const double error = level - target[std::size_t(i)];
                std::cout << (pass == 0 ? "" : "stand-in ") << family()[std::size_t(i)] << " round " << round << ": " << fmt(level, 2) << " LUFS (before "
                          << fmt(target[std::size_t(i)], 2) << "), trim " << fmt(trim) << "\n";
                if (std::abs(error) <= 0.05) break;
                trim = float(std::round((trim - error) * 10.0) / 10.0);
            }
            std::cout << "=> " << (pass == 0 ? "trim " : "stand-in trim ") << family()[std::size_t(i)] << " " << fmt(trim) << "\n";
        }
    }
    lswb::samples::Bank::bypassRecordingsForTests(false);
    return 0;
}

void testRing(Suite& s) {
    using namespace ringtest;
    s.section("ring: the celesta's softer ring (less afterring, less inharmonic metal in the tail; sparkle and level kept)");
    samplesLoaded();
    s.check(!ls::inst::ringBeforeForTests(), "ring: the current celesta family plays by default");
    {
        const auto& p = ls::inst::patch(attacktest::indexNamed("Celesta"));
        s.check(p.spaceCharacter < 0.5f && p.spaceRing < 1.0f && p.spaceShimmer < 1.0f, "ring: the Celesta takes less of the Diamond Vault's crystal modes, their ring and its shimmer",
                fmt(p.spaceCharacter, 2) + " / " + fmt(p.spaceRing, 2) + " / " + fmt(p.spaceShimmer, 2));
    }
    std::vector<Ring> before, now;
    measureAll(before, now);
    const auto all = cases();
    std::array<double, 3> leastAfter {}, leastInharmonic {}, leastShare {};
    leastAfter.fill(100);
    leastInharmonic.fill(100);
    leastShare.fill(100);
    for (std::size_t k = 0; k < all.size(); ++k) {
        const auto& c = all[k];
        const auto& b = before[k];
        const auto& n = now[k];
        const auto g = gateOf(c.instrument);
        const auto name = caseName(c);
        const auto I = std::size_t(c.instrument);
        leastAfter[I] = std::min(leastAfter[I], b.afterDb - n.afterDb);
        leastInharmonic[I] = std::min(leastInharmonic[I], b.inharmonicDb - n.inharmonicDb);
        leastShare[I] = std::min(leastShare[I], b.inharmonicShareDb - n.inharmonicShareDb);
        const bool same = c.instrument == 2;
        s.check(n.afterDb <= b.afterDb - g.after, "ring: " + name + (same ? ": the afterring (after 100 ms) is unchanged" : ": the afterring (after 100 ms) is lower"),
                fmt(b.afterDb) + " -> " + fmt(n.afterDb) + " dB");
        s.check(n.afterRelDb <= b.afterRelDb - g.afterRel, "ring: " + name + (same ? ": the afterring against the strike is unchanged" : ": the afterring against the strike is lower"),
                fmt(b.afterRelDb) + " -> " + fmt(n.afterRelDb) + " dB");
        s.check(n.inharmonicDb <= b.inharmonicDb - g.inharmonic,
                "ring: " + name + (c.instrument == 0 ? ": less inharmonic energy in the tail" : ": no more inharmonic energy in the tail"), fmt(b.inharmonicDb) + " -> " + fmt(n.inharmonicDb) + " dB");
        if (c.dry)
            s.check(n.inharmonicShareDb <= b.inharmonicShareDb - g.shareDry, "ring: " + name + (c.instrument == 0 ? ": a smaller share of the tail is inharmonic" : ": no larger share of the tail is inharmonic"),
                    fmt(b.inharmonicShareDb) + " -> " + fmt(n.inharmonicShareDb) + " dB");
        // The sparkle (the strike's highs) and the strike itself stay.
        s.check(n.sparkleDb >= b.sparkleDb - (c.dry ? g.sparkleDry : g.sparklePlace), "ring: " + name + ": the strike's sparkle is kept", fmt(b.sparkleDb) + " -> " + fmt(n.sparkleDb) + " dB");
        s.check(n.strikeDb >= b.strikeDb - g.strike, "ring: " + name + ": the strike keeps its level", fmt(b.strikeDb) + " -> " + fmt(n.strikeDb) + " dB");
        s.check(n.peakDb <= -0.29, "ring: " + name + ": no clipping", fmt(n.peakDb) + " dBFS");
        // Below full velocity the output's safety limiter is never needed (the clang); at full
        // velocity no more than it was.
        if (c.velocity > 0.99f && c.instrument == 0) s.metric(name + " output limiter", n.outputLimiterDb, " dB");
        s.check(c.velocity < 0.99f ? n.outputLimiterDb > -0.01 : n.outputLimiterDb >= std::min(b.outputLimiterDb, -0.1) - 0.3,
                "ring: " + name + ": the output limiter is not needed more than before", fmt(b.outputLimiterDb, 2) + " -> " + fmt(n.outputLimiterDb, 2) + " dB");
    }
    for (int i = 0; i < 3; ++i) {
        const std::string name = family()[std::size_t(i)];
        s.metric(name + " afterring lower by (least)", leastAfter[std::size_t(i)], " dB");
        s.metric(name + " inharmonic tail lower by (least)", leastInharmonic[std::size_t(i)], " dB");
        s.metric(name + " inharmonic share lower by (least)", leastShare[std::size_t(i)], " dB");
    }
    // Level-matched on the loudness test: the arpeggio in its place, as loud as before and on the target.
    std::array<double, 3> lb {}, ln {};
    roleLoudness(lb, ln);
    for (int i = 0; i < 3; ++i) {
        const std::string name = family()[std::size_t(i)];
        s.check(std::abs(ln[std::size_t(i)] - lb[std::size_t(i)]) <= 0.5, "ring: " + name + " is level-matched with before on the loudness test (its arpeggio in its place)",
                fmt(lb[std::size_t(i)], 2) + " -> " + fmt(ln[std::size_t(i)], 2) + " LUFS");
        s.check(std::abs(ln[std::size_t(i)] - loudness::targetLufs) <= loudness::tolerance, "ring: " + name + " is on the loudness target", fmt(ln[std::size_t(i)], 2) + " LUFS");
    }
    // Without the recordings (the piano under the bars is not there) it still plays, and rings softer.
    {
        lswb::samples::Bank::bypassRecordingsForTests(true);
        ls::inst::useRingBeforeForTests(true);
        const auto b = measure("Celesta", 0.7f, voicingNotes(1), true);
        ls::inst::useRingBeforeForTests(false);
        const auto n = measure("Celesta", 0.7f, voicingNotes(1), true);
        lswb::samples::Bank::bypassRecordingsForTests(false);
        s.check(n.strikeDb > -60.0 && n.afterDb <= b.afterDb - 2.0 && n.inharmonicDb <= b.inharmonicDb - 1.5,
                "ring: without the recordings the Celesta sounds and rings softer too", fmt(b.afterDb) + " -> " + fmt(n.afterDb) + " dB, inharmonic " + fmt(b.inharmonicDb) + " -> " + fmt(n.inharmonicDb));
    }
}

// `--listen-ring dir`: the celesta before (its ring as it was) and now. Alone: a phrase (an
// arpeggio, chords left to ring, a melody with long notes, soft to loud), dry and in the
// Diamond Vault; and in the seven-part test piece (harp arpeggios, a strings pad, a brass
// fanfare, choir, a flute melody, a kalimba ostinato, an organ bass) with the celesta's line
// on top, every instrument in its own place at its defaults, no fader moved.
int listenRing(const juce::String& directory) {
    using namespace loudness;
    samplesLoaded();
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    int written = 0;
    const int celesta = attacktest::indexNamed("Celesta");
    const auto name = [](const juce::String& s) { return s.toLowerCase().replaceCharacter(' ', '-'); };
    // The phrase alone (velocity 80, the loudness test's).
    std::vector<Note> phrase;
    {
        const auto arp = notesOf(Arpeggio, 12);
        for (const auto& n : arp)
            if (n.at < 16.0) phrase.push_back(n);
        // Chords left to ring (two beats each), then a melody with long notes.
        const std::array<std::array<int, 3>, 4> chords { { { 72, 76, 79 }, { 69, 72, 76 }, { 65, 69, 72 }, { 67, 71, 74 } } };
        for (int c = 0; c < 4; ++c)
            for (int n : chords[std::size_t(c)]) phrase.push_back({ 16.0 + 2.0 * c, 2.0, n });
        const std::vector<std::pair<int, double>> melody { { 84, 1 }, { 88, 1 }, { 86, 2 }, { 84, 1 }, { 79, 1 }, { 81, 4 } };
        double t = 24.0;
        for (const auto& [n, beats] : melody) { phrase.push_back({ t, beats - 0.1, n }); t += beats; }
    }
    std::vector<float> ab[2][2];   // [dry/in place][channel]
    for (const bool before : { true, false }) {
        ls::inst::useRingBeforeForTests(before);
        for (int dry = 0; dry < 2; ++dry) {
            Options o;
            if (dry == 1) o.intensity = 0.0f;
            const auto r = play(celesta, phrase, o);
            const auto m = loudnessOf(r);
            const juce::String file = juce::String("celesta-") + (dry == 1 ? "dry-" : "in-place-") + (before ? "before" : "after") + ".wav";
            std::cout << file << ": " << fmt(m.integrated) << " LUFS, peak " << fmt(m.peakDb) << " dBFS\n";
            written += writeWav(dir.getChildFile(file), r.L, r.R, r.sr) ? 1 : 0;
            auto& L = ab[dry][0];
            auto& R = ab[dry][1];
            L.insert(L.end(), r.L.begin(), r.L.end());
            R.insert(R.end(), r.R.begin(), r.R.end());
            if (before) { L.insert(L.end(), 48000, 0.0f); R.insert(R.end(), 48000, 0.0f); }
        }
    }
    ls::inst::useRingBeforeForTests(false);
    written += writeWav(dir.getChildFile("celesta-dry-before-then-after.wav"), ab[1][0], ab[1][1], 48000.0) ? 1 : 0;
    written += writeWav(dir.getChildFile("celesta-in-place-before-then-after.wav"), ab[0][0], ab[0][1], 48000.0) ? 1 : 0;

    // The seven-part piece (16 bars at 120 bpm, the test set's chords) with the celesta on top.
    struct Track { const char* name; std::vector<Note> notes; };
    std::vector<Track> tracks;
    tracks.push_back({ "Concert Harp", notesOf(Arpeggio, 0) });
    tracks.push_back({ "Shimmering Strings", notesOf(Chords, 0) });
    {
        std::vector<Note> call;
        for (int c = 0; c < 8; ++c) {
            const auto& v = progression[std::size_t(c)];
            const double t = c * 8.0;
            call.push_back({ t, 0.9, v[0] + 12 });
            call.push_back({ t + 1.0, 0.4, v[2] + 12 });
            call.push_back({ t + 1.5, 0.4, v[2] + 12 });
            call.push_back({ t + 2.0, 1.9, v[1] + 12 });
        }
        tracks.push_back({ "Fanfare Trumpets", call });
    }
    {
        std::vector<Note> choir;
        for (int c = 0; c < 8; ++c)
            for (int k = 1; k < 4; ++k) choir.push_back({ c * 8.0, 8.0, progression[std::size_t(c)][std::size_t(k)] + 12 });
        tracks.push_back({ "Angelic Choir", choir });
    }
    tracks.push_back({ "Airy Flute", notesOf(Melody, 12) });
    {   // The kalimba: an ostinato (root, fifth, octave, fifth in eighths) an octave up.
        std::vector<Note> ost;
        for (int c = 0; c < 8; ++c) {
            const auto& v = progression[std::size_t(c)];
            const std::array<int, 4> cell { v[0] + 12, v[2] + 12, v[3] + 12, v[2] + 12 };
            for (int k = 0; k < 16; ++k) ost.push_back({ c * 8.0 + 0.5 * k, 0.45, cell[std::size_t(k % 4)] });
        }
        tracks.push_back({ "Bouncy Kalimba", ost });
    }
    {   // The bass: the organ on the roots an octave down, a half note and two quarters a bar.
        std::vector<Note> bass;
        for (int c = 0; c < 8; ++c) {
            const int root = progression[std::size_t(c)][0] - 12;
            for (int bar = 0; bar < 2; ++bar) {
                const double t = c * 8.0 + bar * 4.0;
                bass.push_back({ t, 1.9, root });
                bass.push_back({ t + 2.0, 0.9, root });
                bass.push_back({ t + 3.0, 0.9, root + 7 });
            }
        }
        tracks.push_back({ "Grand Organ", bass });
    }
    // The celesta: chord tones two octaves up, a little figure each bar ending on a note left to ring.
    std::vector<Note> line;
    for (int c = 0; c < 8; ++c) {
        const auto& v = progression[std::size_t(c)];
        const double t = c * 8.0;
        line.push_back({ t, 1.0, v[1] + 24 });
        line.push_back({ t + 1.5, 0.5, v[2] + 24 });
        line.push_back({ t + 2.0, 1.0, v[3] + 24 });
        line.push_back({ t + 3.0, 1.0, v[2] + 24 });
        line.push_back({ t + 4.0, 0.5, v[3] + 24 });
        line.push_back({ t + 4.5, 0.5, v[1] + 24 + 12 });
        line.push_back({ t + 5.0, 3.0, v[2] + 24 });
    }
    std::vector<Render> others(tracks.size());
    {
        std::vector<std::thread> pool;
        for (std::size_t t = 0; t < tracks.size(); ++t)
            pool.emplace_back([&, t] { others[t] = play(mixsong::instrumentNamed(tracks[t].name), tracks[t].notes, { -1, -1, -1, -1, 4.0 }); });
        for (auto& th : pool) th.join();
    }
    std::vector<float> both[2];
    for (const bool before : { true, false }) {
        ls::inst::useRingBeforeForTests(before);
        const auto cel = play(celesta, line, { -1, -1, -1, -1, 4.0 });
        ls::inst::useRingBeforeForTests(false);
        std::vector<float> L, R;
        const auto add = [&](const Render& r) {
            if (L.size() < r.L.size()) { L.resize(r.L.size(), 0.0f); R.resize(r.R.size(), 0.0f); }
            for (std::size_t k = 0; k < r.L.size(); ++k) { L[k] += 0.5f * r.L[k]; R[k] += 0.5f * r.R[k]; }
        };
        for (const auto& r : others) add(r);
        add(cel);
        const auto mc = loudnessOf(cel);
        const auto m = measure(L, R, 48000.0);
        std::cout << (before ? "before" : "after") << ": the celesta's line " << fmt(mc.integrated) << " LUFS; the piece " << fmt(m.integrated) << " LUFS, peak " << fmt(m.peakDb) << " dBFS\n";
        written += writeWav(dir.getChildFile(juce::String("piece-celesta-line-") + (before ? "before" : "after") + ".wav"), cel.L, cel.R, cel.sr) ? 1 : 0;
        written += writeWav(dir.getChildFile(juce::String("piece-") + (before ? "before" : "after") + ".wav"), L, R, 48000.0) ? 1 : 0;
        both[0].insert(both[0].end(), L.begin(), L.end());
        both[1].insert(both[1].end(), R.begin(), R.end());
        if (before) { both[0].insert(both[0].end(), 48000, 0.0f); both[1].insert(both[1].end(), 48000, 0.0f); }
    }
    for (std::size_t t = 0; t < tracks.size(); ++t)
        written += writeWav(dir.getChildFile("piece-" + name(tracks[t].name) + ".wav"), others[t].L, others[t].R, others[t].sr) ? 1 : 0;
    written += writeWav(dir.getChildFile("piece-before-then-after.wav"), both[0], both[1], 48000.0) ? 1 : 0;
    std::cout << written << " listening files written to " << dir.getFullPathName() << "\n";
    return written > 0 ? 0 : 1;
}
