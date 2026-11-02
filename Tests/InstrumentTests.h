// The instrument edition (Direction Update 1): included by LightSeekerTests.cpp (it uses
// its Suite, Host helpers and allocation probe).
//   instrument    the two plug-ins, MIDI playing, Lock instrument, travel with held notes
//   instruments   every instrument: plays, in tune, distinct from its siblings, level-matched,
//                 no hung notes (also during journeys)
//   --calibrate-instruments   measures each instrument's level and prints the trims

// A host for the instrument: no input, a stereo output, MIDI in.
struct InstrumentHost {
    std::unique_ptr<LightSeekerProcessor> p = std::make_unique<LightSeekerProcessor>(true);
    TestPlayHead transport;
    double sr;
    int block;
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;
    std::vector<float> outL, outR;

    explicit InstrumentHost(double sampleRate = 48000, int blockSize = 512, int outputs = 2) : sr(sampleRate), block(blockSize) {
        juce::AudioProcessor::BusesLayout layout;
        layout.outputBuses.add(outputs == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        p->setBusesLayout(layout);
        p->setRateAndBufferSizeDetails(sr, block);
        p->prepareToPlay(sr, block);
        buffer.setSize(outputs, block);
    }
    void set(const juce::String& id, float plain) {
        auto* parameter = p->parameters.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
    }
    void go(int location, int sub) { set(dc::ids::location, float(location)); set(dc::ids::subLocation, float(sub)); }
    void instrument(int index) {
        int l = 0, k = 0;
        ls::inst::instrumentPlace(index, l, k);
        go(l, k);
    }
    void dry() { set(dc::ids::intensity, 0.0f); }
    void noteOn(int note, float velocity = 0.8f, int offset = 0) { midi.addEvent(juce::MidiMessage::noteOn(1, note, velocity), offset); }
    void noteOff(int note, int offset = 0) { midi.addEvent(juce::MidiMessage::noteOff(1, note), offset); }
    void cc(int number, int value) { midi.addEvent(juce::MidiMessage::controllerEvent(1, number, value), 0); }
    void bend(int value) { midi.addEvent(juce::MidiMessage::pitchWheel(1, value), 0); }
    void render(double seconds) {
        auto remaining = static_cast<long long>(seconds * sr);
        while (remaining > 0) {
            const int n = static_cast<int>(std::min<long long>(block, remaining));
            buffer.setSize(buffer.getNumChannels(), n, false, false, true);
            buffer.clear();
            p->processBlock(buffer, midi);
            midi.clear();
            for (int i = 0; i < n; ++i) {
                outL.push_back(buffer.getSample(0, i));
                outR.push_back(buffer.getSample(buffer.getNumChannels() > 1 ? 1 : 0, i));
            }
            remaining -= n;
        }
    }
    std::size_t at(double seconds) const { return static_cast<std::size_t>(seconds * sr); }
    std::size_t end() const { return outL.size(); }
    double rms(std::size_t from, std::size_t to) const {
        double sum = 0;
        to = std::min(to, outL.size());
        for (std::size_t i = from; i < to; ++i) sum += 0.5 * (double(outL[i]) * outL[i] + double(outR[i]) * outR[i]);
        return std::sqrt(sum / double(std::max<std::size_t>(1, to - from)));
    }
    double peak(std::size_t from, std::size_t to) const {
        double m = 0;
        to = std::min(to, outL.size());
        for (std::size_t i = from; i < to; ++i) m = std::max({ m, std::abs(double(outL[i])), std::abs(double(outR[i])) });
        return m;
    }
    bool finite() const {
        for (std::size_t i = 0; i < outL.size(); ++i) if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) return false;
        return true;
    }
    // The loudest window (seconds long) between from and to, RMS.
    double loudest(std::size_t from, std::size_t to, double seconds = 0.4) const {
        const auto w = std::max<std::size_t>(1, static_cast<std::size_t>(seconds * sr));
        double best = 0;
        for (std::size_t i = from; i + w <= std::min(to, outL.size()); i += w / 4) best = std::max(best, rms(i, i + w));
        return best;
    }
    // Largest second difference (a click stands out against the signal's own).
    double maxStep(std::size_t from, std::size_t to) const {
        double m = 0;
        to = std::min(to, outL.size());
        for (std::size_t i = std::max<std::size_t>(from, 2); i < to; ++i)
            m = std::max(m, std::abs(double(outL[i]) - 2.0 * outL[i - 1] + outL[i - 2]));
        return m;
    }
    std::vector<float> mono(std::size_t from, std::size_t to) const {
        std::vector<float> m;
        for (std::size_t i = from; i < std::min(to, outL.size()); ++i) m.push_back(0.5f * (outL[i] + outR[i]));
        return m;
    }
};

// The recordings: the tests use them when prepared (LIGHTSEEKER_SAMPLES); the instruments
// that use them have stand-ins otherwise.
bool samplesLoaded() {
    lswb::samples::Bank::get().load();
    return lswb::samples::Bank::get().waitUntilLoaded(20000);
}

// Chroma: the energy of each pitch class in a mono signal (Hann-windowed FFT, harmonics
// from 60 Hz to 5 kHz each counted at their own pitch class).
std::array<double, 12> chroma(const std::vector<float>& x, double sr) {
    std::array<double, 12> c {};
    constexpr int order = 15, size = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<float> data(size * 2, 0.0f);
    const std::size_t n = std::min<std::size_t>(x.size(), size);
    for (std::size_t i = 0; i < n; ++i) data[i] = x[i] * float(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * double(i) / double(n)));
    fft.performFrequencyOnlyForwardTransform(data.data());
    for (int b = 1; b < size / 2; ++b) {
        const double hz = double(b) * sr / size;
        if (hz < 60.0 || hz > 5000.0) continue;
        const double midi = 69.0 + 12.0 * std::log2(hz / 440.0);
        const int pc = ((int(std::lround(midi)) % 12) + 12) % 12;
        // Only bins near a semitone centre (within 35 cents) count.
        if (std::abs(midi - std::round(midi)) > 0.35) continue;
        c[std::size_t(pc)] += double(data[std::size_t(b)]) * data[std::size_t(b)];
    }
    return c;
}

// The strongest pitch class's energy relative to the pressed one, dB (0: pressed is strongest).
double pitchClassMargin(const std::array<double, 12>& c, int pressed, int& strongest) {
    strongest = 0;
    for (int k = 1; k < 12; ++k) if (c[std::size_t(k)] > c[std::size_t(strongest)]) strongest = k;
    return toDb(std::sqrt(c[std::size_t(strongest)])) - toDb(std::sqrt(std::max(c[std::size_t(pressed % 12)], 1.0e-30)));
}

// The standard test of one instrument, dry (Intensity 0): a three-note chord (C4 E4 G4,
// velocity 0.8) held 1.5 s then 1.5 s of release; and a single note for the tuning.
struct InstrumentRender {
    std::vector<float> chordL, chordR, note;
    double sr = 48000;
};

InstrumentRender renderInstrument(int index, bool dry = true, int noteForTuning = 69) {
    InstrumentRender r;
    {
        InstrumentHost h;
        h.instrument(index);
        if (dry) h.dry();
        h.render(0.1);
        h.noteOn(60); h.noteOn(64); h.noteOn(67);
        h.render(1.5);
        h.noteOff(60); h.noteOff(64); h.noteOff(67);
        h.render(1.5);
        r.chordL.assign(h.outL.begin() + std::ptrdiff_t(h.at(0.1)), h.outL.end());
        r.chordR.assign(h.outR.begin() + std::ptrdiff_t(h.at(0.1)), h.outR.end());
        r.sr = h.sr;
    }
    {
        InstrumentHost h;
        h.instrument(index);
        h.dry();
        h.render(0.05);
        h.noteOn(noteForTuning, 0.8f);
        h.render(1.2);
        h.noteOff(noteForTuning);
        h.render(0.1);
        r.note = h.mono(h.at(0.05 + 0.08), h.at(0.05 + 1.2));
    }
    return r;
}

double loudestDb(const std::vector<float>& l, const std::vector<float>& r, double sr, double from, double to) {
    const auto w = static_cast<std::size_t>(0.4 * sr);
    double best = 0;
    for (auto i = static_cast<std::size_t>(from * sr); i + w <= std::min(l.size(), static_cast<std::size_t>(to * sr)); i += w / 4) {
        double sum = 0;
        for (std::size_t k = i; k < i + w; ++k) sum += 0.5 * (double(l[k]) * l[k] + double(r[k]) * r[k]);
        best = std::max(best, std::sqrt(sum / double(w)));
    }
    return toDb(best);
}

