// Search: the panel (see SearchPanel.h).
#include "SearchPanel.h"
#include "EngravedLookAndFeel.h"

namespace dc::ui {
namespace {
constexpr int margin = 14;          // room for the soft shadow around the plate
constexpr int headerHeight = 22, itemHeight = 40, recentHeight = 30, hintHeight = 44;
constexpr int footerHeight = 50;
using dc::search::Kind;

juce::Path magnifier(juce::Rectangle<float> r) {
    juce::Path p;
    const float d = r.getWidth() * 0.62f;
    const auto lens = juce::Rectangle<float>(d, d).withPosition(r.getX(), r.getY());
    p.addEllipse(lens);
    const auto c = lens.getCentre();
    const float k = d * 0.5f * 0.7071f;
    p.startNewSubPath(c.x + k, c.y + k);
    p.lineTo(r.getRight(), r.getBottom());
    return p;
}
} // namespace

// ---- The field -------------------------------------------------------------------------

bool SearchPanel::Field::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::upKey) { panel.moveSelection(-1); return true; }
    if (key == juce::KeyPress::downKey) { panel.moveSelection(1); return true; }
    if (key == juce::KeyPress::pageUpKey) { panel.moveSelection(-5); return true; }
    if (key == juce::KeyPress::pageDownKey) { panel.moveSelection(5); return true; }
    if (key == juce::KeyPress::returnKey) { panel.chooseSelected(); return true; }
    if (key == juce::KeyPress::escapeKey) { panel.close(); return true; }
    if (key == juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)) { selectAll(); return true; }
    return juce::TextEditor::keyPressed(key);
}

void SearchPanel::Chip::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto r = getLocalBounds().toFloat().reduced(0.5f, 1.5f);
    const bool on = getToggleState();
    g.setColour(on ? colours::blood.withAlpha(down ? 0.95f : 0.85f) : (over ? colours::charcoal.darker(0.03f) : colours::charcoal));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(on ? colours::blood.darker(0.25f) : colours::dust.withAlpha(over ? 0.95f : 0.7f));
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.0f);
    g.setColour(on ? colours::pearl : colours::ivory);
    g.setFont(labelFont(14.5f));
    g.drawText(getButtonText(), r, juce::Justification::centred);
}

// ---- The panel -------------------------------------------------------------------------

SearchPanel::SearchPanel(const dc::search::Index& i, juce::StringArray& r) : index(i), recent(r) {
    setWantsKeyboardFocus(false);
    field.setFont(labelFont(18.0f));
    field.setIndents(30, 5);
    field.setTextToShowWhenEmpty(index.instrumentEdition() ? "Search instruments, places, presets, sounds, moods..." : "Search places, journeys, presets, sounds, moods...",
                                 colours::ash.withAlpha(0.75f));
    field.setColour(juce::TextEditor::backgroundColourId, colours::pearl);
    field.setColour(juce::TextEditor::textColourId, colours::ivory);
    field.setColour(juce::TextEditor::outlineColourId, colours::dust.withAlpha(0.8f));
    field.setColour(juce::TextEditor::focusedOutlineColourId, colours::blood);
    field.setColour(juce::TextEditor::highlightColourId, colours::ember.withAlpha(0.45f));
    field.setColour(juce::TextEditor::highlightedTextColourId, colours::ivory);
    field.setColour(juce::CaretComponent::caretColourId, colours::ivory);
    field.setSelectAllWhenFocused(false);
    field.setEscapeAndReturnKeysConsumed(true);
    field.onTextChange = [this] { refresh(); };
    addAndMakeVisible(field);
    closeButton.setButtonText(juce::String::fromUTF8("\xe2\x9c\x95"));
    closeButton.setTooltip("Close (Esc)");
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible(closeButton);
    std::vector<juce::String> names;
    for (int g = index.instrumentEdition() ? 0 : 1; g < dc::search::groupCount; ++g) names.push_back(dc::search::groupNames[static_cast<std::size_t>(g)]);
    groupChips = static_cast<int>(names.size());
    for (const char* mood : dc::search::moodChips) names.push_back(juce::String(mood).substring(0, 1).toUpperCase() + juce::String(mood).substring(1));
    for (int k = 0; k < static_cast<int>(names.size()); ++k) {
        auto chip = std::make_unique<Chip>(names[static_cast<std::size_t>(k)]);
        chip->setTooltip(k < groupChips ? "Only " + names[static_cast<std::size_t>(k)].toLowerCase() : "Only what sounds " + names[static_cast<std::size_t>(k)].toLowerCase());
        chip->setWantsKeyboardFocus(false);
        chip->setMouseClickGrabsKeyboardFocus(false);
        chip->onClick = [this, k] {
            // One group at a time; moods combine.
            if (k < groupChips && chips[static_cast<std::size_t>(k)]->getToggleState())
                for (int j = 0; j < groupChips; ++j) if (j != k) chips[static_cast<std::size_t>(j)]->setToggleState(false, juce::dontSendNotification);
            refresh();
            field.grabKeyboardFocus();
        };
        addAndMakeVisible(*chip);
        chips.push_back(std::move(chip));
    }
    addAndMakeVisible(list);
    refresh();
}

