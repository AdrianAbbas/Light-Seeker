#include "PluginEditor.h"
#include "World/Presets.h"

using namespace dc::ui;

namespace {
constexpr const char* moduleNames[] = { "I. Horizon", "II. Sunwarmth", "III. The Choir", "IV. Aura",
                                        "V. Echoes", "VI. Halls", "VII. Charms", "VIII. Signature",
                                        "IX. Radiance", "X. Ensemble" };
// The first module parameter of each module (Frame order).
constexpr dc::P moduleStarts[] = { dc::Cutoff, dc::Drive, dc::Voices, dc::Blur, dc::DelayTime, dc::Space, dc::Flicker, dc::SigKind, dc::VaultKind, dc::EnsMix };
int moduleOf(std::size_t index) {
    int group = 0;
    for (int g = 0; g < 10; ++g) if (index >= static_cast<std::size_t>(moduleStarts[g])) group = g;
    return group;
}
}

//==============================================================================
LightSeekerView::Drawer::Drawer(juce::AudioProcessorValueTreeState& state) {
    setOpaque(true);
    hint.setText("Advanced: every module parameter, as an offset from the place (0 % = as designed) or an override "
                 "(\"Place\" = as designed). Offsets travel with you.", juce::dontSendNotification);
    hint.setFont(labelFont(13.0f));
    hint.setColour(juce::Label::textColourId, colours::ash);
    addAndMakeVisible(hint);
    close.setButtonText(juce::String::fromUTF8("\xe2\x9c\x95"));
    close.setTooltip("Close (Esc)");
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(close);
    setWantsKeyboardFocus(true);
    int group = -1;
    for (std::size_t i = 0; i < dc::Count; ++i) {
        if (moduleOf(i) != group) {
            group = moduleOf(i);
            auto heading = std::make_unique<juce::Label>();
            heading->setText(moduleNames[group], juce::dontSendNotification);
            heading->setFont(titleFont(15.0f));
            heading->setColour(juce::Label::textColourId, colours::blood.brighter(0.3f));
            content.addAndMakeVisible(*heading);
            headings.push_back(std::move(heading));
        }
        auto row = std::make_unique<Row>();
        row->group = group;
        row->label.setText(dc::moduleSpecs[i].name, juce::dontSendNotification);
        row->label.setFont(labelFont(13.0f));
        content.addAndMakeVisible(row->label);
        auto* parameter = state.getParameter(dc::ids::advanced(i));
        if (dc::moduleSpecs[i].glide == dc::Glide::Step) {
            row->box = std::make_unique<juce::ComboBox>();
            row->box->addItemList(dc::advancedChoices(dc::moduleSpecs[i]), 1);
            row->boxAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*parameter, *row->box);
            content.addAndMakeVisible(*row->box);
        } else {
            row->slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            row->slider->setTextBoxStyle(juce::Slider::TextBoxRight, true, 56, 18);
            row->slider->setDoubleClickReturnValue(true, 0.0);
            row->slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); row->slider->setColour(juce::Slider::textBoxTextColourId, colours::ash);
            row->sliderAttachment = std::make_unique<juce::SliderParameterAttachment>(*parameter, *row->slider);
            content.addAndMakeVisible(*row->slider);
        }
        rows.push_back(std::move(row));
    }
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void LightSeekerView::Drawer::paint(juce::Graphics& g) {
    g.fillAll(colours::ink);
    g.setColour(colours::dust.withAlpha(0.6f));
    g.drawRect(getLocalBounds(), 1);
}

void LightSeekerView::Drawer::resized() {
    auto area = getLocalBounds().reduced(10);
    auto top = area.removeFromTop(34);
    close.setBounds(top.removeFromRight(28).withSizeKeepingCentre(28, 26));
    hint.setBounds(top);
    viewport.setBounds(area);
    // Two columns of rows, headings spanning their column.
    const int width = viewport.getWidth() - viewport.getScrollBarThickness() - 4;
    const int columnWidth = width / 2, rowHeight = 24, headingHeight = 26;
    std::array<int, 2> y { 0, 0 };
    int headingIndex = -1, group = -1, column = 0;
    for (auto& row : rows) {
        if (row->group != group) {
            group = row->group;
            ++headingIndex;
            column = y[0] <= y[1] ? 0 : 1;
            headings[static_cast<std::size_t>(headingIndex)]->setBounds(column * columnWidth, y[static_cast<std::size_t>(column)], columnWidth - 8, headingHeight);
            y[static_cast<std::size_t>(column)] += headingHeight;
        }
        auto r = juce::Rectangle<int>(column * columnWidth, y[static_cast<std::size_t>(column)], columnWidth - 12, rowHeight);
        row->label.setBounds(r.removeFromLeft(120));
        if (row->slider) row->slider->setBounds(r.reduced(0, 2));
        if (row->box) row->box->setBounds(r.reduced(0, 2).withWidth(juce::jmin(r.getWidth(), 170)));
        y[static_cast<std::size_t>(column)] += rowHeight;
    }
    content.setSize(width, juce::jmax(y[0], y[1]) + 8);
}

//==============================================================================
namespace {
constexpr float stripHeight = 140.0f;
double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }
float ease(float t) { t = juce::jlimit(0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); }
}

void LightSeekerView::LandmarkButton::paintHover(juce::Graphics& g, bool highlighted, bool) {
    if (!highlighted) return;
    // A soft pale halo and a hairline ring around the landmark.
    const auto r = getLocalBounds().toFloat().reduced(3.0f);
    g.setGradientFill(juce::ColourGradient(colours::ivory.withAlpha(0.14f), r.getCentre(), colours::ivory.withAlpha(0.0f),
                                           r.getCentre().translated(r.getWidth() * 0.5f, 0.0f), true));
    g.fillEllipse(r);
    g.setColour(colours::ivory.withAlpha(0.7f));
    g.drawEllipse(r.reduced(2.0f), 0.9f);
}

void LightSeekerView::HotspotButton::paintHover(juce::Graphics& g, bool highlighted, bool) {
    const auto r = getLocalBounds().toFloat();
    const auto ring = juce::Rectangle<float>(22.0f, 22.0f).withCentre({ r.getCentreX(), 14.0f });
    const bool here = getToggleState();
    // A dark halo, a hairline ring and a small centre: crimson where you are.
    g.setGradientFill(juce::ColourGradient(colours::ink.withAlpha(0.55f), ring.getCentre(), colours::ink.withAlpha(0.0f),
                                           ring.getCentre().translated(ring.getWidth() * 0.9f, 0.0f), true));
    g.fillEllipse(ring.expanded(8.0f));
    if (here) {
        g.setGradientFill(juce::ColourGradient(colours::blood.withAlpha(0.45f), ring.getCentre(), colours::blood.withAlpha(0.0f),
                                               ring.getCentre().translated(ring.getWidth() * 0.8f, 0.0f), true));
        g.fillEllipse(ring.expanded(6.0f));
    }
    g.setColour(here ? colours::blood.brighter(0.2f) : colours::ivory.withAlpha(highlighted ? 0.95f : 0.7f));
    g.drawEllipse(ring.reduced(2.0f), here ? 1.6f : 1.0f);
    g.fillEllipse(ring.reduced(8.0f));
    if (highlighted || here) {
        const auto font = labelFont(14.0f);
        const auto text = getButtonText();
        const float width = juce::GlyphArrangement::getStringWidth(font, text) + 18.0f;
        const auto plate = juce::Rectangle<float>(width, 20.0f).withCentre({ r.getCentreX(), 38.0f });
        g.setGradientFill(juce::ColourGradient(colours::ink.withAlpha(0.75f), plate.getCentre(), colours::ink.withAlpha(0.0f),
                                               plate.getCentre().translated(plate.getWidth() * 0.62f, 0.0f), true));
        g.fillRect(plate.expanded(8.0f, 4.0f));
        g.setFont(font);
        g.setColour(here ? colours::blood.brighter(0.45f) : colours::ivory);
        g.drawText(text, plate, juce::Justification::centred);
    }
}

void LightSeekerView::PlaceButton::paintHover(juce::Graphics& g, bool hover, bool down) {
    const auto bounds = getLocalBounds().toFloat().reduced(3.0f, 3.0f);
    const bool selected = getToggleState();
    const auto text = getButtonText();
    const auto font = labelFont(juce::jmin(16.0f, static_cast<float>(getHeight()) * 0.58f));
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, text);
    // Hover (only while the mouse is over it): a soft glow behind the name.
    if (hover || down) {
        g.setGradientFill(juce::ColourGradient(colours::ivory.withAlpha(down ? 0.12f : 0.08f), bounds.getCentre(),
                                               colours::ivory.withAlpha(0.0f), bounds.getCentre().translated(bounds.getWidth() * 0.6f, 0.0f), true));
        g.fillRect(bounds);
    }
    // Selected (stable, whatever the mouse does): a gold diamond before the name and a gold
    // rule under it as wide as the name.
    if (selected) {
        const float cx = bounds.getCentreX() - textWidth * 0.5f - 10.0f, cy = bounds.getCentreY();
        juce::Path diamond;
        diamond.addQuadrilateral(cx, cy - 3.5f, cx + 3.5f, cy, cx, cy + 3.5f, cx - 3.5f, cy);
        g.setColour(colours::blood);
        g.fillPath(diamond);
        const auto rule = juce::Rectangle<float>(textWidth + 8.0f, 1.5f).withCentre({ bounds.getCentreX(), bounds.getBottom() - 1.0f });
        juce::ColourGradient grad(colours::blood.withAlpha(0.0f), rule.getX(), 0, colours::blood.withAlpha(0.0f), rule.getRight(), 0, false);
        grad.addColour(0.5, colours::blood);
        grad.addColour(0.15, colours::blood.withAlpha(0.8f));
        grad.addColour(0.85, colours::blood.withAlpha(0.8f));
        g.setGradientFill(grad);
        g.fillRect(rule);
    } else if (hover) {
        // A faint rule under a hovered name: what a click would choose.
        const auto rule = juce::Rectangle<float>(textWidth * 0.6f, 1.0f).withCentre({ bounds.getCentreX(), bounds.getBottom() - 1.0f });
        g.setColour(colours::blood.withAlpha(0.35f));
        g.fillRect(rule);
    }
    g.setFont(font);
    g.setColour(selected ? colours::ivory : (hover ? colours::ivory.withAlpha(0.92f) : colours::ash));
    g.drawText(text, getLocalBounds().reduced(4, 1), juce::Justification::centred, true);
}

