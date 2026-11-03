// Mix and Punch: listening renders and the brass report. Included by LightSeekerTests.cpp
// after InstrumentTests.h (it uses InstrumentHost, writeWav, toDb, fmt). It sets only
// parameters by their string IDs and skips the ones a build does not have, so the same
// file also builds against the code before Mix and Punch (the "before" renders and
// numbers in docs/DEVLOG.md were made that way).
//
//   --listen-mix dir   a four-instrument mini-song (fanfare brass, strings, choir, harp:
//                      four LIGHT SEEKER instances in their own places, summed), with the
//                      mix defaults and, for comparison, with the MIX set back to the old
//                      design (every instrument's full reverb, no Punch, no added pre-delay;
//                      the brass's own duck stays); the brass with Punch 0 and at its default
//   --brass-report     each brass instrument's rise time, attack-to-sustain ratio,
//                      2-4 kHz presence and sustain level (dry, a held chord)

namespace mixsong {
constexpr double bpm = 100.0, beat = 60.0 / bpm;

void setIfThere(LightSeekerProcessor& p, const char* id, float plain) {
    if (auto* parameter = p.parameters.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}

// A note in the song: start and length in beats, MIDI note, velocity.
struct SongNote { double at, length; int note; float velocity; };

// C - Am - F - G, twice (8 bars of 4/4 at 100 bpm).
const std::array<std::array<int, 3>, 4> chords { { { 48, 52, 55 }, { 45, 52, 57 }, { 41, 48, 53 }, { 43, 50, 55 } } };

std::vector<SongNote> part(int which) {
    std::vector<SongNote> notes;
    for (int bar = 0; bar < 8; ++bar) {
        const auto& c = chords[static_cast<std::size_t>(bar % 4)];
        const double t = bar * 4.0;
        switch (which) {
            case 0: {   // the fanfare brass: a call on beats 1, 2-and, 3 of each bar, chord tones up high
                const int root = c[0] + 24;
                const std::array<int, 4> call { root, c[1] + 24, c[2] + 12, root + 7 };
                notes.push_back({ t, 0.9, call[0], 0.95f });
                notes.push_back({ t + 1.5, 0.45, call[1], 0.8f });
                notes.push_back({ t + 2.0, 1.8, call[2] + (bar % 2 == 1 ? 12 : 0), 0.9f });
                if (bar % 2 == 1) notes.push_back({ t + 3.5, 0.45, call[3], 0.85f });
                break;
            }
            case 1:     // the strings: the chord held, an octave up, legato
                for (int n : c) notes.push_back({ t, 3.95, n + 12, 0.6f });
                break;
            case 2:     // the choir: the upper chord tones, a bar each
                notes.push_back({ t, 3.9, c[1] + 24, 0.55f });
                notes.push_back({ t, 3.9, c[2] + 24, 0.55f });
                break;
            case 3:     // the harp: eighth-note arpeggios
                for (int k = 0; k < 8; ++k) {
                    const int n = c[static_cast<std::size_t>(k % 3)] + 12 * (1 + k / 3);
                    notes.push_back({ t + 0.5 * k, 0.6, n, 0.7f });
                }
                break;
        }
    }
    return notes;
}

// One instrument's part through its own instance, in its own place.
std::pair<std::vector<float>, std::vector<float>> renderPart(int instrument, int which, bool oldMix, double seconds, float punchControl = -1.0f) {
    InstrumentHost h;
    int l = 0, k = 0;
    ls::inst::instrumentPlace(instrument, l, k);
    h.go(l, k);
    if (oldMix) {
        // The mix before: no Punch, the place's whole reverb, no pre-delay or duck (the MIX
        // Reverb control undoes the instrument's drier default share).
        setIfThere(*h.p, "punch", 0.0f);
#ifdef LS_HAS_MIX
        const auto& patch = ls::inst::patch(instrument);
        setIfThere(*h.p, "mixReverb", patch.space > 0.0f ? std::min(1.5f, 1.0f / patch.space) : 1.0f);
        // The brass's pre-delay and duck are its own: the old mix had neither.
        setIfThere(*h.p, "mixPreDelay", -patch.spacePreDelayMs);
#endif
    }
    if (punchControl >= 0.0f) setIfThere(*h.p, "punch", punchControl);
    h.render(0.3);
    auto notes = part(which);
    struct Event { double t; bool on; int note; float velocity; };
    std::vector<Event> ev;
    for (const auto& n : notes) {
        ev.push_back({ n.at * beat, true, n.note, n.velocity });
        ev.push_back({ (n.at + n.length) * beat, false, n.note, 0.0f });
    }
    std::stable_sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) { return a.t < b.t || (a.t == b.t && !a.on && b.on); });
    double now = 0;
    for (const auto& e : ev) {
        if (e.t > now) { h.render(e.t - now); now = e.t; }
        if (e.on) h.noteOn(e.note, e.velocity);
        else h.noteOff(e.note);
    }
    if (seconds > now) h.render(seconds - now);
    const auto skip = static_cast<std::ptrdiff_t>(h.at(0.3));
    return { std::vector<float>(h.outL.begin() + skip, h.outL.end()), std::vector<float>(h.outR.begin() + skip, h.outR.end()) };
}