SearchPanel::~SearchPanel() { stopPreview(); }

void SearchPanel::open() {
    setVisible(true);
    toFront(false);
    refresh();
    field.selectAll();
    if (isShowing()) field.grabKeyboardFocus();
}

void SearchPanel::close() {
    stopPreview();
    if (!isVisible()) return;
    setVisible(false);
    if (onClose) onClose();
}

int SearchPanel::preferredHeight(int maxHeight) const {
    return juce::jmin(maxHeight, 2 * margin + 84 + juce::jmax(3 * itemHeight, contentHeight) + footerHeight);
}

juce::Rectangle<int> SearchPanel::listArea() const {
    const auto plate = getLocalBounds().reduced(margin);
    return { plate.getX() + 6, plate.getY() + 82, plate.getWidth() - 12, juce::jmax(0, plate.getHeight() - 82 - footerHeight) };
}

juce::Rectangle<int> SearchPanel::footerArea() const {
    const auto plate = getLocalBounds().reduced(margin);
    return plate.withTop(plate.getBottom() - footerHeight).reduced(14, 6);
}

void SearchPanel::resized() {
    const auto plate = getLocalBounds().reduced(margin);
    field.setBounds(plate.getX() + 12, plate.getY() + 10, plate.getWidth() - 24 - 38, 32);
    closeButton.setBounds(plate.getRight() - 44, plate.getY() + 12, 32, 28);
    int x = plate.getX() + 12;
    for (int k = 0; k < static_cast<int>(chips.size()); ++k) {
        auto& chip = *chips[static_cast<std::size_t>(k)];
        const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(labelFont(14.5f), chip.getButtonText())) + 22;
        if (k == groupChips) x += 10;   // a little gap between the groups and the moods
        chip.setBounds(x, plate.getY() + 50, w, 24);
        x += w + 5;
    }
    list.setBounds(listArea());
    layoutRows();
}

bool SearchPanel::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    if (key == juce::KeyPress::upKey) { moveSelection(-1); return true; }
    if (key == juce::KeyPress::downKey) { moveSelection(1); return true; }
    if (key == juce::KeyPress::returnKey) { return chooseSelected(); }
    return false;
}

void SearchPanel::paint(juce::Graphics& g) {
    const auto plate = getLocalBounds().reduced(margin).toFloat();
    shadow.drawForRectangle(g, plate.toNearestInt());
    g.setGradientFill(juce::ColourGradient(colours::pearl, 0.0f, plate.getY(), colours::ink, 0.0f, plate.getBottom(), false));
    g.fillRoundedRectangle(plate, 9.0f);
    g.setColour(colours::blood.withAlpha(0.55f));
    g.drawRoundedRectangle(plate.reduced(0.5f), 9.0f, 1.2f);
    // A hairline over the list and over the description.
    g.setColour(colours::dust.withAlpha(0.45f));
    g.fillRect(plate.getX() + 12.0f, plate.getY() + 80.0f, plate.getWidth() - 24.0f, 1.0f);
    g.fillRect(plate.getX() + 12.0f, plate.getBottom() - static_cast<float>(footerHeight), plate.getWidth() - 24.0f, 1.0f);
    // The description of the hovered (else the selected) result.
    const auto footer = footerArea().toFloat();
    const auto text = descriptionShown();
    if (text.isNotEmpty()) {
        g.setColour(colours::ivory.withAlpha(0.92f));
        g.setFont(labelFont(14.5f));
        g.drawFittedText(text, footer.toNearestInt(), juce::Justification::centredLeft, 2, 0.92f);
    } else {
        g.setColour(colours::ash.withAlpha(0.85f));
        g.setFont(labelFont(14.0f));
        g.drawText(juce::String::fromUTF8("\xe2\x86\x91 \xe2\x86\x93  choose    \xc2\xb7    Enter  go there    \xc2\xb7    Esc  close"), footer, juce::Justification::centredLeft);
    }
}

