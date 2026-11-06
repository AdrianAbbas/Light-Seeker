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
};

struct Render {
    std::vector<float> L, R;
    double sr = 48000;
    double outputLimiterDb = 0, punchLimiterDb = 0;
};

// The trims before the rebalance, for one render (restored after).
struct BeforeTrims {
    std::array<float, 31> saved {}, savedStandIn {};
    bool on;
    explicit BeforeTrims(bool enable) : on(enable) {
        if (!on) return;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            saved[std::size_t(i)] = ls::inst::patch(i).trimDb;
            savedStandIn[std::size_t(i)] = ls::inst::patch(i).standInTrimDb;
            ls::inst::setTrimForTests(i, ls::inst::legacyTrimDb(i, false), false);
            ls::inst::setTrimForTests(i, ls::inst::legacyTrimDb(i, true), true);
        }
    }
    ~BeforeTrims() {
        if (!on) return;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            ls::inst::setTrimForTests(i, saved[std::size_t(i)], false);
            ls::inst::setTrimForTests(i, savedStandIn[std::size_t(i)], true);
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
    double outputLimiterDb = 0, punchLimiterDb = 0;
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
    std::cout << "instrument | role | chords | melody | arpeggio | role vs target | output limiter (dB)\n";
    for (int i = 0; i < ls::inst::instrumentCount; ++i) {
        const auto& row = rows[std::size_t(i)];
        std::cout << instrumentName(i) << " | " << partName(roleOf(i).part);
        for (int p = 0; p < 3; ++p)
            std::cout << " | " << (p == roleOf(i).part ? "*" : "") << fmt(row.parts[std::size_t(p)].integrated) << " / " << fmt(row.parts[std::size_t(p)].shortTermMax);
        std::cout << " | " << (row.roleLufs >= targetLufs ? "+" : "") << fmt(row.roleLufs - targetLufs) << " | " << fmt(row.outputLimiterDb, 2) << "\n";
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
        lswb::samples::Bank::bypassForTests(pass == 1);
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
    lswb::samples::Bank::bypassForTests(false);
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
         << "inline constexpr std::array<float, 31> legacyStandInTrims { " << list(legacyStandIn) << " };\n}\n";
    return juce::File::getCurrentWorkingDirectory().getChildFile(path).replaceWithText(text) ? 0 : 1;
}