// An instrument's fingerprint: third-octave band levels (colour, normalised) in the
// attack, the sustain and the release, and the envelope (50 ms frames over 3 s, relative
// to its peak). Two instruments that differ in timbre or in envelope are far apart.
std::vector<double> instrumentPrint(const InstrumentRender& r) {
    std::vector<double> print;
    const auto bands = [&](double from, double to) {
        std::vector<double> levels;
        for (double centre = 100.0; centre <= 10000.0; centre *= 1.2599) {
            juce::IIRFilter low, high;
            high.setCoefficients(juce::IIRCoefficients::makeHighPass(r.sr, centre / 1.122));
            low.setCoefficients(juce::IIRCoefficients::makeLowPass(r.sr, centre * 1.122));
            double sum = 0;
            const auto a = static_cast<std::size_t>(from * r.sr), b = std::min(r.chordL.size(), static_cast<std::size_t>(to * r.sr));
            for (std::size_t i = a; i < b; ++i) {
                const float y = low.processSingleSampleRaw(high.processSingleSampleRaw(0.5f * (r.chordL[i] + r.chordR[i])));
                if (i > a + 200) sum += double(y) * y;
            }
            levels.push_back(toDb(std::sqrt(sum / double(std::max<std::size_t>(1, b - a)))));
        }
        double mean = 0;
        for (double v : levels) mean += v;
        mean /= double(levels.size());
        for (double v : levels) print.push_back(std::max(v - mean, -40.0));
    };
    bands(0.0, 0.15);
    bands(0.5, 1.4);
    bands(1.6, 2.2);
    std::vector<double> env;
    const auto frame = static_cast<std::size_t>(0.05 * r.sr);
    double top = 1.0e-12;
    for (std::size_t i = 0; i + frame <= r.chordL.size(); i += frame) {
        double sum = 0;
        for (std::size_t k = i; k < i + frame; ++k) sum += double(r.chordL[k]) * r.chordL[k];
        env.push_back(std::sqrt(sum / double(frame)));
        top = std::max(top, env.back());
    }
    for (double e : env) print.push_back(0.5 * std::max(toDb(e / top), -50.0));   // weighted half
    return print;
}

double printDistance(const std::vector<double>& a, const std::vector<double>& b) {
    double sum = 0;
    const auto n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) sum += (a[i] - b[i]) * (a[i] - b[i]);
    return std::sqrt(sum / double(std::max<std::size_t>(1, n)));
}

std::string instrumentName(int index) {
    int l = 0, k = 0;
    ls::inst::instrumentPlace(index, l, k);
    return std::string(ls::inst::patch(index).name) + " (" + dc::subLocationName(l, k) + ")";
}

