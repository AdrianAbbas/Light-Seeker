#include "MixPanel.h"

namespace dc::ui {
namespace {
constexpr std::array<const char*, MixPanel::count> ids { dc::ids::mixReverb, dc::ids::mixEchoes, dc::ids::mixModulation, dc::ids::shimmer,
                                                        dc::ids::mixCharacter, dc::ids::mixAtmosphere,
                                                        dc::ids::mixLowCut, dc::ids::mixPreDelay, dc::ids::mixDuck };
constexpr std::array<const char*, MixPanel::count> names { "Reverb", "Echoes", "Modulation", "Shimmer", "Character", "Atmosphere",
                                                          "Low Cut", "Pre-delay", "Duck" };
constexpr std::array<const char*, MixPanel::count> tips {
    "Every reverb and captured space (0 to 150 %; 100 % as designed). The level stays.",
    "The delays, multi-taps, slap echoes and rising delays (0 to 150 %).",
    "Chorus, flutter, tremolo and auto-pan, pitch movement (0 to 150 %; not the shimmer).",
    "Every shimmer, octave shimmer and sparkle in the sound (0 to 150 %; 100 % as designed).",
    "Resonators, bells, harmonic richness, the place's signature effect (0 to 150 %).",
    "The Atmosphere layer's level (0 to 150 % of its own Level; turn it on in Advanced or in the toolbar).",
    "Takes the mud out of the reverbs: everything below 80-600 Hz (Off: as designed).",
    "Starts the reverbs later (or sooner), so the dry note comes first (-50 to +150 ms).",
    "The reverbs dip while you play and bloom in the gaps (0 %: off).",
};
constexpr int faderWidth = 62, left = 12, groupGap = 18;
}

const char* MixPanel::parameterId(int k) noexcept { return ids[static_cast<std::size_t>(std::clamp(k, 0, count - 1))]; }

MixPanel::MixPanel(LightSeekerProcessor& p) : processor(p) {
    setOpaque(true);
    // Double-buffered: the animated scene behind it never redraws its sliders (only a moved
    // slider redraws the cached image).
    setBufferedToImage(true);
    for (std::size_t k = 0; k < sliders.size(); ++k) {
        auto& s = sliders[k];
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 15);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxTextColourId, colours::ash);
        s.setTooltip(tips[k]);
        auto* parameter = processor.parameters.getParameter(ids[k]);
        s.setDoubleClickReturnValue(true, static_cast<double>(parameter->convertFrom0to1(parameter->getDefaultValue())));
        attachments[k] = std::make_unique<juce::SliderParameterAttachment>(*parameter, s);
        addAndMakeVisible(s);
        auto& l = labels[k];
        l.setText(names[k], juce::dontSendNotification);
        l.setFont(labelFont(14.0f));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, colours::ivory.withAlpha(0.92f));
        l.setMinimumHorizontalScale(0.8f);
        addAndMakeVisible(l);
    }
    close.setButtonText(juce::String::fromUTF8("\xe2\x9c\x95"));
    close.setTooltip("Fold the MIX section away");
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(close);
    reset.setTooltip("Every MIX control back to as designed");
    reset.onClick = [this] { resetAll(); };
    addAndMakeVisible(reset);
}

void MixPanel::resetAll() {
    processor.pushUndo();
    for (const auto* id : ids)
        if (auto* parameter = processor.parameters.getParameter(id)) {
            if (std::abs(parameter->getValue() - parameter->getDefaultValue()) <= 1.0e-7f) continue;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
}

void MixPanel::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(colours::pearl, 0.0f, 0.0f, colours::ink, 0.0f, area.getBottom(), false));
    g.fillAll();
    g.setColour(colours::blood.withAlpha(0.75f));
    g.fillRect(0.0f, 0.0f, area.getWidth(), 1.5f);
    g.setColour(colours::dust.withAlpha(0.7f));
    g.drawRect(getLocalBounds(), 1);
    g.setColour(colours::ivory);
    g.setFont(titleFont(15.0f).withExtraKerningFactor(0.2f));
    g.drawText("MIX", left, 6, 80, 20, juce::Justification::centredLeft);
    g.setFont(titleFont(10.0f).withExtraKerningFactor(0.2f));
    g.setColour(colours::ash);
    const int reverbX = left + 6 * faderWidth + groupGap;
    g.drawText("EFFECTS", left, 30, 200, 12, juce::Justification::centredLeft);
    g.drawText("REVERB HELPERS", reverbX, 30, 200, 12, juce::Justification::centredLeft);
    // A gold hairline between the groups.
    const float x = static_cast<float>(reverbX - groupGap / 2);
    juce::ColourGradient rule(colours::blood.withAlpha(0.0f), x, 32.0f, colours::blood.withAlpha(0.0f), x, area.getBottom() - 8.0f, false);
    rule.addColour(0.5, colours::blood.withAlpha(0.55f));
    g.setGradientFill(rule);
    g.fillRect(x, 32.0f, 1.0f, area.getBottom() - 40.0f);
}

void MixPanel::resized() {
    close.setBounds(getWidth() - 34, 4, 28, 24);
    reset.setBounds(getWidth() - 34 - 104, 4, 98, 24);
    const int top = 44, bottom = getHeight() - 6;
    for (int k = 0; k < count; ++k) {
        const int x = left + k * faderWidth + (k >= 6 ? groupGap : 0);
        labels[static_cast<std::size_t>(k)].setBounds(x - 4, bottom - 16, faderWidth + 8, 16);
        sliders[static_cast<std::size_t>(k)].setBounds(x + 2, top, faderWidth - 4, bottom - 16 - top);
    }
}

} // namespace dc::ui
