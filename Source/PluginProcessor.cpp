#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "World/Presets.h"
#include <cmath>
#include <optional>

juce::AudioProcessor::BusesProperties LightSeekerProcessor::busesFor(bool instrument) {
    // The instrument: no audio input, a stereo output. The effect: stereo in and out.
    if (instrument) return BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
    return BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

LightSeekerProcessor::LightSeekerProcessor(bool instrumentEdition)
    : AudioProcessor(busesFor(instrumentEdition)),
      instrumentMode(instrumentEdition),
      parameters(*this, nullptr, "LIGHT_SEEKER", dc::createParameterLayout([this] { return locationIndex(); }, instrumentEdition, [this] { return punchDesignDb(); })) {
    location = parameters.getRawParameterValue(dc::ids::location);
    subLocation = parameters.getRawParameterValue(dc::ids::subLocation);
    intensity = parameters.getRawParameterValue(dc::ids::intensity);
    mix = parameters.getRawParameterValue(dc::ids::mix);
    input = parameters.getRawParameterValue(dc::ids::input);
    output = parameters.getRawParameterValue(dc::ids::output);
    bypass = parameters.getRawParameterValue(dc::ids::bypass);
    travelTime = parameters.getRawParameterValue(dc::ids::travelTime);
    travelSync = parameters.getRawParameterValue(dc::ids::travelSync);
    for (std::size_t m = 0; m < macros.size(); ++m) macros[m] = parameters.getRawParameterValue(dc::ids::macros[m]);
    for (std::size_t i = 0; i < advanced.size(); ++i) advanced[i] = parameters.getRawParameterValue(dc::ids::advanced(i));
    journeyOn = parameters.getRawParameterValue(dc::ids::journeyOn);
    journeyMode = parameters.getRawParameterValue(dc::ids::journeyMode);
    journeyBars = parameters.getRawParameterValue(dc::ids::journeyBars);
    roamOn = parameters.getRawParameterValue(dc::ids::roamOn);
    roamBars = parameters.getRawParameterValue(dc::ids::roamBars);
    roamRandomness = parameters.getRawParameterValue(dc::ids::roamRandomness);
    roamSeed = parameters.getRawParameterValue(dc::ids::roamSeed);
    midiTravel = parameters.getRawParameterValue(dc::ids::midiTravel);
    midiOctave = parameters.getRawParameterValue(dc::ids::midiOctave);
    atmosLevel = parameters.getRawParameterValue(dc::ids::atmosLevel);
    atmosDensity = parameters.getRawParameterValue(dc::ids::atmosDensity);
    atmosSeed = parameters.getRawParameterValue(dc::ids::atmosSeed);
    quality = parameters.getRawParameterValue(dc::ids::quality);
    shimmer = parameters.getRawParameterValue(dc::ids::shimmer);
    mixReverb = parameters.getRawParameterValue(dc::ids::mixReverb);
    mixEchoes = parameters.getRawParameterValue(dc::ids::mixEchoes);
    mixModulation = parameters.getRawParameterValue(dc::ids::mixModulation);
    mixCharacter = parameters.getRawParameterValue(dc::ids::mixCharacter);
    mixAtmosphere = parameters.getRawParameterValue(dc::ids::mixAtmosphere);
    mixLowCut = parameters.getRawParameterValue(dc::ids::mixLowCut);
    mixPreDelay = parameters.getRawParameterValue(dc::ids::mixPreDelay);
    mixDuck = parameters.getRawParameterValue(dc::ids::mixDuck);
    atmosOn = parameters.getRawParameterValue(dc::ids::atmosOn);
    if (instrumentMode) {
        punch = parameters.getRawParameterValue(dc::ids::punch);
        punchTime = parameters.getRawParameterValue(dc::ids::punchTime);
        instTone = parameters.getRawParameterValue(dc::ids::instTone);
        instAttack = parameters.getRawParameterValue(dc::ids::instAttack);
        instRelease = parameters.getRawParameterValue(dc::ids::instRelease);
        instCharacter = parameters.getRawParameterValue(dc::ids::instCharacter);
        instVowel = parameters.getRawParameterValue(dc::ids::instVowel);
        instEnsemble = parameters.getRawParameterValue(dc::ids::instEnsemble);
        instLayer = parameters.getRawParameterValue(dc::ids::instLayer);
        instLevel = parameters.getRawParameterValue(dc::ids::instLevel);
        instOctave = parameters.getRawParameterValue(dc::ids::instOctave);
        instLock = parameters.getRawParameterValue(dc::ids::instLock);
        synth = std::make_unique<ls::inst::Synth>();
        previewSynth = std::make_unique<ls::inst::Synth>();
    }
    // The developer's recordings load on a background thread (once per process): the
    // instrument's, or in the effect only what its Atmosphere layer plays (the breaths).
    lswb::samples::Bank::get().load();
    markPresetBaseline();   // a new instance is "Init", unmodified
}

float LightSeekerProcessor::migratedPunch(float control, int instrument) noexcept {
    const auto& p = ls::inst::patch(instrument);
    if (std::abs(control - 0.5f) < 1.0e-4f || std::abs(p.punchLegacyDb - p.punchDb) < 1.0e-4f) return control;
    return dc::dsp::punchControl(dc::dsp::punchDb(control, p.punchLegacyDb), p.punchDb);
}

float LightSeekerProcessor::migratedLevel(float levelDb, int instrument) noexcept {
    if (std::abs(levelDb) < 1.0e-4f) return levelDb;   // left at its default: the new level
    const auto& p = ls::inst::patch(instrument);
    // How much louder it is now (its stand-in's when its recordings are not there).
    auto& bank = lswb::samples::Bank::get();
    bank.load();
    bank.waitUntilLoaded(3000);
    const bool standIn = ls::inst::patchUsesRecordings(p) && bank.state() != lswb::samples::Bank::Loading && !ls::inst::patchSampled(p);
    return juce::jlimit(-24.0f, 12.0f, levelDb - ls::inst::levelShiftDb(instrument, standIn));
}

float LightSeekerProcessor::punchDesignDb() const noexcept {
    return instrumentMode ? ls::inst::patch(shownInstrument.load(std::memory_order_relaxed)).punchDb : 0.0f;
}

void LightSeekerProcessor::setInstrumentLock(bool on) {
    if (!instrumentMode) return;
    if (on) requestedLock.store(shownInstrument.load());
    setPlain(dc::ids::instLock, on ? 1.0f : 0.0f);
}

void LightSeekerProcessor::chooseInstrument(int index) {
    if (!instrumentMode) return;
    pushUndo();
    requestedLock.store(juce::jlimit(0, ls::inst::instrumentCount - 1, index));
    shownInstrument.store(juce::jlimit(0, ls::inst::instrumentCount - 1, index));
    setPlain(dc::ids::instLock, 1.0f);
}

bool LightSeekerProcessor::startPreview(int instrument) {
    if (!instrumentMode || hostPlaying() || instrument < 0 || instrument >= ls::inst::instrumentCount) return false;
    previewRequest.store(instrument);
    return true;
}

void LightSeekerProcessor::stopPreview() {
    if (instrumentMode) previewRequest.store(-1);
}

void LightSeekerProcessor::renderPreview(float* left, float* right, int numSamples) {
    // A short rising phrase (C E G C, then held) a little below the instrument's level;
    // released when the button is let go (or after a few seconds), refused while the host plays.
    static constexpr std::array<int, 4> phrase { 60, 64, 67, 72 };
    static constexpr double stepSeconds = 0.17, holdSeconds = 3.0;
    const int request = previewRequest.exchange(-2);
    const bool playing = hostIsPlaying.load(std::memory_order_relaxed);
    if ((request == -1 || playing || request >= 0) && previewOn) { previewSynth->allNotesOff(); previewOn = false; }
    if (request >= 0 && !playing) {
        previewSynth->setPatch(request);
        ls::inst::Macros m;
        m.levelDb = previewLevelDb;
        m.shimmer = juce::jlimit(0.0f, dc::dsp::shimmerMax, std::isfinite(shimmer->load()) ? shimmer->load() : 1.0f);
        previewSynth->setMacros(m);
        previewOn = true;
        previewClock = 0;
        previewStep = 0;
    }
    if (!previewOn && previewSynth->activeVoices() == 0) { previewVoicesNow.store(0, std::memory_order_relaxed); return; }
    int at = 0;
    while (at < numSamples) {
        int until = numSamples;
        if (previewOn) {
            const auto next = previewStep < static_cast<int>(phrase.size()) ? static_cast<long long>(stepSeconds * previewStep * rate)
                                                                             : static_cast<long long>(holdSeconds * rate);
            if (next <= previewClock) {
                if (previewStep < static_cast<int>(phrase.size())) previewSynth->noteOn(phrase[static_cast<std::size_t>(previewStep++)], 0.72f);
                else { previewSynth->allNotesOff(); previewOn = false; }
                continue;
            }
            until = static_cast<int>(juce::jmin<long long>(numSamples, at + (next - previewClock)));
        }
        while (at < until) {
            const int n = juce::jmin(until - at, dc::maxChunk);
            auto& l = previewScratch[0];
            auto& r = previewScratch[1];
            std::fill(l.begin(), l.begin() + n, 0.0f);
            std::fill(r.begin(), r.begin() + n, 0.0f);
            previewSynth->render(l.data(), r.data(), n);
            for (int i = 0; i < n; ++i) {
                const auto k = static_cast<std::size_t>(i);
                if (right != nullptr) { left[at + i] += l[k]; right[at + i] += r[k]; }
                else left[at + i] += 0.5f * (l[k] + r[k]);
            }
            at += n;
            previewClock += n;
        }
    }
    previewVoicesNow.store(previewSynth->activeVoices(), std::memory_order_relaxed);
}

void LightSeekerProcessor::postUiMidi(const juce::MidiMessage& m) {
    if (!instrumentMode || m.getRawDataSize() < 1 || m.getRawDataSize() > 3) return;
    const auto* d = m.getRawData();
    const uint32_t packed = uint32_t(d[0]) | (m.getRawDataSize() > 1 ? uint32_t(d[1]) << 8 : 0u) | (m.getRawDataSize() > 2 ? uint32_t(d[2]) << 16 : 0u)
                            | (uint32_t(m.getRawDataSize()) << 24);
    const auto scope = uiFifo.write(1);
    if (scope.blockSize1 > 0) uiMessages[static_cast<std::size_t>(scope.startIndex1)] = packed;
    else if (scope.blockSize2 > 0) uiMessages[static_cast<std::size_t>(scope.startIndex2)] = packed;
}

bool LightSeekerProcessor::keyLit(int note) const noexcept {
    if (note < 0 || note > 127) return false;
    return (litKeys[static_cast<std::size_t>(note >> 5)].load(std::memory_order_relaxed) >> (note & 31)) & 1u;
}

bool LightSeekerProcessor::journeyRunning() const noexcept {
    return journeyOn->load() >= 0.5f && routeLength.load() > 0;
}

bool LightSeekerProcessor::roamRunning() const noexcept { return roamOn->load() >= 0.5f; }

void LightSeekerProcessor::setPlain(const char* id, float plain) {
    if (auto* p = parameters.getParameter(id)) {
        const float value = p->convertTo0to1(plain);
        if (std::abs(p->getValue() - value) <= 1.0e-7f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost(value);
        p->endChangeGesture();
    }
}

juce::String LightSeekerProcessor::stopSteeringLocked() {
    const bool journey = journeyRunning(), roam = roamRunning();
    if (journeyOn->load() >= 0.5f) setPlain(dc::ids::journeyOn, 0.0f);
    if (roam) setPlain(dc::ids::roamOn, 0.0f);
    midiCancel.store(true);
    return journey && roam ? "Journey and Roam" : (journey ? "Journey" : (roam ? "Roam" : ""));
}

juce::String LightSeekerProcessor::travelTo(int l, int sub) {
    l = juce::jlimit(0, dc::LocationCount - 1, l);
    sub = dc::clampSubLocation(l, sub);
    pushUndo();
    juce::String stopped;
    {
        const juce::ScopedValueSetter<bool> guard(applying, true);   // one undo step
        stopped = stopSteeringLocked();
        setPlain(dc::ids::location, static_cast<float>(l));
        setPlain(dc::ids::subLocation, static_cast<float>(sub));
    }
    // After the parameters: a block that sees this epoch also sees them.
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    return stopped;
}

juce::String LightSeekerProcessor::stopSteering() {
    if (!journeyRunning() && !roamRunning()) return {};
    // Stay where the journey is now.
    const int l = currentDestination.load(), k = currentSubLocation.load();
    return travelTo(l, k);
}

void LightSeekerProcessor::applyInit() {
    pushUndo();
    {
        const juce::ScopedValueSetter<bool> guard(applying, true);
        for (auto* p : getParameters()) {
            if (p == getBypassParameter() || p == parameters.getParameter(dc::ids::quality)) continue;
            if (std::abs(p->getValue() - p->getDefaultValue()) > 1.0e-7f) {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->getDefaultValue());
                p->endChangeGesture();
            }
        }
        setRoute({});
        midiCancel.store(true);
    }
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    presetIndex = -1;
    presetName = "Init";
    markPresetBaseline();
}

bool LightSeekerProcessor::presetModified() const {
    if (!hasBaseline) return true;
    const auto now = snapshot();
    if (now.route.size() != presetBaseline.route.size()) return true;
    for (std::size_t k = 0; k < now.route.size(); ++k)
        if (now.route[k].location != presetBaseline.route[k].location || now.route[k].sub != presetBaseline.route[k].sub) return true;
    // Quality and Bypass are settings, not part of a preset.
    const auto& params = getParameters();
    for (int i = 0; i < params.size() && i < static_cast<int>(now.values.size()) && i < static_cast<int>(presetBaseline.values.size()); ++i) {
        auto* p = params[i];
        if (p == getBypassParameter() || p == parameters.getParameter(dc::ids::quality)) continue;
        if (std::abs(now.values[static_cast<std::size_t>(i)] - presetBaseline.values[static_cast<std::size_t>(i)]) > 1.0e-6f) return true;
    }
    return false;
}

int LightSeekerProcessor::qualityMode() const noexcept {
    return juce::jlimit(0, 2, static_cast<int>(std::lround(quality != nullptr ? quality->load() : 1.0f)));
}

bool LightSeekerProcessor::Snapshot::operator==(const Snapshot& o) const {
    if (values != o.values || route.size() != o.route.size()) return false;
    for (std::size_t k = 0; k < route.size(); ++k)
        if (route[k].location != o.route[k].location || route[k].sub != o.route[k].sub) return false;
    return true;
}

LightSeekerProcessor::Snapshot LightSeekerProcessor::snapshot() const {
    Snapshot snap;
    for (auto* p : getParameters()) snap.values.push_back(p->getValue());
    snap.route = route();
    return snap;
}

void LightSeekerProcessor::restore(const Snapshot& snap) {
    const juce::ScopedValueSetter<bool> guard(applying, true);
    const auto& params = getParameters();
    for (int i = 0; i < params.size() && i < static_cast<int>(snap.values.size()); ++i) {
        auto* p = params[i];
        if (p == getBypassParameter() || p == parameters.getParameter(dc::ids::quality)) continue;
        if (std::abs(p->getValue() - snap.values[static_cast<std::size_t>(i)]) > 1.0e-7f) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(snap.values[static_cast<std::size_t>(i)]);
            p->endChangeGesture();
        }
    }
    setRoute(snap.route);
}