//==============================================================================
void testInstrument(Suite& s) {
    s.section("instrument: two plug-ins, MIDI playing, Lock instrument, travel with held notes");
    const bool sampled = samplesLoaded();
    std::cout << "  recordings: " << lswb::samples::Bank::get().message() << "\n";
    {
        LightSeekerProcessor fx(false), inst(true);
        s.check(fx.getName() == "LIGHT SEEKER FX" && inst.getName() == "LIGHT SEEKER", "the two plug-ins' names");
        s.check(!fx.isInstrument() && inst.isInstrument(), "the instrument flag");
        s.check(fx.getBusCount(true) == 1 && inst.getBusCount(true) == 0 && inst.getBusCount(false) == 1, "the effect has an input, the instrument none");
        s.check(inst.acceptsMidi() && !inst.producesMidi(), "the instrument takes MIDI");
        s.check(fx.parameters.getParameter(dc::ids::instLock) == nullptr && inst.parameters.getParameter(dc::ids::instLock) != nullptr,
                "the instrument's parameters exist only in the instrument");
        s.check(inst.getParameters().size() == fx.getParameters().size() + 9, "the instrument adds exactly its nine parameters (Vowel and Ensemble since the recordings update)");
        juce::AudioProcessor::BusesLayout stereo;
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        juce::AudioProcessor::BusesLayout withInput = stereo;
        withInput.inputBuses.add(juce::AudioChannelSet::stereo());
        s.check(inst.checkBusesLayoutSupported(stereo) && !inst.checkBusesLayoutSupported(withInput), "instrument layouts: stereo out, no input");
        s.check(ls::inst::instrumentCount == dc::placeCount, "one instrument per sub-location (31)");
        std::set<std::string> names;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) names.insert(ls::inst::patch(i).name);
        s.check(int(names.size()) == ls::inst::instrumentCount, "every instrument has its own name");
        bool mapping = true;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            int l = 0, k = 0;
            ls::inst::instrumentPlace(i, l, k);
            mapping &= ls::inst::instrumentIndex(l, k) == i;
        }
        s.check(mapping, "instrument index <-> place round trip");
    }
    {   // Silence without notes; a note plays; Intensity 0 is the instrument dry.
        InstrumentHost h;
        h.go(dc::RoyalGardens, 0);
        h.render(1.0);
        s.check(toDb(h.peak(0, h.end())) < -100.0, "no notes: silence", fmt(toDb(h.peak(0, h.end()))) + " dB");
        h.noteOn(60);
        h.render(1.0);
        s.check(toDb(h.loudest(h.at(1.0), h.end())) > -40.0, "a note plays (Concert Harp in its space)", fmt(toDb(h.loudest(h.at(1.0), h.end()))) + " dB");
        s.check(h.finite() && h.peak(0, h.end()) < 1.0, "finite, below full scale");
        s.check(h.p->instrumentIndex() == ls::inst::instrumentIndex(dc::RoyalGardens, 0), "the place chooses its instrument");
    }
    {   // In its space vs dry: the space changes the sound.
        InstrumentHost wet, dryHost;
        wet.go(dc::GoldenCathedral, 0);
        dryHost.go(dc::GoldenCathedral, 0);
        dryHost.dry();
        for (auto* h : { &wet, &dryHost }) {
            h->render(0.2);
            h->noteOn(60); h->noteOn(67);
            h->render(1.0);
            h->noteOff(60); h->noteOff(67);
            h->render(2.0);
        }
        const double tailWet = toDb(wet.rms(wet.at(1.6), wet.at(3.0))), tailDry = toDb(dryHost.rms(dryHost.at(1.6), dryHost.at(3.0)));
        s.check(tailWet > tailDry + 10.0, "the Nave's space rings on after the organ stops (dry does not)", fmt(tailWet) + " vs " + fmt(tailDry) + " dB");
    }
    {   // Travel with a held chord: the new instrument takes the held keys, nothing hangs.
        InstrumentHost h;
        h.go(dc::CelestialCitadel, 1);   // strings
        h.set(dc::ids::travelTime, 1.0f);
        h.render(0.2);
        h.noteOn(60); h.noteOn(64); h.noteOn(67);
        h.render(1.0);
        const auto before = h.end();
        h.go(dc::DragonsPeak, 2);        // choir and brass
        h.render(1.5);
        s.check(h.p->instrumentIndex() == ls::inst::instrumentIndex(dc::DragonsPeak, 2), "travelling changes the instrument");
        s.check(h.p->synthForTest().heldKeys() == 3, "the three held keys are still held after travel");
        s.check(toDb(h.rms(h.at(2.0), h.at(2.7))) > -45.0, "the held chord sounds on the new instrument", fmt(toDb(h.rms(h.at(2.0), h.at(2.7)))) + " dB");
        const double ownStep = h.maxStep(h.at(0.4), before), travelStep = h.maxStep(before, h.at(2.7));
        s.check(travelStep < ownStep * 2.5 + 1.0e-3, "no click when the instrument changes under a held chord", fmt(travelStep, 4) + " vs " + fmt(ownStep, 4));
        h.noteOff(60); h.noteOff(64); h.noteOff(67);
        h.render(6.0);
        s.check(h.p->synthForTest().activeVoices() == 0, "after the release no voice still plays (no hung notes)",
                std::to_string(h.p->synthForTest().activeVoices()) + " voices");
    }
    {   // Lock instrument: travelling keeps it (the space still moves).
        InstrumentHost h;
        h.go(dc::RoyalGardens, 2);   // harpsichord
        h.render(0.2);
        const int harpsichord = h.p->instrumentIndex();
        h.set(dc::ids::instLock, 1.0f);
        h.render(0.1);
        h.go(dc::GoldenCathedral, 0);
        h.render(0.5);
        s.check(h.p->instrumentIndex() == harpsichord, "locked: the harpsichord stays in the Nave");
        s.check(h.p->engine().destination() == dc::GoldenCathedral, "locked: the space travels");
        h.set(dc::ids::instLock, 0.0f);
        h.render(0.2);
        s.check(h.p->instrumentIndex() == ls::inst::instrumentIndex(dc::GoldenCathedral, 0), "unlocked: the Nave's organ again");
        // Choosing another instrument locks it.
        h.p->chooseInstrument(ls::inst::instrumentIndex(dc::FairyGlade, 1));
        h.render(0.2);
        s.check(h.p->instrumentLocked() && h.p->instrumentIndex() == ls::inst::instrumentIndex(dc::FairyGlade, 1), "choosing an instrument plays it here, locked");
        // State round trip.
        juce::MemoryBlock state;
        h.p->getStateInformation(state);
        InstrumentHost h2;
        h2.p->setStateInformation(state.getData(), int(state.getSize()));
        h2.render(0.2);
        s.check(h2.p->instrumentLocked() && h2.p->instrumentIndex() == ls::inst::instrumentIndex(dc::FairyGlade, 1)
                    && h2.p->engine().destination() == dc::GoldenCathedral,
                "state round trip: the locked instrument and the place");
    }
    {   // Sustain pedal, all notes off.
        InstrumentHost h;
        h.go(dc::GoldenCathedral, 0);   // organ: sustains while held
        h.dry();
        h.render(0.1);
        h.cc(64, 127);
        h.noteOn(60);
        h.render(0.4);
        h.noteOff(60);
        h.render(1.0);
        s.check(toDb(h.rms(h.at(1.2), h.at(1.5))) > -40.0, "the pedal holds a released note", fmt(toDb(h.rms(h.at(1.2), h.at(1.5)))) + " dB");
        h.cc(64, 0);
        h.render(1.0);
        s.check(toDb(h.rms(h.at(2.2), h.at(2.5))) < -80.0, "pedal up lets it go", fmt(toDb(h.rms(h.at(2.2), h.at(2.5)))) + " dB");
        h.noteOn(60); h.noteOn(64);
        h.render(0.3);
        h.cc(123, 0);
        h.render(1.0);
        s.check(toDb(h.rms(h.at(3.3), h.at(3.8))) < -80.0 && h.p->synthForTest().heldKeys() == 0, "All Notes Off releases everything");
    }
    {   // Velocity, pitch bend, mod wheel.
        const auto level = [](float velocity) {
            InstrumentHost h;
            h.go(dc::CelestialCitadel, 0);
            h.dry();
            h.render(0.05);
            h.noteOn(60, velocity);
            h.render(0.8);
            return toDb(h.loudest(h.at(0.05), h.end(), 0.2));
        };
        const double soft = level(0.3f), loud = level(1.0f);
        s.check(loud > soft + 4.0, "velocity: a hard note is louder (trumpets)", fmt(soft) + " -> " + fmt(loud) + " dB");
        const auto pitchOf = [](int bendValue, int note) {
            InstrumentHost h;
            h.go(dc::GoldenCathedral, 0);   // organ: steady pitch
            h.dry();
            h.bend(bendValue);
            h.render(0.05);
            h.noteOn(note);
            h.render(1.0);
            const auto c = chroma(h.mono(h.at(0.3), h.at(1.0)), h.sr);
            int strongest = 0;
            pitchClassMargin(c, note, strongest);
            return strongest;
        };
        s.check(pitchOf(16383, 69) == (69 + 2) % 12, "pitch bend up: a whole tone higher (organ, A4 -> B4)");
        s.check(pitchOf(0, 69) == (69 - 2) % 12, "pitch bend down: a whole tone lower (A4 -> G4)");
        const auto wobble = [](int modValue) {
            InstrumentHost h;
            h.go(dc::CelestialCitadel, 3);   // the solo horn
            h.dry();
            h.cc(1, modValue);
            h.render(0.05);
            h.noteOn(57);
            h.render(1.6);
            // The fundamental's period, frame by frame (band-passed zero crossings): its
            // spread is the vibrato.
            auto x = h.mono(h.at(0.7), h.at(1.6));
            juce::IIRFilter band;
            band.setCoefficients(juce::IIRCoefficients::makeBandPass(h.sr, 220.0, 4.0));
            for (auto& v : x) v = band.processSingleSampleRaw(v);
            std::vector<double> crossings;
            for (std::size_t i = 1; i < x.size(); ++i)
                if (x[i - 1] < 0.0f && x[i] >= 0.0f) crossings.push_back(double(i) - double(x[i]) / double(x[i] - x[i - 1]));
            std::vector<double> periods;
            for (std::size_t i = 1; i < crossings.size(); ++i) periods.push_back(crossings[i] - crossings[i - 1]);
            double mean = 0, var = 0;
            for (double v : periods) mean += v;
            mean /= double(std::max<std::size_t>(1, periods.size()));
            for (double v : periods) var += (v - mean) * (v - mean);
            return 1200.0 * std::sqrt(var / double(std::max<std::size_t>(1, periods.size()))) / std::max(mean, 1.0);   // cents
        };
        const double still = wobble(0), moving = wobble(127);
        s.check(moving > still + 4.0, "mod wheel: more vibrato (solo horn)", fmt(still, 1) + " -> " + fmt(moving, 1) + " cents");
    }
    {   // The on-screen keyboard and the octave.
        InstrumentHost h;
        h.go(dc::RoyalGardens, 0);
        h.dry();
        h.render(0.1);
        h.p->postUiMidi(juce::MidiMessage::noteOn(1, 60, 0.8f));
        h.render(0.3);
        s.check(h.p->synthForTest().keyDown(60) && h.p->keyLit(60) && toDb(h.loudest(h.at(0.1), h.end(), 0.1)) > -45.0, "the on-screen keyboard plays (and lights its key)");
        h.p->postUiMidi(juce::MidiMessage::noteOff(1, 60));
        h.render(0.1);
        s.check(!h.p->synthForTest().keyDown(60) && !h.p->keyLit(60), "... and lets go");
    }
    {   // Real time: no allocation in processBlock with notes, bend, pedal, travel and lock.
        InstrumentHost h;
        h.render(0.2);
        for (int k = 0; k < 40; ++k) {
            h.noteOn(48 + k % 24, 0.7f);
            h.bend(8192 + (k % 5) * 1000);
            h.cc(64, (k % 7) < 3 ? 127 : 0);
            h.cc(1, (k * 13) % 128);
            if (k % 3 == 0) h.noteOff(48 + (k + 10) % 24);
            if (k % 10 == 0) { h.go(k % dc::LocationCount, 0); }
            h.render(0.05);
        }
        // Parameter changes allocate on the setter's side (here, this thread); measure
        // processBlock alone.
        std::size_t inBlock = 0;
        for (int k = 0; k < 20; ++k) {
            h.noteOn(60 + k % 12, 0.6f);
            h.bend(4000 + k * 300);
            h.cc(64, k % 2 ? 127 : 0);
            allocationProbe::calls = 0;
            allocationProbe::enabled = true;
            h.buffer.setSize(2, h.block, false, false, true);
            h.p->processBlock(h.buffer, h.midi);
            allocationProbe::enabled = false;
            h.midi.clear();
            inBlock += allocationProbe::calls;
        }
        s.check(inBlock == 0, "no heap allocation in processBlock (notes, bend, pedal)", std::to_string(inBlock) + " allocations");
        s.check(h.finite(), "finite through it all");
    }
    {   // Mono output and odd block sizes.
        InstrumentHost h(44100, 333, 1);
        h.render(0.1);
        h.noteOn(64);
        h.render(0.6);
        s.check(h.finite() && toDb(h.loudest(0, h.end(), 0.1)) > -45.0, "mono output at 44.1 kHz, 333-sample blocks");
    }
    {   // The panel.
        LightSeekerProcessor p(true);
        p.setRateAndBufferSizeDetails(48000, 512);
        p.prepareToPlay(48000, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* e = dynamic_cast<LightSeekerEditor*>(editor.get());
        s.check(e != nullptr && e->instrumentPanel() != nullptr, "the instrument's window has its panel");
        if (e != nullptr && e->instrumentPanel() != nullptr) {
            auto& panel = *e->instrumentPanel();
            s.check(std::abs(double(e->getWidth()) / e->getHeight() - 960.0 / double(560 + dc::ui::InstrumentPanel::panelHeight)) < 0.01,
                    "the window keeps the view and the panel (960 x 756 at 100 %)");
            panel.refreshForTest();
            s.check(panel.instrumentName() == juce::String(ls::inst::patch(p.instrumentIndex()).name), "the panel names the instrument");
            panel.chooseForTest(ls::inst::instrumentIndex(dc::CrystalPalace, 2));
            juce::AudioBuffer<float> b(2, 512);
            juce::MidiBuffer m;
            p.processBlock(b, m);
            panel.refreshForTest();
            s.check(panel.instrumentName() == "Celesta" && p.instrumentLocked(), "choosing on the panel: Celesta, locked");
            s.check(panel.characterLabel() == juce::String(ls::inst::patch(ls::inst::instrumentIndex(dc::CrystalPalace, 2)).character), "the Character macro takes the instrument's name for it");
            panel.keyboardStateForTest().noteOn(1, 72, 0.9f);
            p.processBlock(b, m);
            s.check(p.synthForTest().keyDown(72), "a click on the panel's keyboard plays");
            panel.keyboardStateForTest().noteOff(1, 72, 0.0f);
            p.processBlock(b, m);
            s.check(!p.synthForTest().keyDown(72), "... and lets go");
            panel.bendWheel().setValue(1.0, juce::sendNotificationSync);
            p.processBlock(b, m);
            s.check(p.synthForTest().bendAmount() > 0.95f, "the bend wheel bends");
            panel.bendWheel().setValue(0.0, juce::sendNotificationSync);
            panel.sustainButton().setToggleState(true, juce::sendNotification);
            panel.sustainButton().onClick();
            p.processBlock(b, m);
            s.check(p.synthForTest().sustainDown(), "the sustain button holds the pedal");
        }
        LightSeekerProcessor fx(false);
        std::unique_ptr<juce::AudioProcessorEditor> fxEditor(fx.createEditor());
        auto* fe = dynamic_cast<LightSeekerEditor*>(fxEditor.get());
        s.check(fe != nullptr && fe->instrumentPanel() == nullptr && fe->getHeight() * 960 == fe->getWidth() * 560, "the effect's window is unchanged (960 x 560, no panel)");
    }
    (void) sampled;
}

int synthDebug() {
    ls::inst::Synth syn;
    syn.prepare(48000);
    syn.setPatch(16);
    std::vector<float> l(4800), r(4800);
    const auto run = [&](const char* what) {
        std::fill(l.begin(), l.end(), 0.0f);
        std::fill(r.begin(), r.end(), 0.0f);
        syn.render(l.data(), r.data(), 4800);
        float m = 0;
        for (float x : l) m = std::max(m, std::abs(x));
        std::cout << what << ": voices " << syn.activeVoices() << " held " << syn.heldKeys() << " pedal " << syn.sustainDown() << " peak " << m << "\n";
    };
    syn.handle(juce::MidiMessage::controllerEvent(1, 64, 127));
    syn.noteOn(60, 0.8f);
    run("on");
    syn.noteOff(60);
    run("off (pedal)");
    syn.handle(juce::MidiMessage::controllerEvent(1, 64, 0));
    for (int k = 0; k < 5; ++k) run("pedal up");
    syn.noteOn(60, 0.8f);
    run("on");
    syn.noteOff(60);
    for (int k = 0; k < 5; ++k) run("off");
    return 0;
}

//==============================================================================
// Every instrument (Direction Update 1, milestones 4-6).
constexpr double instrumentTargetDb = -18.0;   // a held chord's loudest 400 ms, dry
constexpr double instrumentLevelTolerance = 2.0;

