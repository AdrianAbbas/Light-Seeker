// Hotfix (GarageBand): the struck, bell, mallet and plucked instruments' attacks. Included by
// LightSeekerTests.cpp (it uses its Suite, InstrumentHost and helpers).
//   attack            every struck/plucked instrument at velocities 0.3 / 0.7 / 1.0 (a note
//                     and a chord, dry, at its default Punch): the attack's peak is not far
//                     above the note's own decay envelope (extrapolated back from after the
//                     Punch time), its default Punch adds nothing on top of the strike, and
//                     nothing clips
//   --attack-report   the same measurements as a table (also built against the code before
//                     Mix and Punch, see the DEVLOG's "Hotfixes")

namespace attacktest {

// The instruments struck by a hammer or mallet, bells, and plucked strings.
inline const std::vector<const char*>& struckInstruments() {
    static const std::vector<const char*> names {
        "Celesta", "Celesta and Harp", "Twinkling Glockenspiel", "Music Box", "Crystal Bells", "Wind Chimes and Bells",
        "Harpsichord", "Fairy Harp", "Concert Harp", "Rising Harp Glissando", "Bouncy Kalimba", "Bouncy Pizzicato",
        "Shimmering Pluck", "Elven Lyre", "Warm Acoustic Guitar",
    };
    return names;
}

inline int indexNamed(const char* name) {
    for (int i = 0; i < ls::inst::instrumentCount; ++i)
        if (juce::String(ls::inst::patch(i).name) == name) return i;
    return -1;
}

struct Attack {
    double samplePeakDb = -200;   // the loudest sample (dBFS)
    double attackDb = -200;       // the loudest 1 ms RMS frame in the first 25 ms
    double envelopeDb = -200;     // the note's own decay (fitted 150-400 ms) extrapolated back to that frame
    double excessDb = 0;          // attackDb - envelopeDb
    double attackMs = 0;          // where the attack peak is
    double bodyDb = -200;         // RMS 150-400 ms
    int nearCeiling = 0;          // samples above -1 dBFS
    double limiterDb = 0;         // the most the Punch safety limiter took (dB, 0 = never acted)
    double outputLimiterDb = 0;   // the most the output's safety limiter took (dB)
    double firstMsDb = -200;      // RMS of the first 3 ms (the strike itself)
};

// One strike, dry, at the instrument's default Punch (or `punch` when >= 0).
inline Attack strike(const char* name, float velocity, const std::vector<int>& notes, float punch = -1.0f, bool dry = true) {
    InstrumentHost h;
    h.instrument(indexNamed(name));
    if (dry) h.dry();
#if LS_HAS_MIX
    if (punch >= 0.0f) h.set(dc::ids::punch, punch);
#else
    juce::ignoreUnused(punch);
#endif
    const double settle = dry ? 0.4 : 6.0;   // the Level trim's glide (and in a place, the travel there) settles first
    h.render(settle);
    for (int n : notes) h.noteOn(n, velocity);
    h.render(0.9);
    for (int n : notes) h.noteOff(n);
    h.render(0.1);
    const auto onset = h.at(settle);
    if (const auto dump = juce::SystemStats::getEnvironmentVariable("LS_ATTACK_DUMP", {}); dump.isNotEmpty() && !dry)
        writeWav(juce::File(dump).getChildFile(juce::String(name).replaceCharacter(' ', '-') + "-" + juce::String(velocity, 1) + ".wav"), h.outL, h.outR, h.sr);
    Attack a;
#if LS_HAS_MIX
    a.limiterDb = toDb(h.p->synthForTest().punchLimiterLowest());
    a.outputLimiterDb = toDb(h.p->outputLimiterLowestForTest());
#endif
    a.firstMsDb = toDb(h.rms(onset, onset + static_cast<std::size_t>(0.003 * h.sr)));
    const auto frame = static_cast<std::size_t>(0.001 * h.sr);
    std::vector<double> env;
    for (auto i = onset; i + frame <= h.at(settle + 0.45); i += frame) env.push_back(h.rms(i, i + frame));
    a.samplePeakDb = toDb(h.peak(onset, h.end()));
    for (auto k = onset; k < h.end(); ++k) a.nearCeiling += std::max(std::abs(h.outL[k]), std::abs(h.outR[k])) > 0.891f ? 1 : 0;
    std::size_t at = 0;
    for (std::size_t k = 0; k < std::min<std::size_t>(env.size(), 25); ++k)
        if (env[k] > env[at]) at = k;
    a.attackDb = toDb(env[at]);
    a.attackMs = double(at);
    // Log-linear fit of the decay after the Punch time (150-400 ms).
    double sx = 0, sy = 0, sxx = 0, sxy = 0, n = 0, body = 0;
    for (std::size_t k = 150; k < std::min<std::size_t>(env.size(), 400); ++k) {
        const double x = double(k), y = toDb(env[k]);
        sx += x; sy += y; sxx += x * x; sxy += x * y; n += 1;
        body += env[k] * env[k];
    }
    const double slope = n > 1 ? (n * sxy - sx * sy) / std::max(1.0e-9, n * sxx - sx * sx) : 0.0;
    const double intercept = n > 0 ? (sy - slope * sx) / n : -200.0;
    // A note that swells (slope > 0 after its strike) is held to its body's level.
    a.envelopeDb = intercept + std::min(0.0, slope) * a.attackMs;
    a.excessDb = a.attackDb - a.envelopeDb;
    a.bodyDb = toDb(std::sqrt(body / std::max(1.0, n)));
    return a;
}

inline const std::vector<int>& noteOf() { static const std::vector<int> n { 72 }; return n; }
inline const std::vector<int>& chordOf() { static const std::vector<int> c { 60, 64, 67 }; return c; }

} // namespace attacktest

