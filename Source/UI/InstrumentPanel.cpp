#include "InstrumentPanel.h"
#include "EngravedLookAndFeel.h"

namespace dc::ui {

InstrumentPanel::InstrumentPanel(LightSeekerProcessor& p) : processor(p) {
    setOpaque(true);
    setLookAndFeel(&look);
    family.setFont(labelFont(13.0f));
    family.setColour(juce::Label::textColourId, colours::ash);
    name.setFont(titleFont(19.0f));
    name.setColour(juce::Label::textColourId, colours::ivory);
    name.setMinimumHorizontalScale(0.7f);
    status.setFont(labelFont(12.5f));
    status.setColour(juce::Label::textColourId, colours::ash);
    for (auto* l : { &family, &name, &status }) addAndMakeVisible(*l);
    previous.setButtonText(juce::String::fromUTF8("\xe2\x80\xb9"));
    next.setButtonText(juce::String::fromUTF8("\xe2\x80\xba"));
    previous.setTooltip("Previous instrument (plays it here, locked)");
    next.setTooltip("Next instrument (plays it here, locked)");
    previous.onClick = [this] { step(-1); };
    next.onClick = [this] { step(1); };
    choose.onClick = [this] { showInstrumentMenu(); };
    choose.setTooltip("Play any place's instrument here (locks it)");
    lock.setTooltip("Keep this instrument while you travel; the space still changes");
    for (auto* b : { static_cast<juce::Component*>(&previous), static_cast<juce::Component*>(&next), static_cast<juce::Component*>(&choose),
                     static_cast<juce::Component*>(&lock) })
        addAndMakeVisible(*b);
    if (auto* param = processor.parameters.getParameter(dc::ids::instLock))
        lockAttachment = std::make_unique<juce::ButtonParameterAttachment>(*param, lock);

    const std::array<const char*, 10> ids { dc::ids::instTone, dc::ids::instAttack, dc::ids::instRelease, dc::ids::instCharacter,
                                            dc::ids::instVowel, dc::ids::instEnsemble, dc::ids::instLevel, dc::ids::intensity,
                                            dc::ids::punch, dc::ids::punchTime };
    const std::array<const char*, 10> names { "Tone", "Attack", "Release", "Character", "Vowel", "Ensemble", "Level", "Intensity", "Punch", "Punch Time" };
    for (std::size_t k = 0; k < macros.size(); ++k) {
        auto& s = macros[k];
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxTextColourId, colours::ivory);
        addAndMakeVisible(s);
        auto& l = macroLabels[k];
        l.setText(names[k], juce::dontSendNotification);
        l.setFont(labelFont(13.5f));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, colours::ivory);
        addAndMakeVisible(l);
        if (auto* param = processor.parameters.getParameter(ids[k]))
            macroAttachments[k] = std::make_unique<juce::SliderParameterAttachment>(*param, s);
    }
    macros[4].setTooltip("The voices' vowel: mmm, ooh, oh, aah, ee (50 %: the instrument's own)");
    macros[5].setTooltip("The voices: one singer at 0 %, the instrument's choir at 50 %, eight singers at 100 %");
    macros[7].setTooltip("How much of the place's space (0 %: the instrument dry)");
    macros[8].setTooltip("Gain on hit: each note starts louder and settles to its level (50 %: the instrument's own; up to +12 dB)");
    macros[9].setTooltip("How long the Punch takes to settle (20-500 ms)");
    for (auto* param : { processor.parameters.getParameter(dc::ids::punch), processor.parameters.getParameter(dc::ids::punchTime) })
        if (param != nullptr) {
            auto& s = macros[param == processor.parameters.getParameter(dc::ids::punch) ? 8u : 9u];
            s.setDoubleClickReturnValue(true, static_cast<double>(param->convertFrom0to1(param->getDefaultValue())));
        }
    mix.setClickingTogglesState(true);
    mix.setTooltip("The MIX section: how much reverb, echoes, modulation, shimmer, character and atmosphere");
    mix.onClick = [this] { if (onMix) onMix(); };
    addAndMakeVisible(mix);
    octave.addItemList({ "-2", "-1", "0", "+1", "+2" }, 1);
    addAndMakeVisible(octave);
    if (auto* param = processor.parameters.getParameter(dc::ids::instOctave))
        octaveAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*param, octave);
    octaveLabel.setText("Octave", juce::dontSendNotification);
    octaveLabel.setJustificationType(juce::Justification::centredRight);
    octaveLabel.setFont(labelFont(13.5f));
    octaveLabel.setColour(juce::Label::textColourId, colours::ivory);
    addAndMakeVisible(octaveLabel);

    // The wheels: bend springs back to the middle; mod stays where it is left.
    bend.setRange(-1.0, 1.0);
    bend.setValue(0.0, juce::dontSendNotification);
    bend.setDoubleClickReturnValue(true, 0.0);
    mod.setRange(0.0, 1.0);
    bend.onDragStart = [this] { draggingBend = true; };
    bend.onDragEnd = [this] { draggingBend = false; bend.setValue(0.0); };
    mod.onDragStart = [this] { draggingMod = true; };
    mod.onDragEnd = [this] { draggingMod = false; };
    bend.onValueChange = [this] {
        const int value = juce::jlimit(0, 16383, juce::roundToInt((bend.getValue() + 1.0) * 8191.5));
        processor.postUiMidi(juce::MidiMessage::pitchWheel(1, value));
    };
    mod.onValueChange = [this] { processor.postUiMidi(juce::MidiMessage::controllerEvent(1, 1, juce::roundToInt(mod.getValue() * 127.0))); };
    bend.setTooltip("Pitch bend");
    mod.setTooltip("Modulation: vibrato (tremolo on struck and plucked instruments)");
    for (auto* l : { &bendLabel, &modLabel }) {
        l->setFont(labelFont(12.5f));
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, colours::ivory);
        addAndMakeVisible(*l);
    }
    bendLabel.setText("Bend", juce::dontSendNotification);
    modLabel.setText("Mod", juce::dontSendNotification);
    addAndMakeVisible(bend);
    addAndMakeVisible(mod);
    sustain.setClickingTogglesState(true);
    sustain.setTooltip("The sustain pedal");
    sustain.onClick = [this] { processor.postUiMidi(juce::MidiMessage::controllerEvent(1, 64, sustain.getToggleState() ? 127 : 0)); };
    addAndMakeVisible(sustain);

    keyboard.setAvailableRange(24, 108);
    // Where the keyboard was scrolled to when the window was last open (Direction Update 2).
    restoreLowest = processor.viewState().valid ? processor.viewState().lowestKey : 36;
    keyboard.setLowestVisibleKey(restoreLowest);
    keyboard.setKeyWidth(15.0f);
    keyboard.setScrollButtonsVisible(true);
    keyboard.setMidiChannel(1);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, colours::pearl);
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff3a3026));
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, colours::dust);
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::blood.withAlpha(0.75f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::ember.withAlpha(0.35f));
    keyboard.setColour(juce::MidiKeyboardComponent::shadowColourId, colours::dust.withAlpha(0.4f));
    keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, colours::charcoal);
    keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId, colours::ivory);
    keyboard.setColour(juce::MidiKeyboardComponent::textLabelColourId, colours::ash);
    keyboard.setWantsKeyboardFocus(false);
    addAndMakeVisible(keyboard);
    keyboardState.addListener(this);
    refreshName();
    startTimerHz(15);
    addMouseListener(this, true);   // stale hovers swept on any mouse event in the panel
}