int instrumentNamed(const char* name) {
    for (int i = 0; i < ls::inst::instrumentCount; ++i)
        if (juce::String(ls::inst::patch(i).name) == name) return i;
    return 0;
}

std::pair<std::vector<float>, std::vector<float>> song(bool oldMix) {
    const double seconds = 8 * 4 * beat + 3.5;
    const std::array<const char*, 4> names { "Fanfare Trumpets", "Warm Strings", "Angelic Choir", "Concert Harp" };
    std::vector<float> L, R;
    for (int which = 0; which < 4; ++which) {
        const auto [l, r] = renderPart(instrumentNamed(names[static_cast<std::size_t>(which)]), which, oldMix, seconds);
        if (L.size() < l.size()) { L.resize(l.size(), 0.0f); R.resize(r.size(), 0.0f); }
        // Four tracks at their default levels, the sum 6 dB down (no master processing).
        for (std::size_t i = 0; i < l.size(); ++i) { L[i] += 0.5f * l[i]; R[i] += 0.5f * r[i]; }
    }
    return { L, R };
}
}

int listenMix(const juce::String& directory) {
    using namespace mixsong;
    samplesLoaded();
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    int written = 0;
    const bool hasMix = LightSeekerProcessor(true).parameters.getParameter("mixReverb") != nullptr;
    {
        const auto [l, r] = song(false);
        written += writeWav(dir.getChildFile(hasMix ? "mini-song-mix-defaults.wav" : "mini-song-before.wav"), l, r, 48000.0) ? 1 : 0;
    }
    if (hasMix) {
        const auto [l, r] = song(true);
        written += writeWav(dir.getChildFile("mini-song-old-mix-settings.wav"), l, r, 48000.0) ? 1 : 0;
        // The brass alone (dry hits in its place): Punch 0, then its default.
        const int brass = instrumentNamed("Fanfare Trumpets");
        const double seconds = 2 * 4 * beat + 2.0;
        std::vector<float> L, R;
        for (const float punch : { 0.0f, 0.5f }) {
            const auto [a, b] = renderPart(brass, 0, false, seconds, punch);
            L.insert(L.end(), a.begin(), a.end());
            R.insert(R.end(), b.begin(), b.end());
        }
        written += writeWav(dir.getChildFile("fanfare-trumpets-punch-0-then-default.wav"), L, R, 48000.0) ? 1 : 0;
        for (const char* name : { "Heroic Horns", "Soaring Horn" }) {
            std::vector<float> L2, R2;
            for (const float punch : { 0.0f, 0.5f }) {
                const auto [a, b] = renderPart(instrumentNamed(name), 0, false, seconds, punch);
                L2.insert(L2.end(), a.begin(), a.end());
                R2.insert(R2.end(), b.begin(), b.end());
            }
            written += writeWav(dir.getChildFile(juce::String(name).toLowerCase().replaceCharacter(' ', '-') + "-punch-0-then-default.wav"), L2, R2, 48000.0) ? 1 : 0;
        }
    }
    std::cout << written << " listening files written to " << dir.getFullPathName() << "\n";
    return written > 0 ? 0 : 1;
}