LightSeekerView::LightSeekerView(LightSeekerProcessor& p)
    : processor(p), cache([this] { sceneReady(); }), drawer(p.parameters),
      journeyPanel(p, [this] { return dc::Stop { shown, dc::clampSubLocation(shown, shownSub) }; }),
      searchPanel(dc::search::index(p.isInstrument()), p.recentSearches) {
    setLookAndFeel(&look);
    setOpaque(true);
    auto* locationParameter = processor.parameters.getParameter(dc::ids::location);
    auto* subParameter = processor.parameters.getParameter(dc::ids::subLocation);

    for (int i = 0; i < dc::LocationCount; ++i) {
        auto& button = landmarks[static_cast<std::size_t>(i)];
        button = std::make_unique<LandmarkButton>(dc::locationNames[static_cast<std::size_t>(i)]);
        button->setClickingTogglesState(false);
        button->setTooltip(dc::locationNames[static_cast<std::size_t>(i)]);
        // Choosing a location travels to its own scene (sub-location 0); the path is
        // drawn, then the view zooms in.
        button->onClick = [this, i] {
            const bool already = i == shown;
            travelManually(i, 0);
            if (already) goTo(View::Place, { i, 0 }, 1);
            else { zoomAt = nowSeconds() + 0.55; wakeTimer(); }
        };
        // The landmark's name plate follows its own hover flag (cleared on exit, when
        // hidden, and by the sweep), so it never stays up.
        button->onHover = [this, i] {
            const bool over = landmarks[static_cast<std::size_t>(i)]->hot();
            if (over != (hovered == i)) { hovered = over ? i : (hovered == i ? -1 : hovered); repaint(); }
        };
        addAndMakeVisible(*button);
    }
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& spot = hotspots[static_cast<std::size_t>(k)];
        spot = std::make_unique<HotspotButton>();
        spot->onClick = [this, k] { travelManually(shown, k); };
        addChildComponent(*spot);
        auto& button = subs[static_cast<std::size_t>(k)];
        button = std::make_unique<PlaceButton>();
        button->setClickingTogglesState(false);
        button->onClick = [this, k] { travelManually(shown, k); };
        addChildComponent(*button);
    }
    locationAttachment = std::make_unique<juce::ParameterAttachment>(*locationParameter, [this](float value) { showLocation(juce::roundToInt(value)); });
    subLocationAttachment = std::make_unique<juce::ParameterAttachment>(*subParameter, [this](float value) { showSubLocation(juce::roundToInt(value)); });

    // Breadcrumb: Map > Location > Place.
    mapCrumb.onClick = [this] { goTo(View::Map, { -1, 0 }, 2); };
    locationCrumb.onClick = [this] {
        if (view == View::Map) goTo(View::Place, { shown, dc::clampSubLocation(shown, shownSub) }, 1);
        else travelManually(shown, 0);
    };
    placeCrumb.setFont(titleFont(16.0f));
    placeCrumb.setColour(juce::Label::textColourId, colours::ivory);
    addAndMakeVisible(mapCrumb);
    addAndMakeVisible(locationCrumb);
    addAndMakeVisible(placeCrumb);

    macroHeading.setText("character", juce::dontSendNotification);
    macroHeading.setFont(labelFont(12.0f));
    macroHeading.setColour(juce::Label::textColourId, colours::ash);
    addAndMakeVisible(macroHeading);
    for (std::size_t m = 0; m < macros.size(); ++m) setUpFader(macros[m], dc::ids::macros[m], "Macro " + juce::String(m + 1));
    setUpFader(input, dc::ids::input, "Input");
    setUpFader(intensity, dc::ids::intensity, "Intensity");
    setUpFader(mix, dc::ids::mix, "Mix");
    setUpFader(shimmer, dc::ids::shimmer, "Shimmer");
    shimmer.slider.setTooltip("Every shimmer, octave shimmer and sparkle in the sound (0 to 150 %; 100 % as designed).");
    setUpFader(output, dc::ids::output, "Output");

    travelLabel.setText("Time", juce::dontSendNotification);
    travelLabel.setFont(labelFont(14.5f));
    addAndMakeVisible(travelLabel);
    travelTime.setTextBoxStyle(juce::Slider::TextBoxRight, true, 62, 18);
    travelTime.setDoubleClickReturnValue(true, 1.5);
    travelTime.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); travelTime.setColour(juce::Slider::textBoxTextColourId, colours::ash);
    travelTimeAttachment = std::make_unique<juce::SliderParameterAttachment>(*processor.parameters.getParameter(dc::ids::travelTime), travelTime);
    addAndMakeVisible(travelTime);
    atmosphereLabel.setText("Atmosphere", juce::dontSendNotification);
    atmosphereLabel.setFont(labelFont(14.5f));
    atmosphereLabel.setJustificationType(juce::Justification::centredRight);
    atmosphereLabel.setTooltip("Sparse sounds of the place (soft wind, distant bells, birdsong, chimes...). Off at 0 %.");
    addAndMakeVisible(atmosphereLabel);
    atmosphere.setTextBoxStyle(juce::Slider::TextBoxRight, true, 48, 18);
    atmosphere.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); atmosphere.setColour(juce::Slider::textBoxTextColourId, colours::ash);
    atmosphere.setDoubleClickReturnValue(true, 0.0);
    atmosphereAttachment = std::make_unique<juce::SliderParameterAttachment>(*processor.parameters.getParameter(dc::ids::atmosLevel), atmosphere);
    addAndMakeVisible(atmosphere);
    travelSync.addItemList(juce::StringArray::fromTokens(dc::travelSyncChoices, "|", ""), 1);
    travelSyncAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*processor.parameters.getParameter(dc::ids::travelSync), travelSync);
    addAndMakeVisible(travelSync);

    bypass.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<juce::ButtonParameterAttachment>(*processor.parameters.getParameter(dc::ids::bypass), bypass);
    addAndMakeVisible(bypass);
    advancedToggle.setClickingTogglesState(true);
    advancedToggle.onClick = [this] {
        // A second click closes it (as do its close button, Esc and a click outside).
        drawer.setVisible(advancedToggle.getToggleState());
        if (drawer.isVisible()) {
            journeyToggle.setToggleState(false, juce::dontSendNotification);
            journeyPanel.setVisible(false);
            drawer.grabKeyboardFocus();
        }
        repaint();
        storeViewState();
    };
    drawer.onClose = [this] { advancedToggle.setToggleState(false, juce::dontSendNotification); drawer.setVisible(false); repaint(); storeViewState(); };
    addAndMakeVisible(advancedToggle);
    journeyToggle.setClickingTogglesState(true);
    journeyToggle.onClick = [this] {
        journeyPanel.refresh();
        journeyPanel.setVisible(journeyToggle.getToggleState());
        if (journeyPanel.isVisible()) {
            advancedToggle.setToggleState(false, juce::dontSendNotification);
            drawer.setVisible(false);
            journeyPanel.grabKeyboardFocus();
        }
        repaint();
        storeViewState();
    };
    journeyPanel.onClose = [this] { journeyToggle.setToggleState(false, juce::dontSendNotification); journeyPanel.setVisible(false); repaint(); storeViewState(); };
    addAndMakeVisible(journeyToggle);
    addChildComponent(journeyPanel);
    // The Journey / Roam badge with its Stop button, and the short note.
    steerBadge.stop.setTooltip("Stop the journey here");
    steerBadge.stop.onClick = [this] {
        const auto stopped = processor.stopSteering();
        wasSteering = false;
        if (stopped.isNotEmpty()) showNotice(stopped + " stopped");
        refreshSteering();
        refreshToolbar();
        journeyPanel.refresh();
    };
    steerBadge.text.setFont(titleFont(13.0f));
    steerBadge.text.setColour(juce::Label::textColourId, colours::blood.brighter(0.5f));
    steerBadge.text.setInterceptsMouseClicks(false, false);
    steerBadge.addAndMakeVisible(steerBadge.text);
    steerBadge.addAndMakeVisible(steerBadge.stop);
    addChildComponent(steerBadge);
    notice.setFont(titleFont(13.0f));
    notice.setJustificationType(juce::Justification::centred);
    notice.setColour(juce::Label::textColourId, colours::ivory);
    notice.setColour(juce::Label::backgroundColourId, colours::ink.withAlpha(0.72f));
    notice.setInterceptsMouseClicks(false, false);
    addChildComponent(notice);
    follower.startTimerHz(10);

    // Search: the magnifier at the top (and Cmd+F) opens the panel over the scene.
    searchButton.setTooltip(juce::String("Search ") + (processor.isInstrument() ? "instruments, places and presets" : "places, journeys and presets")
                            + " (" + (juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX ? "Cmd" : "Ctrl") + "+F)");
    searchButton.setClickingTogglesState(false);
    searchButton.setWantsKeyboardFocus(false);
    searchButton.onClick = [this] { if (searchPanel.isVisible()) closeSearch(); else openSearch(); };
    addAndMakeVisible(searchButton);
    searchPanel.onChoose = [this](int entry) { chooseSearchResult(entry); };
    searchPanel.onClose = [this] { searchButton.setToggleState(false, juce::dontSendNotification); grabKeyboardFocus(); };
    if (processor.isInstrument()) {
        searchPanel.onPreviewStart = [this](int instrument) { return processor.startPreview(instrument); };
        searchPanel.onPreviewStop = [this] { processor.stopPreview(); };
        searchPanel.previewAllowed = [this] { return !processor.hostPlaying(); };
    }
    addChildComponent(searchPanel);

    // Toolbar (Milestone 11).
    presetButton.setTooltip("Presets: favourite spots, journeys, your own");
    presetButton.onClick = [this] { showPresetMenu(); };
    prevPreset.setButtonText(juce::String::fromUTF8("\xe2\x97\x80"));
    nextPreset.setButtonText(juce::String::fromUTF8("\xe2\x96\xb6"));
    prevPreset.onClick = [this] { stepPreset(-1); };
    nextPreset.onClick = [this] { stepPreset(1); };
    slotA.setTooltip("A/B compare: setting A");
    slotB.setTooltip("A/B compare: setting B");
    slotA.onClick = [this] { processor.selectSlot(0); refreshToolbar(); };
    slotB.onClick = [this] { processor.selectSlot(1); refreshToolbar(); };
    copySlot.setButtonText(juce::String::fromUTF8("A\xe2\x86\x92" "B"));
    copySlot.setTooltip("Copy the current setting to the other slot");
    copySlot.onClick = [this] { processor.copyCurrentToOtherSlot(); refreshToolbar(); };
    undoButton.setButtonText(juce::String::fromUTF8("\xe2\x86\xb6"));
    redoButton.setButtonText(juce::String::fromUTF8("\xe2\x86\xb7"));
    undoButton.setTooltip("Undo");
    redoButton.setTooltip("Redo");
    undoButton.onClick = [this] { processor.undo(); refreshToolbar(); journeyPanel.refresh(); };
    redoButton.onClick = [this] { processor.redo(); refreshToolbar(); journeyPanel.refresh(); };
    settingsButton.setButtonText(juce::String::fromUTF8("\xe2\x9a\x99"));
    settingsButton.setTooltip("Window size and quality");
    settingsButton.onClick = [this] { showSettingsMenu(); };
    for (auto* b : { &presetButton, &prevPreset, &nextPreset, &slotA, &slotB, &copySlot, &undoButton, &redoButton, &settingsButton }) addAndMakeVisible(*b);
    for (auto* parameter : processor.getParameters()) parameter->addListener(this);
    animateLife = processor.qualityMode() > 0;
    refreshToolbar();
    controlsToggle.setButtonText(juce::String::fromUTF8("\xe2\x96\xbe"));
    controlsToggle.setTooltip("Show or hide the controls");
    controlsToggle.onClick = [this] {
        controlsShown = !controlsShown;
        if (!controlsShown) closePanels();   // their buttons are hidden with the controls
        controlsToggle.setButtonText(juce::String::fromUTF8(controlsShown ? "\xe2\x96\xbe" : "\xe2\x96\xb4"));
        resized();
        repaint();
        storeViewState();
    };
    addAndMakeVisible(controlsToggle);
    addChildComponent(drawer);

    locationAttachment->sendInitialUpdate();
    subLocationAttachment->sendInitialUpdate();
    // Clicks anywhere in the window (on any child) can close an open panel.
    addMouseListener(this, true);
    setWantsKeyboardFocus(true);
    setSize(defaultWidth, defaultHeight);
    refreshSteering();
    // Direction Update 2: the window comes back as it was left (never back to the map
    // unless it was on the map): the view and scene, the controls strip, the panels open
    // and where they were scrolled to. No zoom: the scene simply appears.
    const auto saved = processor.viewState();
    if (saved.valid) {
        if (!saved.controls) controlsToggle.onClick();
        if (saved.place && shown >= 0) {
            view = View::Place;
            scene = { shown, dc::clampSubLocation(shown, shownSub) };
            updateCrumbs();
            layoutOverlays();
            const auto px = pixelSize();
            cache.request(scene, px.x, px.y);
        }
        if (saved.advanced) {
            advancedToggle.setToggleState(true, juce::dontSendNotification);
            advancedToggle.onClick();
            drawer.setScroll(saved.advancedScroll);
        } else if (saved.journey) {
            journeyToggle.setToggleState(true, juce::dontSendNotification);
            journeyToggle.onClick();
            journeyPanel.setScroll(saved.journeyScroll);
        }
    }
    storeViewState();
}

void LightSeekerView::storeViewState() {
    const bool place = (pending ? pendingView : view) == View::Place;
    const auto shownScene = pending ? pendingScene : scene;
    const bool advanced = drawer.isVisible(), journey = journeyPanel.isVisible();
    const int advancedScroll = drawer.scroll(), journeyScroll = journeyPanel.scroll();
    const bool controls = controlsShown;
    processor.updateViewState([&](LightSeekerProcessor::ViewState& v) {
        v.place = place;
        if (place) { v.location = shownScene.location; v.sub = shownScene.sub; }
        else if (shown >= 0) { v.location = shown; v.sub = dc::clampSubLocation(shown, shownSub); }
        v.controls = controls;
        v.advanced = advanced;
        v.journey = journey;
        if (advanced) v.advancedScroll = advancedScroll;
        if (journey) v.journeyScroll = journeyScroll;
    });
}

void LightSeekerView::scaleChanged() {
    // The window was resized (a new transform, not a new size): the art at the new pixel
    // size. The current frame is scaled until it is ready.
    const auto px = pixelSize();
    cache.request(scene, px.x, px.y);
    wakeTimer();
}

void LightSeekerView::refreshFrame() {
    if (transition.active) return;
    const auto px = pixelSize();
    auto image = cache.get(scene, px.x, px.y);   // queues the scene if it is not ready
    if (image.isValid() && !(image == frame)) { frame = image; refreshAirFrame(nowSeconds()); repaint(); }
}

void LightSeekerView::advanceAirForTest() { refreshAirFrame(nowSeconds()); }

void LightSeekerView::refreshAirFrame(double now) {
    if (!animateLife || airArea().isEmpty() || !frame.isValid()) { airFrame = {}; return; }
    // A new frame (another size, or the scene changed): copied whole once.
    const bool fresh = airFrame.getBounds() != frame.getBounds() || airFrame.getFormat() != frame.getFormat() || airSource != frame;
    if (airFrame.getBounds() != frame.getBounds() || airFrame.getFormat() != frame.getFormat())
        airFrame = juce::Image(frame.getFormat(), frame.getWidth(), frame.getHeight(), false, juce::SoftwareImageType());
    airSource = frame;
    const float ps = static_cast<float>(frame.getWidth()) / static_cast<float>(std::max(1, getWidth()));
    const auto area = airArea();
    const auto pixels = juce::Rectangle<float>(area.toFloat()).transformedBy(juce::AffineTransform::scale(ps)).getSmallestIntegerContainer()
                            .getIntersection(frame.getBounds());
    juce::Graphics g(airFrame);
    if (fresh) g.drawImageAt(frame, 0, 0);
    else { g.saveState(); g.reduceClipRegion(pixels); g.drawImageAt(frame, 0, 0); g.restoreState(); }
    g.reduceClipRegion(pixels);
    paintMapAir(g, now, ps);
}

