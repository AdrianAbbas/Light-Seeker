// The loudness rebalance: every instrument as loud as the others, measured as it is used.
// Included by LightSeekerTests.cpp after MixSong.h (it uses InstrumentHost, mixsong, writeWav,
// toDb, fmt).
//
// The level-match test before this measured a dry three-note chord's loudest 400 ms: right for
// a pad, wrong for a harp (whose loudest 400 ms is its strike) and blind to the place's space,
// the shimmer, the Punch and a pad's long tails piling up under legato chords. In a seven-part
// song the developer had to turn Shimmering Strings down by about 13 dB to sit level with the
// Concert Harp. This replaces it:
//   - a standard test set, played by every instrument in its own place at the shipped defaults
//     (Intensity, MIX, Shimmer, Punch, Atmosphere: the whole chain), velocity 80, 120 bpm, in
//     the instrument's own register: (a) sustained four-note chords, two bars each, legato, for
//     16 bars; (b) a melody of short and long notes, 16 bars; (c) an eighth-note arpeggio over
//     the same chords, 16 bars;
//   - integrated loudness (ITU-R BS.1770-4: K-weighting, 400 ms blocks, absolute and relative
//     gates) and the short-term maximum (3 s) of each part;
//   - each instrument is matched on the part that suits its role: pads, choirs, strings, organ
//     and horn sections on the chords, harps, plucks and bells on the arpeggio, the solo winds
//     and brass on the melody, so a pad and a harp playing their own parts come out equally loud.
//
//   loudness                     the gate (+-1.5 LU of the target on the role's part, recordings
//                                and stand-ins), the pads' build-up, the space's and shimmer's
//                                share on sustained chords, siblings level with each other in the
//                                same place, the brass with its Punch, no clipping at the defaults
//   --loudness-report [before]   the table (every instrument, every part); "before": the trims
//                                before the rebalance
//   --calibrate-instruments f    writes the trims (Source/Synth/InstrumentTrims.h)
//   --listen-loudness dir        a short mix (harp arpeggios, strings pad, brass, choir, flute),
//                                every instrument at its defaults, no fader moved: before vs after

namespace loudness {
constexpr double targetLufs = -20.0;   // every instrument's role part, integrated
constexpr double tolerance = 1.5;      // LU either way
constexpr float velocity = 80.0f / 127.0f;
constexpr double bpm = 120.0, beat = 60.0 / bpm;
constexpr double settleSeconds = 6.0;  // in the place before the first note (see the Hotfixes' lesson)

//==============================================================================
// ITU-R BS.1770-4. The K-weighting (a high shelf, then the RLB high-pass) at any rate, from
// the standard's analogue prototypes (as libebur128); identical to the standard's 48 kHz
// coefficients at 48 kHz.
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    double run(double x) noexcept {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
std::array<Biquad, 2> kWeighting(double sr) {
    std::array<Biquad, 2> k;
    {
        const double f0 = 1681.974450955533, gain = 3.999843853973347, q = 0.7071752369554196;
        const double K = std::tan(juce::MathConstants<double>::pi * f0 / sr);
        const double vh = std::pow(10.0, gain / 20.0), vb = std::pow(vh, 0.4996667741545416);
        const double a0 = 1.0 + K / q + K * K;
        k[0] = { (vh + vb * K / q + K * K) / a0, 2.0 * (K * K - vh) / a0, (vh - vb * K / q + K * K) / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / q + K * K) / a0 };
    }
    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const double K = std::tan(juce::MathConstants<double>::pi * f0 / sr);
        const double a0 = 1.0 + K / q + K * K;
        k[1] = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / q + K * K) / a0 };
    }
    return k;
}

struct Loudness {
    double integrated = -120, shortTermMax = -120, momentaryMax = -120, peakDb = -120;
    std::vector<double> shortTerm;   // every 100 ms (3 s windows ending there)
};

double lufsOf(double meanSquare) { return meanSquare > 0 ? -0.691 + 10.0 * std::log10(meanSquare) : -120.0; }

// L and R (weight 1 each), from `from` to `to` (seconds; to < 0: the end).
Loudness measure(const std::vector<float>& L, const std::vector<float>& R, double sr, double from = 0, double to = -1) {
    Loudness out;
    const auto a = std::min(L.size(), static_cast<std::size_t>(std::max(0.0, from) * sr));
    const auto b = to < 0 ? L.size() : std::min(L.size(), static_cast<std::size_t>(to * sr));
    auto kl = kWeighting(sr), kr = kWeighting(sr);
    const auto step = static_cast<std::size_t>(std::lround(0.1 * sr));
    std::vector<double> segments;   // sum over the channels of each 100 ms segment's mean square
    double sum = 0;
    std::size_t count = 0;
    double peak = 0;
    for (std::size_t i = a; i < b; ++i) {
        const double l = kl[1].run(kl[0].run(L[i])), r = kr[1].run(kr[0].run(R[i]));
        peak = std::max({ peak, std::abs(double(L[i])), std::abs(double(R[i])) });
        sum += l * l + r * r;
        if (++count == step) { segments.push_back(sum / double(step)); sum = 0; count = 0; }
    }
    out.peakDb = toDb(peak);
    const auto window = [&](std::size_t end, std::size_t n) {
        double s = 0;
        for (std::size_t k = end + 1 - n; k <= end; ++k) s += segments[k];
        return s / double(n);
    };
    std::vector<double> blocks;   // 400 ms, 75 % overlap
    for (std::size_t e = 3; e < segments.size(); ++e) {
        blocks.push_back(window(e, 4));
        out.momentaryMax = std::max(out.momentaryMax, lufsOf(blocks.back()));
    }
    for (std::size_t e = 29; e < segments.size(); ++e) {
        out.shortTerm.push_back(lufsOf(window(e, 30)));
        out.shortTermMax = std::max(out.shortTermMax, out.shortTerm.back());
    }
    // Gating: absolute (-70 LUFS), then relative (10 LU under the absolutely gated mean).
    double s1 = 0;
    int n1 = 0;
    for (double z : blocks) if (lufsOf(z) > -70.0) { s1 += z; ++n1; }
    if (n1 == 0) return out;
    const double relative = lufsOf(s1 / n1) - 10.0;
    double s2 = 0;
    int n2 = 0;
    for (double z : blocks) if (lufsOf(z) > -70.0 && lufsOf(z) > relative) { s2 += z; ++n2; }
    out.integrated = n2 > 0 ? lufsOf(s2 / n2) : -120.0;
    return out;
}

//==============================================================================
// The test set. 120 bpm, 4/4, 16 bars each; C - Am - F - G - C - Em - F - G, two bars a chord.
enum Part { Chords = 0, Melody = 1, Arpeggio = 2 };
const char* partName(int p) { return p == Chords ? "chords" : p == Melody ? "melody" : "arpeggio"; }

struct Note { double at, length; int note; };   // beats

// Four-note chords (root, third, fifth, octave) from C3 to A4.
const std::array<std::array<int, 4>, 8> progression { {
    { 48, 52, 55, 60 }, { 57, 60, 64, 69 }, { 53, 57, 60, 65 }, { 55, 59, 62, 67 },
    { 48, 52, 55, 60 }, { 52, 55, 59, 64 }, { 53, 57, 60, 65 }, { 55, 59, 62, 67 },
} };