#if LS_HAS_MIX
void testAttack(Suite& s) {
    using namespace attacktest;
    s.section("attack: the struck and plucked instruments' strikes are clean (GarageBand hotfix: the celesta's clang)");
    samplesLoaded();
    // The list is every struck instrument (no sustain: bells, mallets, celesta, plucked strings).
    int struck = 0, listed = 0;
    for (int i = 0; i < ls::inst::instrumentCount; ++i) {
        const auto& p = ls::inst::patch(i);
        const bool inList = std::find_if(struckInstruments().begin(), struckInstruments().end(), [&](const char* n) { return juce::String(n) == p.name; }) != struckInstruments().end();
        struck += p.sustaining ? 0 : 1;
        listed += inList ? 1 : 0;
        s.check(p.sustaining != inList, std::string(p.name) + ": in the struck list exactly when it has no sustain");
    }
    s.check(listed == int(struckInstruments().size()) && struck == listed, "attack: all " + std::to_string(listed) + " struck/plucked instruments are checked");
    // Defaults: none on the struck and plucked instruments (it can be turned up).
    for (const char* name : struckInstruments())
        s.check(ls::inst::patch(indexNamed(name)).punchDb == 0.0f, std::string(name) + ": Punch off by default", fmt(ls::inst::patch(indexNamed(name)).punchDb) + " dB");
    for (const char* name : struckInstruments()) {
        int bad = 0;
        std::string worst;
        double worstExcess = -100, worstShare = -100, worstStrike = -100, worstOut = 0;
        for (int voicing = 0; voicing < 2; ++voicing)
            for (const float v : { 0.3f, 0.7f, 1.0f }) {
                const auto& notes = voicing == 0 ? noteOf() : chordOf();
                const auto a = strike(name, v, notes);
                const auto z = strike(name, v, notes, 0.0f);
                const auto full = strike(name, v, notes, 1.0f);
                const std::string where = std::string(voicing == 0 ? "C5" : "C4-E4-G4") + " at " + fmt(v, 1);
                // Never clips; below velocity 1 the output's safety limiter is not needed.
                const bool clipFree = a.samplePeakDb <= -0.29 && (v >= 1.0f || a.outputLimiterDb > -0.05);
                // Not far above its own decay envelope (the sharpest designed strike is the
                // Fairy Harp's single note, 11 dB), and its default Punch adds nothing to it.
                const bool clean = a.excessDb <= 13.0 && a.excessDb <= z.excessDb + 0.5;
                const double share = a.attackDb - z.attackDb;
                // Even Punch 100 % leaves the strike itself (its first 3 ms) as it is.
                const double strikeChange = std::abs(full.firstMsDb - z.firstMsDb);
                if (!clipFree || !clean || share > 0.5 || strikeChange > 0.2) {
                    ++bad;
                    worst += " " + where + " (peak " + fmt(a.samplePeakDb) + " dBFS, output limiter " + fmt(a.outputLimiterDb) + " dB, attack " + fmt(a.excessDb)
                             + " dB over its envelope, Punch adds " + fmt(share) + " dB, Punch 100 % moves the strike " + fmt(strikeChange) + " dB);";
                }
                worstExcess = std::max(worstExcess, a.excessDb);
                worstShare = std::max(worstShare, share);
                worstStrike = std::max(worstStrike, strikeChange);
                if (v >= 1.0f) worstOut = std::min(worstOut, a.outputLimiterDb);
            }
        s.check(bad == 0, std::string(name) + ": a clean strike at velocities 0.3 / 0.7 / 1.0 (note and chord): no clipping, not far above its own envelope, Punch not on the strike", worst);
        s.metric(std::string(name) + " attack over its envelope (worst)", worstExcess, " dB");
        s.metric(std::string(name) + " default Punch's share of the attack (worst)", worstShare, " dB");
        s.metric(std::string(name) + " output limiter at velocity 1.0 (most)", worstOut, " dB");
        juce::ignoreUnused(worstStrike);
    }
    // Older sessions: Punch left at its default (50 %) takes the new default; a Punch the
    // user had moved keeps the dB it gave on the instrument it was saved with.
    {
        const int celesta = indexNamed("Celesta");
        const auto& p = ls::inst::patch(celesta);
        s.check(p.punchLegacyDb == 2.5f && p.punchDb == 0.0f, "attack: the Celesta's Punch was +2.5 dB, now off");
        s.check(LightSeekerProcessor::migratedPunch(0.5f, celesta) == 0.5f, "attack: an older session with Punch at its default gets the new default");
        for (const float c : { 0.2f, 0.7f, 0.9f }) {
            const float before = dc::dsp::punchDb(c, p.punchLegacyDb), after = dc::dsp::punchDb(LightSeekerProcessor::migratedPunch(c, celesta), p.punchDb);
            s.check(std::abs(before - after) < 0.01f, "attack: an older session's moved Punch (" + fmt(c * 100.0, 0) + " %) keeps its dB on the Celesta", fmt(before, 2) + " -> " + fmt(after, 2) + " dB");
        }
        const int trumpets = indexNamed("Fanfare Trumpets");
        s.check(LightSeekerProcessor::migratedPunch(0.7f, trumpets) == 0.7f, "attack: an unchanged instrument's Punch is not touched");
        // Through the state: a session saved before (no punchDesign) on the Celesta with Punch 70 %.
        InstrumentHost h;
        h.instrument(celesta);
        h.set(dc::ids::punch, 0.7f);
        juce::MemoryBlock data;
        h.p->getStateInformation(data);
        auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
        s.check(xml != nullptr && xml->hasAttribute("punchDesign"), "attack: a session saved now carries the Punch design");
        if (xml != nullptr) {
            xml->removeAttribute("punchDesign");
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary(*xml, old);
            InstrumentHost g;
            g.p->setStateInformation(old.getData(), static_cast<int>(old.getSize()));
            const float control = g.p->parameters.getParameter(dc::ids::punch)->convertFrom0to1(g.p->parameters.getParameter(dc::ids::punch)->getValue());
            s.check(std::abs(dc::dsp::punchDb(control, p.punchDb) - dc::dsp::punchDb(0.7f, 2.5f)) < 0.05f, "attack: an older session's Punch 70 % on the Celesta opens with the same dB",
                    fmt(dc::dsp::punchDb(control, p.punchDb), 2) + " dB");
            InstrumentHost k;
            k.p->setStateInformation(data.getData(), static_cast<int>(data.getSize()));
            const float again = k.p->parameters.getParameter(dc::ids::punch)->convertFrom0to1(k.p->parameters.getParameter(dc::ids::punch)->getValue());
            s.check(std::abs(again - 0.7f) < 1.0e-3f, "attack: a session saved now reopens with its Punch as it was");
        }
    }
}
#endif