void LightSeekerView::travelManually(int l, int k) {
    const auto stopped = processor.travelTo(l, k);
    wasSteering = false;
    if (stopped.isNotEmpty()) showNotice(stopped + " stopped");
    // Shown explicitly: when the parameter already had this value (a journey had moved
    // the view elsewhere) its attachment does not call back.
    showLocation(processor.locationIndex());
    showSubLocation(dc::clampSubLocation(processor.locationIndex(), k));
    refreshSteering();
    refreshToolbar();
    if (journeyPanel.isVisible()) journeyPanel.refresh();
}

void LightSeekerView::showNotice(const juce::String& text) {
    notice.setText(text, juce::dontSendNotification);
    noticeUntil = nowSeconds() + 2.5;
    notice.setVisible(true);
    notice.toFront(false);
}

void LightSeekerView::refreshSteering() {
    const bool journey = processor.journeyRunning(), roam = processor.roamRunning();
    const bool on = journey || roam;
    if (on) {
        const juce::String what = journey && roam ? "Journey and Roam active" : (journey ? "Journey active" : "Roam active");
        if (steerBadge.text.getText() != what) steerBadge.text.setText(what, juce::dontSendNotification);
    }
    if (on != steerBadge.isVisible()) { steerBadge.setVisible(on); if (on) steerBadge.toFront(false); }
    if (notice.isVisible() && nowSeconds() >= noticeUntil) notice.setVisible(false);
    const auto label = processor.presetDisplayName() + juce::String::fromUTF8("  \xe2\x96\xbe");
    if (presetButton.getButtonText() != label) presetButton.setButtonText(label);
}

void LightSeekerView::chooseInit() {
    processor.applyInit();
    wasSteering = false;
    notice.setVisible(false);
    showLocation(processor.locationIndex());
    showSubLocation(juce::roundToInt(processor.parameters.getRawParameterValue(dc::ids::subLocation)->load()));
    refreshToolbar();
    refreshSteering();
    journeyPanel.refresh();
}

void LightSeekerView::choosePreset(int index) {
    processor.applyFactoryPreset(index);
    wasSteering = false;
    notice.setVisible(false);
    refreshToolbar();
    refreshSteering();
    journeyPanel.refresh();
}

void LightSeekerView::closePanels() {
    journeyToggle.setToggleState(false, juce::dontSendNotification);
    advancedToggle.setToggleState(false, juce::dontSendNotification);
    const bool any = journeyPanel.isVisible() || drawer.isVisible();
    journeyPanel.setVisible(false);
    drawer.setVisible(false);
    if (any) repaint();
    storeViewState();
}

bool LightSeekerView::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)) { openSearch(); return true; }
    if (key == juce::KeyPress::escapeKey && searchPanel.isVisible()) { closeSearch(); return true; }
    if (key == juce::KeyPress::escapeKey && (journeyPanel.isVisible() || drawer.isVisible())) { closePanels(); return true; }
    return false;
}

void LightSeekerView::openSearch() {
    layoutSearch();
    searchButton.setToggleState(true, juce::dontSendNotification);
    searchPanel.open();
}

void LightSeekerView::closeSearch() {
    searchPanel.close();   // calls onClose when it was open
    searchButton.setToggleState(false, juce::dontSendNotification);
}

void LightSeekerView::chooseSearchResult(int entryIndex) {
    const auto& index = dc::search::index(processor.isInstrument());
    if (entryIndex < 0 || entryIndex >= static_cast<int>(index.entries().size())) return;
    const auto e = index.entry(entryIndex);
    closeSearch();
    switch (e.kind) {
        case dc::search::Kind::Instrument:
            // As the instrument menu does: plays it here and locks it.
            processor.chooseInstrument(e.instrument);
            refreshToolbar();
            break;
        case dc::search::Kind::Place:
        case dc::search::Kind::Location:
            // As a click on the map's landmark (then the place in its list) does: on the map the
            // path is drawn and the view zooms in; inside a place the scene crossfades.
            if (view == View::Map && !(pending && pendingView == View::Place)) {
                const bool already = e.location == shown;
                travelManually(e.location, e.sub);
                if (already) goTo(View::Place, { e.location, e.sub }, 1);
                else { zoomAt = nowSeconds() + 0.55; wakeTimer(); }
            } else travelManually(e.location, e.sub);
            break;
        case dc::search::Kind::Preset:
        case dc::search::Kind::Journey:
            choosePreset(e.preset);
            break;
    }
}

void LightSeekerView::layoutSearch() {
    // Top centre over the scene, ending above the controls (their buttons stay clickable).
    const int bottom = controlsShown ? getHeight() - static_cast<int>(stripHeight) - 34 : getHeight() - 44;
    const int w = dc::ui::SearchPanel::panelWidth + 28;
    const int y = 32;
    searchPanel.setBounds((getWidth() - w) / 2, y, w, juce::jmax(200, juce::jmin(470, bottom + 14 - y)));
}

void LightSeekerView::mouseDown(const juce::MouseEvent& e) { pointerDown(e.eventComponent); }

void LightSeekerView::pointerDown(juce::Component* target) {
    // A click outside an open panel closes it. Its own button is left to toggle it
    // (closing here first would make that click open it again).
    const auto within = [target](juce::Component& c) { return target == &c || c.isParentOf(target); };
    if (journeyPanel.isVisible() && !within(journeyPanel) && target != &journeyToggle) journeyPanel.onClose();
    if (drawer.isVisible() && !within(drawer) && target != &advancedToggle) drawer.onClose();
    // Search closes on a click outside it; the click still does what it does (the map stays usable).
    if (searchPanel.isVisible() && !within(searchPanel) && target != &searchButton) closeSearch();
}

void LightSeekerView::SteerBadge::paint(juce::Graphics& g) {
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::ink.withAlpha(0.78f));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(colours::blood.withAlpha(0.8f));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
    // A small crimson light: something is steering the place.
    g.setColour(colours::blood.brighter(0.4f));
    g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre({ 14.0f, r.getCentreY() }));
}

void LightSeekerView::SteerBadge::resized() {
    auto r = getLocalBounds();
    stop.setBounds(r.removeFromRight(58).reduced(3));
    text.setBounds(r.withTrimmedLeft(24));
}

LightSeekerView::~LightSeekerView() {
    storeViewState();
    for (auto* parameter : processor.getParameters()) parameter->removeListener(this);
    follower.stopTimer();
    stopTimer();
    setLookAndFeel(nullptr);
}

void LightSeekerView::setUpFader(Fader& fader, const juce::String& id, const juce::String& name) {
    fader.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 15);
    fader.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); fader.slider.setColour(juce::Slider::textBoxTextColourId, colours::ash);
    fader.slider.setDoubleClickReturnValue(true, static_cast<double>(
        processor.parameters.getParameter(id)->convertFrom0to1(processor.parameters.getParameter(id)->getDefaultValue())));
    fader.attachment = std::make_unique<juce::SliderParameterAttachment>(*processor.parameters.getParameter(id), fader.slider);
    fader.label.setText(name, juce::dontSendNotification);
    fader.label.setJustificationType(juce::Justification::centred);
    fader.label.setFont(labelFont(14.5f));
    fader.label.setColour(juce::Label::textColourId, colours::ivory.withAlpha(0.92f));
    addAndMakeVisible(fader.slider);
    addAndMakeVisible(fader.label);
}

juce::Point<int> LightSeekerView::pixelSize() const {
    // The view is always 960 x 560; the window's scale (transform), the display's
    // density and the quality mode decide the pixels the art is rendered at (capped:
    // more would cost render time and memory for no visible gain).
    float scale = static_cast<float>(juce::Component::getApproximateScaleFactorForComponent(const_cast<LightSeekerView*>(this)));
    if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
        if (getPeer() != nullptr || getParentComponent() != nullptr) scale *= static_cast<float>(display->scale);
    if (processor.qualityMode() == 0) scale = std::min(scale, 1.0f);   // Eco: no Retina art
    scale = juce::jlimit(0.5f, 2.5f, scale);
    return { juce::roundToInt(static_cast<float>(getWidth()) * scale), juce::roundToInt(static_cast<float>(getHeight()) * scale) };
}

juce::Point<float> LightSeekerView::toLocal(juce::Point<float> v) const {
    return { v.x * static_cast<float>(getWidth()) / dc::art::virtualWidth, v.y * static_cast<float>(getHeight()) / dc::art::virtualHeight };
}

juce::Rectangle<float> LightSeekerView::controlsArea() const {
    return controlsShown ? getLocalBounds().toFloat().removeFromBottom(stripHeight) : juce::Rectangle<float>();
}

void LightSeekerView::showLocation(int index) {
    index = juce::jlimit(0, dc::LocationCount - 1, index);
    if (shown >= 0 && index != shown) {
        // A journey on the map: remember where from, for the path.
        pathFrom = shown;
        pathTo = index;
        pathEnded = 0;
        wakeTimer();
    }
    shown = index;
    const int count = dc::subLocationCount(index);
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& button = *subs[static_cast<std::size_t>(k)];
        button.setButtonText(k <= count ? dc::subLocationName(index, k) : "");
        hotspots[static_cast<std::size_t>(k)]->setButtonText(k <= count ? dc::subLocationName(index, k) : "");
    }
    // Only the location's own macros are shown, under their themed names.
    for (int m = 0; m < dc::macroCount; ++m) {
        auto& fader = macros[static_cast<std::size_t>(m)];
        const bool used = m < dc::macroCountFor(index);
        fader.label.setText(dc::macroName(index, m), juce::dontSendNotification);
        fader.slider.setVisible(used && controlsShown);
        fader.label.setVisible(used && controlsShown);
    }
    showSubLocation(shownSub < 0 ? 0 : shownSub);
}

void LightSeekerView::showSubLocation(int index) {
    if (shown < 0) { shownSub = index; return; }
    shownSub = juce::jlimit(0, dc::maxSubLocations, index);
    const int effective = dc::clampSubLocation(shown, shownSub);
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        subs[static_cast<std::size_t>(k)]->setToggleState(k == effective, juce::dontSendNotification);
        hotspots[static_cast<std::size_t>(k)]->setToggleState(k == effective, juce::dontSendNotification);
    }
    for (int i = 0; i < dc::LocationCount; ++i)
        landmarks[static_cast<std::size_t>(i)]->setToggleState(i == shown, juce::dontSendNotification);
    // Inside a location, the view follows the place (automation moves you too).
    if (view == View::Place && !(scene == dc::art::SceneId { shown, effective }))
        goTo(View::Place, { shown, effective }, 0);
    updateCrumbs();
    layoutOverlays();
    repaint();
}

void LightSeekerView::updateCrumbs() {
    if (shown < 0) return;
    const bool map = view == View::Map;
    locationCrumb.setButtonText(dc::locationNames[static_cast<std::size_t>(shown)]);
    placeCrumb.setText(map ? juce::String() : juce::String::fromUTF8("\xe2\x80\xba  ") + dc::subLocationName(shown, dc::clampSubLocation(shown, shownSub)),
                       juce::dontSendNotification);
    mapCrumb.setToggleState(map, juce::dontSendNotification);
}

void LightSeekerView::goTo(View target, dc::art::SceneId id, int kind) {
    if (target == View::Place) id.sub = dc::clampSubLocation(id.location, id.sub);
    pending = true;
    pendingView = target;
    pendingScene = target == View::Map ? dc::art::SceneId { -1, 0 } : id;
    pendingKind = kind;
    zoomAt = 0;
    const auto px = pixelSize();
    cache.request(pendingScene, px.x, px.y);
    wakeTimer();
    storeViewState();
}

void LightSeekerView::wakeTimer() {
    if (!isTimerRunning()) startTimerHz(30);
}

void LightSeekerView::sceneReady() {
    // A scene finished rendering: show it (or start the transition that waited for it).
    timerCallback();
    if (!transition.active && !pending) { refreshFrame(); refreshLights(); }
    repaint();
}