std::vector<Note> notesOf(int part, int shift) {
    std::vector<Note> notes;
    if (part == Chords) {
        // Legato: each chord's notes end exactly where the next chord's begin.
        for (int c = 0; c < 8; ++c)
            for (int n : progression[std::size_t(c)]) notes.push_back({ c * 8.0, 8.0, n + shift });
    } else if (part == Arpeggio) {
        // Up two octaves and back, an eighth each, each note held for its eighth.
        for (int c = 0; c < 8; ++c) {
            const auto& v = progression[std::size_t(c)];
            const std::array<int, 16> run { v[0], v[1], v[2], v[3], v[1] + 12, v[2] + 12, v[3] + 12, v[2] + 12,
                                            v[1] + 12, v[3], v[2], v[1], v[0], v[1], v[2], v[3] };
            for (int k = 0; k < 16; ++k) notes.push_back({ c * 8.0 + 0.5 * k, 0.5, run[std::size_t(k)] + shift });
        }
    } else {
        // Two-bar phrases: quarters and eighths, then a dotted half or a whole note (E4-G5).
        struct M { int note; double beats; };
        const std::vector<std::vector<M>> phrases {
            { { 64, 1 }, { 67, 0.5 }, { 69, 0.5 }, { 67, 1 }, { 64, 1 }, { 72, 3 }, { 71, 1 } },
            { { 69, 0.5 }, { 71, 0.5 }, { 72, 0.5 }, { 76, 0.5 }, { 74, 1 }, { 72, 1 }, { 69, 4 } },
            { { 65, 1 }, { 69, 0.5 }, { 72, 0.5 }, { 77, 1 }, { 76, 1 }, { 72, 3 }, { 69, 1 } },
            { { 67, 0.5 }, { 69, 0.5 }, { 71, 0.5 }, { 74, 0.5 }, { 79, 1 }, { 77, 1 }, { 74, 4 } },
            { { 76, 1 }, { 74, 0.5 }, { 72, 0.5 }, { 74, 1 }, { 76, 1 }, { 79, 3 }, { 76, 1 } },
            { { 76, 0.5 }, { 74, 0.5 }, { 71, 0.5 }, { 67, 0.5 }, { 71, 1 }, { 74, 1 }, { 76, 4 } },
            { { 77, 1 }, { 76, 0.5 }, { 74, 0.5 }, { 72, 1 }, { 69, 1 }, { 72, 3 }, { 74, 1 } },
            { { 71, 0.5 }, { 72, 0.5 }, { 74, 0.5 }, { 77, 0.5 }, { 76, 1 }, { 74, 1 }, { 72, 4 } },
        };
        double t = 0;
        for (const auto& phrase : phrases)
            for (const auto& m : phrase) {
                // Slightly detached, as played (the long notes nearly legato).
                notes.push_back({ t, m.beats - (m.beats < 1.5 ? 0.1 : 0.05), m.note + shift });
                t += m.beats;
            }
    }
    return notes;
}
constexpr double partBeats = 64.0;

// Each instrument's role (the part it is matched on) and its register (semitones from the
// test set's: the bells, celesta, kalimba, pizzicato and fairy harp an octave up, the
// glockenspiel two, the flutes an octave up, the horn's solo an octave down, the violin
// strings and the angelic choir and glass harmonica an octave up).
struct Role { const char* name; int part; int shift; };
const std::array<Role, 31> roles { {
    { "Fanfare Trumpets", Melody, 0 },
    { "Shimmering Strings", Chords, 0 },
    { "Orchestral Chord Pad", Chords, 0 },
    { "Soaring Horn", Melody, -12 },
    { "Rising Harp Glissando", Arpeggio, 0 },
    { "Angelic Choir", Chords, 12 },
    { "Wind Chimes and Bells", Arpeggio, 12 },
    { "Glass Harp", Melody, 0 },
    { "Elven Lyre", Arpeggio, 0 },
    { "Airy Flute", Melody, 12 },
    { "Crystal Bells", Arpeggio, 12 },
    { "Glass Harmonica Pad", Chords, 12 },
    { "Celesta", Arpeggio, 12 },
    { "Warm Acoustic Guitar", Arpeggio, 0 },
    { "Bouncy Kalimba", Arpeggio, 12 },
    { "Pastoral Oboe", Melody, 0 },
    { "Grand Organ", Chords, 0 },
    { "Radiant Choir", Chords, 0 },
    { "Celesta and Harp", Arpeggio, 0 },
    { "Cloud Pad", Chords, 0 },
    { "Breathy Wind Flute", Melody, 12 },
    { "Shimmering Pluck", Arpeggio, 0 },
    { "Concert Harp", Arpeggio, 0 },
    { "Music Box", Arpeggio, 12 },
    { "Harpsichord", Arpeggio, 0 },
    { "Bouncy Pizzicato", Arpeggio, 12 },
    { "Twinkling Glockenspiel", Arpeggio, 24 },
    { "Fairy Harp", Arpeggio, 12 },
    { "Heroic Horns", Chords, 0 },
    { "Warm Strings", Chords, 12 },
    { "Epic Choir and Brass", Chords, 0 },
} };
const Role& roleOf(int instrument) {
    for (const auto& r : roles)
        if (juce::String(r.name) == ls::inst::patch(instrument).name) return r;
    return roles[0];
}
// The register an instrument plays a part in: its own shift, but the chords stay low enough
// for the voices to sit (the shift applies to every part).
int shiftOf(int instrument) { return roleOf(instrument).shift; }

//==============================================================================
struct Options {
    int place = -1;            // play in another instrument's place (-1: its own)
    float intensity = -1, shimmer = -1, punch = -1;   // -1: the default
    double tail = 3.0;         // seconds after the last note
    float level = -99;         // the Level control (dB); -99: its default
};

struct Render {
    std::vector<float> L, R;
    double sr = 48000;
    double outputLimiterDb = 0, punchLimiterDb = 0;
};