void testInstrumentRange(Suite& s, int first, int last, bool gateLevels) {
    std::vector<InstrumentRender> renders(static_cast<std::size_t>(ls::inst::instrumentCount));
    std::vector<std::vector<double>> prints(static_cast<std::size_t>(ls::inst::instrumentCount));
    std::vector<double> levels;
    for (int i = first; i <= last; ++i) {
        const auto name = instrumentName(i);
        auto& r = renders[std::size_t(i)];
        r = renderInstrument(i);
        prints[std::size_t(i)] = instrumentPrint(r);
        const double level = loudestDb(r.chordL, r.chordR, r.sr, 0.0, 1.5);
        levels.push_back(level);
        double peak = 0;
        bool finite = true;
        for (std::size_t k = 0; k < r.chordL.size(); ++k) {
            finite &= std::isfinite(r.chordL[k]) && std::isfinite(r.chordR[k]);
            peak = std::max({ peak, std::abs(double(r.chordL[k])), std::abs(double(r.chordR[k])) });
        }
        s.check(level > -40.0, name + ": plays from MIDI", fmt(level) + " dB");
        s.check(finite && peak < 1.0, name + ": finite, below full scale", "peak " + fmt(toDb(peak)) + " dB");
        // In tune: two notes, the pressed pitch class strongest (or within 3 dB of it).
        for (const int note : { 69, 62 }) {
            const auto rr = note == 69 ? r : renderInstrument(i, true, note);
            int strongest = 0;
            const double margin = pitchClassMargin(chroma(rr.note, rr.sr), note, strongest);
            s.check(margin <= 3.0, name + ": in tune (" + juce::MidiMessage::getMidiNoteName(note, true, true, 4).toStdString() + ")",
                    "strongest pitch class " + juce::MidiMessage::getMidiNoteName(strongest, true, false, 4).toStdString() + ", " + fmt(margin) + " dB over the note");
        }
        if (gateLevels)
            s.check(std::abs(level - instrumentTargetDb) <= instrumentLevelTolerance, name + ": level-matched",
                    fmt(level) + " dB (target " + fmt(instrumentTargetDb) + " +- " + fmt(instrumentLevelTolerance) + ")");
        else
            std::cout << "  [report] " << name << ": " << fmt(level) << " dB\n";
        // Released, a sustained instrument falls away (struck ones ring as they should).
        const double after = loudestDb(r.chordL, r.chordR, r.sr, 2.4, 3.0);
        if (ls::inst::patch(i).sustaining)
            s.check(after < level - 30.0, name + ": lets go when the keys do", fmt(after - level) + " dB a second after");
    }
    // Distinct from its siblings (the other instruments of its location).
    double closest = 1.0e9;
    std::string closestPair;
    for (int i = first; i <= last; ++i) {
        int li = 0, ki = 0;
        ls::inst::instrumentPlace(i, li, ki);
        for (int j = i + 1; j <= last; ++j) {
            int lj = 0, kj = 0;
            ls::inst::instrumentPlace(j, lj, kj);
            const double d = printDistance(prints[std::size_t(i)], prints[std::size_t(j)]);
            if (d < closest) { closest = d; closestPair = instrumentName(i) + " / " + instrumentName(j); }
            if (li == lj) {
                // The colour bands (attack, sustain, release: 3 x 20) and the envelope apart.
                const auto& pa = prints[std::size_t(i)];
                const auto& pb = prints[std::size_t(j)];
                double colour = 0, envelope = 0;
                const std::size_t nb = 60, n = std::min(pa.size(), pb.size());
                for (std::size_t k = 0; k < n; ++k) (k < nb ? colour : envelope) += (pa[k] - pb[k]) * (pa[k] - pb[k]);
                if (d <= 3.0 && std::getenv("LS_PRINT_DUMP") != nullptr) {
                    for (const auto* pp : { &pa, &pb }) {
                        std::cout << "    sustain colour:";
                        for (std::size_t k = 20; k < 40; ++k) std::cout << " " << int(std::lround((*pp)[k]));
                        std::cout << "\n";
                    }
                }
                s.check(d > 3.0, instrumentName(i) + " is distinct from its sibling " + instrumentName(j),
                        "distance " + fmt(d, 2) + " (colour " + fmt(std::sqrt(colour / double(n)), 2) + ", envelope " + fmt(std::sqrt(envelope / double(n)), 2) + ")");
            }
        }
    }
    s.metric("closest pair (" + closestPair + ")", closest);
    if (!levels.empty()) {
        std::sort(levels.begin(), levels.end());
        s.metric("level spread (loudest - quietest)", levels.back() - levels.front(), " dB");
    }
}

void testInstruments(Suite& s) {
    s.section("instruments: every instrument plays, in tune, distinct from its siblings, level-matched");
    const bool sampled = samplesLoaded();
    std::cout << "  recordings: " << lswb::samples::Bank::get().message() << "\n";
    testInstrumentRange(s, 0, ls::inst::instrumentCount - 1, true);
    {   // Without the recordings the stand-ins play (and are level-matched too).
        lswb::samples::Bank::bypassForTests(true);
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            if (!ls::inst::patchUsesRecordings(ls::inst::patch(i))) continue;
            const auto r = renderInstrument(i);
            const double level = loudestDb(r.chordL, r.chordR, r.sr, 0.0, 1.5);
            s.check(std::abs(level - instrumentTargetDb) <= instrumentLevelTolerance + 1.0, instrumentName(i) + ": its stand-in plays without the recordings, level-matched", fmt(level) + " dB");
            int strongest = 0;
            s.check(pitchClassMargin(chroma(r.note, r.sr), 69, strongest) <= 3.0, instrumentName(i) + ": its stand-in is in tune");
        }
        lswb::samples::Bank::bypassForTests(false);
    }
    if (!sampled) std::cout << "  (the recordings were not found: the instruments that use them were tested with their stand-ins)\n";
    {   // No hung notes during journeys: notes held across travels, released at all times.
        InstrumentHost h;
        h.p->setRoute({ { dc::CelestialCitadel, 0 }, { dc::SkyTemple, 1 }, { dc::CrystalPalace, 2 }, { dc::GoldenCathedral, 0 },
                        { dc::FloatingIsles, 2 }, { dc::FairyGlade, 1 }, { dc::DragonsPeak, 2 }, { dc::RoyalGardens, 2 } });
        h.p->setPlayHead(&h.transport);
        h.transport.playing = true;
        h.transport.bpm = 240;
        h.set(dc::ids::journeyOn, 1.0f);
        h.set(dc::ids::journeyBars, 1.0f);
        h.set(dc::ids::travelTime, 0.5f);
        juce::Random rng(11);
        std::set<int> down;
        int changes = 0, last = -1;
        for (int step = 0; step < 160; ++step) {
            if (rng.nextFloat() < 0.5f || down.size() > 6) {
                if (!down.empty()) { auto it = down.begin(); std::advance(it, rng.nextInt(int(down.size()))); h.noteOff(*it); down.erase(it); }
            } else {
                const int note = 48 + rng.nextInt(30);
                if (!down.count(note)) { h.noteOn(note, 0.4f + 0.6f * rng.nextFloat()); down.insert(note); }
            }
            if (step % 23 == 0) h.cc(64, (step / 23) % 2 ? 0 : 127);
            h.render(0.05);
            h.transport.ppq += 0.05 * 4.0;
            if (h.p->instrumentIndex() != last) { ++changes; last = h.p->instrumentIndex(); }
        }
        s.check(changes >= 6, "the journey changed the instrument many times", std::to_string(changes) + " instruments");
        for (int note : down) h.noteOff(note);
        h.cc(64, 0);
        h.set(dc::ids::journeyOn, 0.0f);
        h.render(12.0);
        s.check(h.p->synthForTest().activeVoices() == 0 && h.p->synthForTest().heldKeys() == 0, "after the journey and the last release no note hangs",
                std::to_string(h.p->synthForTest().activeVoices()) + " voices");
        s.check(toDb(h.rms(h.end() - h.at(1.0), h.end())) < -70.0, "... and the sound has died away (space tails included)",
                fmt(toDb(h.rms(h.end() - h.at(1.0), h.end()))) + " dB");
        s.check(h.finite(), "finite");
    }
    {   // Switching instruments under a held chord (presets, the menu): no step larger than
        // the new instrument's own onset (a plucked note starts sharply by nature).
        std::string clicks;
        double worstRatio = 0;
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            InstrumentHost fresh, switched;
            fresh.dry();
            fresh.p->chooseInstrument(i);
            fresh.render(0.1);
            fresh.noteOn(55); fresh.noteOn(62); fresh.noteOn(67);
            fresh.render(0.4);
            const double own = fresh.maxStep(fresh.at(0.1), fresh.end());
            switched.dry();
            switched.p->chooseInstrument((i + 7) % ls::inst::instrumentCount);
            switched.render(0.1);
            switched.noteOn(55); switched.noteOn(62); switched.noteOn(67);
            switched.render(0.5);
            const double before = switched.maxStep(switched.at(0.3), switched.end());
            const auto at = switched.end();
            switched.p->chooseInstrument(i);
            switched.render(0.4);
            const double step = switched.maxStep(at, switched.end());
            // While switching, the old instrument's notes let go as the new one's start: the
            // output is their sum, so the limit is the two instruments' own movement together.
            const double limit = own + before;
            const double ratio = step / std::max(limit, 1.0e-4);
            worstRatio = std::max(worstRatio, ratio);
            if (step > 1.2 * limit + 0.01) clicks += instrumentName(i) + " (" + fmt(step, 3) + " vs " + fmt(limit, 3) + ") ";
        }
        s.check(clicks.empty(), "switching to each instrument under a held chord: no step beyond the two instruments' own", clicks);
        s.metric("worst switch step relative to the two instruments' own (old held, new onset)", worstRatio);
    }
    {   // 31 instruments in a row under a held chord, then silence.
        InstrumentHost h;
        h.dry();
        h.render(0.1);
        h.noteOn(55); h.noteOn(62); h.noteOn(67);
        h.render(0.5);
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            h.p->chooseInstrument(i);
            h.render(0.25);
        }
        s.check(h.finite() && h.peak(0, h.end()) < 1.0, "31 instruments in a row under a held chord: finite, below full scale");
        h.noteOff(55); h.noteOff(62); h.noteOff(67);
        h.render(10.0);
        s.check(h.p->synthForTest().activeVoices() == 0, "... and silence after the release");
    }
}