void LightSeekerView::timerCallback() {
    const double now = nowSeconds();
    const auto px = pixelSize();
    bool busy = false;
    if (zoomAt > 0) {
        if (now >= zoomAt) goTo(View::Place, { shown, dc::clampSubLocation(shown, shownSub) }, 1);
        else busy = true;
    }
    if (pending && !transition.active) {
        busy = true;
        if (cache.ready(pendingScene, px.x, px.y)) {
            transition.active = true;
            transition.kind = pendingKind;
            transition.from = cache.get(scene, px.x, px.y);
            transition.to = cache.get(pendingScene, px.x, px.y);
            transition.fromPlanes = cache.planes(scene, px.x, px.y);
            transition.toPlanes = cache.planes(pendingScene, px.x, px.y);
            embers.clear();
            transition.start = now;
            transition.length = pendingKind == 0 ? 0.32 : 0.5;
            transition.focus = dc::art::mapLandmarks()[static_cast<std::size_t>(juce::jlimit(0, dc::LocationCount - 1,
                                   pendingView == View::Map ? scene.location : pendingScene.location))].position;
            view = pendingView;
            scene = pendingScene;
            pending = false;
            if (transition.to.isValid()) frame = transition.to;
            if (transition.from.isNull()) transition.active = false;   // nothing to transition from
            updateCrumbs();
            layoutOverlays();
        }
    }
    if (transition.active) {
        busy = true;
        if (transition.held < 0.0f && now - transition.start >= transition.length) {
            transition.active = false;
            transition.from = {};
            transition.to = {};
            transition.fromPlanes = {};
            transition.toPlanes = {};
            layoutOverlays();
            refreshFrame();
            refreshLights();
        }
        repaint();
    }
    // The travel path on the map: drawn while travelling, then fades.
    if (view == View::Map && pathTo >= 0) {
        const bool travelling = processor.travelProgress.load() < 1.0f;
        if (travelling) { pathEnded = 0; busy = true; }
        else if (pathEnded <= 0) { pathEnded = now; busy = true; }
        else if (now - pathEnded < 1.5) busy = true;
        else pathTo = -1;
        repaint();
    }
    // Life, only small regions repainted: lights and embers at 15 fps, the mist at 10 fps,
    // the moon's glow at 5 fps.
    const bool painted = planes.layered();
    if (!transition.active && !pending && animateLife && (!lights.empty() || painted)) {
        busy = true;
        ++lifeTick;
        if ((lifeTick & 1) == 0) {
            for (const auto& l : lights)
                if (l.kind != 5) repaint(lifeArea(l));
            if (!emberSources.empty()) {
                std::vector<juce::Rectangle<int>> dirty;
                for (const auto& e : embers) dirty.push_back(juce::Rectangle<float>(e.size * 8.0f, e.size * 8.0f).withCentre(e.p).getSmallestIntegerContainer());
                stepEmbers(2.0f / 30.0f);
                for (const auto& e : embers) dirty.push_back(juce::Rectangle<float>(e.size * 8.0f, e.size * 8.0f).withCentre(e.p).getSmallestIntegerContainer());
                for (const auto& r : dirty) repaint(r);
            }
        }
        if (painted && skyVisible && (lifeTick & 1) == 0) {
            const auto before = birdsArea();
            stepBirds(2.0f / 30.0f, now);
            const auto after = birdsArea();
            if (!before.isEmpty()) repaint(before);
            if (!after.isEmpty()) repaint(after);
        }
        if (painted && lifeTick % 3 == 0 && !mistArea().isEmpty()) repaint(mistArea());
        if (painted && lifeTick % 6 == 0 && !moonArea().isEmpty()) repaint(moonArea());
        // The map's cloud shadows: slow, so 6 fps is smooth (a fraction of a pixel a frame).
        if (painted && lifeTick % 5 == 0 && !airArea().isEmpty()) { refreshAirFrame(now); repaint(airArea()); }
    }
    if (!busy) stopTimer();
}

void LightSeekerView::parameterGestureChanged(int, bool starting) {
    // Only edits made here (on the message thread) open an undo step; host automation
    // never fills the history.
    if (!starting || processor.applyingSnapshot()) return;
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating(); mm != nullptr && mm->isThisTheMessageThread()) {
        processor.pushUndo();
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<LightSeekerView>(this)] { if (safe != nullptr) safe->refreshToolbar(); });
    }
}

void LightSeekerView::refreshToolbar() {
    presetButton.setButtonText(processor.presetDisplayName() + juce::String::fromUTF8("  \xe2\x96\xbe"));
    slotA.setToggleState(processor.currentSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState(processor.currentSlot() == 1, juce::dontSendNotification);
    undoButton.setEnabled(processor.canUndo());
    redoButton.setEnabled(processor.canRedo());
    const bool animate = processor.qualityMode() > 0;
    if (animate != animateLife) setAnimation(animate);
}

void LightSeekerView::stepPreset(int direction) {
    const int count = static_cast<int>(dc::presetsFor(processor.isInstrument()).size());
    const int next = processor.presetIndex < 0 ? (direction > 0 ? 0 : count - 1) : (processor.presetIndex + direction + count) % count;
    processor.applyFactoryPreset(next);
    wasSteering = false;
    notice.setVisible(false);
    refreshToolbar();
    refreshSteering();
    journeyPanel.refresh();
}

void LightSeekerView::showPresetMenu() {
    juce::PopupMenu menu, user;
    menu.addItem(899, "Init / No preset", true, processor.presetIndex < 0 && processor.presetName == "Init" && !processor.presetModified());
    menu.addSeparator();
    // This edition's factory presets (the index applyFactoryPreset takes), by category.
    const auto& presets = dc::presetsFor(processor.isInstrument());
    juce::StringArray categories;
    std::vector<juce::PopupMenu> groups;
    for (int k = 0; k < static_cast<int>(presets.size()); ++k) {
        const juce::String category = presets[static_cast<std::size_t>(k)].category;
        int g = categories.indexOf(category);
        if (g < 0) { categories.add(category); groups.emplace_back(); g = categories.size() - 1; }
        groups[static_cast<std::size_t>(g)].addItem(1 + k, presets[static_cast<std::size_t>(k)].name, true, processor.presetIndex == k);
    }
    for (int g = 0; g < categories.size(); ++g) menu.addSubMenu(categories[g], groups[static_cast<std::size_t>(g)]);
    const auto files = processor.userPresetFolder().findChildFiles(juce::File::findFiles, false, "*.lspreset");
    juce::Array<juce::File> sorted(files);
    sorted.sort();
    for (int k = 0; k < sorted.size(); ++k) user.addItem(1000 + k, sorted[k].getFileNameWithoutExtension());
    if (sorted.isEmpty()) user.addItem(-1, "(none yet)", false);
    menu.addSubMenu("Your presets", user);
    menu.addSeparator();
    menu.addItem(900, "Save as your preset...");
    menu.addItem(901, "Show presets folder");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetButton),
        [safe = juce::Component::SafePointer<LightSeekerView>(this), sorted](int result) {
            if (safe == nullptr || result == 0) return;
            auto& self = *safe;
            if (result == 899) { self.chooseInit(); return; }
            if (result >= 1 && result < 899) { self.choosePreset(result - 1); return; }
            else if (result >= 1000 && result - 1000 < sorted.size()) self.processor.loadUserPreset(sorted[result - 1000]);
            else if (result == 901) { self.processor.userPresetFolder().createDirectory(); self.processor.userPresetFolder().startAsProcess(); }
            else if (result == 900) {
                auto* window = new juce::AlertWindow("Save preset", "Name of your preset:", juce::MessageBoxIconType::NoIcon);
                window->addTextEditor("name", self.processor.presetName);
                window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
                window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                window->enterModalState(true, juce::ModalCallbackFunction::create([safe, window](int button) {
                    if (button == 1 && safe != nullptr) { safe->processor.saveUserPreset(window->getTextEditorContents("name")); safe->refreshToolbar(); }
                }), true);
                return;
            }
            self.refreshToolbar();
            self.refreshSteering();
            self.journeyPanel.refresh();
        });
}

void LightSeekerView::showSettingsMenu() {
    juce::PopupMenu menu, size, quality;
    const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    for (int k = 0; k < 5; ++k)
        size.addItem(1 + k, juce::String(juce::roundToInt(scales[k] * 100.0f)) + " %", true, std::abs(processor.editorScale.load() - scales[k]) < 0.01f);
    const char* names[] = { "Eco (lightest: no animation, mono spectral layer)", "Standard", "High (smoothest animation and art)" };
    for (int k = 0; k < 3; ++k) quality.addItem(10 + k, names[k], true, processor.qualityMode() == k);
    menu.addSubMenu("Window size", size);
    menu.addSubMenu("Quality", quality);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton),
        [safe = juce::Component::SafePointer<LightSeekerView>(this), scales](int result) {
            if (safe == nullptr || result == 0) return;
            if (result >= 1 && result <= 5) { if (safe->onScaleChosen) safe->onScaleChosen(scales[result - 1]); }
            else if (result >= 10 && result <= 12) {
                if (auto* q = safe->processor.parameters.getParameter(dc::ids::quality)) q->setValueNotifyingHost(q->convertTo0to1(static_cast<float>(result - 10)));
                safe->refreshToolbar();
                safe->resized();   // the art is re-rendered at the new quality's size
            }
        });
}

void LightSeekerView::followSteering() {
    // Until the audio thread has seen the latest manual choice, what it reports is the
    // journey just stopped: following it would take the view back there.
    const bool current = processor.steeringCurrent();
    const bool steering = current && processor.steering();
    if (steering) {
        const int l = processor.currentDestination.load(), k = processor.currentSubLocation.load();
        if (l != shown) showLocation(l);
        if (k != dc::clampSubLocation(shown, shownSub)) showSubLocation(k);
    } else if (wasSteering && current) {
        // Back to the Location parameters.
        showLocation(processor.locationIndex());
        showSubLocation(juce::roundToInt(processor.parameters.getRawParameterValue(dc::ids::subLocation)->load()));
    }
    if (current) wasSteering = steering;
    if (journeyPanel.isVisible()) journeyPanel.refresh();
    refreshSteering();
    if (drawer.isVisible() || journeyPanel.isVisible()) storeViewState();   // where they are scrolled to
}

void LightSeekerView::refreshLights() {
    const auto px = pixelSize();
    planes = cache.planes(scene, px.x, px.y);
    lights = cache.lights(scene, px.x, px.y);
    emberSources.clear();
    for (const auto& l : lights)
        if (planes.layered() && (l.kind == 0 || l.kind == 5)) emberSources.push_back(l);
    if (scene.isMap() && planes.layered()) {
        // Final touches: light motes rising over the sunlit land, under the sun and across
        // the fields (sources of motes only, no glow of their own).
        for (const auto& p : { juce::Point<float>(790, 104), juce::Point<float>(470, 122), juce::Point<float>(640, 118),
                               juce::Point<float>(250, 150), juce::Point<float>(830, 200), juce::Point<float>(400, 245) })
            emberSources.push_back({ p, 30.0f, 0 });
    }
    embers.clear();
    birds.clear();
    // A visible sky: the top of the back plane mostly blue (sampled once per scene).
    skyVisible = false;
    if (planes.layered() && planes.back.isValid()) {
        int blue = 0, samples = 0;
        for (int k = 0; k < 16; ++k) {
            const auto c = planes.back.getPixelAt(planes.back.getWidth() * (2 * k + 1) / 32, planes.back.getHeight() / 12);
            blue += c.getBlue() > c.getRed() + 12 ? 1 : 0;
            ++samples;
        }
        skyVisible = blue * 2 > samples;
    }
    nextFlock = nowSeconds() + 6.0 + 10.0 * random.nextDouble();
    if (!lights.empty() || planes.layered()) wakeTimer();
}

juce::Rectangle<int> LightSeekerView::birdsArea() const {
    if (birds.empty()) return {};
    juce::Rectangle<float> box;
    for (const auto& b : birds) box = box.getUnion(juce::Rectangle<float>(b.size * 3.0f, b.size * 2.0f).withCentre(b.p));
    return box.expanded(2.0f).getSmallestIntegerContainer().getIntersection(getLocalBounds());
}

void LightSeekerView::stepBirds(float dt, double now) {
    const float sx = static_cast<float>(getWidth()) / dc::art::virtualWidth;
    if (birds.empty()) {
        if (!skyVisible || now < nextFlock) return;
        // A flock of three to five, in a loose V, from one side across the upper sky.
        const bool fromLeft = random.nextBool();
        const float y = (70.0f + 110.0f * random.nextFloat()) * sx;
        const int count = 3 + random.nextInt(3);
        for (int k = 0; k < count; ++k) {
            Bird b;
            const float back = static_cast<float>((k + 1) / 2) * 14.0f * sx, side = (k % 2 == 0 ? 1.0f : -1.0f) * static_cast<float>((k + 1) / 2) * 9.0f * sx;
            b.p = { fromLeft ? -20.0f * sx - back : static_cast<float>(getWidth()) + 20.0f * sx + back, y + side };
            b.phase = random.nextFloat() * 6.28f;
            b.size = (5.0f + 2.0f * random.nextFloat()) * sx;
            birds.push_back(b);
        }
        flockVelocity = { (fromLeft ? 1.0f : -1.0f) * 48.0f * sx, -2.0f * sx };
        return;
    }
    for (auto& b : birds) { b.p += flockVelocity * dt; b.phase += dt * 9.0f; }
    const float w = static_cast<float>(getWidth());
    if (std::all_of(birds.begin(), birds.end(), [w, sx](const Bird& b) { return b.p.x < -40.0f * sx || b.p.x > w + 40.0f * sx; })) {
        birds.clear();
        nextFlock = now + 25.0 + 25.0 * random.nextDouble();
    }
}

