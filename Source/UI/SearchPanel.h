#pragma once
// Search (both plug-ins): a light results panel over the scene, opened by the magnifier at
// the top of the window or Cmd+F. Results as you type (dc::search: instant, fuzzy), grouped
// (Instruments, Places, Presets) with an icon, where each belongs and its tags; arrow keys
// and Enter, or a click, choose; the hovered or selected result's description shows at the
// bottom. Filter chips under the field (the groups, a few moods); with nothing typed, the
// last eight searches and results ("Recent"). Instruments have a small preview button: a
// short phrase while it is held (never while the host plays).
//
// The panel is not modal: Esc, its close button or a click anywhere outside closes it, and
// that click still reaches the map. Cheap: the list is painted from a buffered image that
// is redrawn only when the results, the selection or the hover change.
#include <juce_gui_basics/juce_gui_basics.h>
#include "World/Search.h"
#include "Hover.h"

namespace dc::ui {

class SearchPanel final : public juce::Component {
public:
    static constexpr int panelWidth = 480;
    static constexpr int maxRecent = 8;
    SearchPanel(const dc::search::Index&, juce::StringArray& recent);
    ~SearchPanel() override;

    // Chosen (a result); the panel has already remembered it in Recent.
    std::function<void(int entry)> onChoose;
    std::function<void()> onClose;
    // The preview of an instrument: start (false: not possible now) and stop.
    std::function<bool(int instrument)> onPreviewStart;
    std::function<void()> onPreviewStop;
    std::function<bool()> previewAllowed;

    // Shown with the field focused and its text selected (typing replaces it).
    void open();
    void close();
    // The height the panel wants (its list at most `maxHeight` tall in all).
    int preferredHeight(int maxHeight) const;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool hitTest(int x, int y) override;   // the plate, not its shadow

    // Keyboard navigation (and tests): move through the selectable rows; choose the selected.
    void moveSelection(int delta);
    bool chooseSelected();
    // Tests: type into the field (as the user would), the rows shown, the selection.
    void setQuery(const juce::String&);
    juce::String query() const { return field.getText(); }
    std::vector<int> shownEntries() const;     // result rows in display order (entries)
    std::vector<juce::String> shownRecentQueries() const;
    int selectedEntry() const;                 // -1: none (or a recent search)
    int selectedRow() const noexcept { return selected; }
    juce::String descriptionShown() const;
    juce::String headerTexts() const;          // the group headings shown, "|"-separated
    juce::Button& chipButton(int k) { return *chips[static_cast<std::size_t>(k)]; }
    int chipCount() const noexcept { return static_cast<int>(chips.size()); }
    juce::TextEditor& fieldForTest() { return field; }
    juce::Button& closeButtonForTest() { return closeButton; }
    juce::Component& listForTest() { return list; }
    // Tests: the list's row for an entry (list coordinates), or empty.
    juce::Rectangle<int> rowBounds(int entry) const;
    juce::Rectangle<int> previewBounds(int entry) const;
    void hoverRowForTest(int row) { setHovered(row); }
    bool previewing() const noexcept { return previewRow >= 0; }

private:
    struct Row {
        enum Type { Header, Item, RecentQuery, Hint } type = Item;
        int entry = -1;
        int group = 0;
        juce::String text;
        int y = 0, h = 0;
    };
    // The field: the arrow keys, Enter and Esc go to the panel, everything else is typing.
    class Field final : public juce::TextEditor {
    public:
        explicit Field(SearchPanel& p) : panel(p) {}
        bool keyPressed(const juce::KeyPress&) override;
    private:
        SearchPanel& panel;
    };
    class Chip final : public juce::Button {
    public:
        Chip(const juce::String& text) : juce::Button(text) { setClickingTogglesState(true); }
        void paintButton(juce::Graphics&, bool over, bool down) override;
    };
    class List final : public juce::Component, public Hoverable {
    public:
        explicit List(SearchPanel& p) : panel(p) { setBufferedToImage(true); setOpaque(false); }
        // The hovered row goes when the mouse is not over the list (the hover guard).
        bool clearHoverUnless(juce::Point<float> screen) override {
            if (panel.hovered < 0 || detail::over(*this, screen)) return false;
            panel.setHovered(-1);
            return true;
        }
        bool hoverShown() const override { return panel.hovered >= 0; }
        void paint(juce::Graphics& g) override { panel.paintList(g); }
        void mouseMove(const juce::MouseEvent& e) override { panel.listMouseMove(e); }
        void mouseExit(const juce::MouseEvent&) override { panel.setHovered(-1); }
        void mouseDown(const juce::MouseEvent& e) override { panel.listMouseDown(e); }
        void mouseUp(const juce::MouseEvent& e) override { panel.listMouseUp(e); }
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { panel.scrollBy(-w.deltaY * 120.0f); }
    private:
        SearchPanel& panel;
    };

    void refresh();
    void rebuildRows();
    void layoutRows();
    bool selectable(int row) const;
    void select(int row);
    void setHovered(int row);
    void ensureVisible(int row);
    void scrollBy(float pixels);
    int rowAt(int y) const;
    bool overPreview(int row, juce::Point<int>) const;
    bool chooseRow(int row);
    void remember(const juce::String& item);
    void stopPreview();
    void paintList(juce::Graphics&);
    void paintIcon(juce::Graphics&, dc::search::Kind, juce::Rectangle<float>, bool recentQuery) const;
    void listMouseMove(const juce::MouseEvent&);
    void listMouseDown(const juce::MouseEvent&);
    void listMouseUp(const juce::MouseEvent&);
    dc::search::Filter currentFilter() const;
    juce::Rectangle<int> listArea() const;
    juce::Rectangle<int> footerArea() const;

    const dc::search::Index& index;
    juce::StringArray& recent;
    Field field { *this };
    juce::TextButton closeButton;
    std::vector<std::unique_ptr<Chip>> chips;   // the groups, then the moods
    int groupChips = 0;
    List list { *this };
    std::vector<Row> rows;
    int contentHeight = 0;
    int scroll = 0;
    int selected = -1, hovered = -1;
    int previewRow = -1;
    juce::DropShadow shadow { juce::Colours::black.withAlpha(0.28f), 18, { 0, 5 } };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SearchPanel)
};

// The magnifier at the top of the window (pearl plate, gold glass).
class SearchButton final : public juce::Button {
public:
    SearchButton() : juce::Button("Search") {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
};

} // namespace dc::ui
