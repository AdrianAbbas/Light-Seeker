#pragma once
// The look of the controls (light): pearl and ivory panels, warm taupe lines, readable
// dark sepia text and one accent, radiant gold, to sit on the bright painted art. Slim
// faders with pearl caps and a gold value track; text buttons without boxes (a gold
// underline marks what is on); soft shadows; Cinzel for titles and Cormorant Garamond for
// labels (both SIL OFL, embedded). The names below are roles: `ink` the deepest
// background, `ivory` the main text, `blood` the accent (gold here).
#include <juce_gui_basics/juce_gui_basics.h>

namespace dc::ui {
namespace colours {
inline const juce::Colour ink { 0xfff8f4ec };        // pearl: panels, menus, tooltips
inline const juce::Colour charcoal { 0xffefe7d8 };   // ivory: fields, the second panel tone
inline const juce::Colour dust { 0xffb9ab92 };       // warm taupe: lines, disabled
inline const juce::Colour ash { 0xff6f604a };        // soft sepia: secondary text
inline const juce::Colour ivory { 0xff35291b };      // dark sepia: the main text
inline const juce::Colour blood { 0xffc08a22 };      // radiant gold: the accent
inline const juce::Colour ember { 0xfff2c75c };      // bright gold: glows and light motes
inline const juce::Colour pearl { 0xfffffcf6 };      // the faders' caps
}

// Cinzel (titles, place names, group headings).
juce::Font titleFont(float height);
// Cormorant Garamond (labels, buttons, values).
juce::Font labelFont(float height);
// A system font for symbols the serif faces lack (arrows, the gear).
juce::Font symbolFont(float height);

class EngravedLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    EngravedLookAndFeel();
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    juce::Font getLabelFont(juce::Label&) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
};
}