void LightSeekerProcessor::pushUndo() {
    if (applying) return;
    auto snap = snapshot();
    if (!undoStack.empty() && undoStack.back() == snap) return;
    undoStack.push_back(std::move(snap));
    if (undoStack.size() > 64) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

bool LightSeekerProcessor::undo() {
    if (undoStack.empty()) return false;
    auto now = snapshot();
    // Skip snapshots identical to the present (an edit that changed nothing).
    while (!undoStack.empty() && undoStack.back() == now) undoStack.pop_back();
    if (undoStack.empty()) return false;
    redoStack.push_back(now);
    restore(undoStack.back());
    undoStack.pop_back();
    return true;
}

bool LightSeekerProcessor::redo() {
    if (redoStack.empty()) return false;
    undoStack.push_back(snapshot());
    restore(redoStack.back());
    redoStack.pop_back();
    return true;
}

void LightSeekerProcessor::selectSlot(int slot) {
    slot = juce::jlimit(0, 1, slot);
    if (slot == abSlot) return;
    slots[static_cast<std::size_t>(abSlot)] = snapshot();
    slotUsed[static_cast<std::size_t>(abSlot)] = true;
    abSlot = slot;
    if (slotUsed[static_cast<std::size_t>(slot)]) { pushUndo(); restore(slots[static_cast<std::size_t>(slot)]); }
    else { slots[static_cast<std::size_t>(slot)] = snapshot(); slotUsed[static_cast<std::size_t>(slot)] = true; }
}

void LightSeekerProcessor::copyCurrentToOtherSlot() {
    const int other = 1 - abSlot;
    slots[static_cast<std::size_t>(other)] = snapshot();
    slotUsed[static_cast<std::size_t>(other)] = true;
}

void LightSeekerProcessor::applyFactoryPreset(int index) {
    const auto& presets = dc::presetsFor(instrumentMode);
    if (index < 0 || index >= static_cast<int>(presets.size())) return;
    pushUndo();
    const auto& preset = presets[static_cast<std::size_t>(index)];
    const juce::ScopedValueSetter<bool> guard(applying, true);
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p)) {
            if (p == getBypassParameter() || p == parameters.getParameter(dc::ids::quality)) continue;
            float value = ranged->getDefaultValue();
            for (const auto& [id, plain] : preset.values)
                if (ranged->getParameterID() == id) value = ranged->convertTo0to1(plain);
            if (std::abs(ranged->getValue() - value) > 1.0e-7f) {
                ranged->beginChangeGesture();
                ranged->setValueNotifyingHost(value);
                ranged->endChangeGesture();
            }
        }
    setRoute(preset.route);
    // An instrument preset that plays another sub-location's instrument here locks it.
    if (instrumentMode && preset.instrument >= 0) {
        requestedLock.store(preset.instrument);
        shownInstrument.store(preset.instrument);
    }
    midiCancel.store(true);
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    presetIndex = index;
    presetName = preset.name;
    markPresetBaseline();
}