void SearchPanel::paintOverChildren(juce::Graphics& g) {
    // The magnifier inside the field.
    const auto f = field.getBounds().toFloat();
    g.setColour(colours::blood.darker(0.15f));
    g.strokePath(magnifier(juce::Rectangle<float>(15.0f, 15.0f).withCentre({ f.getX() + 16.0f, f.getCentreY() })),
                 juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

bool SearchPanel::hitTest(int x, int y) { return getLocalBounds().reduced(margin).contains(x, y); }

// ---- Rows ------------------------------------------------------------------------------

dc::search::Filter SearchPanel::currentFilter() const {
    dc::search::Filter f;
    for (int k = 0; k < groupChips; ++k)
        if (chips[static_cast<std::size_t>(k)]->getToggleState()) f.group = k + (index.instrumentEdition() ? 0 : 1);
    for (int k = groupChips; k < static_cast<int>(chips.size()); ++k)
        if (chips[static_cast<std::size_t>(k)]->getToggleState()) f.moods.push_back(dc::search::moodChips[static_cast<std::size_t>(k - groupChips)]);
    return f;
}

void SearchPanel::refresh() {
    stopPreview();
    rebuildRows();
    scroll = 0;
    hovered = -1;
    selected = -1;
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) if (selectable(r)) { selected = r; break; }
    layoutRows();
    list.repaint();
    repaint();
}

void SearchPanel::rebuildRows() {
    rows.clear();
    const auto q = field.getText().trim();
    const auto filter = currentFilter();
    if (q.isEmpty() && filter.group < 0 && filter.moods.empty()) {
        // Recent: the last searches and results, newest first.
        bool any = false;
        for (const auto& item : recent) {
            if (item.startsWith("e:")) {
                const int e = index.find(item.substring(2).toStdString());
                if (e < 0) continue;
                if (!any) rows.push_back({ Row::Header, -1, 0, "Recent" });
                any = true;
                rows.push_back({ Row::Item, e, 0, {} });
            } else if (item.startsWith("q:")) {
                if (!any) rows.push_back({ Row::Header, -1, 0, "Recent" });
                any = true;
                rows.push_back({ Row::RecentQuery, -1, 0, item.substring(2) });
            }
        }
        if (!any)
            rows.push_back({ Row::Hint, -1, 0, index.instrumentEdition() ? "Type a name, a sound or a mood: harp, choir, pad, calm, echo, guitar..."
                                                                         : "Type a name, a sound or a mood: choir, bells, echo, calm, shimmer, journey..." });
        return;
    }
    const auto results = index.search(q.toStdString(), filter, filter.group >= 0 ? 1000 : 8);
    int lastGroup = -1;
    for (const auto& r : results.items) {
        const int g = static_cast<int>(dc::search::groupOf(index.entry(r.entry).kind));
        if (g != lastGroup) {
            const int total = results.groupTotals[static_cast<std::size_t>(g)];
            juce::String heading = dc::search::groupNames[static_cast<std::size_t>(g)];
            int shownInGroup = 0;
            for (const auto& other : results.items) if (static_cast<int>(dc::search::groupOf(index.entry(other.entry).kind)) == g) ++shownInGroup;
            heading << "  " << (shownInGroup < total ? juce::String(shownInGroup) + " of " + juce::String(total) : juce::String(total));
            rows.push_back({ Row::Header, -1, g, heading });
            lastGroup = g;
        }
        rows.push_back({ Row::Item, r.entry, g, {} });
    }
    if (results.items.empty())
        rows.push_back({ Row::Hint, -1, 0, q.isEmpty() ? juce::String("Nothing here with these filters.")
                                                       : juce::String::fromUTF8("Nothing found for \xe2\x80\x9c") + q + juce::String::fromUTF8("\xe2\x80\x9d. Try a sound or a mood.") });
}

void SearchPanel::layoutRows() {
    int y = 2;
    for (auto& r : rows) {
        r.h = r.type == Row::Header ? headerHeight : (r.type == Row::Item ? itemHeight : (r.type == Row::RecentQuery ? recentHeight : hintHeight));
        r.y = y;
        y += r.h;
    }
    contentHeight = y + 2;
    scroll = juce::jlimit(0, juce::jmax(0, contentHeight - list.getHeight()), scroll);
}

bool SearchPanel::selectable(int r) const {
    if (r < 0 || r >= static_cast<int>(rows.size())) return false;
    const auto t = rows[static_cast<std::size_t>(r)].type;
    return t == Row::Item || t == Row::RecentQuery;
}

void SearchPanel::select(int r) {
    if (!selectable(r) || r == selected) return;
    selected = r;
    ensureVisible(r);
    list.repaint();
    repaint(footerArea());
}

void SearchPanel::setHovered(int r) {
    if (!selectable(r)) r = -1;
    if (r == hovered) return;
    hovered = r;
    list.repaint();
    repaint(footerArea());
}

void SearchPanel::moveSelection(int delta) {
    if (rows.empty()) return;
    int r = selected;
    const int step = delta > 0 ? 1 : -1;
    for (int n = std::abs(delta); n > 0; --n) {
        int next = r + step;
        while (next >= 0 && next < static_cast<int>(rows.size()) && !selectable(next)) next += step;
        if (!selectable(next)) break;
        r = next;
    }
    if (r < 0) for (int k = 0; k < static_cast<int>(rows.size()); ++k) if (selectable(k)) { r = k; break; }
    hovered = -1;   // the keyboard leads: the description follows the selection
    select(r);
    list.repaint();
    repaint(footerArea());
}

void SearchPanel::ensureVisible(int r) {
    if (r < 0) return;
    const auto& row = rows[static_cast<std::size_t>(r)];
    // Keep the group heading in view above the first result of a group.
    int top = row.y;
    if (r > 0 && rows[static_cast<std::size_t>(r - 1)].type == Row::Header) top = rows[static_cast<std::size_t>(r - 1)].y;
    if (top < scroll) scroll = top;
    else if (row.y + row.h > scroll + list.getHeight()) scroll = row.y + row.h - list.getHeight();
    scroll = juce::jlimit(0, juce::jmax(0, contentHeight - list.getHeight()), scroll);
}

void SearchPanel::scrollBy(float pixels) {
    const int before = scroll;
    scroll = juce::jlimit(0, juce::jmax(0, contentHeight - list.getHeight()), scroll + juce::roundToInt(pixels));
    if (scroll != before) list.repaint();
}

int SearchPanel::rowAt(int y) const {
    y += scroll;
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        const auto& row = rows[static_cast<std::size_t>(r)];
        if (y >= row.y && y < row.y + row.h) return r;
    }
    return -1;
}

