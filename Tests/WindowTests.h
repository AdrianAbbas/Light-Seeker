// Direction Update 2: the window (included by LightSeekerTests.cpp, "editor" group).
//   birds     a frame with doves over the scene: every partial repaint (the birds' region)
//             is drawn from the scene underneath, never a fill, at every window size, also
//             while the art at a new size is still being prepared
//   restore   a closed and reopened window comes back exactly as it was left (also from a
//             saved session): the view and scene, the panels, the controls, the keyboard
//   hover     hover clears when the mouse leaves; the selected place stays marked

namespace windowtests {
// Paints the editor into `backing` only inside `dirty` (editor coordinates): what the
// operating system does with a repaint of a region.
void paintRegion(juce::Component& editor, juce::Image& backing, juce::Rectangle<int> dirty) {
    juce::Graphics g(backing);
    g.reduceClipRegion(dirty);
    editor.paintEntireComponent(g, true);
}
// The share of pixels in `area` that differ from `reference` by more than `tolerance` (any channel).
double changedShare(const juce::Image& a, const juce::Image& reference, juce::Rectangle<int> area, int tolerance) {
    area = area.getIntersection(a.getBounds()).getIntersection(reference.getBounds());
    if (area.isEmpty()) return 0.0;
    const juce::Image::BitmapData da(a, juce::Image::BitmapData::readOnly), db(reference, juce::Image::BitmapData::readOnly);
    long long changed = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x) {
            const auto p = da.getPixelColour(x, y), q = db.getPixelColour(x, y);
            if (std::abs(p.getRed() - q.getRed()) > tolerance || std::abs(p.getGreen() - q.getGreen()) > tolerance
                || std::abs(p.getBlue() - q.getBlue()) > tolerance) ++changed;
        }
    return double(changed) / double(area.getWidth() * area.getHeight());
}
// The share of near-white pixels (all channels >= 240) in `area`.
double whiteShare(const juce::Image& a, juce::Rectangle<int> area) {
    area = area.getIntersection(a.getBounds());
    if (area.isEmpty()) return 0.0;
    const juce::Image::BitmapData d(a, juce::Image::BitmapData::readOnly);
    long long white = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x) {
            const auto p = d.getPixelColour(x, y);
            if (p.getRed() >= 240 && p.getGreen() >= 240 && p.getBlue() >= 230) ++white;
        }
    return double(white) / double(area.getWidth() * area.getHeight());
}
struct Window {
    std::unique_ptr<LightSeekerProcessor> p;
    std::unique_ptr<juce::AudioProcessorEditor> base;
    LightSeekerEditor* editor = nullptr;
    explicit Window(bool instrument = false) : p(std::make_unique<LightSeekerProcessor>(instrument)) {
        p->setRateAndBufferSizeDetails(48000, 512);
        p->prepareToPlay(48000, 512);
        open();
    }
    void open() {
        base.reset(p->createEditor());
        editor = dynamic_cast<LightSeekerEditor*>(base.get());
    }
    void close() { base.reset(); editor = nullptr; }
    LightSeekerView& view() { return editor->view(); }
    void go(int location, int sub) {
        view().mapButton().onClick();
        view().settleForTest();
        view().locationButton(location).onClick();
        view().settleForTest();
        view().subLocationButton(sub).onClick();
        view().settleForTest();
    }
};
} // namespace windowtests

// A frame with the doves over a sky scene, written for review (docs/snapshots).
int windowSnapshots(const juce::String& directory) {
    using namespace windowtests;
    juce::File dir(directory);
    dir.createDirectory();
    Window w;
    w.editor->setScale(1.5f);
    w.go(dc::SkyTemple, 0);
    auto& view = w.view();
    view.startFlockForTest();
    view.stepBirdsForTest(9.0f);
    const auto image = w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f);
    const auto birds = w.editor->getLocalArea(&view, view.birdsAreaForTest()).expanded(60, 40).getIntersection(image.getBounds());
    int written = 0;
    for (const auto& [name, img] : { std::pair<const char*, juce::Image> { "birds-over-scene.png", image },
                                     std::pair<const char*, juce::Image> { "birds-over-scene-detail.png", image.getClippedImage(birds).rescaled(birds.getWidth() * 3, birds.getHeight() * 3) } }) {
        auto file = dir.getChildFile(name);
        file.deleteFile();
        juce::FileOutputStream stream(file);
        if (stream.openedOk() && juce::PNGImageFormat().writeImageToStream(img, stream)) ++written;
    }
    return written == 2 ? 0 : 1;
}