int calibrateInstruments(const juce::String& path) {
    samplesLoaded();
    std::array<float, 31> trims {}, standIns {};
    for (int pass = 0; pass < 2; ++pass) {
        lswb::samples::Bank::bypassForTests(pass == 1);
        for (int i = 0; i < ls::inst::instrumentCount; ++i) {
            const auto& p = ls::inst::patch(i);
            if (pass == 1 && !ls::inst::patchUsesRecordings(p)) { standIns[std::size_t(i)] = trims[std::size_t(i)]; continue; }
            const auto r = renderInstrument(i);
            const double level = loudestDb(r.chordL, r.chordR, r.sr, 0.0, 1.5);
            const float current = pass == 1 || (ls::inst::patchUsesRecordings(p) && !ls::inst::patchSampled(p)) ? p.standInTrimDb : p.trimDb;
            const float trim = float(std::round((current + instrumentTargetDb - level) * 10.0) / 10.0);
            (pass == 0 ? trims : standIns)[std::size_t(i)] = trim;
            std::cout << (pass == 0 ? "" : "stand-in ") << instrumentName(i) << ": " << fmt(level) << " dB -> trim " << fmt(trim) << "\n";
        }
    }
    lswb::samples::Bank::bypassForTests(false);
    const auto list = [](const std::array<float, 31>& a) {
        juce::String out;
        for (std::size_t i = 0; i < a.size(); ++i) out << (i ? ", " : "") << juce::String(a[i], 1) << "f";
        return out;
    };
    juce::String text;
    text << "#pragma once\n"
         << "// Level trims of the 31 instruments (dB), written by `LightSeekerTests --calibrate-instruments\n"
         << "// Source/Synth/InstrumentTrims.h`: each brings a held chord (C4 E4 G4, velocity 0.8, dry) to\n"
         << "// the same loudness. `standIn` is for builds without the recordings (their stand-ins play).\n"
         << "#include <array>\n\nnamespace ls::inst {\n"
         << "inline constexpr std::array<float, 31> instrumentTrims { " << list(trims) << " };\n"
         << "inline constexpr std::array<float, 31> standInTrims { " << list(standIns) << " };\n}\n";
    return juce::File::getCurrentWorkingDirectory().getChildFile(path).replaceWithText(text) ? 0 : 1;
}

// Listening renders (CI artifact): every instrument plays the same short phrase and chord,
// first dry, then in its own place; and one travel through every location with a chord held.
void playPhrase(InstrumentHost& h) {
    const int melody[] { 60, 64, 67, 72, 71, 67, 69, 72 };
    for (int n : melody) {
        h.noteOn(n, 0.75f);
        h.render(0.32);
        h.noteOff(n);
        h.render(0.03);
    }
    h.noteOn(53, 0.7f); h.noteOn(60, 0.7f); h.noteOn(64, 0.7f); h.noteOn(69, 0.75f);
    h.render(1.8);
    h.noteOff(53); h.noteOff(60); h.noteOff(64); h.noteOff(69);
    h.render(2.2);
}

int listenInstruments(const juce::String& directory) {
    samplesLoaded();
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    int written = 0;
    for (int i = 0; i < ls::inst::instrumentCount; ++i) {
        int l = 0, k = 0;
        ls::inst::instrumentPlace(i, l, k);
        std::vector<float> outL, outR;
        for (const bool dry : { true, false }) {
            InstrumentHost h;
            h.go(l, k);
            if (dry) h.dry();
            h.render(0.2);
            playPhrase(h);
            outL.insert(outL.end(), h.outL.begin(), h.outL.end());
            outR.insert(outR.end(), h.outR.begin(), h.outR.end());
        }
        const auto name = juce::String(i + 1).paddedLeft('0', 2) + "-" + juce::String(dc::subLocationName(l, k)).toLowerCase().replaceCharacter(' ', '-')
                          + "-" + juce::String(ls::inst::patch(i).name).toLowerCase().replaceCharacter(' ', '-') + "-dry-then-place.wav";
        written += writeWav(dir.getChildFile(name.removeCharacters("'")), outL, outR, 48000.0) ? 1 : 0;
    }
    {   // A chord held while travelling through the ten locations (2 s each, 1 s travel).
        InstrumentHost h;
        h.set(dc::ids::travelTime, 1.0f);
        h.go(0, 0);
        h.render(0.2);
        h.noteOn(55, 0.7f); h.noteOn(62, 0.7f); h.noteOn(67, 0.7f);
        for (int l = 0; l < dc::LocationCount; ++l) {
            h.go(l, 0);
            h.render(2.0);
        }
        h.noteOff(55); h.noteOff(62); h.noteOff(67);
        h.render(4.0);
        written += writeWav(dir.getChildFile("travel-with-a-held-chord.wav"), h.outL, h.outR, 48000.0) ? 1 : 0;
    }
    std::cout << written << " files in " << dir.getFullPathName() << "\n";
    return written == ls::inst::instrumentCount + 1 ? 0 : 1;
}

// Presets, CPU (report), the window's snapshot.
void testInstrumentPresets(Suite& s) {
    s.section("instrument presets and CPU");
    const auto& presets = dc::instrumentPresets();
    int instruments = 0, journeys = 0, carried = 0;
    for (const auto& p : presets) {
        const std::string category = p.category;
        instruments += category == "Instruments" ? 1 : 0;
        carried += category == "Carried elsewhere" ? 1 : 0;
        journeys += category == "Journeys" ? 1 : 0;
    }
    s.check(int(presets.size()) >= 30, "at least 30 instrument presets", std::to_string(presets.size()));
    s.check(journeys >= 5 && carried >= 5, "instruments in their places, carried elsewhere and journeys",
            std::to_string(instruments) + " / " + std::to_string(carried) + " / " + std::to_string(journeys));
    std::set<std::string> names;
    for (const auto& p : presets) names.insert(p.name);
    s.check(names.size() == presets.size(), "every preset has its own name");
    std::set<int> covered;
    std::string silent, wrong;
    for (int k = 0; k < int(presets.size()); ++k) {
        const auto& preset = presets[std::size_t(k)];
        InstrumentHost h;
        h.p->setPlayHead(&h.transport);
        h.transport.playing = true;
        h.p->applyFactoryPreset(k);
        h.render(0.3);
        h.noteOn(60, 0.8f); h.noteOn(67, 0.8f);
        h.render(1.2);
        h.transport.ppq += 1.5 * 2.0;
        const double level = toDb(h.loudest(h.at(0.3), h.end()));
        if (level < -40.0) silent += std::string(preset.name) + " (" + fmt(level) + " dB) ";
        if (preset.instrument >= 0) {
            if (!(h.p->instrumentLocked() && h.p->instrumentIndex() == preset.instrument)) wrong += std::string(preset.name) + " ";
        }
        covered.insert(h.p->instrumentIndex());
        s.check(h.p->presetDisplayName() == juce::String(preset.name), std::string("preset loads unmodified: ") + preset.name, h.p->presetDisplayName().toStdString());
    }
    s.check(silent.empty(), "every instrument preset plays", silent);
    s.check(wrong.empty(), "carried presets play their instrument (locked) in another place", wrong);
    s.check(int(covered.size()) == ls::inst::instrumentCount, "the presets reach every instrument", std::to_string(covered.size()) + " of 31");
    {   // A user preset: saved and loaded with its locked instrument.
        InstrumentHost h;
        h.p->chooseInstrument(ls::inst::instrumentIndex(dc::GoldenCathedral, 0));
        h.go(dc::SkyTemple, 1);
        h.render(0.2);
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ls-instrument-preset-test.lsinst");
        juce::MemoryBlock data;
        h.p->getStateInformation(data);
        auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), int(data.getSize()));
        s.check(xml != nullptr && xml->writeTo(file), "a user preset is written");
        InstrumentHost h2;
        s.check(h2.p->loadUserPreset(file), "... and loads");
        h2.render(0.2);
        s.check(h2.p->instrumentLocked() && h2.p->instrumentIndex() == ls::inst::instrumentIndex(dc::GoldenCathedral, 0)
                    && h2.p->engine().destination() == dc::SkyTemple,
                "... with its locked instrument and its place (the organ in the Sanctum)");
        file.deleteFile();
        s.check(h.p->userPresetFolder().getFileName() == "Instrument Presets", "instrument presets live in their own folder");
    }
    {   // CPU (report only): a four-note chord held in each location's arrival place.
        double worst = 0, sum = 0;
        std::string worstName;
        for (int l = 0; l < dc::LocationCount; ++l)
            for (int k = 0; k <= dc::subLocationCount(l); ++k) {
                InstrumentHost h;
                h.go(l, k);
                h.render(0.5);
                h.noteOn(48); h.noteOn(55); h.noteOn(64); h.noteOn(71);
                const auto start = std::chrono::steady_clock::now();
                h.render(3.0);
                const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                const double load = 100.0 * seconds / 3.0;
                sum += load;
                if (load > worst) { worst = load; worstName = instrumentName(ls::inst::instrumentIndex(l, k)); }
            }
        s.metric("instrument CPU, four notes in the place's space, mean (one core, this machine)", sum / ls::inst::instrumentCount, " %");
        s.metric("instrument CPU, worst (" + worstName + ")", worst, " %");
    }
}