InstrumentPanel::~InstrumentPanel() {
    storeKeyboard();
    stopTimer();
    keyboardState.removeListener(this);
    setLookAndFeel(nullptr);
}

void InstrumentPanel::handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) {
    if (channel == hostChannel) return;
    processor.postUiMidi(juce::MidiMessage::noteOn(1, note, juce::jlimit(0.05f, 1.0f, velocity)));
}

void InstrumentPanel::handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float) {
    if (channel == hostChannel) return;
    processor.postUiMidi(juce::MidiMessage::noteOff(1, note));
}

void InstrumentPanel::step(int direction) {
    const int index = (processor.instrumentIndex() + direction + ls::inst::instrumentCount) % ls::inst::instrumentCount;
    processor.chooseInstrument(index);
    refreshName();
}

void InstrumentPanel::showInstrumentMenu() {
    juce::PopupMenu menu;
    menu.addItem(1000, "Follow the place (unlock)", true, !processor.instrumentLocked());
    menu.addSeparator();
    for (int l = 0; l < dc::LocationCount; ++l) {
        juce::PopupMenu sub;
        for (int k = 0; k <= dc::subLocationCount(l); ++k) {
            const int index = ls::inst::instrumentIndex(l, k);
            sub.addItem(index + 1, juce::String(ls::inst::patch(index).name) + "  (" + dc::subLocationName(l, k) + ")", true,
                        processor.instrumentIndex() == index);
        }
        menu.addSubMenu(dc::locationNames[static_cast<std::size_t>(l)], sub);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&choose), [safe = juce::Component::SafePointer<InstrumentPanel>(this)](int chosen) {
        if (safe == nullptr || chosen <= 0) return;
        if (chosen == 1000) safe->processor.setInstrumentLock(false);
        else safe->processor.chooseInstrument(chosen - 1);
        safe->refreshName();
    });
}