juce::File LightSeekerProcessor::userPresetFolder() const {
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile("Application Support");
   #endif
    return base.getChildFile("LightSeeker").getChildFile(instrumentMode ? "Instrument Presets" : "Presets");
}

bool LightSeekerProcessor::saveUserPreset(const juce::String& name) {
    const auto clean = juce::File::createLegalFileName(name.trim());
    if (clean.isEmpty()) return false;
    const auto folder = userPresetFolder();
    if (!folder.createDirectory()) return false;
    juce::MemoryBlock data;
    getStateInformation(data);
    auto xml = getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
    if (xml == nullptr) return false;
    xml->setAttribute("presetName", name.trim());
    const bool ok = xml->writeTo(folder.getChildFile(clean + (instrumentMode ? ".lsinst" : ".lspreset")));
    if (ok) { presetName = name.trim(); presetIndex = -1; markPresetBaseline(); }
    return ok;
}

bool LightSeekerProcessor::loadUserPreset(const juce::File& file) {
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return false;
    pushUndo();
    const int keepQuality = qualityMode();
    const float keepScale = editorScale.load();
    const auto keepView = viewState();
    juce::MemoryBlock data;
    copyXmlToBinary(*xml, data);
    {
        const juce::ScopedValueSetter<bool> guard(applying, true);
        setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    }
    // Presets carry sounds, not the window or the quality setting.
    if (auto* q = parameters.getParameter(dc::ids::quality)) q->setValueNotifyingHost(q->convertTo0to1(static_cast<float>(keepQuality)));
    editorScale.store(keepScale);
    if (keepView.valid) updateViewState([&keepView](ViewState& v) { v = keepView; });
    presetName = xml->getStringAttribute("presetName", file.getFileNameWithoutExtension());
    presetIndex = -1;
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    markPresetBaseline();
    return true;
}