int instrumentSnapshot(const juce::String& path) {
    samplesLoaded();
    LightSeekerProcessor p(true);
    p.setRateAndBufferSizeDetails(48000, 512);
    p.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> b(2, 512);
    juce::MidiBuffer m;
    m.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
    m.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f), 0);
    m.addEvent(juce::MidiMessage::noteOn(1, 67, 0.8f), 0);
    p.travelTo(dc::CrystalPalace, 0);
    p.processBlock(b, m);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* e = dynamic_cast<LightSeekerEditor*>(editor.get());
    if (e == nullptr) return 1;
    e->view().settleForTest();
    if (auto* panel = e->instrumentPanel()) panel->refreshForTest();
    const auto image = e->createComponentSnapshot(e->getLocalBounds(), true, 1.0f);
    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.deleteFile();
    juce::FileOutputStream out(file);
    juce::PNGImageFormat png;
    const bool ok = out.openedOk() && png.writeImageToStream(image, out);
    std::cout << (ok ? "snapshot written to " : "could not write ") << file.getFullPathName() << "\n";
    return ok ? 0 : 1;
}

//==============================================================================
// The recordings update: the instruments that now play the developer's new recordings
// (the voice, the banjo, the piano, the col legno, the Minilogue's other patches).
//   recorded   each plays from MIDI and is in tune across its range (cents, each note),
//              its sustained notes loop without a click, its Vowel and Ensemble macros
//              act (voices), it stays level-matched with and distinct from its siblings,
//              its stand-in plays without the recordings; the breath in the atmosphere
//   --listen-changed dir   the same melody and chord progression on every changed
//              instrument, dry and in its place (WAV)
struct ChangedInstrument { const char* name; int low, high; bool sustained; const char* recordings; };
constexpr ChangedInstrument changedInstruments[] = {
    { "Orchestral Chord Pad", 48, 84, true, "voice (soft takes) as the Voices" },
    { "Angelic Choir", 52, 88, true, "voice (soft takes and falsetto, ooh), stacks; formant choir above E5" },
    { "Celesta", 60, 96, false, "piano (long notes) under the bars" },
    { "Radiant Choir", 48, 84, true, "voice (full chest and falsetto, aah), stacks" },
    { "Cloud Pad", 48, 84, true, "Minilogue soft pad (long), pulse (long)" },
    { "Shimmering Pluck", 50, 86, false, "banjo, Minilogue soft pad (short)" },
    { "Bouncy Pizzicato", 55, 88, false, "violin pizzicato, col legno" },
    { "Epic Choir and Brass", 43, 79, true, "voice (full chest, aah, and falsetto above), stacks, drones" },
};

int instrumentByName(const char* name) {
    for (int i = 0; i < ls::inst::instrumentCount; ++i)
        if (juce::String(ls::inst::patch(i).name) == name) return i;
    return -1;
}

// A note's pitch error (cents): the harmonics near the expected ones (each within 60
// cents), each at its parabolic peak, weighted by level, over a second of the sustain.
double centsOff(const std::vector<float>& x, double sr, int note) {
    constexpr int order = 16, size = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<float> data(size * 2, 0.0f);
    const std::size_t n = std::min<std::size_t>(x.size(), size);
    for (std::size_t i = 0; i < n; ++i) data[i] = x[i] * float(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * double(i) / double(n)));
    fft.performFrequencyOnlyForwardTransform(data.data());
    const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    double sum = 0, weight = 0;
    for (int k = 1; k <= 4; ++k) {
        const double target = f0 * k;
        if (target > 0.4 * sr) break;
        const int lo = int(target * std::pow(2.0, -0.6 / 12.0) * size / sr), hi = int(target * std::pow(2.0, 0.6 / 12.0) * size / sr) + 1;
        int best = lo;
        for (int b = lo; b <= hi; ++b) if (data[std::size_t(b)] > data[std::size_t(best)]) best = b;
        if (best <= lo || best >= hi) continue;   // no peak inside the window
        const double a = std::log(data[std::size_t(best - 1)] + 1e-12), b0 = std::log(data[std::size_t(best)] + 1e-12), c = std::log(data[std::size_t(best + 1)] + 1e-12);
        const double shift = 0.5 * (a - c) / (a - 2 * b0 + c);
        const double hz = (best + shift) * sr / size / k;
        const double w = double(data[std::size_t(best)]) / k;
        sum += w * 1200.0 * std::log2(hz / f0);
        weight += w;
    }
    return weight > 0 ? sum / weight : 999.0;
}