// The brass report: a held C major chord (velocity 0.9, dry), measured in 5 ms frames.
int brassReport() {
    using namespace mixsong;
    samplesLoaded();
    std::cout << "instrument, rise 10-90 % (ms), attack peak vs sustain (dB), presence 2-4 kHz attack / sustain (dB of the total), sustain (dBFS)\n";
    for (const char* name : { "Fanfare Trumpets", "Soaring Horn", "Heroic Horns", "Epic Choir and Brass", "Grand Organ" }) {
        for (const float velocity : { 0.6f, 0.95f }) {
            InstrumentHost h;
            h.instrument(instrumentNamed(name));
            h.dry();
            h.render(0.1);
            for (int n : { 60, 64, 67 }) h.noteOn(n, velocity);
            h.render(1.4);
            for (int n : { 60, 64, 67 }) h.noteOff(n);
            h.render(0.3);
            const auto start = h.at(0.1);
            const auto frame = static_cast<std::size_t>(0.005 * h.sr);
            std::vector<double> env;
            for (auto i = start; i + frame <= h.at(1.5); i += frame) {
                double sum = 0;
                for (auto k = i; k < i + frame; ++k) sum += 0.5 * (double(h.outL[k]) * h.outL[k] + double(h.outR[k]) * h.outR[k]);
                env.push_back(std::sqrt(sum / double(frame)));
            }
            double peak = 0;
            std::size_t peakAt = 0;
            for (std::size_t k = 0; k < std::min<std::size_t>(env.size(), 40); ++k) if (env[k] > peak) { peak = env[k]; peakAt = k; }
            double sustain = 0;
            int count = 0;
            for (std::size_t k = 160; k < std::min<std::size_t>(env.size(), 260); ++k) { sustain += env[k] * env[k]; ++count; }
            sustain = std::sqrt(sustain / std::max(1, count));
            const double top = std::max(peak, sustain);
            std::size_t t10 = 0, t90 = 0;
            while (t10 < env.size() && env[t10] < 0.1 * top) ++t10;
            t90 = t10;
            while (t90 < env.size() && env[t90] < 0.9 * top) ++t90;
            const auto band = [&](double from, double to) {
                juce::IIRFilter lo, hi, lo2, hi2;
                hi.setCoefficients(juce::IIRCoefficients::makeHighPass(h.sr, 2000.0));
                hi2.setCoefficients(juce::IIRCoefficients::makeHighPass(h.sr, 2000.0));
                lo.setCoefficients(juce::IIRCoefficients::makeLowPass(h.sr, 4000.0));
                lo2.setCoefficients(juce::IIRCoefficients::makeLowPass(h.sr, 4000.0));
                double inBand = 0, all = 0;
                for (auto k = start; k < h.at(to); ++k) {
                    const float x = 0.5f * (h.outL[k] + h.outR[k]);
                    const float y = lo2.processSingleSampleRaw(lo.processSingleSampleRaw(hi2.processSingleSampleRaw(hi.processSingleSampleRaw(x))));
                    if (k >= h.at(from)) { inBand += double(y) * y; all += double(x) * x; }
                }
                return 10.0 * std::log10(std::max(1.0e-20, inBand) / std::max(1.0e-20, all));
            };
            std::cout << name << " (velocity " << fmt(velocity, 2) << "), " << fmt(double(t90 - t10) * 5.0, 0) << ", " << fmt(toDb(peak) - toDb(sustain), 1)
                      << " (peak at " << fmt(double(peakAt) * 5.0, 0) << " ms), " << fmt(band(0.1, 0.25), 1) << " / " << fmt(band(0.9, 1.4), 1)
                      << ", " << fmt(toDb(sustain), 1) << "\n";
        }
    }
    return 0;
}