void LightSeekerProcessor::setRoute(const std::vector<dc::Stop>& stops) {
    const int n = juce::jmin(static_cast<int>(stops.size()), dc::maxStops);
    routeLength.store(0);
    for (int k = 0; k < n; ++k) {
        const auto& st = stops[static_cast<std::size_t>(k)];
        const int l = juce::jlimit(0, dc::LocationCount - 1, st.location);
        routeStops[static_cast<std::size_t>(k)].store(l * 8 + dc::clampSubLocation(l, st.sub));
    }
    routeLength.store(n);
}

std::vector<dc::Stop> LightSeekerProcessor::route() const {
    std::vector<dc::Stop> out;
    const int n = routeLength.load();
    for (int k = 0; k < n; ++k) {
        const int code = routeStops[static_cast<std::size_t>(k)].load();
        out.push_back({ code / 8, code % 8 });
    }
    return out;
}

int LightSeekerProcessor::locationIndex() const noexcept {
    const float v = location != nullptr ? location->load() : 0.0f;
    return std::isfinite(v) ? juce::jlimit(0, dc::LocationCount - 1, static_cast<int>(std::lround(v))) : 0;
}

void LightSeekerProcessor::prepareToPlay(double sampleRate, int) {
    rate = std::isfinite(sampleRate) ? juce::jlimit(8000.0, 384000.0, sampleRate) : 48000.0;
    world.prepare(rate);
    atmosphere.prepare(rate);
    limiter.prepare(rate);
    latency = world.latencySamples();
    for (auto& d : dryDelay) d.prepare(static_cast<std::size_t>(latency + 8));
    for (auto* s : { &inputGain, &outputGain, &wetMix }) s->setTime(0.02, rate);
    inputGain.snap(dc::dsp::dbToGain(input->load()));
    outputGain.snap(dc::dsp::dbToGain(output->load()));
    wetMix.snap(mix->load());
    bypassRamp.prepare(0.025, rate);
    bypassRamp.value = bypass->load() >= 0.5f ? 1.0f : 0.0f;
    neutralRamp.prepare(0.03, rate);
    neutralRamp.value = intensity->load() <= neutralIntensity ? 1.0f : 0.0f;
    setLatencySamples(latency);
    // The recordings: give the background load a moment so the first notes are sampled.
    lswb::samples::Bank::get().waitUntilLoaded(instrumentMode ? 1500 : 300);
    if (instrumentMode) {
        synth->prepare(rate);
        previewSynth->prepare(rate);
        previewOn = false;
        uiMidiScratch.ensureSize(2048);
    }
    inputLevel = 0;
    outputLevel = 0;
}

bool LightSeekerProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    if (instrumentMode) {
        // The instrument: no input; a stereo (or mono) output.
        const auto out = layout.getMainOutputChannelSet();
        return layout.getMainInputChannelSet().isDisabled() && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
    }
    // mono->mono, stereo->stereo and mono->stereo (Logic offers it on mono tracks).
    const auto in = layout.getMainInputChannelSet(), out = layout.getMainOutputChannelSet();
    const auto monoOrStereo = [](const juce::AudioChannelSet& set) {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };
    return monoOrStereo(in) && monoOrStereo(out) && in.size() <= out.size();
}

void LightSeekerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    if (instrumentMode) {
        // The instrument plays the host's MIDI (no MIDI travel: the notes are for playing).
        midiLocation = -1;
        midiCancel.store(false);
        blockMidi = &midi;
        process(buffer, false);
        blockMidi = nullptr;
        return;
    }
    // A manual choice of place cancels the place MIDI chose.
    if (midiCancel.exchange(false)) midiLocation = -1;
    // MIDI travel: the last note-on in the travel octave chooses the place.
    if (midiTravel->load() >= 0.5f) {
        const int base = juce::jlimit(0, 8, static_cast<int>(std::lround(midiOctave->load()))) * 12;
        for (const auto meta : midi) {
            const auto message = meta.getMessage();
            if (!message.isNoteOn()) continue;
            dc::Stop chosen;
            if (dc::midiPlace(message.getNoteNumber(), base, midiLocation >= 0 ? midiLocation : locationIndex(), chosen)) {
                midiLocation = chosen.location;
                midiSub = chosen.sub;
            }
        }
    } else midiLocation = -1;
    process(buffer, false);
}
void LightSeekerProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    // The instrument bypassed still plays (dry, without its place); the effect passes its input.
    blockMidi = instrumentMode ? &midi : nullptr;
    process(buffer, true);
    blockMidi = nullptr;
}