// --attack-report: one line per instrument, velocity and voicing.
int attackReport() {
    using namespace attacktest;
    samplesLoaded();
    std::cout << "instrument, voicing, velocity, Punch (dB at this velocity), sample peak (dBFS), attack peak (dB, at ms), "
                 "decay envelope there (dB), attack over its envelope (dB), body 150-400 ms (dB)";
#if LS_HAS_MIX
    std::cout << ", attack with Punch 0 (dB), Punch's share of the attack (dB), safety limiter (dB)";
#endif
    std::cout << "\n";
    for (const char* name : struckInstruments()) {
        if (indexNamed(name) < 0) continue;
        for (int voicing = 0; voicing < 2; ++voicing)
            for (const float v : { 0.3f, 0.7f, 1.0f }) {
                const auto& notes = voicing == 0 ? noteOf() : chordOf();
                const auto a = strike(name, v, notes);
                double punchDb = 0;
#if LS_HAS_MIX
                punchDb = dc::dsp::punchDb(0.5f, ls::inst::patch(indexNamed(name)).punchDb) * (0.35f + 0.65f * std::pow(v, 1.3f));
#endif
                std::cout << name << ", " << (voicing == 0 ? "C5" : "C4-E4-G4") << ", " << fmt(v, 1) << ", " << fmt(punchDb, 1) << ", "
                          << fmt(a.samplePeakDb, 1) << ", " << fmt(a.attackDb, 1) << " (" << fmt(a.attackMs, 0) << " ms), "
                          << fmt(a.envelopeDb, 1) << ", " << fmt(a.excessDb, 1) << ", " << fmt(a.bodyDb, 1);
#if LS_HAS_MIX
                const auto z = strike(name, v, notes, 0.0f);
                std::cout << ", " << fmt(z.attackDb, 1) << ", " << fmt(a.attackDb - z.attackDb, 1) << ", " << fmt(a.limiterDb, 1);
#endif
                std::cout << "\n";
            }
    }
    std::cout << "\nIn its place (Intensity and Mix as saved, velocity 1.0): sample peak (dBFS), samples above -1 dBFS\n";
    for (const char* name : struckInstruments()) {
        if (indexNamed(name) < 0) continue;
        for (const float v : { 0.7f, 1.0f }) {
            const auto a = strike(name, v, chordOf(), juce::SystemStats::getEnvironmentVariable("LS_ATTACK_PUNCH", "-1").getFloatValue(), false);
            std::cout << name << " in place, C4-E4-G4, " << fmt(v, 1) << ", " << fmt(a.samplePeakDb, 1) << ", " << a.nearCeiling << "\n";
        }
    }
    return 0;
}