void InstrumentPanel::refreshName() {
    const int index = processor.instrumentIndex();
    const auto& p = ls::inst::patch(index);
    int l = 0, k = 0;
    ls::inst::instrumentPlace(index, l, k);
    name.setText(p.name, juce::dontSendNotification);
    family.setText(juce::String(dc::locationNames[static_cast<std::size_t>(l)]) + juce::String::fromUTF8(" \xc2\xb7 ") + dc::subLocationName(l, k),
                   juce::dontSendNotification);
    macroLabels[3].setText(p.character, juce::dontSendNotification);
    macros[8].updateText();   // the Punch reads in dB of this instrument's own
    // Vowel and Ensemble only act on the voices (dimmed elsewhere).
    const bool voiced = ls::inst::patchHasVoice(p);
    for (std::size_t k : { std::size_t(4), std::size_t(5) }) {
        macros[k].setEnabled(voiced);
        macros[k].setAlpha(voiced ? 1.0f : 0.4f);
        macroLabels[k].setAlpha(voiced ? 1.0f : 0.4f);
    }
    const bool sampled = ls::inst::patchSampled(p);
    bool usesSamples = false;
    usesSamples = ls::inst::patchUsesRecordings(p);
    status.setText(juce::String(p.family) + (usesSamples ? (sampled ? juce::String(", recorded") : juce::String(", synthesised (recordings not found)")) : juce::String()),
                   juce::dontSendNotification);
    shownIndex = index;
    shownLocked = processor.instrumentLocked();
}

void InstrumentPanel::storeKeyboard() {
    const int lowest = keyboard.getLowestVisibleKey();
    if (lowest != storedLowest) {
        storedLowest = lowest;
        processor.updateViewState([lowest](LightSeekerProcessor::ViewState& v) { v.lowestKey = lowest; });
    }
}

void InstrumentPanel::timerCallback() {
    if (processor.instrumentIndex() != shownIndex || processor.instrumentLocked() != shownLocked) refreshName();
    storeKeyboard();
    // The host's notes light the keys (on a channel the panel never sends back).
    for (int note = 0; note < 128; ++note) {
        const bool lit = processor.keyLit(note);
        if (lit == shownLit[static_cast<std::size_t>(note)]) continue;
        shownLit[static_cast<std::size_t>(note)] = lit;
        if (lit) keyboardState.noteOn(hostChannel, note, 0.8f);
        else keyboardState.noteOff(hostChannel, note, 0.0f);
    }
    if (!draggingBend && std::abs(bend.getValue() - processor.bendShown()) > 0.01) bend.setValue(processor.bendShown(), juce::dontSendNotification);
    if (!draggingMod && std::abs(mod.getValue() - processor.modShown()) > 0.01) mod.setValue(processor.modShown(), juce::dontSendNotification);
    if (sustain.getToggleState() != processor.sustainShown() && !sustain.isMouseButtonDown())
        sustain.setToggleState(processor.sustainShown(), juce::dontSendNotification);
}

void InstrumentPanel::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(colours::ink, 0.0f, 0.0f, colours::charcoal, 0.0f, area.getBottom(), false));
    g.fillAll();
    g.setColour(colours::blood.withAlpha(0.7f));
    g.fillRect(0.0f, 0.0f, area.getWidth(), 1.5f);
    g.setColour(colours::dust.withAlpha(0.5f));
    g.fillRect(262.0f, 10.0f, 1.0f, 92.0f);   // instrument | macros
    g.fillRect(12.0f, 108.0f, area.getWidth() - 24.0f, 1.0f);   // the keyboard row
    g.setColour(colours::ash);
    g.setFont(titleFont(10.5f));
    g.drawText("INSTRUMENT", 272, 8, 200, 14, juce::Justification::centredLeft);
}

void InstrumentPanel::resized() {
    // Top row: the instrument (0-262) and its macros (262-); bottom row: the wheels and
    // the pedal (0-150), the keyboard (150-).
    family.setBounds(12, 6, 246, 16);
    name.setBounds(10, 22, 248, 26);
    previous.setBounds(10, 52, 28, 24);
    choose.setBounds(42, 52, 124, 24);
    next.setBounds(170, 52, 28, 24);
    mix.setBounds(204, 52, 52, 24);
    lock.setBounds(10, 80, 140, 24);
    octaveLabel.setBounds(150, 80, 56, 24);
    octave.setBounds(208, 80, 48, 24);
    int x = 280;
    for (std::size_t k = 0; k < macros.size(); ++k) {
        macroLabels[k].setBounds(x - 6, 22, 62, 16);
        macros[k].setBounds(x, 38, 50, 66);
        x += k == 5 || k == 7 ? 62 : 56;
    }
    status.setBounds(x + 4, 22, getWidth() - x - 14, 60);
    bendLabel.setBounds(8, 112, 34, 14);
    modLabel.setBounds(42, 112, 34, 14);
    bend.setBounds(12, 126, 26, 62);
    mod.setBounds(46, 126, 26, 62);
    sustain.setBounds(80, 150, 66, 30);
    const int lowest = keyboard.getLowestVisibleKey();
    keyboard.setBounds(154, 114, getWidth() - 162, getHeight() - 120);
    keyboard.setKeyWidth(juce::jmax(10.0f, static_cast<float>(keyboard.getWidth() - 30) / 50.0f));
    // Where it was scrolled to (as left, Direction Update 2), once it has its width.
    keyboard.setLowestVisibleKey(restoreLowest >= 0 ? restoreLowest : lowest);
    restoreLowest = -1;
}

} // namespace dc::ui