// Everything the rebalance changed, as it shipped before it (restored after): the instruments'
// trims, the library brass's trims (re-fitted at the new levels) and the Fairy Harp's share of
// its court's sympathetic strings. The same code otherwise: "before" is the shipped state.
struct BeforeTrims {
    struct Saved { float trim, standIn, library, libraryStandIn, character; };
    std::array<Saved, 31> saved {};
    bool on;
    // The library trims as they shipped (Source/Synth/LibraryTrims.h before the rebalance).
    static float libraryBefore(const char* name) {
        const std::string n(name);
        return n == "Fanfare Trumpets" || n == "Epic Choir and Brass" ? 8.1f : n == "Orchestral Chord Pad" ? 7.7f
             : n == "Soaring Horn" || n == "Heroic Horns" ? 8.6f : 0.0f;
    }
    explicit BeforeTrims(bool enable) : on(enable) {
        if (!on) return;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            const auto& p = ls::inst::patch(i);
            saved[std::size_t(i)] = { p.trimDb, p.standInTrimDb, p.libraryTrimDb, p.libraryStandInTrimDb, p.spaceCharacter };
            ls::inst::setTrimForTests(i, ls::inst::legacyTrimDb(i, false), false);
            ls::inst::setTrimForTests(i, ls::inst::legacyTrimDb(i, true), true);
            ls::inst::setLibraryTrimForTests(i, libraryBefore(p.name), false);
            ls::inst::setLibraryTrimForTests(i, libraryBefore(p.name), true);
            ls::inst::setSpaceCharacterForTests(i, 1.0f);
        }
    }
    ~BeforeTrims() {
        if (!on) return;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            const auto& v = saved[std::size_t(i)];
            ls::inst::setTrimForTests(i, v.trim, false);
            ls::inst::setTrimForTests(i, v.standIn, true);
            ls::inst::setLibraryTrimForTests(i, v.library, false);
            ls::inst::setLibraryTrimForTests(i, v.libraryStandIn, true);
            ls::inst::setSpaceCharacterForTests(i, v.character);
        }
    }
};

void setPlain(InstrumentHost& h, const char* id, float v) { mixsong::setIfThere(*h.p, id, v); }

// Notes through one LIGHT SEEKER instance, from the first note on (the settling cut away).
// (The "before" trims are set around a whole batch: BeforeTrims.)
Render play(int instrument, const std::vector<Note>& notes, const Options& o = {}) {
    InstrumentHost h;
    if (o.place >= 0 && o.place != instrument) {
        h.instrument(o.place);
        h.render(0.05);
        h.p->chooseInstrument(instrument);   // locked: the instrument in that place
    } else h.instrument(instrument);
    if (o.intensity >= 0) setPlain(h, dc::ids::intensity, o.intensity);
    if (o.shimmer >= 0) setPlain(h, dc::ids::shimmer, o.shimmer);
    if (o.punch >= 0) setPlain(h, dc::ids::punch, o.punch);
    if (o.level > -98) setPlain(h, dc::ids::instLevel, o.level);
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
    Render r;
    const auto skip = static_cast<std::ptrdiff_t>(h.at(settleSeconds));
    r.L.assign(h.outL.begin() + skip, h.outL.end());
    r.R.assign(h.outR.begin() + skip, h.outR.end());
    r.sr = h.sr;
    r.outputLimiterDb = toDb(h.p->outputLimiterLowestForTest());
    r.punchLimiterDb = toDb(h.p->synthForTest().punchLimiterLowest());
    return r;
}

Render playPart(int instrument, int part, const Options& o = {}) { return play(instrument, notesOf(part, shiftOf(instrument)), o); }
Loudness loudnessOf(const Render& r) { return measure(r.L, r.R, r.sr); }

// The instruments' own role parts, rendered on several threads (each its own processor).
template <typename F>
void forEachInstrument(F&& f, int threads = 0) {
    if (threads <= 0) threads = juce::jlimit(1, 8, int(std::thread::hardware_concurrency()));
    if (const char* t = std::getenv("LS_LOUDNESS_THREADS")) threads = juce::jlimit(1, 32, juce::String(t).getIntValue());
    std::atomic<int> next { 0 };
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back([&] {
            for (int i = next++; i < ls::inst::instrumentCount; i = next++) f(i);
        });
    for (auto& t : pool) t.join();
}

struct Row {
    std::array<Loudness, 3> parts {};
    double roleLufs = 0;
    double outputLimiterDb = 0, punchLimiterDb = 0;   // the most on any part
    std::array<double, 3> outputLimiter {}, punchLimiter {};   // per part
};

// Every instrument, the three parts (or only its role's), in its own place at the defaults.
std::vector<Row> table(bool before, bool allParts) {
    std::vector<Row> rows(static_cast<std::size_t>(ls::inst::instrumentCount));
    BeforeTrims trims(before);
    forEachInstrument([&](int i) {
        auto& row = rows[std::size_t(i)];
        for (int part = 0; part < 3; ++part) {
            if (!allParts && part != roleOf(i).part) continue;
            const auto r = playPart(i, part);
            row.parts[std::size_t(part)] = loudnessOf(r);
            row.outputLimiter[std::size_t(part)] = r.outputLimiterDb;
            row.punchLimiter[std::size_t(part)] = r.punchLimiterDb;
            row.outputLimiterDb = std::min(row.outputLimiterDb, r.outputLimiterDb);
            row.punchLimiterDb = std::min(row.punchLimiterDb, r.punchLimiterDb);
        }
        row.roleLufs = row.parts[std::size_t(roleOf(i).part)].integrated;
    });
    return rows;
}
} // namespace loudness

// Trims are changed only while no render runs (the threads read them).
int loudnessReport(bool before) {
    using namespace loudness;
    samplesLoaded();
    std::cout << "recordings: " << lswb::samples::Bank::get().message() << "\n";
    std::cout << "captured spaces: " << (dc::Vault::capturedSpacesBuiltIn() ? "yes" : "no (fallbacks)") << "\n";
    std::cout << (before ? "BEFORE (the trims before the rebalance)" : "NOW") << ": integrated LUFS / short-term max per part; role part marked *; target "
              << fmt(targetLufs) << " LUFS\n";
    const auto rows = table(before, true);
    std::cout << "instrument | role | chords | melody | arpeggio | role vs target | output limiter, chords/melody/arpeggio (dB) | Punch limiter (dB) | peaks (dBFS)\n";
    for (int i = 0; i < ls::inst::instrumentCount; ++i) {
        const auto& row = rows[std::size_t(i)];
        std::cout << instrumentName(i) << " | " << partName(roleOf(i).part);
        for (int p = 0; p < 3; ++p)
            std::cout << " | " << (p == roleOf(i).part ? "*" : "") << fmt(row.parts[std::size_t(p)].integrated) << " / " << fmt(row.parts[std::size_t(p)].shortTermMax);
        std::cout << " | " << (row.roleLufs >= targetLufs ? "+" : "") << fmt(row.roleLufs - targetLufs) << " |";
        for (int p = 0; p < 3; ++p) std::cout << " " << fmt(row.outputLimiter[std::size_t(p)], 2);
        std::cout << " |";
        for (int p = 0; p < 3; ++p) std::cout << " " << fmt(row.punchLimiter[std::size_t(p)], 2);
        std::cout << " |";
        for (int p = 0; p < 3; ++p) std::cout << " " << fmt(row.parts[std::size_t(p)].peakDb, 1);
        std::cout << "\n";
    }
    return 0;
}