void LightSeekerProcessor::renderInstrument(juce::AudioBuffer<float>& buffer, int numSamples) {
    // Which instrument: the place's (where the world is, or travelling to), unless locked.
    const bool lock = instLock->load() >= 0.5f;
    const int request = requestedLock.exchange(-1);
    if (request >= 0) lockedPlace = request;
    // Locked without a choice (automation, a host): the instrument playing now stays.
    if (lock && lockedPlace < 0) lockedPlace = synth->patchIndex();
    // Unlocked: follow the place (a choice made this very block waits for its lock).
    if (!lock && request < 0) lockedPlace = -1;
    wasLocked = lock;
    const int place = lock ? lockedPlace : ls::inst::instrumentIndex(world.destination(), world.destinationSub());
    synth->setPatch(place);
    lockedInstrument.store(lock ? lockedPlace : -1, std::memory_order_relaxed);
    const auto finiteOr = [](float v, float fallback) { return std::isfinite(v) ? v : fallback; };
    ls::inst::Macros m;
    m.tone = finiteOr(instTone->load(), 0.5f);
    m.attack = finiteOr(instAttack->load(), 0.5f);
    m.release = finiteOr(instRelease->load(), 0.5f);
    m.character = finiteOr(instCharacter->load(), 0.5f);
    m.vowel = finiteOr(instVowel->load(), 0.5f);
    m.ensemble = finiteOr(instEnsemble->load(), 0.5f);
    m.layer = finiteOr(instLayer->load(), 0.5f);
    m.levelDb = finiteOr(instLevel->load(), 0.0f);
    m.octave = juce::jlimit(-2, 2, static_cast<int>(std::lround(finiteOr(instOctave->load(), 0.0f))));
    m.shimmer = juce::jlimit(0.0f, dc::dsp::shimmerMax, finiteOr(shimmer->load(), 1.0f));
    m.modulation = juce::jlimit(0.0f, dc::mixMax, finiteOr(mixModulation->load(), 1.0f));
    m.punch = juce::jlimit(0.0f, 1.0f, finiteOr(punch->load(), 0.5f));
    m.punchSeconds = juce::jlimit(0.02f, 0.5f, finiteOr(punchTime->load(), 120.0f) * 0.001f);
    synth->setMacros(m);

    // The on-screen keyboard, wheels and pedal join at the start of the block.
    uiMidiScratch.clear();
    {
        const auto scope = uiFifo.read(uiFifo.getNumReady());
        const auto take = [this](int start, int size) {
            for (int k = 0; k < size; ++k) {
                const uint32_t packed = uiMessages[static_cast<std::size_t>(start + k)];
                const int length = static_cast<int>(packed >> 24);
                const juce::uint8 bytes[3] { juce::uint8(packed & 0xff), juce::uint8((packed >> 8) & 0xff), juce::uint8((packed >> 16) & 0xff) };
                uiMidiScratch.addEvent(bytes, juce::jlimit(1, 3, length), 0);
            }
        };
        take(scope.startIndex1, scope.blockSize1);
        take(scope.startIndex2, scope.blockSize2);
    }
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    buffer.clear(0, numSamples);
    // A mono output: the instrument's right half goes into a scratch row.
    float* r = right != nullptr ? right : work[1].data();
    int at = 0;
    const auto renderTo = [&](int until) {
        until = juce::jlimit(at, numSamples, until);
        while (at < until) {
            const int n = right != nullptr ? until - at : juce::jmin(until - at, dc::maxChunk);
            if (right == nullptr) std::fill(r, r + n, 0.0f);
            synth->render(left + at, right != nullptr ? r + at : r, n);
            if (right == nullptr) for (int i = 0; i < n; ++i) left[at + i] = 0.5f * (left[at + i] + r[i]);
            at += n;
        }
    };
    for (const auto meta : uiMidiScratch) synth->handle(meta.getMessage());
    if (blockMidi != nullptr)
        for (const auto meta : *blockMidi) {
            renderTo(meta.samplePosition);
            synth->handle(meta.getMessage());
        }
    renderTo(numSamples);
    renderPreview(left, right, numSamples);

    // For the editor: lit keys, wheels, pedal.
    for (std::size_t w = 0; w < litKeys.size(); ++w) {
        uint32_t bits = 0;
        for (int b = 0; b < 32; ++b) if (synth->keyDown(static_cast<int>(w) * 32 + b)) bits |= 1u << b;
        litKeys[w].store(bits, std::memory_order_relaxed);
    }
    bendNow.store(synth->bendAmount(), std::memory_order_relaxed);
    modNow.store(synth->modAmount(), std::memory_order_relaxed);
    sustainNow.store(synth->sustainDown(), std::memory_order_relaxed);
    voicesNow.store(synth->activeVoices(), std::memory_order_relaxed);
    shownInstrument.store(synth->patchIndex(), std::memory_order_relaxed);
}