juce::Rectangle<int> SearchPanel::rowBounds(int entry) const {
    for (const auto& row : rows)
        if (row.type == Row::Item && row.entry == entry) return { 0, row.y - scroll, list.getWidth(), row.h };
    return {};
}

juce::Rectangle<int> SearchPanel::previewBounds(int entry) const {
    const auto r = rowBounds(entry);
    if (r.isEmpty() || index.entry(entry).kind != Kind::Instrument || onPreviewStart == nullptr) return {};
    return juce::Rectangle<int>(26, 26).withCentre({ r.getRight() - 24, r.getCentreY() });
}

bool SearchPanel::overPreview(int r, juce::Point<int> p) const {
    if (r < 0 || rows[static_cast<std::size_t>(r)].type != Row::Item) return false;
    return previewBounds(rows[static_cast<std::size_t>(r)].entry).expanded(3).contains(p);
}

std::vector<int> SearchPanel::shownEntries() const {
    std::vector<int> out;
    for (const auto& row : rows) if (row.type == Row::Item) out.push_back(row.entry);
    return out;
}

std::vector<juce::String> SearchPanel::shownRecentQueries() const {
    std::vector<juce::String> out;
    for (const auto& row : rows) if (row.type == Row::RecentQuery) out.push_back(row.text);
    return out;
}

int SearchPanel::selectedEntry() const {
    return selectable(selected) && rows[static_cast<std::size_t>(selected)].type == Row::Item ? rows[static_cast<std::size_t>(selected)].entry : -1;
}

