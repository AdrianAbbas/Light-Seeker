#pragma once
// The instrument edition's panel (Direction Update 1), under the world: the instrument
// of the place (its name and family, choose another, Lock instrument), its macros (Tone,
// Attack, Release and its own Character; Vowel and Ensemble on the voices), Level, Octave and the place's Intensity, the
// pitch-bend and mod wheels, the sustain pedal and an on-screen keyboard lit by the notes
// the host plays. Light styled: pearl and ivory, gold value tracks.
//
// Cheap to draw: the background is two gradients and three lines; a 15 Hz timer only
// touches what changed (lit keys, the wheels following the host, the name), so only those
// components repaint.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "EngravedLookAndFeel.h"
#include "Hover.h"

namespace dc::ui {

class InstrumentPanel final : public juce::Component, private juce::Timer, private juce::MidiKeyboardState::Listener {
public:
    static constexpr int panelHeight = 196;
    explicit InstrumentPanel(LightSeekerProcessor&);
    ~InstrumentPanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    // Stale hovers cleared on every mouse move, enter and exit (Direction Update 2).
    void mouseMove(const juce::MouseEvent& e) override { sweepHover(*this, e.getScreenPosition().toFloat()); }
    void mouseEnter(const juce::MouseEvent& e) override { sweepHover(*this, e.getScreenPosition().toFloat()); }
    void mouseExit(const juce::MouseEvent& e) override { sweepHover(*this, e.getScreenPosition().toFloat()); }

    // Tests.
    juce::String instrumentName() const { return name.getText(); }
    juce::String characterLabel() const { return macroLabels[3].getText(); }
    bool voiceMacrosEnabled() const { return macros[4].isEnabled() && macros[5].isEnabled(); }
    juce::Slider& macroSlider(int k) { return macros[std::size_t(k)]; }
    juce::Button& lockButton() { return lock; }
    juce::Button& nextButton() { return next; }
    juce::Button& previousButton() { return previous; }
    juce::Button& sustainButton() { return sustain; }
    juce::Slider& bendWheel() { return bend; }
    juce::Slider& modWheelSlider() { return mod; }
    juce::MidiKeyboardComponent& keyboardComponent() { return keyboard; }
    juce::MidiKeyboardState& keyboardStateForTest() { return keyboardState; }
    void refreshForTest() { timerCallback(); }
    void chooseForTest(int index) { processor.chooseInstrument(index); timerCallback(); }

private:
    void timerCallback() override;
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void showInstrumentMenu();
    void step(int direction);
    void refreshName();
    void storeKeyboard();
    int storedLowest = -1, restoreLowest = -1;

    LightSeekerProcessor& processor;
    EngravedLookAndFeel look;
    juce::Label family, name, status;
    juce::TextButton previous, next, choose { "Instruments" };
    juce::ToggleButton lock { "Lock instrument" };
    std::unique_ptr<juce::ButtonParameterAttachment> lockAttachment;
    std::array<juce::Slider, 8> macros;      // Tone, Attack, Release, Character, Vowel, Ensemble, Level, Intensity
    std::array<juce::Label, 8> macroLabels;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>, 8> macroAttachments;
    juce::ComboBox octave;
    juce::Label octaveLabel;
    std::unique_ptr<juce::ComboBoxParameterAttachment> octaveAttachment;
    juce::Slider bend { juce::Slider::LinearVertical, juce::Slider::NoTextBox }, mod { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Label bendLabel, modLabel;
    juce::TextButton sustain { "Sustain" };
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    std::array<bool, 128> shownLit {};
    int shownIndex = -1;
    bool shownLocked = false, draggingBend = false, draggingMod = false;
    static constexpr int hostChannel = 16;   // lit keys from the host (never sent back)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentPanel)
};

} // namespace dc::ui