void LightSeekerProcessor::process(juce::AudioBuffer<float>& buffer, bool forcedBypass) {
    juce::ScopedNoDenormals noDenormals;
    const auto begin = juce::Time::getHighResolutionTicks();
    // Read before the parameters (they are written before the epoch moves on).
    const int epoch = manualEpoch.load(std::memory_order_acquire);
    double bpm = 120, beatsPerBar = 4;
    std::optional<double> hostBeats;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto tempo = pos->getBpm())
                if (std::isfinite(*tempo)) bpm = juce::jlimit(20.0, 400.0, *tempo);
            if (auto sig = pos->getTimeSignature())
                if (sig->numerator > 0 && sig->denominator > 0) beatsPerBar = juce::jlimit(1.0, 32.0, sig->numerator * 4.0 / sig->denominator);
            hostIsPlaying.store(pos->getIsPlaying(), std::memory_order_relaxed);
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    if (std::isfinite(*ppq)) hostBeats = *ppq;
        }

    // Inputs and outputs are counted separately: with mono->stereo the right output
    // carries no input and must still be written.
    const int numSamples = buffer.getNumSamples();
    const int bufferChannels = buffer.getNumChannels();
    const int outputs = juce::jmin(bufferChannels, getTotalNumOutputChannels());
    // The instrument's "input" is its own sound, rendered into the output channels below.
    const int inputs = instrumentMode ? outputs : juce::jmin(bufferChannels, getTotalNumInputChannels());
    for (int ch = inputs; ch < bufferChannels; ++ch) buffer.clear(ch, 0, numSamples);
    if (inputs < 1 || outputs < 1 || numSamples <= 0) return;
    auto* left = buffer.getWritePointer(0);
    auto* right = outputs > 1 ? buffer.getWritePointer(1) : nullptr;
    const bool stereoInput = inputs > 1;

    const auto finiteOr = [](float v, float fallback) { return std::isfinite(v) ? v : fallback; };
    const float amount = juce::jlimit(0.0f, 1.0f, finiteOr(intensity->load(), 1));
    controls.location = locationIndex();
    controls.sub = juce::jlimit(0, dc::maxSubLocations, static_cast<int>(std::lround(finiteOr(subLocation->load(), 0))));
    // Journeys, roaming and MIDI travel steer the place (Milestone 9). The position is
    // the host's when it plays (deterministic: the same bar, the same place), otherwise
    // a free-running count at the host's tempo.
    const double beats = hostBeats ? *hostBeats : freeBeats;
    freeBeats = hostBeats ? *hostBeats : freeBeats + static_cast<double>(buffer.getNumSamples()) / rate * bpm / 60.0;
    bool steering = false;
    if (roamOn->load() >= 0.5f) {
        const auto rateIndex = static_cast<std::size_t>(juce::jlimit(0, static_cast<int>(dc::roamBarChoices.size()) - 1, static_cast<int>(std::lround(roamBars->load()))));
        const double beatsPerStep = dc::roamBarChoices[rateIndex] * beatsPerBar;
        const auto step = static_cast<long long>(std::floor(std::max(0.0, beats) / beatsPerStep));
        controls.sub = dc::roamPlace(controls.location, step, juce::jlimit(0.0f, 1.0f, finiteOr(roamRandomness->load(), 0.5f)),
                                     juce::jlimit(1, 999, static_cast<int>(std::lround(roamSeed->load()))));
        steering = true;
    }
    const int stops = routeLength.load(std::memory_order_acquire);
    if (journeyOn->load() >= 0.5f && stops > 0) {
        const int index = dc::journeyStop(beats, stops, juce::jlimit(1.0f, 16.0f, journeyBars->load()) * beatsPerBar,
                                          juce::jlimit(0, 2, static_cast<int>(std::lround(journeyMode->load()))));
        const int code = routeStops[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
        controls.location = code / 8;
        controls.sub = code % 8;
        steering = true;
    }
    if (midiTravel->load() >= 0.5f && midiLocation >= 0) {
        controls.location = midiLocation;
        controls.sub = midiSub;
        steering = true;
    }
    steered.store(steering, std::memory_order_relaxed);
    controls.intensity = amount;
    controls.shimmer = juce::jlimit(0.0f, dc::dsp::shimmerMax, finiteOr(shimmer->load(), 1.0f));
    // Mix: the MIX groups and the reverb helpers. The instrument adds its own: how much
    // of the place's reverbs it takes, and the pre-delay and duck that put its dry hit first.
    {
        auto& mx = controls.mix;
        mx.reverb = juce::jlimit(0.0f, dc::mixMax, finiteOr(mixReverb->load(), 1.0f));
        mx.echoes = juce::jlimit(0.0f, dc::mixMax, finiteOr(mixEchoes->load(), 1.0f));
        mx.modulation = juce::jlimit(0.0f, dc::mixMax, finiteOr(mixModulation->load(), 1.0f));
        mx.character = juce::jlimit(0.0f, dc::mixMax, finiteOr(mixCharacter->load(), 1.0f));
        const float cut = finiteOr(mixLowCut->load(), 0.0f);
        mx.lowCutHz = cut > dc::lowCutOff ? juce::jlimit(80.0f, dc::lowCutMax, cut) : 0.0f;
        mx.preDelayMs = juce::jlimit(dc::preDelayMin, dc::preDelayMax, finiteOr(mixPreDelay->load(), 0.0f));
        mx.duck = juce::jlimit(0.0f, 1.0f, finiteOr(mixDuck->load(), 0.0f));
        if (instrumentMode) {
            const auto& patch = ls::inst::patch(synth->patchIndex());
            mx.reverb = juce::jlimit(0.0f, dc::mixMax, mx.reverb * patch.space);
            mx.character = juce::jlimit(0.0f, dc::mixMax, mx.character * patch.spaceCharacter);
            mx.preDelayMs = juce::jlimit(dc::preDelayMin, dc::preDelayMax, mx.preDelayMs + patch.spacePreDelayMs);
            mx.duck = 1.0f - (1.0f - mx.duck) * (1.0f - juce::jlimit(0.0f, 1.0f, patch.spaceDuck));
        }
    }
    for (std::size_t m = 0; m < macros.size(); ++m) controls.macros[m] = juce::jlimit(0.0f, 1.0f, finiteOr(macros[m]->load(), 0.5f));
    for (std::size_t i = 0; i < advanced.size(); ++i) controls.advanced[i] = finiteOr(advanced[i]->load(), 0);
    const auto sync = static_cast<std::size_t>(juce::jlimit(0, static_cast<int>(dc::travelSyncBeats.size()) - 1,
                                                            static_cast<int>(std::lround(finiteOr(travelSync->load(), 0)))));
    controls.travelSeconds = sync == 0 ? static_cast<double>(juce::jlimit(0.0f, 10.0f, finiteOr(travelTime->load(), 1.5f)))
                                       : dc::dsp::beatsToSeconds(dc::travelSyncBeats[sync], bpm);
    controls.bpm = bpm;
    world.setQuality(qualityMode());
    world.setControls(controls);
    if (instrumentMode) renderInstrument(buffer, numSamples);
    inputGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 24.0f, finiteOr(input->load(), 0))));
    outputGain.set(dc::dsp::dbToGain(juce::jlimit(-24.0f, 12.0f, finiteOr(output->load(), 0))));
    wetMix.set(juce::jlimit(0.0f, 1.0f, finiteOr(mix->load(), 1)));
    const float bypassTarget = forcedBypass || bypass->load() >= 0.5f ? 1.0f : 0.0f;
    const float neutralTarget = amount <= neutralIntensity ? 1.0f : 0.0f;

    // The Atmosphere: its level times the MIX Atmosphere control, while it is on.
    const float atmosAmount = atmosOn->load() < 0.5f ? 0.0f : juce::jlimit(0.0f, 1.0f, finiteOr(atmosLevel->load(), 0));
    const float atmosMix = juce::jlimit(0.0f, dc::mixMax, finiteOr(mixAtmosphere->load(), 1.0f));
    const float atmosDense = juce::jlimit(0.0f, 1.0f, finiteOr(atmosDensity->load(), 0.35f));
    const int atmosSeedValue = juce::jlimit(1, 999, static_cast<int>(std::lround(finiteOr(atmosSeed->load(), 1))));
    const double beatsPerSample = bpm / 60.0 / rate;
    float inPeak = 0, outPeak = 0;
    for (int start = 0; start < numSamples; start += dc::maxChunk) {
        const int n = juce::jmin(dc::maxChunk, numSamples - start);
        float* l = work[0].data();
        float* r = work[1].data();
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const float rawL = left[start + i], rawR = stereoInput ? right[start + i] : rawL;
            const float x0 = std::isfinite(rawL) ? juce::jlimit(-32.0f, 32.0f, rawL) : 0.0f;
            const float x1 = std::isfinite(rawR) ? juce::jlimit(-32.0f, 32.0f, rawR) : 0.0f;
            inPeak = juce::jmax(inPeak, std::abs(x0), std::abs(x1));
            dryDelay[0].push(x0);
            dryDelay[1].push(x1);
            dryAligned[0][k] = dryDelay[0].readInteger(static_cast<std::size_t>(latency) + 1);
            dryAligned[1][k] = dryDelay[1].readInteger(static_cast<std::size_t>(latency) + 1);
            const float g = inputGain.next();
            l[i] = x0 * g;
            r[i] = x1 * g;
        }
        world.process(l, r, n, dryAligned[0].data(), dryAligned[1].data());
        // The Atmosphere joins the wet signal (Mix, Intensity 0 and Bypass remove it too).
        if (atmosAmount > 0.0f || atmosphere.activeVoices() > 0)
            atmosphere.process(l, r, n, beats + beatsPerSample * start, beatsPerSample, world.destination(), world.destinationSub(),
                               atmosAmount, atmosDense, atmosSeedValue, dryAligned[0].data(), dryAligned[1].data(), atmosMix);
        for (int i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            const float m = wetMix.next(), og = outputGain.next(), bypassed = bypassRamp.next(bypassTarget);
            const float dry = neutralRamp.next(neutralTarget);
            const float dl = dryAligned[0][k], dr = dryAligned[1][k];
            float ol = (dl + (l[i] - dl) * m) * og, orr = (dr + (r[i] - dr) * m) * og;
            ol += dry * (dl * og - ol);
            orr += dry * (dr * og - orr);
            limiter.process(ol, orr);
            ol += bypassed * (dl - ol);
            orr += bypassed * (dr - orr);
            if (!std::isfinite(ol)) ol = 0;
            if (!std::isfinite(orr)) orr = 0;
            if (right != nullptr) { left[start + i] = ol; right[start + i] = orr; }
            else left[start + i] = 0.5f * (ol + orr);
            outPeak = juce::jmax(outPeak, std::abs(ol), std::abs(orr));
        }
    }
    inputLevel.store(juce::jmax(inPeak, inputLevel.load() * 0.85f), std::memory_order_relaxed);
    outputLevel.store(juce::jmax(outPeak, outputLevel.load() * 0.85f), std::memory_order_relaxed);
    travelProgress.store(world.travelProgress(), std::memory_order_relaxed);
    duckShown.store(world.duckGainNow(), std::memory_order_relaxed);
    currentDestination.store(world.destination(), std::memory_order_relaxed);
    currentSubLocation.store(world.destinationSub(), std::memory_order_relaxed);
    processedEpoch.store(epoch, std::memory_order_release);
    const auto seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin);
    cpuLoad.store(0.9f * cpuLoad.load() + 0.1f * static_cast<float>(seconds * rate / numSamples), std::memory_order_relaxed);
}

