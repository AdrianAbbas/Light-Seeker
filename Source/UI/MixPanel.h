#pragma once
// Mix and Punch: the MIX section (both plug-ins). It opens above the controls strip from
// the MIX button in the Levels group (and, in the instrument, from the instrument panel),
// and folds away with the same button or its own close button. Not closed by a click
// elsewhere: it is a mixer you keep open while you play.
//
//   EFFECTS  Reverb, Echoes, Modulation, Shimmer, Character, Atmosphere (0-150 %, 100 % = as designed)
//   REVERB   Low Cut (Off, 80-600 Hz), Pre-delay (-50..+150 ms), Duck (0 % = off)
//
// Light styled (pearl and ivory, gold value tracks, dark text). Cheap: its background is
// one gradient, a border and two headings; nothing animates.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EngravedLookAndFeel.h"

namespace dc::ui {

class MixPanel final : public juce::Component {
public:
    static constexpr int panelWidth = 596, panelHeight = 158;
    static constexpr int count = 9;
    explicit MixPanel(LightSeekerProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    std::function<void()> onClose;
    // Every control back to as designed (one undo step).
    void resetAll();
    // Tests.
    juce::Slider& slider(int k) noexcept { return sliders[static_cast<std::size_t>(k)]; }
    juce::String label(int k) const { return labels[static_cast<std::size_t>(k)].getText(); }
    juce::Button& closeButton() noexcept { return close; }
    juce::Button& resetButton() noexcept { return reset; }
    static const char* parameterId(int k) noexcept;

private:
    LightSeekerProcessor& processor;
    std::array<juce::Slider, count> sliders;
    std::array<juce::Label, count> labels;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>, count> attachments;
    juce::TextButton close, reset { "As designed" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixPanel)
};

} // namespace dc::ui