// The trims: each instrument's role part to the target, in its own place at the defaults, with
// the recordings (instrumentTrims) and without them (standInTrims, the instruments that use
// them). A few passes (the output limiter and Punch's own limiter are not linear).
int calibrateLoudness(const juce::String& path) {
    using namespace loudness;
    samplesLoaded();
    std::cout << "recordings: " << lswb::samples::Bank::get().message() << "\n";
    std::array<float, 31> trims {}, standIns {};
    for (int pass = 0; pass < 2; ++pass) {
        lswb::samples::Bank::bypassRecordingsForTests(pass == 1);
        auto& out = pass == 0 ? trims : standIns;
        std::array<bool, 31> todo {};
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            const auto& p = ls::inst::patch(i);
            todo[std::size_t(i)] = pass == 0 || ls::inst::patchUsesRecordings(p);
            out[std::size_t(i)] = pass == 0 ? p.trimDb : (todo[std::size_t(i)] ? p.standInTrimDb : trims[std::size_t(i)]);
        }
        for (int round = 0; round < 4; ++round) {
            std::array<double, 31> level {};
            for (int i = 0; i < ls::inst::instrumentCount; ++i) ls::inst::setTrimForTests(i, out[std::size_t(i)], pass == 1);
            forEachInstrument([&](int i) {
                if (todo[std::size_t(i)]) level[std::size_t(i)] = loudnessOf(playPart(i, roleOf(i).part)).integrated;
            });
            double worst = 0;
            for (int i = 0; i < ls::inst::instrumentCount; ++i) {
                if (!todo[std::size_t(i)]) continue;
                const double error = level[std::size_t(i)] - targetLufs;
                worst = std::max(worst, std::abs(error));
                std::cout << (pass == 0 ? "" : "stand-in ") << "round " << round << " " << instrumentName(i) << ": " << fmt(level[std::size_t(i)]) << " LUFS, trim "
                          << fmt(out[std::size_t(i)]) << " -> ";
                if (std::abs(error) > 0.1) out[std::size_t(i)] = float(std::round((out[std::size_t(i)] - error) * 10.0) / 10.0);
                std::cout << fmt(out[std::size_t(i)]) << "\n";
                todo[std::size_t(i)] = std::abs(error) > 0.1;
            }
            std::cout << "round " << round << ": worst " << fmt(worst, 2) << " LU\n";
            if (worst <= 0.1) break;
        }
        for (int i = 0; i < ls::inst::instrumentCount; ++i) ls::inst::setTrimForTests(i, out[std::size_t(i)], pass == 1);
    }
    // How much louder each instrument's own part is now than before the rebalance (in each
    // condition): an older session where Level had been moved opens that much lower.
    std::array<float, 31> shift {}, standInShift {};
    for (int pass = 0; pass < 2; ++pass) {
        lswb::samples::Bank::bypassRecordingsForTests(pass == 1);
        std::array<double, 31> now {}, then {};
        forEachInstrument([&](int i) { now[std::size_t(i)] = loudnessOf(playPart(i, roleOf(i).part)).integrated; });
        {
            BeforeTrims before(true);
            forEachInstrument([&](int i) { then[std::size_t(i)] = loudnessOf(playPart(i, roleOf(i).part)).integrated; });
        }
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            (pass == 0 ? shift : standInShift)[std::size_t(i)] = float(std::round((now[std::size_t(i)] - then[std::size_t(i)]) * 10.0) / 10.0);
            std::cout << (pass == 0 ? "" : "stand-in ") << instrumentName(i) << ": " << fmt(then[std::size_t(i)]) << " -> " << fmt(now[std::size_t(i)]) << " LUFS\n";
        }
    }
    lswb::samples::Bank::bypassRecordingsForTests(false);
    const auto list = [](const auto& a) {
        juce::String s;
        for (std::size_t i = 0; i < a.size(); ++i) s << (i ? ", " : "") << juce::String(a[i], 1) << "f";
        return s;
    };
    std::array<float, 31> legacy {}, legacyStandIn {};
    for (int i = 0; i < ls::inst::instrumentCount; ++i) {
        legacy[std::size_t(i)] = ls::inst::legacyTrimDb(i, false);
        legacyStandIn[std::size_t(i)] = ls::inst::legacyTrimDb(i, true);
    }
    juce::String text;
    text << "#pragma once\n"
         << "// Level trims of the 31 instruments (dB), written by `LightSeekerTests --calibrate-instruments\n"
         << "// Source/Synth/InstrumentTrims.h`: each brings its own kind of part (chords, melody or\n"
         << "// arpeggio; the loudness rebalance), played in its own place at the shipped defaults, to the\n"
         << "// same integrated loudness (ITU-R BS.1770). `standIn` is for builds without the recordings\n"
         << "// (their stand-ins play).\n"
         << "#include <array>\n\nnamespace ls::inst {\n"
         << "inline constexpr std::array<float, 31> instrumentTrims { " << list(trims) << " };\n"
         << "inline constexpr std::array<float, 31> standInTrims { " << list(standIns) << " };\n"
         << "// The trims before the loudness rebalance (a dry held chord's loudest 400 ms): kept so a\n"
         << "// session saved before it, where Level had been moved, opens at the loudness it had.\n"
         << "inline constexpr std::array<float, 31> legacyInstrumentTrims { " << list(legacy) << " };\n"
         << "inline constexpr std::array<float, 31> legacyStandInTrims { " << list(legacyStandIn) << " };\n"
         << "// How much louder each instrument's own part (in its place, at the defaults) is than before\n"
         << "// the rebalance (LU; the trims, the library brass's re-fitted trims, the Fairy Harp's share of\n"
         << "// its court): an older session where Level had been moved opens that much lower.\n"
         << "inline constexpr std::array<float, 31> levelShift { " << list(shift) << " };\n"
         << "inline constexpr std::array<float, 31> standInLevelShift { " << list(standInShift) << " };\n}\n";
    return juce::File::getCurrentWorkingDirectory().getChildFile(path).replaceWithText(text) ? 0 : 1;
}

//==============================================================================
namespace loudness {
std::vector<float> sineOf(double hz, double dbfs, double seconds, double sr) {
    std::vector<float> x(static_cast<std::size_t>(seconds * sr));
    const double a = std::pow(10.0, dbfs / 20.0);
    for (std::size_t i = 0; i < x.size(); ++i) x[i] = float(a * std::sin(2.0 * juce::MathConstants<double>::pi * hz * double(i) / sr));
    return x;
}

bool isChordRole(int i) { return roleOf(i).part == Chords; }

// A chord held long (24 s): its short-term loudness late against early (a pad must not keep
// building up as its tails and voices pile up).
double holdBuildUp(int i) {
    std::vector<Note> notes;
    for (int n : progression[0]) notes.push_back({ 0.0, 48.0, n + shiftOf(i) });
    const auto r = play(i, notes, { -1, -1, -1, -1, 1.0 });
    const auto early = measure(r.L, r.R, r.sr, 3.0, 6.0).integrated, late = measure(r.L, r.R, r.sr, 21.0, 24.0).integrated;
    return late - early;
}

// Notes added one a bar (C3 G3 C4 E4 G4 C5 E5 G5, each held to the end): the loudness with
// eight notes held against four, beyond what the same notes add up to played alone (their
// powers summed: uncorrelated voices, each in its own register). A pad whose voices pile up
// (stacked unisons, tails, a shared stage that swells) grows more than that.
double stackExcess(int i, double& four, double& eight, double& expected) {
    const std::array<int, 8> stack { 48, 55, 60, 64, 67, 72, 76, 79 };
    std::vector<Note> notes;
    for (int k = 0; k < 8; ++k) notes.push_back({ 4.0 * k, 4.0 * (8 - k) + 8.0, stack[std::size_t(k)] + shiftOf(i) });
    const auto r = play(i, notes, { -1, -1, -1, -1, 1.0 });
    four = measure(r.L, r.R, r.sr, 7.0, 8.0).integrated;       // the last second of bar 4 (four held)
    eight = measure(r.L, r.R, r.sr, 18.0, 20.0).integrated;    // eight held, settled
    // Each note alone, held as long, measured 4-6 s into it (settled, as the stack's notes are).
    double first4 = 0, all8 = 0;
    for (int k = 0; k < 8; ++k) {
        const auto one = play(i, { { 0.0, 14.0, stack[std::size_t(k)] + shiftOf(i) } }, { -1, -1, -1, -1, 0.5 });
        const double power = std::pow(10.0, (measure(one.L, one.R, one.sr, 4.0, 6.0).integrated + 0.691) / 10.0);
        all8 += power;
        if (k < 4) first4 += power;
    }
    expected = 10.0 * std::log10(all8 / std::max(first4, 1.0e-20));
    return (eight - four) - expected;
}
} // namespace loudness