void testWindow(Suite& s) {
    using namespace windowtests;
    s.section("window: birds over the scene, the window reopens as it was left, hover clears (Direction Update 2)");
    const bool paintedArt = dc::art::paintedFolder() != juce::File();
    // ---- Birds: partial repaints from the scene underneath, at every window size.
    for (float scale : { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f }) {
        Window w;
        w.editor->setScale(scale);
        w.go(dc::SkyTemple, 0);   // Cloud Steps: an open sky
        auto& view = w.view();
        const auto label = "birds at " + std::to_string(int(scale * 100.0f)) + " %";
        if (paintedArt) s.check(view.frameForTest().isValid(), label + ": the scene's frame is ready");
        view.startFlockForTest();
        view.stepBirdsForTest(9.0f);
        if (!s.check(view.birdCount() > 0 && !view.birdsAreaForTest().isEmpty(), label + ": a flock is in the sky")) continue;
        const auto bounds = w.editor->getLocalBounds();
        auto backing = w.editor->createComponentSnapshot(bounds, true, 1.0f);   // the window as shown
        // The birds fly on: the old and new regions are repainted (as the timer does).
        const auto before = w.editor->getLocalArea(&view, view.birdsAreaForTest());
        view.stepBirdsForTest(0.5f);
        const auto after = w.editor->getLocalArea(&view, view.birdsAreaForTest());
        const auto dirty = before.getUnion(after);
        paintRegion(*w.editor, backing, before);
        paintRegion(*w.editor, backing, after);
        const auto full = w.editor->createComponentSnapshot(bounds, true, 1.0f);   // a whole fresh frame
        view.clearBirdsForTest();
        const auto scene = w.editor->createComponentSnapshot(bounds, true, 1.0f);  // the scene without birds
        const double mismatch = changedShare(backing, full, dirty, 3);
        s.check(mismatch < 0.002, label + ": the repainted region matches a whole fresh frame", fmt(mismatch * 100.0, 2) + " % differ");
        const double birdsOnly = changedShare(backing, scene, dirty, 24);
        s.check(birdsOnly < 0.25, label + ": only the birds' strokes differ from the scene (no rectangle)", fmt(birdsOnly * 100.0) + " % of the region");
        // The art for a new pixel size not ready yet (the window was just resized, or the
        // host repaints before it is prepared): the region still shows the scene.
        view.forgetScenesForTest();
        view.startFlockForTest();
        view.stepBirdsForTest(9.0f);
        auto stale = scene.createCopy();
        const auto region = w.editor->getLocalArea(&view, view.birdsAreaForTest());
        paintRegion(*w.editor, stale, region);
        view.clearBirdsForTest();
        paintRegion(*w.editor, stale, region);   // the birds gone again: their region repainted
        const double drift = changedShare(stale, scene, region, 6);
        s.check(drift < 0.01, label + ": with the art not ready, a repaint shows the scene (never a fill)", fmt(drift * 100.0, 2) + " % differ");
        s.check(whiteShare(stale, region) <= whiteShare(scene, region) + 0.01, label + ": no white rectangle where the birds flew",
                fmt(whiteShare(stale, region) * 100.0) + " % white vs " + fmt(whiteShare(scene, region) * 100.0) + " % in the scene");
    }
    // A resized window: the frame is scaled at once, then the art at the new size replaces it.
    {
        Window w;
        w.go(dc::SkyTemple, 0);
        auto& view = w.view();
        const auto first = view.frameForTest();
        w.editor->setScale(1.5f);
        auto backing = w.editor->createComponentSnapshot(w.editor->getLocalBounds(), true, 1.0f);
        s.check(whiteShare(backing, backing.getBounds().withHeight(backing.getHeight() / 2)) < 0.5, "resized: the scaled previous frame is shown meanwhile (no blank)");
        view.settleForTest();
        s.check(view.frameForTest().isValid() && view.frameForTest().getWidth() > first.getWidth(), "resized: then the art at the new size",
                std::to_string(first.getWidth()) + " -> " + std::to_string(view.frameForTest().getWidth()));
    }

    // ---- Restore: close and reopen; and a saved session.
    {
        Window w(true);
        w.go(dc::GoldenCathedral, 2);
        auto& view = w.view();
        view.advancedButton().setToggleState(true, juce::dontSendNotification);
        view.advancedButton().onClick();
        if (auto* vp = view.drawerViewportForTest()) vp->setViewPosition(0, 120);
        const int scrolled = view.drawerViewportForTest()->getViewPositionY();
        w.editor->setScale(1.25f);
        int lowestKey = -1;
        if (auto* panel = w.editor->instrumentPanel()) {
            panel->keyboardComponent().setLowestVisibleKey(48);   // as far as it scrolls at this width
            lowestKey = panel->keyboardComponent().getLowestVisibleKey();
        }
        w.close();
        w.open();
        auto& again = w.view();
        again.settleForTest();
        s.check(!again.showingMap() && again.shownScene() == dc::art::SceneId { dc::GoldenCathedral, 2 },
                "restore: the reopened window shows the same place's scene (not the world map)");
        s.check(again.subLocationButton(2).getToggleState(), "restore: the same place is selected in the list");
        s.check(again.advancedOpen() && !again.journeyOpen(), "restore: the Advanced drawer is open again");
        s.check(again.drawerViewportForTest()->getViewPositionY() == scrolled, "restore: ... scrolled where it was",
                std::to_string(again.drawerViewportForTest()->getViewPositionY()) + " vs " + std::to_string(scrolled));
        s.check(std::abs(w.editor->getWidth() - juce::roundToInt(960 * 1.25f)) <= 1, "restore: the same window size (125 %)");
        if (auto* panel = w.editor->instrumentPanel())
            s.check(panel->keyboardComponent().getLowestVisibleKey() == lowestKey, "restore: the keyboard where it was scrolled to",
                    std::to_string(panel->keyboardComponent().getLowestVisibleKey()) + " vs " + std::to_string(lowestKey));
        // The Journey panel and the controls hidden; then the map.
        again.advancedButton().setToggleState(false, juce::dontSendNotification);
        again.advancedButton().onClick();
        again.journeyButton().setToggleState(true, juce::dontSendNotification);
        again.journeyButton().onClick();
        w.close();
        w.open();
        s.check(w.view().journeyOpen() && !w.view().advancedOpen(), "restore: the Journey panel open again (and only it)");
        w.view().closePanelsForTest();
        w.view().controlsButton().onClick();   // hide the controls
        w.close();
        w.open();
        s.check(!w.view().controlsShownForTest() && !w.view().journeyOpen(), "restore: the controls stay hidden, the panel closed");
        w.view().controlsButton().onClick();
        w.view().mapButton().onClick();
        w.view().settleForTest();
        w.close();
        w.open();
        w.view().settleForTest();
        s.check(w.view().showingMap(), "restore: left on the map, it reopens on the map");
        // A saved session (the project reopened): a new processor from the saved state.
        w.go(dc::FairyGlade, 1);
        w.view().journeyButton().setToggleState(true, juce::dontSendNotification);
        w.view().journeyButton().onClick();
        juce::MemoryBlock block;
        w.p->getStateInformation(block);
        w.close();
        Window other(true);
        other.close();
        other.p->setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        other.open();
        other.view().settleForTest();
        s.check(!other.view().showingMap() && other.view().shownScene() == dc::art::SceneId { dc::FairyGlade, 1 } && other.view().journeyOpen(),
                "restore: a saved session reopens on the same scene with the Journey panel open");
        // A user preset carries sounds, not the window.
        const auto keep = other.p->viewState();
        juce::File preset = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ls-view-test.lsinst");
        {
            juce::MemoryBlock data;
            Window third(true);
            third.go(dc::CrystalPalace, 0);
            third.p->getStateInformation(data);
            if (auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()))) xml->writeTo(preset);
        }
        other.p->loadUserPreset(preset);
        preset.deleteFile();
        const auto now = other.p->viewState();
        s.check(now.place == keep.place && now.journey == keep.journey && now.location == keep.location && now.sub == keep.sub,
                "restore: loading a preset keeps the window as it is");
        // An older session without the window's state opens on the map, as before.
        LightSeekerProcessor fresh(true);
        s.check(!fresh.viewState().valid, "restore: a new instance has no stored window (opens on the map)");
    }

    // ---- Hover: clears when the mouse leaves; the selected place stays marked.
    {
        Window w;
        w.go(dc::BlossomMeadow, 0);
        auto& view = w.view();
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto event = [&](juce::Component& c) {
            return juce::MouseEvent(src, c.getLocalBounds().getCentre().toFloat(), {}, juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                    &c, &c, juce::Time::getCurrentTime(), c.getLocalBounds().getCentre().toFloat(), juce::Time::getCurrentTime(), 1, false);
        };
        const auto enter = [&](juce::Component& c) { c.mouseEnter(event(c)); };
        const auto leave = [&](juce::Component& c) { c.mouseExit(event(c)); };
        const int count = dc::subLocationCount(dc::BlossomMeadow) + 1;
        // Click the second place, then pass the mouse over every other entry and away.
        auto& clicked = view.subLocationButton(1);
        enter(clicked);
        clicked.onClick();
        view.settleForTest();
        leave(clicked);
        int stuck = 0;
        for (int k = 0; k < count; ++k) {
            auto& b = view.subLocationButton(k);
            enter(b);
            if (!view.placeHoverShown(k)) ++stuck;   // must show while over it
            leave(b);
            if (view.placeHoverShown(k)) ++stuck;
        }
        s.check(stuck == 0, "hover: every place in the list highlights while hovered and clears when the mouse leaves (after a click)");
        int selected = 0;
        for (int k = 0; k < count; ++k) selected += view.subLocationButton(k).getToggleState() ? 1 : 0;
        s.check(selected == 1 && view.subLocationButton(1).getToggleState(), "hover: the selected place stays marked (only it)");
        // A missed exit (the entry hidden and shown again under the mouse, or the host not
        // sending the exit): the next mouse move anywhere clears it.
        for (int k = 0; k < count; ++k) enter(view.subLocationButton(k));
        view.subLocationButton(0).setState(juce::Button::buttonOver);
        const int cleared = view.sweepHoverForTest({ -5000.0f, -5000.0f });
        int left = 0;
        for (int k = 0; k < count; ++k) left += view.placeHoverShown(k) || view.subLocationButton(k).getState() == juce::Button::buttonOver ? 1 : 0;
        s.check(cleared > 0 && left == 0, "hover: a missed exit is cleared by the next mouse move elsewhere", std::to_string(left) + " still lit");
        // Hidden while hovered (a travel's transition): the hover is gone when it shows again.
        enter(view.hotspotButton(0));
        view.hotspotButton(0).setVisible(false);
        view.hotspotButton(0).setVisible(true);
        s.check(!view.hotspotHoverShown(0), "hover: a hotspot hidden while hovered shows no hover when it comes back");
        // Every button of the window and the instrument panel: a stale hover state cleared.
        Window inst(true);
        inst.go(dc::SkyTemple, 1);
        std::vector<juce::Button*> all;
        std::function<void(juce::Component&)> collect = [&](juce::Component& c) {
            for (auto* child : c.getChildren()) {
                if (auto* b = dynamic_cast<juce::Button*>(child); b != nullptr && b->isVisible() && c.isVisible()) all.push_back(b);
                collect(*child);
            }
        };
        collect(*inst.editor);
        for (auto* b : all) b->setState(juce::Button::buttonOver);
        inst.view().sweepHoverForTest({ -5000.0f, -5000.0f });
        if (auto* panel = inst.editor->instrumentPanel()) dc::ui::sweepHover(*panel, { -5000.0f, -5000.0f });
        int lit = 0;
        for (auto* b : all) lit += b->getState() == juce::Button::buttonOver ? 1 : 0;
        s.check(all.size() > 20 && lit == 0, "hover: every hoverable button (" + std::to_string(all.size()) + ", the map, place list, toolbar, panel) clears",
                std::to_string(lit) + " still lit");
        // On the map: the landmark's name plate follows the hover and goes on exit.
        inst.view().mapButton().onClick();
        inst.view().settleForTest();
        auto& mark = inst.view().locationButton(dc::RoyalGardens);
        enter(mark);
        const bool shownOver = inst.view().hoveredLandmark() == dc::RoyalGardens;
        leave(mark);
        s.check(shownOver && inst.view().hoveredLandmark() == -1 && !inst.view().landmarkHoverShown(dc::RoyalGardens),
                "hover: a landmark's name plate shows while hovered and goes when the mouse leaves");
    }
}