LightSeekerProcessor::ViewState LightSeekerProcessor::viewState() const {
    const juce::SpinLock::ScopedLockType sl(viewLock);
    return view;
}

void LightSeekerProcessor::updateViewState(const std::function<void(ViewState&)>& change) {
    const juce::SpinLock::ScopedLockType sl(viewLock);
    change(view);
    view.valid = true;
}

juce::AudioProcessorParameter* LightSeekerProcessor::getBypassParameter() const { return parameters.getParameter(dc::ids::bypass); }
juce::AudioProcessorEditor* LightSeekerProcessor::createEditor() { return new LightSeekerEditor(*this); }

void LightSeekerProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto state = parameters.copyState();
    state.setProperty("version", 4, nullptr);
    // GarageBand hotfix: the Punch design (2: the struck instruments' new defaults). States
    // without it come from before; see setStateInformation.
    state.setProperty("punchDesign", 2, nullptr);
    // The loudness rebalance: the instruments' level design (1: each matched on its own part in
    // its place). States without it come from before; see setStateInformation.
    state.setProperty("levelDesign", 1, nullptr);
    state.setProperty("editorScale", static_cast<double>(editorScale.load()), nullptr);
    state.setProperty("presetName", presetName, nullptr);
    state.setProperty("presetModified", presetModified(), nullptr);
    juce::StringArray stops;
    for (const auto& st : route()) stops.add(juce::String(st.location) + ":" + juce::String(st.sub));
    state.setProperty("route", stops.joinIntoString(","), nullptr);
    if (instrumentMode) state.setProperty("lockedInstrument", lockedInstrument.load() >= 0 ? lockedInstrument.load() : shownInstrument.load(), nullptr);
    // The window as it was left (Direction Update 2): a child older versions ignore.
    if (const auto v = viewState(); v.valid) {
        juce::ValueTree w("EditorView");
        w.setProperty("place", v.place, nullptr);
        w.setProperty("location", v.location, nullptr);
        w.setProperty("sub", v.sub, nullptr);
        w.setProperty("controls", v.controls, nullptr);
        w.setProperty("advanced", v.advanced, nullptr);
        w.setProperty("journey", v.journey, nullptr);
        w.setProperty("advancedScroll", v.advancedScroll, nullptr);
        w.setProperty("journeyScroll", v.journeyScroll, nullptr);
        w.setProperty("lowestKey", v.lowestKey, nullptr);
        w.setProperty("mix", v.mix, nullptr);
        state.appendChild(w, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, data);
}