juce::String SearchPanel::headerTexts() const {
    juce::StringArray out;
    for (const auto& row : rows) if (row.type == Row::Header) out.add(row.text);
    return out.joinIntoString("|");
}

juce::String SearchPanel::descriptionShown() const {
    const int r = hovered >= 0 ? hovered : selected;
    if (!selectable(r)) return {};
    const auto& row = rows[static_cast<std::size_t>(r)];
    if (row.type == Row::RecentQuery) return juce::String::fromUTF8("Search again for \xe2\x80\x9c") + row.text + juce::String::fromUTF8("\xe2\x80\x9d.");
    return juce::String::fromUTF8(index.entry(row.entry).description.c_str());
}

// ---- Choosing ----------------------------------------------------------------------------

void SearchPanel::remember(const juce::String& item) {
    recent.removeString(item);
    recent.insert(0, item);
    while (recent.size() > maxRecent) recent.remove(recent.size() - 1);
}

bool SearchPanel::chooseSelected() { return chooseRow(selected); }

bool SearchPanel::chooseRow(int r) {
    if (!selectable(r)) return false;
    const auto row = rows[static_cast<std::size_t>(r)];
    if (row.type == Row::RecentQuery) {
        setQuery(row.text);
        field.grabKeyboardFocus();
        return true;
    }
    stopPreview();
    const auto q = field.getText().trim();
    if (q.isNotEmpty()) remember("q:" + q);
    remember("e:" + juce::String(index.entry(row.entry).key));
    if (onChoose) onChoose(row.entry);
    return true;
}

void SearchPanel::setQuery(const juce::String& text) {
    field.setText(text, false);
    field.moveCaretToEnd();
    refresh();
}

void SearchPanel::stopPreview() {
    if (previewRow < 0) return;
    previewRow = -1;
    if (onPreviewStop) onPreviewStop();
    list.repaint();
}

void SearchPanel::listMouseMove(const juce::MouseEvent& e) { setHovered(rowAt(e.y)); }

void SearchPanel::listMouseDown(const juce::MouseEvent& e) {
    const int r = rowAt(e.y);
    if (!selectable(r)) return;
    if (overPreview(r, e.getPosition())) {
        const auto& entry = index.entry(rows[static_cast<std::size_t>(r)].entry);
        if ((previewAllowed == nullptr || previewAllowed()) && onPreviewStart != nullptr && onPreviewStart(entry.instrument)) {
            previewRow = r;
            list.repaint();
        }
        return;
    }
    select(r);
}

void SearchPanel::listMouseUp(const juce::MouseEvent& e) {
    if (previewRow >= 0) { stopPreview(); return; }   // the preview stops on release
    const int r = rowAt(e.y);
    if (r >= 0 && r == rowAt(e.getMouseDownY()) && !overPreview(r, e.getMouseDownPosition()) && e.mouseWasClicked()) chooseRow(r);
}

// ---- Painting ----------------------------------------------------------------------------