void testLoudness(Suite& s) {
    using namespace loudness;
    s.section("loudness: every instrument as loud as the others on its own kind of part, in its own place, at the defaults (BS.1770)");
    const bool sampled = samplesLoaded();
    std::cout << "  recordings: " << lswb::samples::Bank::get().message() << "\n";
    const bool captured = dc::Vault::capturedSpacesBuiltIn();
    const bool library = ls::inst::patchLibraryLoaded(ls::inst::patch(mixsong::instrumentNamed("Fanfare Trumpets")));
    std::cout << "  captured spaces: " << (captured ? "yes" : "no") << ", library brass: " << (library ? "yes" : "no") << "\n";
    // The gate holds for the conditions the trims were calibrated in (CI: recordings or their
    // stand-ins, the library brass, the captured spaces). A build without the captures plays
    // fallback spaces in two places, one without the library a synthesised brass: those are
    // reported there, not gated.
    const auto gated = [&](int i) {
        int l = 0, k = 0;
        ls::inst::instrumentPlace(i, l, k);
        if (!captured && onCapturedFallback(placeName(l, k))) return false;
        if (!library && ls::inst::patchUsesLibrary(ls::inst::patch(i))) return false;
        return true;
    };
    {   // The meter against the standard's reference points (EBU Tech 3341, BS.2217).
        for (const double sr : { 48000.0, 44100.0 }) {
            const auto x = sineOf(1000.0, -23.0, 20.0, sr);
            const auto m = measure(x, x, sr);
            s.check(std::abs(m.integrated + 23.0) <= 0.1, "meter: a 1 kHz sine at -23 dBFS in both channels reads -23.0 LUFS (" + fmt(sr / 1000.0) + " kHz)", fmt(m.integrated, 2));
            s.check(std::abs(m.shortTermMax + 23.0) <= 0.1, "meter: ... and its short-term maximum too", fmt(m.shortTermMax, 2));
            std::vector<float> silent(x.size(), 0.0f);
            const auto one = measure(x, silent, sr);
            s.check(std::abs(one.integrated + 26.01) <= 0.1, "meter: in one channel only, 3 dB less (-26.0 LUFS)", fmt(one.integrated, 2));
            const auto a = sineOf(1000.0, -36.0, 10.0, sr), b = sineOf(1000.0, -23.0, 60.0, sr);
            std::vector<float> gate(a);
            gate.insert(gate.end(), b.begin(), b.end());
            gate.insert(gate.end(), a.begin(), a.end());
            const auto g = measure(gate, gate, sr);
            s.check(std::abs(g.integrated + 23.0) <= 0.1, "meter: the relative gate (-36 / -23 / -36 dBFS: -23.0 LUFS)", fmt(g.integrated, 2));
            const auto low = sineOf(25.0, -23.0, 20.0, sr);
            s.check(measure(low, low, sr).integrated < -30.0, "meter: K-weighting (25 Hz counts far less)", fmt(measure(low, low, sr).integrated, 2));
        }
    }
    {
        std::set<std::string> covered;
        for (int i = 0; i < ls::inst::instrumentCount; ++i)
            if (juce::String(roleOf(i).name) == ls::inst::patch(i).name) covered.insert(roleOf(i).name);
        s.check(int(covered.size()) == ls::inst::instrumentCount, "every instrument has its role and register", std::to_string(covered.size()) + " of 31");
    }

    // The gate: each instrument's role part, in its own place at the defaults. Every part is
    // rendered (the other two are printed for the table), at its default Punch.
    const auto rows = table(false, true);
    double low = 1e9, high = -1e9;
    for (int i = 0; i < ls::inst::instrumentCount; ++i) {
        const auto& row = rows[std::size_t(i)];
        const auto name = instrumentName(i);
        std::cout << "  [loudness] " << name << " (" << partName(roleOf(i).part) << "): chords " << fmt(row.parts[0].integrated) << " / " << fmt(row.parts[0].shortTermMax)
                  << ", melody " << fmt(row.parts[1].integrated) << " / " << fmt(row.parts[1].shortTermMax) << ", arpeggio " << fmt(row.parts[2].integrated) << " / "
                  << fmt(row.parts[2].shortTermMax) << " LUFS (integrated / short-term max)\n";
        const std::string detail = fmt(row.roleLufs) + " LUFS on its " + partName(roleOf(i).part) + " (target " + fmt(targetLufs) + " +- " + fmt(tolerance) + ")";
        if (gated(i)) {
            s.check(std::abs(row.roleLufs - targetLufs) <= tolerance, name + ": level-matched (integrated loudness of its own part, in its place, at the defaults)", detail);
            low = std::min(low, row.roleLufs);
            high = std::max(high, row.roleLufs);
        } else std::cout << "  [report] " << name << ": " << detail << " (fallback space or no library: not gated)\n";
        // No clipping with its default Punch: the output's safety limiter (a peak catcher, not a
        // part of the sound) idle on every part at velocity 80, Punch's own limiter too.
        double worstPeak = -120;
        for (const auto& p : row.parts) worstPeak = std::max(worstPeak, p.peakDb);
        s.check(worstPeak <= -0.3 && row.outputLimiterDb >= -1.0,
                name + ": no clipping on any part at its default Punch (peak <= -0.3 dBFS, the output's safety limiter takes <= 1 dB)",
                "peak " + fmt(worstPeak) + " dBFS, output limiter " + fmt(row.outputLimiterDb, 2) + " dB");
        // Punch's own ceiling (it takes back only what Punch added, above -0.4 dBFS in the synth:
        // never the note itself) may shave the hits of a part the instrument is not made for (the
        // trumpets' four-note chords); on its own part it leaves Punch nearly whole (the horn
        // section's +8 dB hits on four-note chords lose about 1 dB of it).
        const double ownPunchLimiter = row.punchLimiter[std::size_t(roleOf(i).part)];
        s.check(ownPunchLimiter >= -1.5, name + ": on its own part Punch's ceiling takes <= 1.5 dB of its hits", fmt(ownPunchLimiter, 2) + " dB");
        if (row.punchLimiterDb < -1.0)
            std::cout << "  [report] " << name << ": Punch's ceiling took up to " << fmt(row.punchLimiterDb, 2) << " dB of its hits (chords/melody/arpeggio: "
                      << fmt(row.punchLimiter[0], 2) << " / " << fmt(row.punchLimiter[1], 2) << " / " << fmt(row.punchLimiter[2], 2) << ")\n";
    }
    s.metric("role-part loudness spread (loudest - quietest)", high - low, " LU");

    // Without the recordings: the stand-ins, on the same gate.
    {
        lswb::samples::Bank::bypassRecordingsForTests(true);
        std::array<double, 31> level {};
        forEachInstrument([&](int i) {
            if (ls::inst::patchUsesRecordings(ls::inst::patch(i))) level[std::size_t(i)] = loudnessOf(playPart(i, roleOf(i).part)).integrated;
        });
        lswb::samples::Bank::bypassRecordingsForTests(false);
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            if (!ls::inst::patchUsesRecordings(ls::inst::patch(i))) continue;
            const std::string detail = fmt(level[std::size_t(i)]) + " LUFS";
            if (gated(i)) s.check(std::abs(level[std::size_t(i)] - targetLufs) <= tolerance, instrumentName(i) + ": its stand-in (no recordings) level-matched too", detail);
            else std::cout << "  [report] " << instrumentName(i) << " stand-in: " << detail << "\n";
        }
    }
    if (!sampled) std::cout << "  (the recordings were not found: the instruments that use them were measured with their stand-ins)\n";

    // The sustained instruments (their role is the chords): no build-up.
    {
        std::array<double, 31> halves {}, hold {}, growth {}, four {}, eight {}, expected {}, dryDiff {}, shimmerDiff {};
        forEachInstrument([&](int i) {
            if (!isChordRole(i)) return;
            const auto r = playPart(i, Chords);
            halves[std::size_t(i)] = measure(r.L, r.R, r.sr, 8 * 4 * beat, 16 * 4 * beat).integrated - measure(r.L, r.R, r.sr, 0.0, 8 * 4 * beat).integrated;
            hold[std::size_t(i)] = holdBuildUp(i);
            growth[std::size_t(i)] = stackExcess(i, four[std::size_t(i)], eight[std::size_t(i)], expected[std::size_t(i)]);
            // The space and the shimmer on sustained chords: in the place at the defaults against
            // the same chords dry (Intensity 0) and with the Shimmer off.
            Options dry;
            dry.intensity = 0.0f;
            Options noShimmer;
            noShimmer.shimmer = 0.0f;
            const double wet = loudnessOf(r).integrated;
            dryDiff[std::size_t(i)] = wet - loudnessOf(playPart(i, Chords, dry)).integrated;
            shimmerDiff[std::size_t(i)] = wet - loudnessOf(playPart(i, Chords, noShimmer)).integrated;
        });
        double worstSpace = -99, worstShimmer = -99;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            if (!isChordRole(i)) continue;
            const auto name = instrumentName(i);
            std::cout << "  [build-up] " << name << ": bars 9-16 vs 1-8 " << fmt(halves[std::size_t(i)], 2) << " LU, held 24 s " << fmt(hold[std::size_t(i)], 2)
                      << " LU, four -> eight notes " << fmt(eight[std::size_t(i)] - four[std::size_t(i)], 2) << " LU (alone they add " << fmt(expected[std::size_t(i)], 2)
                      << "), the space +" << fmt(dryDiff[std::size_t(i)], 2) << " LU, the shimmer +" << fmt(shimmerDiff[std::size_t(i)], 2) << " LU\n";
            s.check(halves[std::size_t(i)] <= 1.0, name + ": its legato chords do not build up (bars 9-16 against 1-8, within +1 LU)", fmt(halves[std::size_t(i)], 2) + " LU");
            s.check(hold[std::size_t(i)] <= 1.0, name + ": a chord held 24 s does not keep growing (21-24 s against 3-6 s, within +1 LU)", fmt(hold[std::size_t(i)], 2) + " LU");
            s.check(growth[std::size_t(i)] <= 1.5, name + ": eight notes held against four grow no more than the same notes add up to alone (within +1.5 LU)",
                    fmt(four[std::size_t(i)]) + " -> " + fmt(eight[std::size_t(i)]) + " LUFS, the notes alone add " + fmt(expected[std::size_t(i)], 2) + " LU: "
                        + (growth[std::size_t(i)] >= 0 ? "+" : "") + fmt(growth[std::size_t(i)], 2) + " LU beyond");
            s.check(dryDiff[std::size_t(i)] <= 2.0, name + ": its place's space adds little loudness on sustained chords (in place against dry, <= +2 LU)",
                    fmt(dryDiff[std::size_t(i)], 2) + " LU");
            s.check(shimmerDiff[std::size_t(i)] <= 1.0, name + ": the shimmer adds little loudness on sustained chords (Shimmer 100 % against 0 %, <= +1 LU)",
                    fmt(shimmerDiff[std::size_t(i)], 2) + " LU");
            worstSpace = std::max(worstSpace, dryDiff[std::size_t(i)]);
            worstShimmer = std::max(worstShimmer, shimmerDiff[std::size_t(i)]);
        }
        s.metric("sustained chords: most the space adds (in place - dry)", worstSpace, " LU");
        s.metric("sustained chords: most the shimmer adds (100 % - 0 %)", worstShimmer, " LU");
    }

    // Siblings (the instruments of one location, its family) level with each other in the same
    // place: each pair played, locked, in each other's places, each on its own part.
    {
        struct Pair { int a, b, place; double la = 0, lb = 0; };
        std::vector<Pair> pairs;
        for (int l = 0; l < dc::LocationCount; ++l)
            for (int x = 0; x <= dc::subLocationCount(l); ++x)
                for (int y = x + 1; y <= dc::subLocationCount(l); ++y)
                    for (int where : { x, y }) pairs.push_back({ ls::inst::instrumentIndex(l, x), ls::inst::instrumentIndex(l, y), ls::inst::instrumentIndex(l, where) });
        std::atomic<int> next { 0 };
        std::vector<std::thread> pool;
        int threads = juce::jlimit(1, 8, int(std::thread::hardware_concurrency()));
        if (const char* t = std::getenv("LS_LOUDNESS_THREADS")) threads = juce::jlimit(1, 32, juce::String(t).getIntValue());
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&] {
                for (int k = next++; k < int(pairs.size()); k = next++) {
                    auto& p = pairs[std::size_t(k)];
                    Options o;
                    o.place = p.place;
                    p.la = loudnessOf(playPart(p.a, roleOf(p.a).part, o)).integrated;
                    p.lb = loudnessOf(playPart(p.b, roleOf(p.b).part, o)).integrated;
                }
            });
        for (auto& t : pool) t.join();
        double worst = 0;
        for (const auto& p : pairs) {
            if (!gated(p.a) || !gated(p.b) || !gated(p.place)) continue;
            const double d = std::abs(p.la - p.lb);
            worst = std::max(worst, d);
            int l = 0, k = 0;
            ls::inst::instrumentPlace(p.place, l, k);
            s.check(d <= 2.0 * tolerance, std::string(ls::inst::patch(p.a).name) + " and " + ls::inst::patch(p.b).name + " level with each other in " + dc::subLocationName(l, k)
                        + " (each on its own part, within " + fmt(2.0 * tolerance) + " LU)",
                    fmt(p.la) + " / " + fmt(p.lb) + " LUFS");
        }
        s.metric("siblings in the same place: furthest apart", worst, " LU");
    }

    // Old sessions: Level left at its default (0 dB) gets the new level; a Level the user had
    // moved keeps the loudness it had on the instrument it was saved with.
    {
        bool defaults = true;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) defaults &= LightSeekerProcessor::migratedLevel(0.0f, i) == 0.0f;
        s.check(defaults, "an older session with Level at its default (0 dB) opens with the new level, every instrument");
        const int strings = mixsong::instrumentNamed("Shimmering Strings");
        const auto& p = ls::inst::patch(strings);
        const float moved = LightSeekerProcessor::migratedLevel(-13.0f, strings);
        s.check(std::abs(moved - (-13.0f - ls::inst::levelShiftDb(strings))) < 1.0e-3f && ls::inst::levelShiftDb(strings) < -4.0f,
                "an older session's moved Level is lowered by how much louder the instrument now is (Shimmering Strings at -13 dB)", fmt(moved, 2) + " dB");
        juce::ignoreUnused(p);
        s.check(LightSeekerProcessor::migratedLevel(12.0f, mixsong::instrumentNamed("Concert Harp")) <= 12.0f
                    && LightSeekerProcessor::migratedLevel(-24.0f, strings) >= -24.0f, "... within the Level range");
        // Through the state: a session saved before (no levelDesign) on the strings at -13 dB.
        InstrumentHost h;
        h.instrument(strings);
        h.set(dc::ids::instLevel, -13.0f);
        juce::MemoryBlock data;
        h.p->getStateInformation(data);
        auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
        s.check(xml != nullptr && xml->hasAttribute("levelDesign"), "a session saved now carries the level design");
        if (xml != nullptr) {
            xml->removeAttribute("levelDesign");
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary(*xml, old);
            InstrumentHost g;
            g.p->setStateInformation(old.getData(), static_cast<int>(old.getSize()));
            const auto* level = g.p->parameters.getParameter(dc::ids::instLevel);
            const float opened = level->convertFrom0to1(level->getValue());
            // As loud as it was: the strings' chords with the old trims at -13 dB against the new
            // trims at the Level the old session opens with.
            double before = 0;
            {
                BeforeTrims legacy(true);
                Options o;
                o.level = -13.0f;
                before = loudnessOf(playPart(strings, Chords, o)).integrated;
            }
            Options o;
            o.level = opened;
            const double after = loudnessOf(playPart(strings, Chords, o)).integrated;
            s.check(std::abs(after - before) <= 0.2, "an older session's moved Level on Shimmering Strings plays as loud as it did",
                    fmt(before, 2) + " -> " + fmt(after, 2) + " LUFS (Level -13.0 -> " + fmt(opened, 2) + " dB)");
            InstrumentHost k;
            k.p->setStateInformation(data.getData(), static_cast<int>(data.getSize()));
            const auto* again = k.p->parameters.getParameter(dc::ids::instLevel);
            s.check(std::abs(again->convertFrom0to1(again->getValue()) + 13.0f) < 1.0e-3f, "a session saved now reopens with its Level as it was");
            // The same for the Soaring Horn (its library brass re-fitted too) at +6 dB.
            {
                const int horn = mixsong::instrumentNamed("Soaring Horn");
                double was = 0;
                {
                    BeforeTrims legacy(true);
                    Options o;
                    o.level = 6.0f;
                    was = loudnessOf(playPart(horn, Melody, o)).integrated;
                }
                Options o;
                o.level = LightSeekerProcessor::migratedLevel(6.0f, horn);
                const double is = loudnessOf(playPart(horn, Melody, o)).integrated;
                s.check(std::abs(is - was) <= 0.3, "an older session's moved Level on the Soaring Horn plays as loud as it did",
                        fmt(was, 2) + " -> " + fmt(is, 2) + " LUFS (Level +6.0 -> " + fmt(o.level, 2) + " dB)");
            }
            // A session from before the Punch hotfix too (neither design): both migrate.
            xml->removeAttribute("punchDesign");
            juce::MemoryBlock older;
            juce::AudioProcessor::copyXmlToBinary(*xml, older);
            InstrumentHost m;
            m.p->setStateInformation(older.getData(), static_cast<int>(older.getSize()));
            const auto* l2 = m.p->parameters.getParameter(dc::ids::instLevel);
            s.check(std::abs(l2->convertFrom0to1(l2->getValue()) - opened) < 1.0e-3f, "a session from before the Punch hotfix: its Level migrates as well");
        }
    }

    // The brass after the Punch work: level-matched with its Punch (above), and what the Punch
    // adds to its loudness and to its loudest moments.
    {
        const std::array<const char*, 4> brassNames { "Fanfare Trumpets", "Soaring Horn", "Heroic Horns", "Epic Choir and Brass" };
        std::array<Loudness, 4> withPunch {}, without {};
        std::array<double, 4> limiter {};
        std::atomic<int> next { 0 };
        std::vector<std::thread> pool;
        for (int t = 0; t < 4; ++t)
            pool.emplace_back([&] {
                for (int k = next++; k < 4; k = next++) {
                    const int i = mixsong::instrumentNamed(brassNames[std::size_t(k)]);
                    const auto r = playPart(i, roleOf(i).part);
                    withPunch[std::size_t(k)] = loudnessOf(r);
                    limiter[std::size_t(k)] = std::min(r.outputLimiterDb, r.punchLimiterDb);
                    Options o;
                    o.punch = 0.0f;
                    without[std::size_t(k)] = loudnessOf(playPart(i, roleOf(i).part, o));
                }
            });
        for (auto& t : pool) t.join();
        for (std::size_t k = 0; k < brassNames.size(); ++k) {
            const int i = mixsong::instrumentNamed(brassNames[k]);
            const auto name = instrumentName(i);
            std::cout << "  [brass] " << name << ": " << fmt(without[k].integrated) << " LUFS with Punch 0, " << fmt(withPunch[k].integrated) << " at its default (+"
                      << fmt(withPunch[k].integrated - without[k].integrated) << " LU); momentary max " << fmt(without[k].momentaryMax) << " -> " << fmt(withPunch[k].momentaryMax)
                      << " LUFS; peak " << fmt(withPunch[k].peakDb) << " dBFS; limiters " << fmt(limiter[k], 2) << " dB\n";
            // Level-matched with its Punch and never near clipping: the gates above.
        }
    }
}