void LightSeekerProcessor::setStateInformation(const void* data, int size) {
    // Malformed or foreign data is ignored; values are clamped; missing parameters keep
    // their defaults (a state from an older version stays loadable).
    if (data == nullptr || size <= 0 || size > 1024 * 1024) return;
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;
    const auto incoming = juce::ValueTree::fromXml(*xml);
    std::vector<std::pair<juce::RangedAudioParameter*, float>> values;
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
            values.push_back({ ranged, ranged->convertFrom0to1(ranged->getDefaultValue()) });
    for (const auto& child : incoming) {
        const auto id = child.getProperty("id").toString();
        for (auto& [parameter, value] : values)
            if (parameter->getParameterID() == id) {
                const float v = static_cast<float>(child.getProperty("value", value));
                if (!std::isfinite(v)) return;
                value = parameter->getNormalisableRange().snapToLegalValue(v);
            }
    }
    // GarageBand hotfix: the struck and plucked instruments' Punch defaults changed. A
    // session from before with Punch left at its default (50 %) simply gets the new default;
    // one where Punch had been moved keeps the dB it had on the instrument it was saved
    // with (the control is re-expressed against the new default).
    // The loudness rebalance: the instruments' levels changed. A session from before with
    // Level left at its default (0 dB) simply gets the new level; one where Level had been
    // moved keeps the loudness it had on the instrument it was saved with.
    const bool oldPunch = !incoming.hasProperty("punchDesign"), oldLevel = !incoming.hasProperty("levelDesign");
    if (instrumentMode && (oldPunch || oldLevel)) {
        float location = 0, sub = 0, locked = 0, *punchValue = nullptr, *levelValue = nullptr;
        for (auto& [parameter, value] : values) {
            if (parameter->getParameterID() == dc::ids::location) location = value;
            else if (parameter->getParameterID() == dc::ids::subLocation) sub = value;
            else if (parameter->getParameterID() == dc::ids::instLock) locked = value;
            else if (parameter->getParameterID() == dc::ids::punch) punchValue = &value;
            else if (parameter->getParameterID() == dc::ids::instLevel) levelValue = &value;
        }
        // The instrument it played: the locked one, else its place's.
        const int index = locked >= 0.5f && incoming.hasProperty("lockedInstrument")
                              ? juce::jlimit(0, ls::inst::instrumentCount - 1, static_cast<int>(incoming.getProperty("lockedInstrument")))
                              : ls::inst::instrumentIndex(juce::roundToInt(location), juce::roundToInt(sub));
        if (oldPunch && punchValue != nullptr) *punchValue = migratedPunch(*punchValue, index);
        if (oldLevel && levelValue != nullptr) *levelValue = migratedLevel(*levelValue, index);
    }
    for (auto& [parameter, value] : values) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    // The window's scale and the preset name (version 4).
    if (incoming.hasProperty("editorScale")) {
        const auto scale = static_cast<float>(static_cast<double>(incoming.getProperty("editorScale")));
        if (std::isfinite(scale)) editorScale.store(juce::jlimit(0.5f, 2.0f, scale));
    }
    if (incoming.hasProperty("presetName")) presetName = incoming.getProperty("presetName").toString();
    // The window as it was left (Direction Update 2), when the state has it.
    if (const auto w = incoming.getChildWithName("EditorView"); w.isValid()) {
        updateViewState([&w](ViewState& v) {
            v.place = static_cast<bool>(w.getProperty("place", false));
            v.location = juce::jlimit(0, dc::LocationCount - 1, static_cast<int>(w.getProperty("location", 0)));
            v.sub = juce::jlimit(0, dc::maxSubLocations, static_cast<int>(w.getProperty("sub", 0)));
            v.controls = static_cast<bool>(w.getProperty("controls", true));
            v.advanced = static_cast<bool>(w.getProperty("advanced", false));
            v.journey = static_cast<bool>(w.getProperty("journey", false)) && !v.advanced;
            v.advancedScroll = juce::jlimit(0, 100000, static_cast<int>(w.getProperty("advancedScroll", 0)));
            v.journeyScroll = juce::jlimit(0, 100000, static_cast<int>(w.getProperty("journeyScroll", 0)));
            v.lowestKey = juce::jlimit(0, 127, static_cast<int>(w.getProperty("lowestKey", 36)));
            v.mix = static_cast<bool>(w.getProperty("mix", false));
        });
    }
    // The journey route (version 3); older states have none.
    std::vector<dc::Stop> stops;
    for (const auto& token : juce::StringArray::fromTokens(incoming.getProperty("route").toString(), ",", ""))
        if (token.containsChar(':') && static_cast<int>(stops.size()) < dc::maxStops)
            stops.push_back({ juce::jlimit(0, dc::LocationCount - 1, token.upToFirstOccurrenceOf(":", false, false).getIntValue()),
                              juce::jlimit(0, dc::maxSubLocations, token.fromFirstOccurrenceOf(":", false, false).getIntValue()) });
    setRoute(stops);
    // The instrument a locked instance plays (Direction Update 1).
    if (instrumentMode && incoming.hasProperty("lockedInstrument") && instLock->load() >= 0.5f) {
        const int index = juce::jlimit(0, ls::inst::instrumentCount - 1, static_cast<int>(incoming.getProperty("lockedInstrument")));
        requestedLock.store(index);
        shownInstrument.store(index);
    }
    manualEpoch.fetch_add(1, std::memory_order_acq_rel);
    // Unmodified when saved: what was loaded is the preset; otherwise it stays modified.
    if (!applying) {
        if (static_cast<bool>(incoming.getProperty("presetModified", false))) hasBaseline = false;
        else markPresetBaseline();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LightSeekerProcessor(JucePlugin_IsSynth != 0); }