void SearchPanel::paintIcon(juce::Graphics& g, Kind kind, juce::Rectangle<float> r, bool recentQuery) const {
    const auto stroke = juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(colours::blood.darker(0.2f));
    juce::Path p;
    if (recentQuery) { g.strokePath(magnifier(r.reduced(2.0f)), stroke); return; }
    switch (kind) {
        case Kind::Instrument: {
            // A note: head, stem, flag.
            const auto head = juce::Rectangle<float>(r.getWidth() * 0.46f, r.getHeight() * 0.34f).withPosition(r.getX() + 1.0f, r.getBottom() - r.getHeight() * 0.36f);
            p.addEllipse(head);
            g.fillPath(p, juce::AffineTransform::rotation(-0.35f, head.getCentreX(), head.getCentreY()));
            juce::Path s;
            const float x = head.getRight() - 1.0f;
            s.startNewSubPath(x, head.getCentreY());
            s.lineTo(x, r.getY() + 1.0f);
            s.quadraticTo(x + r.getWidth() * 0.45f, r.getY() + r.getHeight() * 0.25f, x + r.getWidth() * 0.32f, r.getY() + r.getHeight() * 0.52f);
            g.strokePath(s, stroke);
            break;
        }
        case Kind::Place: {
            // A map pin: a ring on a point.
            const float w = r.getWidth() * 0.66f;
            const auto c = juce::Point<float>(r.getCentreX(), r.getY() + w * 0.5f);
            p.startNewSubPath(c.x - w * 0.42f, c.y + w * 0.28f);
            p.lineTo(r.getCentreX(), r.getBottom());
            p.lineTo(c.x + w * 0.42f, c.y + w * 0.28f);
            p.addEllipse(juce::Rectangle<float>(w, w).withCentre(c));
            g.strokePath(p, stroke);
            g.fillEllipse(juce::Rectangle<float>(w * 0.36f, w * 0.36f).withCentre(c));
            break;
        }
        case Kind::Location: {
            // A small tower with battlements.
            const float x0 = r.getX() + r.getWidth() * 0.15f, x1 = r.getRight() - r.getWidth() * 0.15f, y0 = r.getY() + r.getHeight() * 0.18f;
            const float m = (x1 - x0) / 5.0f;
            p.startNewSubPath(x0, r.getBottom());
            p.lineTo(x0, y0);
            for (int k = 0; k < 5; ++k) {
                const float y = (k % 2 == 0) ? y0 : y0 + m;
                p.lineTo(x0 + m * static_cast<float>(k), y);
                p.lineTo(x0 + m * static_cast<float>(k + 1), y);
            }
            p.lineTo(x1, y0);
            p.lineTo(x1, r.getBottom());
            p.closeSubPath();
            g.strokePath(p, stroke);
            g.fillRoundedRectangle(juce::Rectangle<float>(m * 1.2f, r.getHeight() * 0.3f).withCentre({ r.getCentreX(), r.getBottom() - r.getHeight() * 0.15f }), 1.5f);
            break;
        }
        case Kind::Preset: {
            // A four-pointed star of light.
            const auto c = r.getCentre();
            const float a = r.getWidth() * 0.5f, b = r.getWidth() * 0.13f;
            p.startNewSubPath(c.x, c.y - a);
            p.quadraticTo(c.x + b, c.y - b, c.x + a, c.y);
            p.quadraticTo(c.x + b, c.y + b, c.x, c.y + a);
            p.quadraticTo(c.x - b, c.y + b, c.x - a, c.y);
            p.quadraticTo(c.x - b, c.y - b, c.x, c.y - a);
            p.closeSubPath();
            g.fillPath(p);
            break;
        }
        case Kind::Journey: {
            // A winding path of dots to a small flag.
            juce::Path route;
            route.startNewSubPath(r.getX() + 1.0f, r.getBottom() - 2.0f);
            route.cubicTo(r.getX() + r.getWidth() * 0.9f, r.getBottom() - 1.0f, r.getX() + r.getWidth() * 0.05f, r.getY() + r.getHeight() * 0.35f, r.getRight() - 4.0f, r.getY() + 4.0f);
            juce::Path dashed;
            const float dashes[] { 2.2f, 2.4f };
            juce::PathStrokeType(1.5f).createDashedStroke(dashed, route, dashes, 2);
            g.fillPath(dashed);
            juce::Path flag;
            flag.addTriangle(r.getRight() - 4.0f, r.getY(), r.getRight() + 1.0f, r.getY() + 2.5f, r.getRight() - 4.0f, r.getY() + 5.0f);
            g.fillPath(flag);
            break;
        }
    }
}