void LightSeekerView::paintBirds(juce::Graphics& g) {
    for (const auto& b : birds) {
        const auto box = juce::Rectangle<float>(b.size * 3.0f, b.size * 2.0f).withCentre(b.p);
        if (!g.clipRegionIntersects(box.getSmallestIntegerContainer())) continue;
        // A dove: two wing strokes flapping (a soft V), white with a faint shadow line.
        const float flap = std::sin(b.phase) * 0.6f;
        juce::Path wings;
        wings.startNewSubPath(b.p.x - b.size, b.p.y - b.size * flap);
        wings.quadraticTo(b.p.x - b.size * 0.4f, b.p.y - b.size * (0.2f + 0.3f * flap), b.p.x, b.p.y);
        wings.quadraticTo(b.p.x + b.size * 0.4f, b.p.y - b.size * (0.2f + 0.3f * flap), b.p.x + b.size, b.p.y - b.size * flap);
        g.setColour(juce::Colour(0xff7a7090).withAlpha(0.35f));
        g.strokePath(wings, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation(0.0f, 0.8f));
        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.strokePath(wings, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

juce::Rectangle<int> LightSeekerView::lifeArea(const dc::art::LightSource& l) const {
    const float sx = static_cast<float>(getWidth()) / dc::art::virtualWidth;
    const auto c = toLocal(l.position);
    if (l.kind == 3) return juce::Rectangle<float>(static_cast<float>(getWidth()), 70.0f * sx).withCentre({ static_cast<float>(getWidth()) * 0.5f, c.y }).toNearestInt();
    const float r = (l.kind == 2 ? 10.0f : (l.kind == 6 ? 14.0f : l.radius * 1.15f)) * sx;
    return juce::Rectangle<float>(2 * r, 2 * r).withCentre(c).toNearestInt().expanded(2);
}

void LightSeekerView::paintLife(juce::Graphics& g, double now) {
    const float sx = static_cast<float>(getWidth()) / dc::art::virtualWidth;
    const auto t = static_cast<float>(std::fmod(now, 1000.0));
    const juce::Colour pale { 0xfffffbea };   // glints and mist: warm white
    int index = 0;
    for (const auto& l : lights) {
        const auto area = lifeArea(l);
        ++index;
        if (!g.clipRegionIntersects(area)) continue;
        const auto c = toLocal(l.position);
        const float n = dc::art::noise2(t * 7.0f, static_cast<float>(index) * 3.7f, 5) * 0.7f + dc::art::noise2(t * 2.1f, static_cast<float>(index), 9) * 0.3f;
        switch (l.kind) {
            case 0: case 1: {
                const float r = l.radius * sx * (l.kind == 0 ? 1.05f : 0.9f);
                const juce::Colour warm = colours::ember;
                const float alpha = (l.kind == 0 ? 0.05f : 0.035f) + (l.kind == 0 ? 0.12f : 0.07f) * n;
                g.setGradientFill(juce::ColourGradient(warm.withAlpha(alpha), c, warm.withAlpha(0.0f), c.translated(r, 0), true));
                g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c));
                break;
            }
            case 4: {
                // A lit window: a slow, faint breathing of its golden glow.
                const float r = l.radius * sx;
                const float alpha = 0.04f + 0.09f * n;
                g.setGradientFill(juce::ColourGradient(colours::blood.withAlpha(alpha), c, colours::blood.withAlpha(0.0f), c.translated(r, 0), true));
                g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c));
                break;
            }
            case 6: {
                // The lord's eyes: a slow, menacing pulse.
                const float pulse = 0.5f + 0.5f * std::sin(t * 1.3f + static_cast<float>(index));
                const float r = 12.0f * sx;
                g.setGradientFill(juce::ColourGradient(colours::ember.withAlpha(0.1f + 0.35f * pulse), c, colours::blood.withAlpha(0.0f), c.translated(r, 0), true));
                g.fillEllipse(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c));
                break;
            }
            case 2: {
                const float tw = std::pow(dc::art::noise2(t * 3.0f, static_cast<float>(index) * 5.3f, 13), 3.0f);
                g.setColour(pale.withAlpha(0.15f + 0.7f * tw));
                const float r = (2.0f + 3.0f * tw) * sx;
                g.drawLine(c.x - r, c.y, c.x + r, c.y, 0.8f);
                g.drawLine(c.x, c.y - r, c.x, c.y + r, 0.8f);
                break;
            }
            case 3: {
                // Fog: two soft pale banks drifting slowly across.
                const float h = 50.0f * sx, w = static_cast<float>(getWidth());
                for (int k = 0; k < 2; ++k) {
                    const float x = std::fmod(t * (6.0f + 3.0f * static_cast<float>(k)) * sx + static_cast<float>(k) * w * 0.55f + static_cast<float>(index) * 97.0f, w * 1.6f) - w * 0.3f;
                    const auto bank = juce::Rectangle<float>(w * 0.6f, h).withCentre({ x, c.y + (static_cast<float>(k) - 0.5f) * h * 0.3f });
                    g.setGradientFill(juce::ColourGradient(pale.withAlpha(0.07f), bank.getCentre(), pale.withAlpha(0.0f), bank.getCentre().translated(bank.getWidth() * 0.5f, 0), true));
                    g.fillEllipse(bank);
                }
                break;
            }
            default: break;
        }
    }
}

void LightSeekerView::settleForTest() {
    const auto px = pixelSize();
    if (zoomAt > 0) { zoomAt = 0; goTo(View::Place, { shown, dc::clampSubLocation(shown, shownSub) }, 1); }
    if (pending) cache.renderNow(pendingScene, px.x, px.y);
    if (!cache.ready(scene, px.x, px.y)) cache.renderNow(scene, px.x, px.y);
    transition.held = -1.0f;
    timerCallback();
    transition.active = false;
    transition.from = {};
    transition.to = {};
    transition.fromPlanes = {};
    transition.toPlanes = {};
    pathTo = -1;
    layoutOverlays();
    refreshFrame();
    refreshLights();
    stopTimer();
}

void LightSeekerView::holdTransitionForTest(float t) {
    const auto px = pixelSize();
    if (zoomAt > 0) { zoomAt = 0; goTo(View::Place, { shown, dc::clampSubLocation(shown, shownSub) }, 1); }
    if (!cache.ready(scene, px.x, px.y)) cache.renderNow(scene, px.x, px.y);
    if (pending && !cache.ready(pendingScene, px.x, px.y)) cache.renderNow(pendingScene, px.x, px.y);
    timerCallback();
    transition.held = juce::jlimit(0.0f, 1.0f, t);
    stopTimer();
}

void LightSeekerView::advanceLifeForTest(int frames) {
    for (int k = 0; k < frames; ++k) stepEmbers(1.0f / 15.0f);
}

juce::Rectangle<int> LightSeekerView::moonArea() const {
    if (planes.moonRadius <= 0.0f) return {};
    const float sx = static_cast<float>(getWidth()) / dc::art::virtualWidth;
    const auto c = toLocal(planes.moon);
    const float r = planes.moonRadius * sx * 2.1f;
    return juce::Rectangle<float>(2 * r, 2 * r).withCentre(c).getSmallestIntegerContainer().getIntersection(getLocalBounds());
}

juce::Rectangle<int> LightSeekerView::mistArea() const {
    // The low ground where the mist drifts (only scenes with a fog layer).
    // Above the controls: under them the mist would only make the controls repaint.
    if (!planes.fog.isValid()) return {};
    const float sy = static_cast<float>(getHeight()) / dc::art::virtualHeight;
    if (view == View::Map && scene.isMap()) {
        // The map: the haze drifts along the horizon, at the foot of the far ranges.
        return juce::Rectangle<float>(0.0f, 62.0f * sy, static_cast<float>(getWidth()), 78.0f * sy).getSmallestIntegerContainer();
    }
    const float bottom = controlsShown ? controlsArea().getY() - 36.0f : 540.0f * sy;
    return juce::Rectangle<float>(0.0f, bottom - 150.0f * sy, static_cast<float>(getWidth()), 150.0f * sy).getSmallestIntegerContainer();
}

juce::Rectangle<int> LightSeekerView::airArea() const {
    // The map's land, from just under the horizon to above the controls.
    if (view != View::Map || !scene.isMap() || !planes.layered()) return {};
    const float sy = static_cast<float>(getHeight()) / dc::art::virtualHeight;
    const float bottom = controlsShown ? controlsArea().getY() - 36.0f : static_cast<float>(getHeight());
    const float top = 92.0f * sy;
    if (bottom <= top) return {};
    return juce::Rectangle<float>(0.0f, top, static_cast<float>(getWidth()), bottom - top).getSmallestIntegerContainer();
}

const juce::Image& LightSeekerView::scaledTile(std::size_t slot, const juce::Image& source, int width, int height) {
    auto& tile = scaledTiles[slot];
    width = std::max(1, width); height = std::max(1, height);
    if (tile.getWidth() != width || tile.getHeight() != height)
        tile = source.rescaled(width, height, juce::Graphics::highResamplingQuality);
    return tile;
}

void LightSeekerView::paintMapAir(juce::Graphics& g, double now, float ps) {
    const auto area = juce::Rectangle<float>(airArea().toFloat()).transformedBy(juce::AffineTransform::scale(ps)).getSmallestIntegerContainer();
    if (area.isEmpty() || !g.clipRegionIntersects(area)) return;
    // Shadows of passing clouds drifting across the sunlit land: two sheets, the far one
    // smaller and slower (perspective), soft lavender shade. A tileable texture, scaled.
    const auto& tex = dc::art::cloudShadowTexture();
    const float sx = ps * static_cast<float>(getWidth()) / dc::art::virtualWidth;
    const float t = static_cast<float>(std::fmod(now, 20000.0));
    const auto band = area.toFloat();
    const float bottom = ps * static_cast<float>(getHeight()) * 540.0f / dc::art::virtualHeight;   // the land's own extent
    const struct { float from, to, scale, speed; } sheets[] = { { 0.0f, 0.42f, 0.55f, 2.6f }, { 0.28f, 1.0f, 1.0f, 5.5f } };
    g.saveState();
    g.reduceClipRegion(area);
    std::size_t slot = 0;
    for (const auto& sheet : sheets) {
        const int y0 = juce::roundToInt(band.getY() + (bottom - band.getY()) * sheet.from), y1 = juce::roundToInt(band.getY() + (bottom - band.getY()) * sheet.to);
        const int w = juce::roundToInt(520.0f * sx * sheet.scale);
        const auto& tile = scaledTile(slot++, tex, w, y1 - y0);
        int x = static_cast<int>(std::fmod(t * sheet.speed * sx, static_cast<float>(w)));
        if (x > 0) x -= w;
        for (; x < area.getRight(); x += w)
            if (g.clipRegionIntersects({ x, y0, w, tile.getHeight() })) g.drawImageAt(tile, x, y0);
    }
    g.restoreState();
}

void LightSeekerView::paintMoonGlow(juce::Graphics& g, double now) {
    const auto area = moonArea();
    if (area.isEmpty() || !g.clipRegionIntersects(area)) return;
    // A faint, slow breathing of golden light around the sun (about 7 s per breath).
    const float sx = static_cast<float>(getWidth()) / dc::art::virtualWidth;
    const auto c = toLocal(planes.moon);
    const float r = planes.moonRadius * sx;
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(std::fmod(now, 1000.0)) * juce::MathConstants<float>::twoPi / 7.0f);
    juce::ColourGradient glow(colours::blood.withAlpha(0.0f), c, colours::blood.withAlpha(0.0f), c.translated(r * 2.1f, 0.0f), true);
    glow.addColour(0.42, colours::blood.withAlpha(0.03f + 0.07f * pulse));
    g.setGradientFill(glow);
    g.fillEllipse(juce::Rectangle<float>(4.2f * r, 4.2f * r).withCentre(c));
}

void LightSeekerView::paintMist(juce::Graphics& g, double now) {
    const auto area = mistArea();
    if (area.isEmpty() || !g.clipRegionIntersects(area)) return;
    // Two sheets of wisps, tileable, drifting slowly at different speeds (parallax),
    // tinted slate: slow fog drift over the low ground.
    const auto& tex = dc::art::mistTexture();
    const float t = static_cast<float>(std::fmod(now, 10000.0));
    const auto band = area.toFloat();
    const struct { float height, speed, alpha; } sheets[] = { { 1.0f, 7.0f, 0.5f }, { 0.7f, -4.5f, 0.38f } };
    g.saveState();
    g.reduceClipRegion(area);
    std::size_t slot = 2;
    for (const auto& sheet : sheets) {
        const int h = juce::roundToInt(band.getHeight() * sheet.height);
        const int w = juce::roundToInt(static_cast<float>(h) * static_cast<float>(tex.getWidth()) / static_cast<float>(tex.getHeight()));
        const auto& tile = scaledTile(slot++, tex, w, h);
        const int y = juce::roundToInt(band.getBottom()) - h;
        int x = static_cast<int>(std::fmod(t * sheet.speed * static_cast<float>(getWidth()) / dc::art::virtualWidth, static_cast<float>(w)));
        if (x > 0) x -= w;
        g.setOpacity(sheet.alpha);
        for (; x < area.getRight(); x += w)
            if (g.clipRegionIntersects({ x, y, w, h })) g.drawImageAt(tile, x, y);
    }
    g.restoreState();
}