// Clicks: samples whose second difference stands far above the signal's own around them
// (a discontinuity, e.g. a loop that jumps): |d2| over 12 times its 20 ms RMS (and above
// -70 dBFS). Returns how many, from `from` on.
int clicks(const InstrumentHost& h, std::size_t from) {
    int count = 0;
    const auto w = static_cast<std::size_t>(0.02 * h.sr);
    for (const auto* ch : { &h.outL, &h.outR }) {
        const auto& x = *ch;
        std::vector<double> d2(x.size(), 0.0);
        for (std::size_t i = 2; i < x.size(); ++i) d2[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
        std::vector<double> sq(x.size() + 1, 0.0);
        for (std::size_t i = 0; i < x.size(); ++i) sq[i + 1] = sq[i] + d2[i] * d2[i];
        for (std::size_t i = std::max(from, w); i + w < x.size(); ++i) {
            const double local = std::sqrt((sq[i + w] - sq[i - w] - d2[i] * d2[i]) / double(2 * w - 1));
            if (std::abs(d2[i]) > 12.0 * local && std::abs(d2[i]) > 3.0e-4) { ++count; i += w; }
        }
    }
    return count;
}

double correlationLR(const InstrumentHost& h, std::size_t from, std::size_t to) {
    double lr = 0, ll = 0, rr = 0;
    for (std::size_t i = from; i < std::min(to, h.outL.size()); ++i) {
        lr += double(h.outL[i]) * h.outR[i];
        ll += double(h.outL[i]) * h.outL[i];
        rr += double(h.outR[i]) * h.outR[i];
    }
    return lr / std::sqrt(std::max(ll * rr, 1.0e-30));
}

// The colour of a stretch (third-octave levels, normalised): as in instrumentPrint.
std::vector<double> colourOf(const InstrumentHost& h, double from, double to) {
    InstrumentRender r;
    r.chordL.assign(h.outL.begin(), h.outL.end());
    r.chordR.assign(h.outR.begin(), h.outR.end());
    r.sr = h.sr;
    std::vector<double> print = instrumentPrint(r);
    juce::ignoreUnused(from, to);
    return print;
}

void testRecordedInstruments(Suite& s) {
    s.section("recorded: the instruments that play the new recordings (voice, banjo, piano, col legno, Minilogue)");
    const bool sampled = samplesLoaded();
    std::cout << "  recordings: " << lswb::samples::Bank::get().message() << "\n";
    for (const auto* id : { "voice-ooh", "voice-aah", "voice-oh", "voice-ee", "voice-mm", "choir-ooh", "choir-aah", "choir-mm", "voice-drone",
                            "banjo", "piano-long", "breath", "violin-collegno", "mlg-softpad-short", "mlg-pulse-long" })
        if (sampled) s.check(lswb::samples::Bank::get().instrument(id) != nullptr, std::string("the recordings include ") + id);
    if (!sampled) std::cout << "  [report] no recordings: only the stand-ins are checked\n";
    for (const auto& c : changedInstruments) {
        const int i = instrumentByName(c.name);
        s.check(i >= 0, std::string(c.name) + " exists");
        if (i < 0) continue;
        const auto name = instrumentName(i);
        const auto& p = ls::inst::patch(i);
        if (sampled) s.check(ls::inst::patchSampled(p), name + ": plays its recordings (" + c.recordings + ")");
        // In tune across its range: a note every 4 semitones from low to high (and the top).
        std::string worst;
        double worstCents = 0;
        int silent = 0, wrongClass = 0, notes = 0;
        for (int note = c.low; note <= c.high; note += note + 4 > c.high && note != c.high ? c.high - note : 4) {
            InstrumentHost h;
            h.instrument(i);
            h.dry();
            h.render(0.05);
            h.noteOn(note, 0.8f);
            h.render(1.4);
            h.noteOff(note);
            h.render(0.1);
            ++notes;
            const auto x = h.mono(h.at(0.05 + (c.sustained ? 0.45 : 0.06)), h.at(0.05 + 1.4));
            const double level = toDb(h.loudest(0, h.end()));
            if (level < -45.0) { ++silent; continue; }
            int strongest = 0;
            if (pitchClassMargin(chroma(x, h.sr), note, strongest) > 3.0) ++wrongClass;
            const double cents = centsOff(x, h.sr, note);
            if (std::abs(cents) > std::abs(worstCents)) {
                worstCents = cents;
                worst = juce::MidiMessage::getMidiNoteName(note, true, true, 4).toStdString();
            }
            if (note == c.high) break;
        }
        s.check(silent == 0, name + ": plays from MIDI across its range (" + std::to_string(notes) + " notes)", std::to_string(silent) + " silent");
        s.check(wrongClass == 0, name + ": the played pitch class strongest on every note", std::to_string(wrongClass) + " wrong");
        s.check(std::abs(worstCents) <= 25.0, name + ": in tune across its range", "worst " + worst + " " + fmt(worstCents) + " cents");
        // Sustained: held notes loop without a click (a note and a chord held 8 s).
        if (c.sustained) {
            InstrumentHost h;
            h.instrument(i);
            h.dry();
            h.render(0.05);
            const int mid = (c.low + c.high) / 2;
            h.noteOn(mid - 7, 0.8f); h.noteOn(mid, 0.8f); h.noteOn(mid + 4, 0.8f);
            h.render(8.0);
            const int n = clicks(h, h.at(0.6));
            s.check(n == 0, name + ": held notes loop without a click (8 s)", std::to_string(n) + " clicks");
            const double early = h.rms(h.at(1.0), h.at(2.0)), late = h.rms(h.at(6.0), h.at(8.0));
            s.check(toDb(late) > toDb(early) - 6.0 && toDb(late) < toDb(early) + 3.0, name + ": a held chord stays steady while it loops",
                    fmt(toDb(late) - toDb(early)) + " dB from 1-2 s to 6-8 s");
        }
        // Level-matched with its siblings (their held chords' loudest 400 ms, dry).
        int l = 0, k = 0;
        ls::inst::instrumentPlace(i, l, k);
        const auto mine = renderInstrument(i);
        const double level = loudestDb(mine.chordL, mine.chordR, mine.sr, 0.0, 1.5);
        double far = 0;
        for (int j = 0; j <= dc::subLocationCount(l); ++j) {
            const int other = ls::inst::instrumentIndex(l, j);
            if (other == i) continue;
            const auto r = renderInstrument(other);
            far = std::max(far, std::abs(level - loudestDb(r.chordL, r.chordR, r.sr, 0.0, 1.5)));
        }
        s.check(far <= 3.0, name + ": level-matched with its siblings", "furthest " + fmt(far) + " dB");
    }
    if (sampled) {
        // The voices' macros: Vowel changes the colour, Ensemble the width; the level stays.
        for (const char* voiceName : { "Radiant Choir", "Angelic Choir" }) {
            const int i = instrumentByName(voiceName);
            const auto name = instrumentName(i);
            const auto hold = [&](float vowel, float ensemble) {
                InstrumentHost h;
                h.instrument(i);
                h.dry();
                h.set(dc::ids::instVowel, vowel);
                h.set(dc::ids::instEnsemble, ensemble);
                h.render(0.05);
                h.noteOn(60, 0.8f); h.noteOn(64, 0.8f); h.noteOn(67, 0.8f);
                h.render(2.5);
                return h;
            };
            const auto closed = hold(0.0f, 0.5f), open = hold(1.0f, 0.5f), one = hold(0.5f, 0.0f), full = hold(0.5f, 1.0f);
            const auto a = colourOf(closed, 0, 0), b = colourOf(open, 0, 0);
            double d = 0;
            for (std::size_t k = 20; k < 40; ++k) d += (a[k] - b[k]) * (a[k] - b[k]);
            d = std::sqrt(d / 20.0);
            s.check(d > 1.5, name + ": the Vowel macro changes the vowel (closed to open)", "colour distance " + fmt(d, 2) + " dB");
            const double lc = toDb(closed.loudest(closed.at(0.5), closed.end())), lo = toDb(open.loudest(open.at(0.5), open.end()));
            s.check(std::abs(lc - lo) <= 4.0, name + ": ... at about the same level", fmt(lo - lc) + " dB");
            const double c1 = correlationLR(one, one.at(0.8), one.end()), c8 = correlationLR(full, full.at(0.8), full.end());
            s.check(c1 - c8 > 0.15, name + ": the Ensemble macro widens one singer into a choir", "L/R correlation " + fmt(c1, 2) + " -> " + fmt(c8, 2));
            const double l1 = toDb(one.loudest(one.at(0.5), one.end())), l8 = toDb(full.loudest(full.at(0.5), full.end()));
            s.check(l8 - l1 > -1.0 && l8 - l1 < 6.0, name + ": ... a little fuller, not much louder", fmt(l8 - l1) + " dB");
            // Turning both macros while a chord is held: no click.
            InstrumentHost h;
            h.instrument(i);
            h.dry();
            h.render(0.05);
            h.noteOn(60, 0.8f); h.noteOn(67, 0.8f);
            for (int step = 0; step <= 40; ++step) {
                h.set(dc::ids::instVowel, float(step) / 40.0f);
                h.set(dc::ids::instEnsemble, 1.0f - float(step) / 40.0f);
                h.render(0.08);
            }
            s.check(clicks(h, h.at(0.6)) == 0 && h.finite(), name + ": turning Vowel and Ensemble under a held chord, no click");
        }
        s.check(dc::Atmosphere::breathRecorded(), "the Windswept Isle's atmosphere breathes the recorded breaths");
    }
    {   // The stand-ins: without the recordings every changed instrument still plays in tune.
        lswb::samples::Bank::bypassForTests(true);
        for (const auto& c : changedInstruments) {
            const int i = instrumentByName(c.name);
            if (i < 0) continue;
            int bad = 0;
            for (const int note : { c.low + 5, (c.low + c.high) / 2, c.high - 5 }) {
                InstrumentHost h;
                h.instrument(i);
                h.dry();
                h.render(0.05);
                h.noteOn(note, 0.8f);
                h.render(1.2);
                int strongest = 0;
                const auto x = h.mono(h.at(0.05 + (c.sustained ? 0.45 : 0.06)), h.at(1.25));
                if (toDb(h.loudest(0, h.end())) < -45.0 || pitchClassMargin(chroma(x, h.sr), note, strongest) > 3.0) ++bad;
            }
            s.check(bad == 0, instrumentName(i) + ": its stand-in plays in tune without the recordings", std::to_string(bad) + " of 3 notes wrong");
        }
        s.check(!dc::Atmosphere::breathRecorded(), "without the recordings the breath is a synthesised gust");
        lswb::samples::Bank::bypassForTests(false);
    }
}

// The same short melody and chord progression (I vi IV V in C) on each changed instrument.
void playMelodyAndChords(InstrumentHost& h) {
    const int melody[] { 64, 67, 72, 71, 69, 67, 65, 64 };
    for (int n : melody) {
        h.noteOn(n, 0.75f);
        h.render(0.4);
        h.noteOff(n);
        h.render(0.05);
    }
    const int chords[4][4] { { 48, 60, 64, 67 }, { 45, 57, 60, 64 }, { 41, 57, 60, 65 }, { 43, 55, 59, 62 } };
    for (const auto& chord : chords) {
        for (int n : chord) h.noteOn(n, 0.72f);
        h.render(1.3);
        for (int n : chord) h.noteOff(n);
        h.render(0.08);
    }
    h.noteOn(48, 0.72f); h.noteOn(60, 0.72f); h.noteOn(64, 0.72f); h.noteOn(67, 0.72f);
    h.render(2.0);
    for (int n : { 48, 60, 64, 67 }) h.noteOff(n);
    h.render(2.5);
}

int listenChanged(const juce::String& directory) {
    const bool sampled = samplesLoaded();
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(directory);
    dir.createDirectory();
    int written = 0, expected = 0;
    for (const auto& c : changedInstruments) {
        const int i = instrumentByName(c.name);
        if (i < 0) continue;
        int l = 0, k = 0;
        ls::inst::instrumentPlace(i, l, k);
        for (const bool dry : { true, false }) {
            ++expected;
            InstrumentHost h;
            h.go(l, k);
            if (dry) h.dry();
            h.render(0.2);
            playMelodyAndChords(h);
            const auto file = (juce::String(ls::inst::patch(i).name).toLowerCase().replaceCharacter(' ', '-') + (dry ? "-1-dry" : "-2-in-")
                               + (dry ? juce::String() : juce::String(dc::subLocationName(l, k)).toLowerCase().replaceCharacter(' ', '-'))
                               + (sampled ? "" : "-stand-in") + ".wav").removeCharacters("'");
            written += writeWav(dir.getChildFile(file), h.outL, h.outR, 48000.0) ? 1 : 0;
            std::cout << file << ": loudest " << fmt(toDb(h.loudest(0, h.end()))) << " dBFS, peak " << fmt(toDb(h.peak(0, h.end()))) << " dBFS\n";
        }
    }
    std::cout << written << " files in " << dir.getFullPathName() << (sampled ? " (with the recordings)" : " (stand-ins: no recordings)") << "\n";
    return written == expected ? 0 : 1;
}

// Direction Update 2: the Rising Harp Glissando (Cloud Steps). Its run must only use the
// notes held (chord tones and their octaves; one note alone: octaves and its fifth), rise
// into the played note, and share one sweep per chord. Checked twice: the pitches the
// synth chose (its run log) and the sound (no pitch class outside the chord gains energy
// compared with the same chord plucked without the run). And the harp itself in tune.
void testGlissando(Suite& s) {
    s.section("glissando: the Rising Harp Glissando stays in key (held chords in several keys) and the harp is in tune");
    const int harp = instrumentByName("Rising Harp Glissando");
    s.check(harp >= 0, "the Rising Harp Glissando exists");
    if (harp < 0) return;
    const auto setCharacter = [](InstrumentHost& h, float v) { h.set(dc::ids::instCharacter, v); };
    // Out-of-chord energy (dB, relative to the whole) of the first 0.6 s.
    const auto outside = [](const std::vector<float>& x, double sr, const std::set<int>& classes) {
        const auto c = chroma(x, sr);
        double in = 0, out = 0;
        for (int k = 0; k < 12; ++k) (classes.count(k) ? in : out) += c[std::size_t(k)];
        return 10.0 * std::log10(std::max(out, 1.0e-30) / std::max(in + out, 1.0e-30));
    };
    struct Chord { const char* name; std::vector<int> notes; };
    const std::vector<Chord> chords {
        { "C major (C4 E4 G4)", { 60, 64, 67 } },     { "A minor (A3 C4 E4)", { 57, 60, 64 } },
        { "F# major (F#4 A#4 C#5)", { 66, 70, 73 } }, { "Bb major 7 (Bb3 D4 F4 A4)", { 58, 62, 65, 69 } },
        { "E minor, open (E3 B3 G4)", { 52, 59, 67 } }, { "Db sus2 (Db4 Eb4 Ab4)", { 61, 63, 68 } },
    };
    int sweeps = 0, wrong = 0, notRising = 0, notBelow = 0, oldUnheard = 0;
    double worstAdded = -99;
    std::string worstChord;
    for (const auto& chord : chords) {
        std::set<int> classes;
        for (int n : chord.notes) classes.insert(n % 12);
        const int top = *std::max_element(chord.notes.begin(), chord.notes.end());
        double plainOut = 0, runOut = 0, oldOut = 0;
        // The old run, for comparison (that the check hears the fault): each note of the
        // chord followed by its own major pentatonic above it, every 42 ms.
        {
            InstrumentHost h;
            h.instrument(harp);
            h.dry();
            setCharacter(h, 0.0f);
            h.render(0.05);
            for (int n : chord.notes) h.noteOn(n, 0.8f);
            const int steps[] = { 2, 4, 7, 9 };
            for (int i = 0; i < 4; ++i) {
                h.render(0.042);
                for (int n : chord.notes) h.noteOn(n + steps[i], 0.65f - 0.08f * float(i));
            }
            h.render(0.6);
            oldOut = outside(h.mono(h.at(0.05), h.at(0.05 + 0.6)), h.sr, classes);
        }
        for (int pass = 0; pass < 2; ++pass) {
            InstrumentHost h;
            h.instrument(harp);
            h.dry();
            setCharacter(h, pass == 0 ? 0.0f : 0.5f);
            h.render(0.05);
            auto& synth = const_cast<ls::inst::Synth&>(h.p->synthForTest());
            synth.clearRunLog();
            // The reference (pass 0): the chord's tones and their octaves over the two
            // octaves under its top, plucked together without the run (softer below).
            if (pass == 0) {
                for (int m = top - 24; m <= top; ++m)
                    if (classes.count(m % 12)) h.noteOn(m, std::count(chord.notes.begin(), chord.notes.end(), m) ? 0.8f : 0.5f);
            } else
                for (int n : chord.notes) h.noteOn(n, 0.8f);
            h.render(0.8);
            const auto x = h.mono(h.at(0.05), h.at(0.05 + 0.6));
            (pass == 0 ? plainOut : runOut) = outside(x, h.sr, classes);
            if (pass == 0) continue;
            const int count = synth.runLogCount();
            if (count == 4) ++sweeps;
            int last = -1;
            for (int k = 0; k < count; ++k) {
                const auto e = synth.runLogEntry(k);
                if (!classes.count(e.pitch % 12)) ++wrong;
                if (e.pitch <= last) ++notRising;
                if (e.pitch >= top) ++notBelow;
                last = e.pitch;
            }
            std::cout << "  " << chord.name << ": run";
            for (int k = 0; k < count; ++k) std::cout << " " << juce::MidiMessage::getMidiNoteName(synth.runLogEntry(k).pitch, true, true, 4);
            std::cout << " -> " << juce::MidiMessage::getMidiNoteName(top, true, true, 4) << "\n";
        }
        const double added = runOut - std::max(plainOut, -14.0);
        std::cout << "    out-of-chord energy: " << fmt(runOut) << " dB with the run, " << fmt(plainOut) << " dB its tones plucked plainly, "
                  << fmt(oldOut) << " dB the old pentatonic run\n";
        if (added > worstAdded) { worstAdded = added; worstChord = chord.name; }
        if (oldOut < std::max(plainOut, -14.0) + 3.0) ++oldUnheard;
    }
    s.check(sweeps == int(chords.size()), "glissando: one sweep per chord (4 grace notes at Character 50 %), not one per note",
            std::to_string(sweeps) + " of " + std::to_string(chords.size()));
    s.check(wrong == 0, "glissando: every grace note is a chord tone (or its octave), six chords in six keys", std::to_string(wrong) + " outside");
    s.check(notRising == 0 && notBelow == 0, "glissando: the run rises into the chord's top note",
            std::to_string(notRising) + " not rising, " + std::to_string(notBelow) + " at or above the top");
    // Out-of-chord energy (the harp's own harmonics: its twelfth and seventeenth) at most
    // 2 dB above its chord tones plucked plainly (or -14 dB of the whole, if they are cleaner).
    s.check(worstAdded <= 2.0, "glissando: in key by ear (no more out-of-chord energy than its chord tones and octaves plucked plainly)",
            "worst " + worstChord + " +" + fmt(worstAdded) + " dB");
    s.check(oldUnheard == 0, "glissando: the check hears the old pentatonic run's clash (at least 3 dB more out-of-chord energy)",
            std::to_string(oldUnheard) + " chords not heard");
    s.metric("glissando: out-of-chord energy added by the run (worst chord, dB)", worstAdded);

    // One note alone: octaves and its fifth only, rising into it; it lands about 4 x 24 ms later.
    {
        int bad = 0, total = 0;
        for (int note : { 48, 62, 67, 75, 81 }) {
            InstrumentHost h;
            h.instrument(harp);
            h.dry();
            h.render(0.05);
            auto& synth = const_cast<ls::inst::Synth&>(h.p->synthForTest());
            synth.clearRunLog();
            h.noteOn(note, 0.8f);
            h.render(0.4);
            int last = -1;
            for (int k = 0; k < synth.runLogCount(); ++k) {
                const int pitch = synth.runLogEntry(k).pitch;
                const int interval = ((note - pitch) % 12 + 12) % 12;
                ++total;
                if (!(interval == 0 || interval == 5) || pitch >= note || pitch <= last) ++bad;   // an octave, or the fifth (a fourth below)
                last = pitch;
            }
        }
        s.check(total >= 15 && bad == 0, "glissando: a single note sweeps only through its octaves and fifth, up into it",
                std::to_string(bad) + " of " + std::to_string(total) + " wrong");
    }
    // A chord changed: the next chord's run uses the new chord (keys held at the time).
    {
        InstrumentHost h;
        h.instrument(harp);
        h.dry();
        h.render(0.05);
        auto& synth = const_cast<ls::inst::Synth&>(h.p->synthForTest());
        for (int n : { 60, 64, 67 }) h.noteOn(n);
        h.render(0.5);
        for (int n : { 60, 64, 67 }) h.noteOff(n);
        synth.clearRunLog();
        for (int n : { 62, 65, 69 }) h.noteOn(n);   // D minor
        h.render(0.5);
        int wrongNext = 0;
        for (int k = 0; k < synth.runLogCount(); ++k) {
            const int pc = synth.runLogEntry(k).pitch % 12;
            if (pc != 2 && pc != 5 && pc != 9) ++wrongNext;
        }
        s.check(synth.runLogCount() == 4 && wrongNext == 0, "glissando: the next chord (C major, then D minor) sweeps through the new chord",
                std::to_string(wrongNext) + " outside of " + std::to_string(synth.runLogCount()));
    }
    // Character 0: no run, the note at once; the landing at about 96 ms with the run.
    {
        double onset[2] {};
        for (int pass = 0; pass < 2; ++pass) {
            InstrumentHost h;
            h.instrument(harp);
            h.dry();
            setCharacter(h, pass == 0 ? 0.0f : 0.5f);
            h.render(0.1);
            h.noteOn(72, 0.9f);
            h.render(0.5);
            // The landing: the strongest 5 ms window.
            const auto w = h.at(0.005);
            double best = 0;
            for (std::size_t i = h.at(0.1); i + w < h.end(); i += w / 2) {
                const double r = h.rms(i, i + w);
                if (r > best) { best = r; onset[pass] = double(i) / h.sr - 0.1; }
            }
        }
        s.check(onset[0] < 0.02, "glissando: Character 0 plays the note at once (no run)", fmt(onset[0] * 1000.0) + " ms");
        s.check(onset[1] > 0.07 && onset[1] < 0.14, "glissando: with the run the note lands on top of it (about 100 ms)", fmt(onset[1] * 1000.0) + " ms");
    }
    // The harp itself: in tune from C2 to C7 (single notes, no run), a cents report.
    {
        double worst = 0;
        std::string worstNote, report;
        for (int note = 36; note <= 96; note += 3) {
            InstrumentHost h;
            h.instrument(harp);
            h.dry();
            setCharacter(h, 0.0f);
            h.render(0.05);
            h.noteOn(note, 0.8f);
            h.render(1.2);
            const auto x = h.mono(h.at(0.05 + 0.08), h.at(0.05 + 1.2));
            const double cents = centsOff(x, h.sr, note);
            report += " " + juce::MidiMessage::getMidiNoteName(note, true, true, 4).toStdString() + " " + fmt(cents);
            if (std::abs(cents) > std::abs(worst)) { worst = cents; worstNote = juce::MidiMessage::getMidiNoteName(note, true, true, 4).toStdString(); }
        }
        std::cout << "  harp tuning (cents):" << report << "\n";
        s.check(std::abs(worst) <= 8.0, "glissando: the harp pluck in tune from C2 to C7 (within 8 cents)", "worst " + worstNote + " " + fmt(worst) + " cents");
        s.metric("glissando: harp tuning, worst note (cents)", worst);
    }
}