void SearchPanel::paintList(juce::Graphics& g) {
    const int w = list.getWidth(), h = list.getHeight();
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        const auto& row = rows[static_cast<std::size_t>(r)];
        const int y = row.y - scroll;
        if (y + row.h < 0 || y > h) continue;
        const auto area = juce::Rectangle<int>(0, y, w, row.h);
        switch (row.type) {
            case Row::Header: {
                g.setColour(colours::blood.darker(0.35f));
                g.setFont(titleFont(11.5f));
                g.drawText(row.text.toUpperCase(), area.withTrimmedLeft(10).withTrimmedTop(4), juce::Justification::centredLeft);
                break;
            }
            case Row::Hint: {
                g.setColour(colours::ash);
                g.setFont(labelFont(15.0f));
                g.drawFittedText(row.text, area.reduced(12, 4), juce::Justification::centredLeft, 2);
                break;
            }
            case Row::RecentQuery:
            case Row::Item: {
                const bool isSelected = r == selected, isHovered = r == hovered;
                const auto plate = area.toFloat().reduced(2.0f, 1.5f);
                if (isSelected) {
                    g.setColour(colours::ember.withAlpha(0.30f));
                    g.fillRoundedRectangle(plate, 6.0f);
                    g.setColour(colours::blood);
                    g.fillRoundedRectangle(plate.withWidth(3.0f).reduced(0.0f, 4.0f), 1.5f);
                } else if (isHovered) {
                    g.setColour(colours::ember.withAlpha(0.14f));
                    g.fillRoundedRectangle(plate, 6.0f);
                }
                if (row.type == Row::RecentQuery) {
                    paintIcon(g, Kind::Place, juce::Rectangle<float>(16.0f, 16.0f).withCentre({ 22.0f, static_cast<float>(y) + row.h * 0.5f }), true);
                    g.setColour(colours::ivory);
                    g.setFont(labelFont(16.0f));
                    g.drawText(juce::String::fromUTF8("\xe2\x80\x9c") + row.text + juce::String::fromUTF8("\xe2\x80\x9d"), area.withTrimmedLeft(42).withTrimmedRight(10),
                               juce::Justification::centredLeft, true);
                    break;
                }
                const auto& e = index.entry(row.entry);
                paintIcon(g, e.kind, juce::Rectangle<float>(16.0f, 16.0f).withCentre({ 22.0f, static_cast<float>(y) + row.h * 0.5f }), false);
                const bool preview = e.kind == Kind::Instrument && onPreviewStart != nullptr;
                const int right = preview ? 48 : 10;
                g.setColour(colours::ivory);
                g.setFont(titleFont(13.5f));
                g.drawText(juce::String::fromUTF8(e.name.c_str()), 42, y + 4, w - 42 - right, 18, juce::Justification::centredLeft, true);
                juce::String sub = juce::String::fromUTF8(e.where.c_str());
                juce::StringArray tags;
                for (std::size_t t = 0; t < e.tags.size() && t < 4; ++t) tags.add(juce::String::fromUTF8(e.tags[t].c_str()));
                if (tags.size() > 0) sub << juce::String::fromUTF8("   \xc2\xb7   ") << tags.joinIntoString(", ");
                g.setColour(colours::ash);
                g.setFont(labelFont(13.5f));
                g.drawText(sub, 42, y + 21, w - 42 - right, 16, juce::Justification::centredLeft, true);
                if (preview) {
                    // The preview button: a small round plate with a play mark; gold while held.
                    const auto b = previewBounds(row.entry).toFloat();
                    const bool held = previewRow == r;
                    const bool allowed = previewAllowed == nullptr || previewAllowed();
                    g.setColour(held ? colours::blood : colours::pearl);
                    g.fillEllipse(b);
                    g.setColour(colours::blood.withAlpha(allowed ? 0.9f : 0.35f));
                    g.drawEllipse(b.reduced(0.5f), 1.0f);
                    juce::Path play;
                    const auto c = b.getCentre();
                    play.addTriangle(c.x - 3.5f, c.y - 5.0f, c.x - 3.5f, c.y + 5.0f, c.x + 5.5f, c.y);
                    g.setColour(held ? colours::pearl : colours::blood.darker(0.2f).withAlpha(allowed ? 1.0f : 0.35f));
                    g.fillPath(play);
                }
                break;
            }
        }
    }
    // A thin scroll indicator when the list is longer than the panel.
    if (contentHeight > h && h > 0) {
        const float frac = static_cast<float>(h) / static_cast<float>(contentHeight);
        const float top = static_cast<float>(scroll) / static_cast<float>(contentHeight) * static_cast<float>(h);
        g.setColour(colours::dust.withAlpha(0.6f));
        g.fillRoundedRectangle(static_cast<float>(w) - 4.0f, top, 3.0f, juce::jmax(16.0f, frac * static_cast<float>(h)), 1.5f);
    }
}

// ---- The magnifier button --------------------------------------------------------------

void SearchButton::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto r = getLocalBounds().toFloat().reduced(1.5f);
    const bool on = getToggleState();
    g.setColour(colours::ink.withAlpha(on || down ? 0.98f : (over ? 0.94f : 0.82f)));
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(colours::blood.withAlpha(on || over ? 0.95f : 0.6f));
    g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.1f);
    g.setColour(on ? colours::blood.darker(0.2f) : colours::ivory.withAlpha(0.9f));
    g.strokePath(magnifier(juce::Rectangle<float>(14.0f, 14.0f).withCentre(r.getCentre())),
                 juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace dc::ui