void LightSeekerView::stepEmbers(float dt) {
    if (emberSources.empty()) { embers.clear(); return; }
    const float sx = static_cast<float>(getWidth()) / dc::art::virtualWidth;
    for (auto& e : embers) {
        e.age += dt;
        e.phase += dt * 2.3f;
        e.v.x += std::sin(e.phase) * 9.0f * sx * dt;
        e.p += e.v * dt;
    }
    // Embers die when they burn out or drift into the controls (no repaints there).
    const float floor = controlsShown ? controlsArea().getY() - 40.0f : static_cast<float>(getHeight());
    embers.erase(std::remove_if(embers.begin(), embers.end(), [floor](const Ember& e) { return e.age >= e.life || e.p.y > floor || e.p.y < 0.0f; }), embers.end());
    std::vector<const dc::art::LightSource*> sources;
    for (const auto& src : emberSources)
        if (toLocal(src.position).y < floor - 10.0f) sources.push_back(&src);
    if (sources.empty()) return;
    const int wanted = juce::jlimit(6, 16, static_cast<int>(sources.size()) * 5);
    while (static_cast<int>(embers.size()) < wanted) {
        const auto& src = *sources[static_cast<std::size_t>(random.nextInt(static_cast<int>(sources.size())))];
        const float spread = juce::jmax(6.0f, src.radius * 0.8f) * sx;
        Ember e;
        e.p = toLocal(src.position) + juce::Point<float>((random.nextFloat() - 0.5f) * 2.0f * spread, (random.nextFloat() - 0.6f) * spread * 0.5f);
        e.v = { (random.nextFloat() - 0.5f) * 8.0f * sx, -(4.0f + 9.0f * random.nextFloat()) * sx };   // light motes drift slowly up
        e.age = 0;
        e.life = 3.0f + 4.0f * random.nextFloat();
        e.size = (0.9f + 1.3f * random.nextFloat()) * sx;
        e.phase = random.nextFloat() * 6.28f;
        embers.push_back(e);
    }
}

void LightSeekerView::paintEmbers(juce::Graphics& g) {
    for (const auto& e : embers) {
        const auto box = juce::Rectangle<float>(e.size * 8.0f, e.size * 8.0f).withCentre(e.p);
        if (!g.clipRegionIntersects(box.getSmallestIntegerContainer())) continue;
        const float u = e.age / e.life;
        const float alpha = juce::jlimit(0.0f, 1.0f, std::min(u * 6.0f, 1.0f) * (1.0f - u));
        g.setGradientFill(juce::ColourGradient(colours::ember.withAlpha(0.35f * alpha), e.p, colours::blood.withAlpha(0.0f), e.p.translated(e.size * 3.5f, 0.0f), true));
        g.fillEllipse(juce::Rectangle<float>(e.size * 7.0f, e.size * 7.0f).withCentre(e.p));
        g.setColour(juce::Colour(0xfffff3c8).withAlpha(0.9f * alpha * (0.6f + 0.4f * std::sin(e.phase * 3.0f))));
        g.fillEllipse(juce::Rectangle<float>(e.size, e.size).withCentre(e.p));
    }
}

void LightSeekerView::drawPlanes(juce::Graphics& g, const dc::art::Planes& p, const juce::Image& flat, std::array<float, 4> scales,
                                    juce::Point<float> about, float alpha) {
    // Each layer scaled about a point by its own factor (back, mid, fog, near): parallax.
    if (alpha <= 0.0f) return;
    const auto bounds = getLocalBounds().toFloat();
    const auto draw = [&](const juce::Image& image, float scale, juce::Rectangle<int> area, int fullW, int fullH) {
        if (image.isNull()) return;
        const float fx = bounds.getWidth() / static_cast<float>(fullW), fy = bounds.getHeight() / static_cast<float>(fullH);
        const auto place = juce::AffineTransform::scale(static_cast<float>(area.getWidth()) / static_cast<float>(image.getWidth()),
                                                        static_cast<float>(area.getHeight()) / static_cast<float>(image.getHeight()))
                               .translated(static_cast<float>(area.getX()), static_cast<float>(area.getY()))
                               .scaled(fx, fy)
                               .followedBy(juce::AffineTransform::scale(scale, scale, about.x, about.y));
        g.setOpacity(alpha);
        g.drawImageTransformed(image, place);
    };
    if (!p.layered()) {
        const auto& image = flat.isValid() ? flat : p.flat;
        if (image.isValid()) draw(image, scales[1], image.getBounds(), image.getWidth(), image.getHeight());
        g.setOpacity(1.0f);
        return;
    }
    const int w = p.back.getWidth(), h = p.back.getHeight();
    draw(p.back, scales[0], p.back.getBounds(), w, h);
    draw(p.mid, scales[1], { 0, 0, w, h }, w, h);
    draw(p.fog, scales[2], { 0, 0, w, h }, w, h);
    draw(p.near, scales[3], p.nearArea, w, h);
    g.setOpacity(1.0f);
}

void LightSeekerView::layoutOverlays() {
    const bool map = view == View::Map && !transition.active && !pending;
    const bool place = view == View::Place && !transition.active && !pending;
    for (int i = 0; i < dc::LocationCount; ++i) {
        const auto& l = dc::art::mapLandmarks()[static_cast<std::size_t>(i)];
        const auto c = toLocal(l.position);
        const float r = l.radius * static_cast<float>(getWidth()) / dc::art::virtualWidth;
        auto& button = *landmarks[static_cast<std::size_t>(i)];
        button.setBounds(juce::Rectangle<float>(2 * r, 2 * r).withCentre(c).toNearestInt());
        button.setVisible(map);
    }
    const int count = shown >= 0 ? dc::subLocationCount(shown) : 0;
    const auto spots = shown >= 0 ? dc::art::sceneHotspots(shown, dc::clampSubLocation(shown, shownSub)) : std::vector<dc::art::Hotspot> {};
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& spot = *hotspots[static_cast<std::size_t>(k)];
        const auto found = std::find_if(spots.begin(), spots.end(), [k](const dc::art::Hotspot& h) { return h.sub == k; });
        if (found != spots.end() && k <= count) {
            const auto c = toLocal(found->position);
            spot.setBounds(juce::Rectangle<int>(160, 52).withCentre({ juce::roundToInt(c.x), juce::roundToInt(c.y) + 12 }));
            spot.setVisible(place);
        } else spot.setVisible(false);
    }
    // The compact list of places, top right.
    const int rowHeight = 26, listWidth = 172;
    for (int k = 0; k < dc::placesPerLocation; ++k) {
        auto& button = *subs[static_cast<std::size_t>(k)];
        button.setBounds(getWidth() - listWidth - 14, 16 + k * rowHeight, listWidth, rowHeight);
        // Shown throughout a place (also while travelling between its places): hiding it
        // under the mouse and showing it again was one way a hover could stick.
        button.setVisible(view == View::Place && k <= count);
    }
    // The magnifier, top right, left of the place list.
    searchButton.setBounds(getWidth() - listWidth - 14 - 40, 14, 34, 28);
    // Breadcrumb, top left.
    mapCrumb.setBounds(14, 14, 74, 28);
    const int locationWidth = 30 + juce::roundToInt(juce::GlyphArrangement::getStringWidth(labelFont(16.0f), locationCrumb.getButtonText()));
    locationCrumb.setBounds(mapCrumb.getRight() + 4, 14, locationWidth, 28);
    placeCrumb.setBounds(locationCrumb.getRight(), 14, 320, 28);
    locationCrumb.setVisible(!map || true);
    placeCrumb.setVisible(view == View::Place);
    // The Journey badge and the note: top centre, clear of the breadcrumb's text and the
    // place list.
    const int crumbText = placeCrumb.isVisible() ? placeCrumb.getX() + juce::roundToInt(juce::GlyphArrangement::getStringWidth(titleFont(16.0f), placeCrumb.getText())) + 14
                                                 : locationCrumb.getRight();
    const int badgeX = juce::jlimit(0, searchButton.getX() - 8 - 236, juce::jmax((getWidth() - 236) / 2, crumbText + 12));
    steerBadge.setBounds(badgeX, 14, 236, 28);
    notice.setBounds(badgeX, 46, 236, 24);
}

void LightSeekerView::paintTransition(juce::Graphics& g, double now) {
    const float t = ease(transition.held >= 0.0f ? transition.held : static_cast<float>((now - transition.start) / transition.length));
    const auto bounds = getLocalBounds().toFloat();
    const auto focus = toLocal(transition.focus);
    const auto centre = bounds.getCentre();
    g.fillAll(colours::ink);
    const auto& from = transition.fromPlanes;
    const auto& to = transition.toPlanes;
    if (transition.kind == 1) {
        // Into a place: the map's layers rush past at their own speeds towards the
        // landmark (the clouds fastest), while the place's layers settle in.
        drawPlanes(g, from, transition.from, { 1.0f + 1.5f * t, 1.0f + 2.4f * t, 1.0f + 2.9f * t, 1.0f + 3.8f * t }, focus, 1.0f);
        drawPlanes(g, to, transition.to, { 1.08f - 0.08f * t, 1.16f - 0.16f * t, 1.22f - 0.22f * t, 1.34f - 0.34f * t }, centre, t);
    } else if (transition.kind == 2) {
        // Out to the map: the place recedes, the map's layers fall back into place.
        drawPlanes(g, from, transition.from, { 1.0f + 0.1f * t, 1.0f + 0.18f * t, 1.0f + 0.24f * t, 1.0f + 0.34f * t }, centre, 1.0f);
        const float u = 1.0f - t;
        drawPlanes(g, to, transition.to, { 1.0f + 1.5f * u, 1.0f + 2.4f * u, 1.0f + 2.9f * u, 1.0f + 3.8f * u }, focus, t);
    } else {
        drawPlanes(g, {}, transition.from, { 1, 1, 1, 1 }, centre, 1.0f);
        drawPlanes(g, {}, transition.to, { 1, 1, 1, 1 }, centre, t);
    }
}