// `--loudness-probe "Instrument name" part [id=value ...]`: one part (chords, melody, arpeggio),
// in its own place, with parameters set (plain values by ID; "place=Instrument" plays it
// there): integrated, short-term and momentary maxima, peak, the limiters (investigation aid).
int loudnessProbe(int argc, char** argv, int at) {
    using namespace loudness;
    samplesLoaded();
    const int i = mixsong::instrumentNamed(argv[at]);
    const juce::String partArg(argv[at + 1]);
    const int part = partArg == "chords" ? Chords : partArg == "melody" ? Melody : Arpeggio;
    Options o;
    std::vector<std::pair<juce::String, float>> sets;
    for (int k = at + 2; k < argc; ++k) {
        const juce::String a(argv[k]);
        const auto id = a.upToFirstOccurrenceOf("=", false, false), value = a.fromFirstOccurrenceOf("=", false, false);
        if (id == "place") o.place = mixsong::instrumentNamed(value.toRawUTF8());
        else if (id == "intensity") o.intensity = value.getFloatValue();
        else if (id == "shimmer") o.shimmer = value.getFloatValue();
        else if (id == "punch") o.punch = value.getFloatValue();
        else sets.push_back({ id, value.getFloatValue() });
    }
    // Other parameters: through a host of its own (the same as play(), with the sets applied).
    InstrumentHost h;
    if (o.place >= 0 && o.place != i) { h.instrument(o.place); h.render(0.05); h.p->chooseInstrument(i); }
    else h.instrument(i);
    if (o.intensity >= 0) setPlain(h, dc::ids::intensity, o.intensity);
    if (o.shimmer >= 0) setPlain(h, dc::ids::shimmer, o.shimmer);
    if (o.punch >= 0) setPlain(h, dc::ids::punch, o.punch);
    for (const auto& [id, v] : sets) setPlain(h, id.toRawUTF8(), v);
    h.render(settleSeconds);
    struct Event { double t; bool on; int note; };
    std::vector<Event> ev;
    double last = 0;
    for (const auto& n : notesOf(part, shiftOf(i))) {
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
    h.render(last + 3.0 - now);
    const auto skip = static_cast<std::ptrdiff_t>(h.at(settleSeconds));
    std::vector<float> L(h.outL.begin() + skip, h.outL.end()), R(h.outR.begin() + skip, h.outR.end());
    const auto m = measure(L, R, h.sr);
    std::cout << instrumentName(i) << ", " << partName(part) << ": " << fmt(m.integrated, 2) << " LUFS, short-term max " << fmt(m.shortTermMax) << ", momentary max "
              << fmt(m.momentaryMax) << ", peak " << fmt(m.peakDb) << " dBFS, output limiter " << fmt(toDb(h.p->outputLimiterLowestForTest()), 2) << " dB, Punch limiter "
              << fmt(toDb(h.p->synthForTest().punchLimiterLowest()), 2) << " dB\n";
    // Where the loudest peaks are (the first 8 above -1 dBFS before the limiter is unknown: after it).
    int shown = 0;
    for (std::size_t k = 0; k < L.size() && shown < 6; ++k)
        if (std::max(std::abs(L[k]), std::abs(R[k])) > 0.9f) { std::cout << "  near the ceiling at " << fmt(double(k) / h.sr, 3) << " s\n"; ++shown; k += std::size_t(h.sr * 0.5); }
    if (const char* dump = std::getenv("LS_PROBE_WAV")) writeWav(juce::File(dump), L, R, h.sr);
    return 0;
}

// `--listen-loudness dir`: a short mix (16 bars at 120 bpm, the test set's chords): Concert Harp
// arpeggios, a Shimmering Strings pad, a Fanfare Trumpets call, Angelic Choir, an Airy Flute
// melody. Five LIGHT SEEKER instances in their own places, every control at its default, no
// fader moved (summed 6 dB down, no master processing); once with the trims before the
// rebalance (the same code otherwise), once with the new ones; each alone and before-then-after.
int listenLoudness(const juce::String& directory) {
    using namespace loudness;
    samplesLoaded();
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    struct Track { const char* name; std::vector<Note> notes; };
    std::vector<Track> tracks;
    {   // The harp: the test set's arpeggio.
        tracks.push_back({ "Concert Harp", notesOf(Arpeggio, 0) });
        // The strings: the chords, legato.
        tracks.push_back({ "Shimmering Strings", notesOf(Chords, 0) });
        // The brass: a fanfare call at the start of each chord (the root, the fifth twice, a held third; G4-C6).
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
        // The choir: the upper chord tones, an octave up (its register), a chord at a time.
        std::vector<Note> choir;
        for (int c = 0; c < 8; ++c)
            for (int k = 1; k < 4; ++k) choir.push_back({ c * 8.0, 8.0, progression[std::size_t(c)][std::size_t(k)] + 12 });
        tracks.push_back({ "Angelic Choir", choir });
        // The flute: the test set's melody in its register.
        tracks.push_back({ "Airy Flute", notesOf(Melody, 12) });
    }
    int written = 0;
    std::vector<float> both[2];
    for (const bool before : { true, false }) {
        BeforeTrims trims(before);
        std::vector<Render> renders(tracks.size());
        std::vector<std::thread> pool;
        for (std::size_t t = 0; t < tracks.size(); ++t)
            pool.emplace_back([&, t] { renders[t] = play(mixsong::instrumentNamed(tracks[t].name), tracks[t].notes, { -1, -1, -1, -1, 4.0 }); });
        for (auto& th : pool) th.join();
        std::vector<float> L, R;
        for (std::size_t t = 0; t < tracks.size(); ++t) {
            const auto& r = renders[t];
            if (L.size() < r.L.size()) { L.resize(r.L.size(), 0.0f); R.resize(r.R.size(), 0.0f); }
            for (std::size_t k = 0; k < r.L.size(); ++k) { L[k] += 0.5f * r.L[k]; R[k] += 0.5f * r.R[k]; }
            const auto m = loudnessOf(r);
            std::cout << (before ? "before" : "after") << ": " << tracks[t].name << " " << fmt(m.integrated) << " LUFS (short-term max " << fmt(m.shortTermMax) << ")\n";
            written += writeWav(dir.getChildFile(juce::String(before ? "before-" : "after-") + juce::String(tracks[t].name).toLowerCase().replaceCharacter(' ', '-') + ".wav"),
                                r.L, r.R, r.sr) ? 1 : 0;
        }
        const auto m = measure(L, R, 48000.0);
        std::cout << (before ? "before" : "after") << ": the mix " << fmt(m.integrated) << " LUFS, peak " << fmt(m.peakDb) << " dBFS\n";
        written += writeWav(dir.getChildFile(before ? "mix-before.wav" : "mix-after.wav"), L, R, 48000.0) ? 1 : 0;
        both[0].insert(both[0].end(), L.begin(), L.end());
        both[1].insert(both[1].end(), R.begin(), R.end());
        if (before) {   // a second of silence between them
            both[0].insert(both[0].end(), 48000, 0.0f);
            both[1].insert(both[1].end(), 48000, 0.0f);
        }
    }
    written += writeWav(dir.getChildFile("mix-before-then-after.wav"), both[0], both[1], 48000.0) ? 1 : 0;
    std::cout << written << " listening files written to " << dir.getFullPathName() << "\n";
    return written > 0 ? 0 : 1;
}