void LightSeekerView::paintMapOverlay(juce::Graphics& g) {
    const auto& marks = dc::art::mapLandmarks();
    const float sx = static_cast<float>(getWidth()) / dc::art::virtualWidth;
    // The title, small, over the sky.
    {
        const auto font = titleFont(19.0f).withExtraKerningFactor(0.22f);
        const juce::String title("LIGHT SEEKER");
        const auto box = juce::Rectangle<float>(static_cast<float>(getWidth()) * 0.5f - 160.0f, 14.0f, 320.0f, 26.0f).translated(150.0f, 0.0f);
        g.setFont(font);
        g.setColour(colours::ink.withAlpha(0.6f));
        g.drawText(title, box.translated(0.0f, 1.5f), juce::Justification::centred);
        g.setColour(colours::ivory.withAlpha(0.88f));
        g.drawText(title, box, juce::Justification::centred);
        juce::ColourGradient rule(colours::blood.withAlpha(0.0f), box.getX() + 40.0f, 0, colours::blood.withAlpha(0.0f), box.getRight() - 40.0f, 0, false);
        rule.addColour(0.5, colours::blood.withAlpha(0.9f));
        g.setGradientFill(rule);
        g.fillRect(juce::Rectangle<float>(box.getX() + 40.0f, box.getBottom() + 2.0f, box.getWidth() - 80.0f, 1.0f));
    }
    // The current location: a crimson glow beneath it and a small diamond above.
    if (shown >= 0) {
        const auto& l = marks[static_cast<std::size_t>(shown)];
        const auto c = toLocal(l.position);
        const float r = l.radius * sx;
        juce::ColourGradient glow(colours::blood.withAlpha(0.42f), c, colours::blood.withAlpha(0.0f), c.translated(r * 1.7f, 0), true);
        glow.addColour(0.5, colours::blood.withAlpha(0.16f));
        g.setGradientFill(glow);
        g.fillEllipse(juce::Rectangle<float>(3.4f * r, 3.4f * r).withCentre(c));
        g.setColour(colours::blood.withAlpha(0.85f));
        g.drawEllipse(juce::Rectangle<float>(2 * r, 1.25f * r).withCentre(c.translated(0.0f, r * 0.55f)), 1.0f);
    }
    // Every landmark's name, small, under it: crimson for where you are.
    for (int i = 0; i < dc::LocationCount; ++i) {
        const auto& l = marks[static_cast<std::size_t>(i)];
        const auto c = toLocal(l.position);
        const bool here = i == shown;
        const auto font = titleFont(12.0f).withExtraKerningFactor(0.1f);
        const juce::String name = juce::String(dc::locationNames[static_cast<std::size_t>(i)]).toUpperCase();
        const float width = juce::GlyphArrangement::getStringWidth(font, name) + 10.0f;
        auto box = juce::Rectangle<float>(width, 16.0f).withCentre({ c.x, c.y + l.radius * sx * 0.62f + 14.0f });
        box = box.withX(juce::jlimit(4.0f, static_cast<float>(getWidth()) - width - 4.0f, box.getX()));
        if (!g.clipRegionIntersects(box.expanded(14.0f, 8.0f).getSmallestIntegerContainer())) continue;
        g.setGradientFill(juce::ColourGradient(colours::ink.withAlpha(0.6f), box.getCentre(), colours::ink.withAlpha(0.0f),
                                               box.getCentre().translated(box.getWidth() * 0.62f, 0.0f), true));
        g.fillRect(box.expanded(10.0f, 4.0f));
        g.setFont(font);
        g.setColour(here ? colours::blood.brighter(0.45f) : (i == hovered ? colours::ivory : colours::ivory.withAlpha(0.78f)));
        g.drawText(name, box, juce::Justification::centred);
        if (here) {
            juce::Path diamond;
            const auto d = juce::Point<float>(box.getX() - 4.0f, box.getCentreY());
            diamond.addQuadrilateral(d.x, d.y - 3.5f, d.x + 3.5f, d.y, d.x, d.y + 3.5f, d.x - 3.5f, d.y);
            g.setColour(colours::blood.brighter(0.2f));
            g.fillPath(diamond);
        }
    }
    // The journey: a dashed red path, drawn as far as the sound has travelled.
    if (pathFrom >= 0 && pathTo >= 0 && pathFrom != pathTo) {
        const auto a = toLocal(marks[static_cast<std::size_t>(pathFrom)].position), b = toLocal(marks[static_cast<std::size_t>(pathTo)].position);
        const float progress = processor.travelProgress.load() < 1.0f ? processor.travelProgress.load() : 1.0f;
        const float fade = pathEnded > 0 ? juce::jlimit(0.0f, 1.0f, 1.0f - static_cast<float>((nowSeconds() - pathEnded) / 1.5)) : 1.0f;
        const auto mid = (a + b) * 0.5f + juce::Point<float>(-(b.y - a.y), b.x - a.x) * 0.18f;
        juce::Path curve;
        curve.startNewSubPath(a);
        curve.quadraticTo(mid, b);
        juce::Path partial;
        const float length = curve.getLength();
        juce::PathStrokeType(2.4f).createDashedStroke(partial, curve, std::array<float, 2> { 7.0f, 5.0f }.data(), 2);
        g.saveState();
        // Reveal the dashes up to the travelled point.
        const auto head = curve.getPointAlongPath(length * progress);
        juce::Path reveal;
        reveal.addEllipse(juce::Rectangle<float>(1, 1).withCentre(a));
        for (int k = 0; k <= 24; ++k) {
            const auto pt = curve.getPointAlongPath(length * progress * static_cast<float>(k) / 24.0f);
            reveal.addEllipse(juce::Rectangle<float>(16, 16).withCentre(pt));
        }
        g.reduceClipRegion(reveal);
        g.setColour(colours::blood.withAlpha(0.9f * fade));
        g.fillPath(partial);
        g.restoreState();
        g.setColour(colours::blood.withAlpha(fade));
        g.fillEllipse(juce::Rectangle<float>(7, 7).withCentre(head));
    }
    // The journey route, if one is set: faint red dotted lines from stop to stop.
    {
        const auto stops = processor.route();
        for (std::size_t k = 1; k < stops.size(); ++k) {
            const auto a = toLocal(marks[static_cast<std::size_t>(stops[k - 1].location)].position), b = toLocal(marks[static_cast<std::size_t>(stops[k].location)].position);
            if (a == b) continue;
            juce::Path seg; seg.startNewSubPath(a); seg.lineTo(b);
            juce::Path dots;
            const float pattern[] = { 2.0f, 6.0f };
            juce::PathStrokeType(1.6f).createDashedStroke(dots, seg, pattern, 2);
            g.setColour(colours::blood.withAlpha(0.55f));
            g.fillPath(dots);
        }
        for (std::size_t k = 0; k < stops.size(); ++k) {
            const auto c = toLocal(marks[static_cast<std::size_t>(stops[k].location)].position).translated(18.0f, -18.0f);
            g.setColour(colours::ink.withAlpha(0.7f));
            g.fillEllipse(juce::Rectangle<float>(16, 16).withCentre(c));
            g.setColour(colours::blood.brighter(0.3f));
            g.setFont(labelFont(11.0f));
            g.drawText(juce::String(static_cast<int>(k) + 1), juce::Rectangle<float>(16, 16).withCentre(c), juce::Justification::centred);
        }
    }
    // The hovered landmark's name, on a dark plate.
    if (hovered >= 0) {
        const auto& l = marks[static_cast<std::size_t>(hovered)];
        const auto c = toLocal(l.position);
        const juce::String text = juce::String(dc::locationNames[static_cast<std::size_t>(hovered)]) + "  \xc2\xb7  "
                                + juce::String(dc::subLocationCount(hovered) + 1) + " places";
        const auto font = titleFont(17.0f);
        const float width = juce::GlyphArrangement::getStringWidth(font, juce::String::fromUTF8(text.toRawUTF8())) + 24.0f;
        auto plate = juce::Rectangle<float>(width, 28.0f).withCentre({ c.x, c.y - l.radius * static_cast<float>(getWidth()) / dc::art::virtualWidth - 22.0f });
        plate = plate.withX(juce::jlimit(6.0f, static_cast<float>(getWidth()) - width - 6.0f, plate.getX()));
        g.setGradientFill(juce::ColourGradient(colours::ink.withAlpha(0.82f), plate.getCentre(), colours::ink.withAlpha(0.0f),
                                               plate.getCentre().translated(plate.getWidth() * 0.6f, 0.0f), true));
        g.fillRect(plate.expanded(14.0f, 6.0f));
        juce::ColourGradient rule(colours::blood.withAlpha(0.0f), plate.getX(), 0, colours::blood.withAlpha(0.0f), plate.getRight(), 0, false);
        rule.addColour(0.5, colours::blood);
        g.setGradientFill(rule);
        g.fillRect(plate.withTop(plate.getBottom() - 1.0f));
        g.setColour(colours::ivory);
        g.setFont(font);
        g.drawText(juce::String::fromUTF8(text.toRawUTF8()), plate, juce::Justification::centred);
    }
}

void LightSeekerView::paint(juce::Graphics& g) {
    const double now = nowSeconds();
    const auto px = pixelSize();
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    if (transition.active) paintTransition(g, now);
    else {
        // Double-buffered (Direction Update 2): every region, however small, is painted
        // again from the frame (the scene at rest); never a fill over the art. Only before
        // the very first scene is ready, a soft sky.
        juce::ignoreUnused(px);
        if (view == View::Map && airFrame.isValid() && animateLife) g.drawImage(airFrame, getLocalBounds().toFloat());
        else if (frame.isValid()) g.drawImage(frame, getLocalBounds().toFloat());
        else {
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffa9c8ec), 0.0f, 0.0f, juce::Colour(0xfff6ecd8), 0.0f, static_cast<float>(getHeight()) * 0.7f, false));
            g.fillAll();
            g.setColour(colours::ash.withAlpha(0.6f));
            g.setFont(labelFont(14.0f));
            g.drawText(scene.isMap() ? "unrolling the map..." : "travelling...", getLocalBounds(), juce::Justification::centred);
        }
        if (animateLife && planes.layered()) {
            paintMoonGlow(g, now);
            paintLife(g, now);
            paintMist(g, now);
            paintEmbers(g);
            paintBirds(g);
        } else if (view == View::Place && animateLife) paintLife(g, now);
        if (view == View::Map && !pending) paintMapOverlay(g);
    }
    // Breadcrumb plate.
    // Soft dark vignettes behind the breadcrumb (top left) and the place list (top right).
    const auto vignette = [&g](juce::Rectangle<float> r) {
        // An elliptical soft shade (a small radial falloff image stretched over the area).
        static const juce::Image falloff = [] {
            juce::Image image(juce::Image::ARGB, 64, 64, true, juce::SoftwareImageType());
            juce::Graphics ig(image);
            juce::ColourGradient shade(colours::ink.withAlpha(0.72f), 32.0f, 32.0f, colours::ink.withAlpha(0.0f), 64.0f, 32.0f, true);
            shade.addColour(0.55, colours::ink.withAlpha(0.5f));
            ig.setGradientFill(shade);
            ig.fillAll();
            return image;
        }();
        g.drawImage(falloff, r.expanded(r.getWidth() * 0.25f, r.getHeight() * 0.45f), juce::RectanglePlacement::stretchToFit);
    };
    const float crumbRight = static_cast<float>(placeCrumb.isVisible() ? placeCrumb.getX() + juce::roundToInt(juce::GlyphArrangement::getStringWidth(titleFont(16.0f), placeCrumb.getText())) + 14
                                                                     : locationCrumb.getRight());
    vignette(juce::Rectangle<float>(0.0f, 0.0f, crumbRight + 10.0f, 52.0f));
    if (view == View::Place && !transition.active && shown >= 0) {
        const int count = dc::subLocationCount(shown);
        vignette(juce::Rectangle<float>(static_cast<float>(getWidth()) - 200.0f, 0.0f, 200.0f, static_cast<float>((count + 1) * 26 + 26)));
        // A hairline beside the list.
        juce::ColourGradient rule(colours::blood.withAlpha(0.0f), 0.0f, 16.0f, colours::blood.withAlpha(0.0f), 0.0f, static_cast<float>(16 + (count + 1) * 26), false);
        rule.addColour(0.5, colours::blood.withAlpha(0.6f));
        g.setGradientFill(rule);
        g.fillRect(static_cast<float>(getWidth()) - 190.0f, 16.0f, 1.0f, static_cast<float>((count + 1) * 26));
    }
    // The controls: a dark veil fading up into the art, a crimson hairline, small group
    // headings and thin dividers (no boxes).
    if (controlsShown && g.clipRegionIntersects(controlsArea().withTop(controlsArea().getY() - 36.0f).getSmallestIntegerContainer())) {
        const auto strip = controlsArea();
        const auto veil = strip.withTop(strip.getY() - 36.0f);
        juce::ColourGradient fade(colours::ink.withAlpha(0.0f), 0.0f, veil.getY(), colours::ink.withAlpha(0.9f), 0.0f, strip.getY() + 30.0f, false);
        fade.addColour(0.45, colours::ink.withAlpha(0.55f));
        g.setGradientFill(fade);
        g.fillRect(veil);
        juce::ColourGradient line(colours::blood.withAlpha(0.0f), strip.getX(), 0.0f, colours::blood.withAlpha(0.0f), strip.getRight(), 0.0f, false);
        line.addColour(0.5, colours::blood.withAlpha(0.7f));
        g.setGradientFill(line);
        g.fillRect(strip.getX(), strip.getY(), strip.getWidth(), 1.0f);
        const float rowTop = strip.getY() + 40.0f;
        g.setFont(titleFont(10.5f).withExtraKerningFactor(0.2f));
        g.setColour(colours::ash.withAlpha(0.7f));
        g.drawText("CHARACTER", juce::Rectangle<float>(16.0f, rowTop, 200.0f, 12.0f), juce::Justification::centredLeft);
        g.drawText("LEVELS", juce::Rectangle<float>(248.0f, rowTop, 200.0f, 12.0f), juce::Justification::centredLeft);
        g.drawText("TRAVEL", juce::Rectangle<float>(520.0f, rowTop, 200.0f, 12.0f), juce::Justification::centredLeft);
        for (float x : { 232.0f, 504.0f, 776.0f }) {
            juce::ColourGradient div(colours::ivory.withAlpha(0.0f), x, rowTop, colours::ivory.withAlpha(0.0f), x, strip.getBottom() - 8.0f, false);
            div.addColour(0.5, colours::ivory.withAlpha(0.16f));
            g.setGradientFill(div);
            g.fillRect(x, rowTop, 1.0f, strip.getBottom() - 8.0f - rowTop);
        }
        juce::ColourGradient tool(colours::ivory.withAlpha(0.0f), 16.0f, 0.0f, colours::ivory.withAlpha(0.0f), strip.getRight() - 16.0f, 0.0f, false);
        tool.addColour(0.5, colours::ivory.withAlpha(0.08f));
        g.setGradientFill(tool);
        g.fillRect(16.0f, strip.getY() + 36.0f, strip.getWidth() - 32.0f, 1.0f);
    }
}

void LightSeekerView::resized() {
    const int w = getWidth(), h = getHeight();
    const int top = h - static_cast<int>(stripHeight);
    const auto place = [](Fader& f, juce::Rectangle<int> r) {
        f.label.setBounds(r.removeFromBottom(17));
        f.slider.setBounds(r.reduced(2, 0));
    };
    // Row 1: the toolbar (presets, A/B, undo/redo, settings; Atmosphere on the right).
    const int ty = top + 8, th = 24;
    prevPreset.setBounds(14, ty, 24, th);
    presetButton.setBounds(38, ty, 224, th);
    nextPreset.setBounds(262, ty, 24, th);
    slotA.setBounds(304, ty, 28, th);
    slotB.setBounds(332, ty, 28, th);
    copySlot.setBounds(360, ty, 46, th);
    undoButton.setBounds(422, ty, 28, th);
    redoButton.setBounds(450, ty, 28, th);
    settingsButton.setBounds(492, ty, 28, th);
    atmosphereLabel.setBounds(w - 270, ty, 90, th);
    atmosphere.setBounds(w - 178, ty, 164, th);
    // Row 2: character (the place's macros), levels, travel, and the three switches.
    const int row = top + 54, rowHeight = h - 6 - row;
    for (int m = 0; m < dc::macroCount; ++m)
        place(macros[static_cast<std::size_t>(m)], { 14 + m * 54, row, 54, rowHeight });
    const int mainX = 246;
    // Levels: Input, Intensity, Mix, Shimmer (final touches), Output.
    place(input, { mainX, row, 48, rowHeight });
    place(intensity, { mainX + 48, row, 58, rowHeight });
    place(mix, { mainX + 106, row, 48, rowHeight });
    place(shimmer, { mainX + 154, row, 58, rowHeight });
    place(output, { mainX + 212, row, 48, rowHeight });
    travelLabel.setBounds(520, row + 2, 60, 22);
    travelTime.setBounds(574, row + 2, 190, 22);
    travelSync.setBounds(522, row + 36, 150, 24);
    journeyToggle.setBounds(792, row - 2, 150, 26);
    bypass.setBounds(792, row + 25, 150, 26);
    advancedToggle.setBounds(792, row + 52, 150, 26);
    macroHeading.setBounds(0, 0, 0, 0);
    controlsToggle.setBounds(w - 38, controlsShown ? top - 30 : h - 32, 30, 26);
    for (auto* c : std::initializer_list<juce::Component*> { &input.slider, &input.label, &intensity.slider, &intensity.label, &mix.slider, &mix.label,
                                                             &shimmer.slider, &shimmer.label, &output.slider, &output.label, &travelLabel, &travelTime, &travelSync, &bypass, &advancedToggle, &journeyToggle,
                                                             &macroHeading, &atmosphere, &atmosphereLabel, &presetButton, &prevPreset, &nextPreset, &slotA, &slotB,
                                                             &copySlot, &undoButton, &redoButton, &settingsButton })
        c->setVisible(controlsShown);
    if (shown >= 0)
        for (int m = 0; m < dc::macroCount; ++m) {
            const bool used = m < dc::macroCountFor(shown);
            macros[static_cast<std::size_t>(m)].slider.setVisible(used && controlsShown);
            macros[static_cast<std::size_t>(m)].label.setVisible(used && controlsShown);
        }
    // The panels end above the controls (their buttons stay clickable) and, with the
    // controls hidden, above the show-controls button. The Journey panel sits on the
    // left so the place list (top right) stays free.
    const int panelBottom = controlsShown ? top - 34 : h - 44;
    drawer.setBounds(200, 56, w - 214, panelBottom - 56);
    journeyPanel.setBounds(14, 56, 330, panelBottom - 56);
    layoutOverlays();
    layoutSearch();
    const auto px = pixelSize();
    cache.request(scene, px.x, px.y);
    wakeTimer();
}

//==============================================================================
LightSeekerView::JourneyPanel::JourneyPanel(LightSeekerProcessor& p, std::function<dc::Stop()> place)
    : processor(p), currentPlace(std::move(place)) {
    setOpaque(true);
    setWantsKeyboardFocus(true);
    close.setButtonText(juce::String::fromUTF8("\xe2\x9c\x95"));
    close.setTooltip("Close (Esc)");
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(close);
    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    body.painter = [this](juce::Graphics& g) {
        g.setColour(colours::blood.brighter(0.3f));
        g.setFont(titleFont(15.0f));
        g.drawText("Roam", 12, roam.getY() - 26, 200, 20, juce::Justification::centredLeft);
        g.drawText("MIDI travel", 12, midi.getY() - 26, 200, 20, juce::Justification::centredLeft);
        g.drawText("Atmosphere", 12, atmosDensity.getY() - 24, 200, 20, juce::Justification::centredLeft);
    };
    auto& state = processor.parameters;
    journeyAttachment = std::make_unique<juce::ButtonParameterAttachment>(*state.getParameter(dc::ids::journeyOn), journey);
    roamAttachment = std::make_unique<juce::ButtonParameterAttachment>(*state.getParameter(dc::ids::roamOn), roam);
    midiAttachment = std::make_unique<juce::ButtonParameterAttachment>(*state.getParameter(dc::ids::midiTravel), midi);
    mode.addItemList(juce::StringArray::fromTokens(dc::journeyModeChoices, "|", ""), 1);
    roamRate.addItemList(juce::StringArray::fromTokens(dc::roamBarNames, "|", ""), 1);
    octave.addItemList(juce::StringArray::fromTokens(dc::midiOctaveNames, "|", ""), 1);
    modeAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*state.getParameter(dc::ids::journeyMode), mode);
    rateAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*state.getParameter(dc::ids::roamBars), roamRate);
    octaveAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*state.getParameter(dc::ids::midiOctave), octave);
    for (auto* sl : { &bars, &randomness, &seed }) {
        sl->setTextBoxStyle(juce::Slider::TextBoxRight, true, 56, 18);
        sl->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); sl->setColour(juce::Slider::textBoxTextColourId, colours::ash);
    }
    barsAttachment = std::make_unique<juce::SliderParameterAttachment>(*state.getParameter(dc::ids::journeyBars), bars);
    for (auto* sl : { &atmosDensity, &atmosSeed }) {
        sl->setTextBoxStyle(juce::Slider::TextBoxRight, true, 56, 18);
        sl->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); sl->setColour(juce::Slider::textBoxTextColourId, colours::ash);
    }
    atmosDensityAttachment = std::make_unique<juce::SliderParameterAttachment>(*state.getParameter(dc::ids::atmosDensity), atmosDensity);
    atmosSeedAttachment = std::make_unique<juce::SliderParameterAttachment>(*state.getParameter(dc::ids::atmosSeed), atmosSeed);
    atmosDensityLabel.setText("Density", juce::dontSendNotification);
    atmosSeedLabel.setText("Seed", juce::dontSendNotification);
    randomAttachment = std::make_unique<juce::SliderParameterAttachment>(*state.getParameter(dc::ids::roamRandomness), randomness);
    seedAttachment = std::make_unique<juce::SliderParameterAttachment>(*state.getParameter(dc::ids::roamSeed), seed);
    barsLabel.setText("per stop", juce::dontSendNotification);
    randomLabel.setText("Randomness", juce::dontSendNotification);
    seedLabel.setText("Seed", juce::dontSendNotification);
    midiHint.setText("Notes C..A of this octave choose the ten locations, C..E an octave higher the places. "
                     "Needs a host that sends MIDI to effects (Logic: side-chain MIDI; VST3 hosts). GarageBand does not.",
                     juce::dontSendNotification);
    for (auto* label : { &barsLabel, &randomLabel, &seedLabel, &atmosDensityLabel, &atmosSeedLabel }) label->setFont(labelFont(14.0f));
    midiHint.setFont(labelFont(11.5f));
    midiHint.setColour(juce::Label::textColourId, colours::ash);
    routeText.setFont(labelFont(13.0f));
    routeText.setJustificationType(juce::Justification::topLeft);
    add.onClick = [this] {
        auto stops = processor.route();
        if (static_cast<int>(stops.size()) < dc::maxStops) { stops.push_back(currentPlace()); processor.setRoute(stops); }
        refresh();
    };
    removeLast.onClick = [this] { auto stops = processor.route(); if (!stops.empty()) stops.pop_back(); processor.setRoute(stops); refresh(); };
    clear.onClick = [this] { processor.setRoute({}); refresh(); };
    for (auto* c : std::initializer_list<juce::Component*> { &journey, &roam, &midi, &mode, &roamRate, &octave, &bars, &randomness, &seed,
                                                             &add, &removeLast, &clear, &routeText, &barsLabel, &randomLabel, &seedLabel, &midiHint,
                                                             &atmosDensity, &atmosSeed, &atmosDensityLabel, &atmosSeedLabel })
        body.addAndMakeVisible(*c);
    refresh();
}

void LightSeekerView::JourneyPanel::refresh() {
    juce::String text;
    int k = 0;
    for (const auto& st : processor.route())
        text << ++k << ".  " << dc::locationNames[static_cast<std::size_t>(st.location)] << " \xc2\xb7 " << dc::subLocationName(st.location, st.sub) << "\n";
    if (k == 0) text = "No route yet: go to a place and \"Add this place\" (up to 8 stops).";
    routeText.setText(juce::String::fromUTF8(text.toRawUTF8()), juce::dontSendNotification);
}

void LightSeekerView::JourneyPanel::paint(juce::Graphics& g) {
    g.fillAll(colours::ink.withAlpha(0.96f));
    g.setColour(colours::dust.withAlpha(0.6f));
    g.drawRect(getLocalBounds(), 1);
    g.setColour(colours::blood.brighter(0.3f));
    g.setFont(titleFont(15.0f));
    g.drawText("Journey", 12, 8, 200, 20, juce::Justification::centredLeft);
}

void LightSeekerView::JourneyPanel::resized() {
    // Title row (with the close button), then the contents, scrolling when the panel is
    // shorter than they are.
    close.setBounds(getWidth() - 34, 5, 28, 24);
    viewport.setBounds(getLocalBounds().withTrimmedTop(32).reduced(1, 0).withTrimmedBottom(1));
    const int w = viewport.getWidth() - viewport.getScrollBarThickness();
    journey.setBounds(10, 2, 100, 24);
    mode.setBounds(120, 2, w - 132, 24);
    bars.setBounds(10, 30, w - 90, 22);
    barsLabel.setBounds(w - 78, 30, 70, 22);
    routeText.setBounds(12, 56, w - 24, 110);
    add.setBounds(10, 168, 120, 26);
    removeLast.setBounds(134, 168, 100, 26);
    clear.setBounds(238, 168, w - 248, 26);
    roam.setBounds(10, 224, 80, 22);
    roamRate.setBounds(120, 224, w - 132, 22);
    randomLabel.setBounds(10, 250, 90, 20);
    randomness.setBounds(100, 250, w - 110, 20);
    seedLabel.setBounds(10, 272, 90, 20);
    seed.setBounds(100, 272, w - 110, 20);
    midi.setBounds(10, 322, 120, 22);
    octave.setBounds(140, 322, w - 152, 22);
    midiHint.setBounds(10, 346, w - 20, 44);
    atmosDensityLabel.setBounds(10, 416, 90, 20);
    atmosDensity.setBounds(100, 416, w - 110, 20);
    atmosSeedLabel.setBounds(10, 438, 90, 20);
    atmosSeed.setBounds(100, 438, w - 110, 20);
    body.setSize(w, 466);
}

//==============================================================================
LightSeekerEditor::LightSeekerEditor(LightSeekerProcessor& p) : AudioProcessorEditor(p), processor(p), content(p) {
    // Read the saved scale first: making the editor resizable clamps its (still empty)
    // size, and resized() stores the scale.
    const float scale = juce::jlimit(0.5f, 2.0f, processor.editorScale.load());
    setOpaque(true);
    addAndMakeVisible(content);
    if (processor.isInstrument()) {
        panel = std::make_unique<dc::ui::InstrumentPanel>(processor);
        addAndMakeVisible(*panel);
    }
    content.onScaleChosen = [this](float scale) { setScale(scale); };
    setResizable(true, true);
    const int h = baseHeight();
    setResizeLimits(defaultWidth / 2, h / 2, defaultWidth * 2, h * 2);
    if (auto* constrainer = getConstrainer()) constrainer->setFixedAspectRatio(static_cast<double>(defaultWidth) / h);
    setSize(juce::roundToInt(static_cast<float>(defaultWidth) * scale), juce::roundToInt(static_cast<float>(h) * scale));
}

bool LightSeekerEditor::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)) { content.openSearch(); return true; }
    if (key == juce::KeyPress::escapeKey && content.searchOpen()) { content.closeSearch(); return true; }
    return false;
}

void LightSeekerEditor::setScale(float scale) {
    scale = juce::jlimit(0.5f, 2.0f, scale);
    setSize(juce::roundToInt(static_cast<float>(defaultWidth) * scale), juce::roundToInt(static_cast<float>(baseHeight()) * scale));
}

void LightSeekerEditor::resized() {
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(defaultWidth);
    content.setBounds(0, 0, defaultWidth, defaultHeight);
    content.setTransform(juce::AffineTransform::scale(scale));
    content.scaleChanged();
    if (panel != nullptr) {
        panel->setBounds(0, defaultHeight, defaultWidth, dc::ui::InstrumentPanel::panelHeight);
        panel->setTransform(juce::AffineTransform::scale(scale));
    }
    processor.editorScale.store(scale);
}
