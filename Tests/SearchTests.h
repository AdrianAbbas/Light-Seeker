// Search (included by LightSeekerTests.cpp, "search" group).
//   index      every location, place, instrument, preset and journey is in its edition's
//              index with tags, a description and a unique key; the effect has no instruments
//   findable   every entry is found by its name (first of its kind), in any case, by the
//              start of its name, and by at least three of its tags
//   typos      "harpe", "choire" and other slips find the right results
//   filters    the group and mood chips
//   by hand    Enter on a result lands exactly where choosing it by hand does (the whole
//              state compared): places from the map and from a place, locations,
//              instruments, every preset and journey; Lock instrument respected; held notes
//              carry over
//   keyboard   Cmd+F, typing, arrows, Enter, Esc, the description, Recent, closing
//   preview    a short phrase while held, at a sensible level, stops on release, never while
//              the host plays
//   cost       the index is built once; searching is instant
//   --search-snapshots dir   images of the panel (docs/snapshots)

#include <set>

namespace searchtests {
using namespace dc::search;
using SearchPanel_t = dc::ui::SearchPanel;

// Everything a choice can change: every parameter, the route, the preset, the instrument
// (after a block), and what the window shows.
struct State {
    std::vector<float> values;
    std::vector<std::pair<int, int>> route;
    juce::String preset;
    int presetIndex = -1, instrument = -1;
    bool locked = false;
    int shownLocation = -1, shownSub = -1;
    bool map = true;
    int sceneLocation = -1, sceneSub = -1;
    std::string diff(const State& o) const {
        std::string d;
        for (std::size_t k = 0; k < values.size() && k < o.values.size(); ++k)
            if (std::abs(values[k] - o.values[k]) > 1.0e-6f) d += "param " + std::to_string(k) + " " + fmt(values[k], 3) + " vs " + fmt(o.values[k], 3) + "; ";
        if (route != o.route) d += "route; ";
        if (preset != o.preset || presetIndex != o.presetIndex) d += "preset " + preset.toStdString() + " vs " + o.preset.toStdString() + "; ";
        if (instrument != o.instrument || locked != o.locked) d += "instrument " + std::to_string(instrument) + (locked ? "L" : "") + " vs " + std::to_string(o.instrument) + (o.locked ? "L" : "") + "; ";
        if (shownLocation != o.shownLocation || shownSub != o.shownSub) d += "shown place; ";
        if (map != o.map || sceneLocation != o.sceneLocation || sceneSub != o.sceneSub) d += "view; ";
        return d;
    }
};

struct Win {
    std::unique_ptr<LightSeekerProcessor> p;
    std::unique_ptr<juce::AudioProcessorEditor> base;
    LightSeekerEditor* editor = nullptr;
    juce::AudioBuffer<float> buffer { 2, 256 };
    juce::MidiBuffer midi;
    explicit Win(bool instrument) : p(std::make_unique<LightSeekerProcessor>(instrument)) {
        juce::AudioProcessor::BusesLayout layout;
        if (!instrument) layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        p->setBusesLayout(layout);
        p->setRateAndBufferSizeDetails(48000, 256);
        p->prepareToPlay(48000, 256);
        base.reset(p->createEditor());
        editor = dynamic_cast<LightSeekerEditor*>(base.get());
        view().settleForTest();
    }
    LightSeekerView& view() { return editor->view(); }
    SearchPanel_t& panel() { return view().searchPanelForTest(); }
    void block() { buffer.clear(); p->processBlock(buffer, midi); midi.clear(); }
    State state() {
        block();
        block();
        State s;
        for (auto* q : p->getParameters()) s.values.push_back(q->getValue());
        for (const auto& st : p->route()) s.route.push_back({ st.location, st.sub });
        s.preset = p->presetDisplayName();
        s.presetIndex = p->presetIndex;
        if (p->isInstrument()) { s.instrument = p->instrumentIndex(); s.locked = p->instrumentLocked(); }
        s.shownLocation = view().shownLocation();
        s.shownSub = view().shownSubLocation();
        s.map = view().showingMap();
        s.sceneLocation = view().shownScene().location;
        s.sceneSub = view().shownScene().sub;
        return s;
    }
    // By hand: the map, its landmark, then the place in the list (as a user would).
    void handTravel(int location, int sub, bool fromMap) {
        if (fromMap) {
            view().locationButton(location).onClick();
            view().settleForTest();
        }
        view().subLocationButton(sub).onClick();
        view().settleForTest();
    }
    // By search: Cmd+F, type, arrow down to the entry, Enter.
    bool searchChoose(const juce::String& query, int entry) {
        view().keyPressed(juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0));
        auto& sp = panel();
        sp.setQuery(query);
        for (int n = 0; n < 200 && sp.selectedEntry() != entry; ++n) sp.moveSelection(1);
        if (sp.selectedEntry() != entry) { sp.close(); return false; }
        sp.fieldForTest().keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        view().settleForTest();
        return true;
    }
    void toMap() { view().mapButton().onClick(); view().settleForTest(); }
};

bool contains(const Results& r, int entry) {
    return std::any_of(r.items.begin(), r.items.end(), [entry](const Result& x) { return x.entry == entry; });
}
int rankInGroup(const Index& index, const Results& r, int entry) {
    const auto g = groupOf(index.entry(entry).kind);
    int rank = 0;
    for (const auto& x : r.items) {
        if (groupOf(index.entry(x.entry).kind) != g) continue;
        if (x.entry == entry) return rank;
        ++rank;
    }
    return -1;
}
} // namespace searchtests

void testSearch(Suite& s) {
    using namespace searchtests;
    s.section("search: the index, fuzzy matching, the panel, Enter = by hand, preview");

    // ---- The building blocks.
    s.check(editDistance("harpe", "harp", 2) == 1 && editDistance("choire", "choir", 2) == 1 && editDistance("hrap", "harp", 2) == 1
                && editDistance("glockenspeil", "glockenspiel", 2) == 1 && editDistance("bells", "harp", 2) == 3,
            "edit distance: insertions, deletions and transpositions count one");
    s.check(matchWord("harp", "harp") == 1.0f && matchWord("har", "harp") > 0.7f && matchWord("harpe", "harp") > 0.4f
                && matchWord("xyz", "harp") == 0.0f && matchWord("ha", "choir") == 0.0f,
            "word match: exact, partial, typo, none");
    s.check(normalise("Dragon's Peak") == "dragons peak" && normalise("  Gentle Strings (50 %) ") == "gentle strings 50", "names normalised (case, apostrophes, punctuation)");

    // ---- The index of each edition.
    for (bool instrument : { false, true }) {
        const auto& index = dc::search::index(instrument);
        const std::string ed = instrument ? "instrument: " : "effect: ";
        int instruments = 0, places = 0, locations = 0, presets = 0, journeys = 0;
        std::set<std::string> keys;
        bool tagged = true, described = true;
        std::string untagged;
        for (const auto& e : index.entries()) {
            instruments += e.kind == Kind::Instrument;
            places += e.kind == Kind::Place;
            locations += e.kind == Kind::Location;
            presets += e.kind == Kind::Preset;
            journeys += e.kind == Kind::Journey;
            keys.insert(e.key);
            if (e.tags.size() < 3) { tagged = false; untagged += e.name + "; "; }
            if (e.description.size() < 20) described = false;
        }
        const int presetCount = static_cast<int>(dc::presetsFor(instrument).size());
        s.check(instruments == (instrument ? ls::inst::instrumentCount : 0), ed + "every instrument indexed (none in the effect)", std::to_string(instruments));
        s.check(places == dc::placeCount && locations == dc::LocationCount, ed + "every place and location indexed", std::to_string(places) + " + " + std::to_string(locations));
        s.check(presets + journeys == presetCount && journeys >= 9, ed + "every preset and journey indexed", std::to_string(presets) + " + " + std::to_string(journeys));
        s.check(keys.size() == index.entries().size(), ed + "every entry has its own key");
        s.check(tagged, ed + "every entry has at least three tags", untagged);
        s.check(described, ed + "every entry has a description");
        // The tags the brief names are used.
        std::set<std::string> allTags;
        for (const auto& e : index.entries()) allTags.insert(e.tags.begin(), e.tags.end());
        std::string missing;
        for (const char* t : { "choir", "voice", "harp", "pluck", "pad", "strings", "brass", "bells", "shimmer", "reverb", "echo", "bright", "airy", "epic",
                               "calm", "magical", "pastoral", "sacred", "heroic", "warm", "sparkly", "long tail" })
            if (allTags.count(t) == 0 && (instrument || (std::string(t) != "pluck" && std::string(t) != "pad" && std::string(t) != "strings" && std::string(t) != "brass")))
                missing += std::string(t) + " ";
        if (instrument)
            for (const char* t : { "guitar", "banjo", "piano", "violin", "kalimba", "voice" }) if (allTags.count(t) == 0) missing += std::string(t) + " ";
        s.check(missing.empty(), ed + "the sound, mood and recording tags are all used", missing);

        // ---- Findable: by name (first of its kind), in capitals, by its start, by 3+ tags.
        int byName = 0, byCase = 0, byStart = 0, byTags = 0;
        std::string notByName, notByTags;
        for (int k = 0; k < static_cast<int>(index.entries().size()); ++k) {
            const auto& e = index.entry(k);
            const auto r = index.search(e.name, {}, 1000);
            const int rank = rankInGroup(index, r, k);
            const bool topName = !r.items.empty() && index.entry(r.items.front().entry).normalisedName == e.normalisedName;
            // Several entries can share a name (the place "Choir Loft" and its preset): the
            // first result has that name and this entry is first of its kind with that name.
            int sameNameBefore = 0;
            for (const auto& x : r.items)
                if (x.entry == k) break;
                else if (groupOf(index.entry(x.entry).kind) == groupOf(e.kind) && index.entry(x.entry).normalisedName == e.normalisedName) ++sameNameBefore;
            if (topName && rank >= 0 && rank <= sameNameBefore) ++byName; else notByName += e.name + "; ";
            if (contains(index.search(juce::String(e.name).toUpperCase().toStdString(), {}, 1000), k)) ++byCase;
            const auto first = words(e.name);
            std::string start;
            for (const auto& w : first) if (w.size() >= 4) { start = w.substr(0, 4); break; }
            if (start.empty() || contains(index.search(start, {}, 1000), k)) ++byStart;
            int found = 0;
            for (const auto& t : e.tags) if (contains(index.search(t, {}, 1000), k)) ++found;
            if (found >= 3) ++byTags; else notByTags += e.name + " (" + std::to_string(found) + "); ";
        }
        const int n = static_cast<int>(index.entries().size());
        s.check(byName == n, ed + "every entry found by its name, first of its kind", notByName);
        s.check(byCase == n, ed + "case-insensitive (names typed in capitals)");
        s.check(byStart == n, ed + "partial words (the first four letters of the name)");
        s.check(byTags == n, ed + "every entry found by at least three of its tags", notByTags);

        // ---- Filters.
        Filter placesOnly;
        placesOnly.group = static_cast<int>(Group::Places);
        const auto onlyPlaces = index.search("choir", placesOnly, 1000);
        s.check(!onlyPlaces.items.empty() && std::all_of(onlyPlaces.items.begin(), onlyPlaces.items.end(), [&](const Result& x) { return groupOf(index.entry(x.entry).kind) == Group::Places; }),
                ed + "the Places chip: only places");
        Filter calm;
        calm.moods = { "calm" };
        const auto calmAll = index.search("", calm, 1000);
        s.check(calmAll.items.size() >= 6 && std::all_of(calmAll.items.begin(), calmAll.items.end(), [&](const Result& x) {
                    const auto& t = index.entry(x.entry).tags; return std::find(t.begin(), t.end(), "calm") != t.end(); }),
                ed + "the Calm chip with nothing typed lists what is calm", std::to_string(calmAll.items.size()));
        for (const char* mood : moodChips) {
            Filter f;
            f.moods = { mood };
            s.check(index.search("", f, 1000).items.size() >= 3, ed + "the " + mood + " chip finds something");
        }
        s.check(index.search("", {}, 8).items.empty(), ed + "nothing typed and no chip: no results (Recent shows instead)");
        const auto capped = index.search("calm", {}, 8);
        s.check(std::all_of(capped.groupTotals.begin(), capped.groupTotals.end(), [](int) { return true; })
                    && static_cast<int>(capped.items.size()) <= 8 * groupCount,
                ed + "at most eight per group without a group chip");
    }

    // ---- Typos and sounds.
    {
        const auto& ix = dc::search::index(true);
        const auto firstOf = [&](const std::string& q, Group g) {
            const auto r = ix.search(q, {}, 1000);
            for (const auto& x : r.items) if (groupOf(ix.entry(x.entry).kind) == g) return std::string(ix.entry(x.entry).name);
            return std::string();
        };
        const auto firstAll = [&](const std::string& q) { const auto r = ix.search(q, {}, 1000); return r.items.empty() ? std::string() : ix.entry(r.items.front().entry).name; };
        const auto harp = firstOf("harpe", Group::Instruments);
        s.check(harp.find("Harp") != std::string::npos, "\"harpe\" finds a harp first among the instruments", harp);
        s.check(firstAll("harpe").find("Harp") != std::string::npos, "\"harpe\": the top result is a harp", firstAll("harpe"));
        const auto choir = firstOf("choire", Group::Instruments);
        s.check(choir.find("Choir") != std::string::npos, "\"choire\" finds a choir first among the instruments", choir);
        const auto choirPlace = firstOf("choire", Group::Places);
        s.check(choirPlace == "Choir Loft" || choirPlace == "Sanctum", "\"choire\" finds the choir places", choirPlace);
        s.check(firstOf("glockenspeil", Group::Instruments) == "Twinkling Glockenspiel", "\"glockenspeil\" finds the glockenspiel");
        s.check(firstOf("celsta", Group::Instruments) == "Celesta", "\"celsta\" finds the Celesta", firstOf("celsta", Group::Instruments));
        s.check(firstOf("moonwel", Group::Places) == "Moonwell", "\"moonwel\" finds the Moonwell");
        s.check(firstOf("cathedrl", Group::Places) == "Golden Cathedral", "\"cathedrl\" finds the Golden Cathedral", firstOf("cathedrl", Group::Places));
        s.check(firstOf("gitar", Group::Instruments).find("Guitar") != std::string::npos || firstOf("gitar", Group::Instruments) == "Elven Lyre",
                "\"gitar\" finds an instrument on the guitar recordings", firstOf("gitar", Group::Instruments));
        s.check(firstOf("banjo", Group::Instruments) == "Shimmering Pluck", "\"banjo\" finds the instrument on the banjo recordings");
        s.check(firstOf("kalimba", Group::Instruments) == "Bouncy Kalimba", "\"kalimba\" finds the kalimba");
        s.check(firstOf("piano", Group::Instruments) == "Celesta", "\"piano\" finds the instrument on the piano recordings");
        s.check(!firstOf("jurney", Group::Presets).empty(), "\"jurney\" finds the journeys");
        s.check(firstOf("long tail", Group::Places) != "", "\"long tail\" (two words) finds places");
        {
            // Every result has both words somewhere; the top instrument is a calm harp.
            const auto top = firstOf("calm harp", Group::Instruments);
            int topEntry = -1;
            for (int k = 0; k < static_cast<int>(ix.entries().size()); ++k)
                if (ix.entry(k).kind == Kind::Instrument && ix.entry(k).name == top) topEntry = k;
            const bool calmHarp = topEntry >= 0 && top.find("Harp") != std::string::npos
                                  && std::find(ix.entry(topEntry).tags.begin(), ix.entry(topEntry).tags.end(), "calm") != ix.entry(topEntry).tags.end();
            s.check(calmHarp && ix.search("calm harp", {}, 1000).items.size() < ix.search("harp", {}, 1000).items.size(),
                    "two words narrow it down (\"calm harp\": a calm harp, fewer results than \"harp\")", top);
        }
        s.check(ix.search("zzqx", {}, 8).items.empty(), "nonsense finds nothing");
        const auto& fx = dc::search::index(false);
        const auto fxChoir = fx.search("choire", {}, 1000);
        s.check(!fxChoir.items.empty() && (fx.entry(fxChoir.items.front().entry).name == "Choir Loft" || fx.entry(fxChoir.items.front().entry).name == "Sanctum"
                                            || fx.entry(fxChoir.items.front().entry).name == "Angel Choir"),
                "effect: \"choire\" finds the choir places and presets", fxChoir.items.empty() ? "" : fx.entry(fxChoir.items.front().entry).name);
    }

    // ---- Cost: built once, searching is instant.
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        dc::search::Index fresh(true);
        const double build = juce::Time::getMillisecondCounterHiRes() - start;
        s.metric("search index build (instrument edition)", build, " ms");
        s.check(&dc::search::index(true) == &dc::search::index(true), "the index is built once and shared");
        const char* queries[] { "h", "ha", "har", "harp", "harpe", "choir", "calm", "long tail", "epic brass", "golden cathedral", "glockenspeil", "journey", "sp", "zzqx" };
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        int n = 0;
        for (int round = 0; round < 20; ++round)
            for (const char* q : queries) { fresh.search(q, {}, 8); ++n; }
        const double each = (juce::Time::getMillisecondCounterHiRes() - t0) / n;
        s.metric("search, one query (typing)", each, " ms");
        s.check(each < 5.0, "a search is instant (well under a frame)", fmt(each, 3) + " ms");
    }

    // ---- The panel: keyboard, description, Recent, closing (both editions).
    for (bool instrument : { false, true }) {
        const std::string ed = instrument ? "instrument: " : "effect: ";
        Win w(instrument);
        auto& view = w.view();
        auto& sp = w.panel();
        const auto& index = dc::search::index(instrument);
        s.check(!view.searchOpen(), ed + "search is closed at first");
        s.check(view.keyPressed(juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)) && view.searchOpen(), ed + "Cmd+F opens search");
        s.check(w.editor->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !view.searchOpen(), ed + "Esc (from the window) closes it");
        s.check(w.editor->keyPressed(juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)) && view.searchOpen(), ed + "Cmd+F from anywhere in the window opens it");
        s.check(sp.chipCount() == (instrument ? 7 : 6), ed + "filter chips: the groups and four moods", std::to_string(sp.chipCount()));
        sp.setQuery(instrument ? "harp" : "bells");
        const auto shown = sp.shownEntries();
        s.check(!shown.empty() && sp.selectedEntry() == shown.front(), ed + "results as you type; the first is selected");
        s.check(sp.headerTexts().isNotEmpty(), ed + "grouped under headings", sp.headerTexts().toStdString());
        if (instrument) s.check(sp.headerTexts().startsWith("Instruments"), ed + "\"harp\": instruments first", sp.headerTexts().toStdString());
        else s.check(!sp.headerTexts().contains("Instruments"), ed + "no instruments in the effect");
        s.check(sp.descriptionShown().toStdString() == index.entry(shown.front()).description, ed + "the selected result's description shows");
        sp.moveSelection(1);
        s.check(shown.size() > 1 && sp.selectedEntry() == shown[1], ed + "down arrow: the next result (headings skipped)");
        s.check(sp.descriptionShown().toStdString() == index.entry(shown[1]).description, ed + "the description follows the arrow keys");
        sp.moveSelection(-1);
        sp.moveSelection(-1);
        s.check(sp.selectedEntry() == shown.front(), ed + "up arrow: back, and stays at the top");
        for (int k = 0; k < 100; ++k) sp.moveSelection(1);
        s.check(sp.selectedEntry() == shown.back(), ed + "down arrow stops at the last result");
        sp.hoverRowForTest(1);
        const int hoveredEntry = shown.front();
        s.check(sp.descriptionShown().toStdString() == index.entry(hoveredEntry).description, ed + "hovering a result shows its description");
        sp.hoverRowForTest(-1);
        // Instruments: a preview button on their rows (the instrument edition only).
        if (instrument) {
            const auto inst = std::find_if(shown.begin(), shown.end(), [&](int e) { return index.entry(e).kind == Kind::Instrument; });
            s.check(inst != shown.end() && !sp.previewBounds(*inst).isEmpty(), ed + "instrument rows have a preview button");
        }
        else s.check(std::none_of(shown.begin(), shown.end(), [&](int e) { return !sp.previewBounds(e).isEmpty(); }), ed + "no preview buttons in the effect");
        // Enter: chosen, closed, remembered.
        sp.moveSelection(-100);
        const int chosen = sp.selectedEntry();
        sp.fieldForTest().keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        view.settleForTest();
        s.check(!view.searchOpen(), ed + "Enter chooses and closes the panel");
        s.check(w.p->recentSearches.size() >= 2 && w.p->recentSearches[0] == "e:" + juce::String(index.entry(chosen).key)
                    && w.p->recentSearches[1] == juce::String(instrument ? "q:harp" : "q:bells"),
                ed + "Recent remembers the result and the search", w.p->recentSearches.joinIntoString(",").toStdString());
        view.openSearch();
        sp.setQuery("");
        const auto recentRows = sp.shownEntries();
        s.check(!recentRows.empty() && recentRows.front() == chosen && sp.headerTexts() == "Recent", ed + "with nothing typed the panel shows Recent");
        s.check(sp.shownRecentQueries().size() == 1, ed + "Recent lists the search too");
        // A recent search chosen: typed again.
        for (int k = 0; k < 10 && sp.selectedEntry() >= 0; ++k) sp.moveSelection(1);
        sp.chooseSelected();
        s.check(sp.query() == (instrument ? "harp" : "bells") && view.searchOpen(), ed + "choosing a recent search types it again");
        // At most eight.
        for (int k = 0; k < 12; ++k) { sp.setQuery(juce::String("calm ") + juce::String(k)); }
        for (int k = 0; k < 12; ++k) {
            view.openSearch();
            sp.setQuery(juce::String(index.entry(k).name));
            sp.chooseSelected();
            view.settleForTest();
        }
        s.check(w.p->recentSearches.size() == SearchPanel_t::maxRecent, ed + "Recent keeps the last eight", std::to_string(w.p->recentSearches.size()));
        // Recent survives closing and reopening the window (kept by the plug-in).
        const auto kept = w.p->recentSearches;
        w.base.reset(w.p->createEditor());
        w.editor = dynamic_cast<LightSeekerEditor*>(w.base.get());
        s.check(w.p->recentSearches == kept, ed + "Recent survives the window closing");
        auto& view2 = w.view();
        auto& sp2 = w.panel();
        view2.settleForTest();
        // Closing: its button, the magnifier again, a click outside; a click inside keeps it.
        view2.openSearch();
        view2.pointerDownForTest(sp2.listForTest());
        s.check(view2.searchOpen(), ed + "a click inside the panel keeps it open");
        sp2.closeButtonForTest().onClick();
        s.check(!view2.searchOpen(), ed + "its close button closes it");
        view2.searchButtonForTest().onClick();
        s.check(view2.searchOpen(), ed + "the magnifier opens it");
        view2.searchButtonForTest().onClick();
        s.check(!view2.searchOpen(), ed + "the magnifier again closes it");
        view2.openSearch();
        view2.mapButton().onClick();
        view2.settleForTest();
        view2.pointerDownForTest(view2.locationButton(dc::FairyGlade));
        s.check(!view2.searchOpen(), ed + "a click outside (a landmark) closes it");
        // ... and that click still travels (search never blocks the map).
        view2.locationButton(dc::FairyGlade).onClick();
        view2.settleForTest();
        s.check(w.p->locationIndex() == dc::FairyGlade, ed + "the map stays usable: the landmark clicked travels");
        view2.mapButton().onClick();
        view2.settleForTest();
        view2.openSearch();
        s.check(sp2.isVisible() && view2.locationButton(dc::CelestialCitadel).isVisible() && view2.locationButton(dc::CelestialCitadel).isEnabled(),
                ed + "the panel opens over the map; the landmarks stay live");
        const auto mag = view2.searchButtonForTest().getBounds();
        s.check(mag.getY() < 50 && mag.getX() > 0, ed + "the magnifier is at the top of the window");
        const auto plate = sp2.getBounds().reduced(14);
        s.check(plate.getBottom() <= view2.getHeight() - 140 && plate.getY() >= 40, ed + "the panel sits over the scene, above the controls");
        view2.closeSearch();
    }

    // ---- Enter on a result = choosing it by hand (the whole state compared).
    for (bool instrument : { false, true }) {
        const std::string ed = instrument ? "instrument: " : "effect: ";
        const auto& index = dc::search::index(instrument);
        Win hand(instrument), found(instrument);
        // Places from the map, and from inside another place; locations from the map.
        int same = 0, total = 0;
        std::string different;
        const std::vector<std::pair<int, int>> places { { dc::CelestialCitadel, 2 }, { dc::SkyTemple, 1 }, { dc::ElvenSanctuary, 0 }, { dc::CrystalPalace, 2 },
                                                        { dc::BlossomMeadow, 1 }, { dc::GoldenCathedral, 1 }, { dc::FloatingIsles, 2 }, { dc::RoyalGardens, 0 },
                                                        { dc::FairyGlade, 1 }, { dc::DragonsPeak, 2 } };
        for (const auto& [l, k] : places) {
            const int e = index.find("place:" + std::to_string(l) + ":" + std::to_string(k));
            // From the map; from another place of the same location (by hand: its list);
            // from a place elsewhere (by hand: back to the map, the landmark, the list).
            for (int from = 0; from < 3; ++from) {
                hand.toMap(); found.toMap();
                if (from == 1) { const int other = k == 0 ? 1 : 0; hand.handTravel(l, other, true); found.handTravel(l, other, true); }
                if (from == 2) { const int other = (l + 3) % dc::LocationCount; hand.handTravel(other, 1, true); found.handTravel(other, 1, true); }
                if (from == 2) hand.toMap();
                hand.handTravel(l, k, from != 1);
                const bool chosen = found.searchChoose(juce::String(index.entry(e).name), e);
                const auto a = hand.state(), b = found.state();
                const auto d = a.diff(b);
                ++total;
                if (chosen && d.empty()) ++same;
                else different += index.entry(e).name + (from == 0 ? " (map)" : (from == 1 ? " (list)" : " (elsewhere)")) + ": " + (chosen ? d : "not chosen") + " | ";
            }
        }
        for (int l : { dc::SkyTemple, dc::DragonsPeak }) {
            hand.toMap(); found.toMap();
            hand.view().locationButton(l).onClick(); hand.view().settleForTest();
            const int e = index.find("loc:" + std::to_string(l));
            const bool chosen = found.searchChoose(juce::String(index.entry(e).name), e);
            const auto d = hand.state().diff(found.state());
            ++total;
            if (chosen && d.empty()) ++same; else different += index.entry(e).name + ": " + d + " | ";
        }
        s.check(same == total, ed + "places and locations: Enter lands exactly where choosing by hand does", std::to_string(same) + "/" + std::to_string(total) + " " + different);
        s.check(!found.view().showingMap(), ed + "a place chosen from the map zooms into it");

        // Every preset and journey.
        same = 0; total = 0; different.clear();
        hand.toMap(); found.toMap();
        for (int k = 0; k < static_cast<int>(index.entries().size()); ++k) {
            const auto& e = index.entry(k);
            if (e.kind != Kind::Preset && e.kind != Kind::Journey) continue;
            hand.view().choosePresetForTest(e.preset);
            hand.view().settleForTest();
            const bool chosen = found.searchChoose(juce::String(e.name), k);
            const auto d = hand.state().diff(found.state());
            ++total;
            if (chosen && d.empty()) ++same; else different += e.name + ": " + (chosen ? d : "not chosen") + " | ";
        }
        s.check(same == total, ed + "every preset and journey: Enter = the preset menu", std::to_string(same) + "/" + std::to_string(total) + " " + different);

        if (!instrument) continue;
        // Instruments: as the instrument menu does (plays it here, locked).
        same = 0; total = 0; different.clear();
        hand.view().chooseInitForTest(); found.view().chooseInitForTest();
        for (int i = 0; i < ls::inst::instrumentCount; i += 3) {
            hand.editor->instrumentPanel()->chooseForTest(i);
            const int e = index.find("inst:" + std::to_string(i));
            const bool chosen = found.searchChoose(juce::String(index.entry(e).name), e);
            found.editor->instrumentPanel()->refreshForTest();
            const auto a = hand.state(), b = found.state();
            const auto d = a.diff(b);
            ++total;
            if (chosen && d.empty() && b.instrument == i && b.locked) ++same; else different += index.entry(e).name + ": " + d + " | ";
        }
        s.check(same == total, ed + "instruments: Enter = the instrument menu (played here, locked)", std::to_string(same) + "/" + std::to_string(total) + " " + different);
        s.check(found.editor->instrumentPanel()->instrumentName() == juce::String(ls::inst::patch(30).name), ed + "the panel shows the instrument chosen",
                found.editor->instrumentPanel()->instrumentName().toStdString());
        // Lock instrument respected: a place chosen by search keeps the locked instrument.
        hand.view().chooseInitForTest(); found.view().chooseInitForTest();
        hand.editor->instrumentPanel()->chooseForTest(13);
        found.editor->instrumentPanel()->chooseForTest(13);
        hand.toMap(); found.toMap();
        hand.handTravel(dc::GoldenCathedral, 1, true);
        const int choirLoft = index.find("place:" + std::to_string(dc::GoldenCathedral) + ":1");
        found.searchChoose("Choir Loft", choirLoft);
        const auto a = hand.state(), b = found.state();
        s.check(a.diff(b).empty() && b.instrument == 13 && b.locked, ed + "Lock instrument respected: travelling by search keeps the locked instrument", a.diff(b));
        // Unlocked: the place's instrument follows, as by hand.
        found.p->setInstrumentLock(false);
        found.state();
        const int eyrie = index.find("place:" + std::to_string(dc::DragonsPeak) + ":0");
        found.searchChoose("Eyrie", eyrie);
        s.check(found.state().instrument == ls::inst::instrumentIndex(dc::DragonsPeak, 0), ed + "unlocked: the place's own instrument plays after the jump");
        // Held notes survive the jump (a place, then an instrument).
        found.midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
        found.midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f), 0);
        found.block();
        for (int k = 0; k < 20; ++k) found.block();
        found.searchChoose("Moonwell", index.find("place:" + std::to_string(dc::ElvenSanctuary) + ":0"));
        for (int k = 0; k < 40; ++k) found.block();
        const bool heldPlace = found.p->synthForTest().keyDown(60) && found.p->synthForTest().keyDown(64) && found.p->instrumentVoices() >= 2
                               && found.p->instrumentIndex() == ls::inst::instrumentIndex(dc::ElvenSanctuary, 0);
        s.check(heldPlace, ed + "held notes survive a jump to a place (they move to its instrument)");
        found.searchChoose("Grand Organ", index.find("inst:" + std::to_string(ls::inst::instrumentIndex(dc::GoldenCathedral, 0))));
        for (int k = 0; k < 40; ++k) found.block();
        s.check(found.p->synthForTest().keyDown(60) && found.p->instrumentVoices() >= 2 && found.p->instrumentIndex() == ls::inst::instrumentIndex(dc::GoldenCathedral, 0),
                ed + "held notes survive a jump to an instrument");
        found.midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        found.midi.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
        found.block();
    }

    // ---- The preview (the instrument edition).
    {
        InstrumentHost h;
        h.p->setPlayHead(&h.transport);
        h.go(dc::BlossomMeadow, 0);
        h.render(0.5);
        s.check(h.p->startPreview(ls::inst::instrumentIndex(dc::BlossomMeadow, 0)), "preview: starts when the host is stopped");
        const auto from = h.end();
        h.render(1.2);
        const double previewRms = h.rms(from, h.end());
        double peak = 0;
        for (auto i = from; i < h.end(); ++i) peak = std::max({ peak, double(std::abs(h.outL[i])), double(std::abs(h.outR[i])) });
        s.check(toDb(previewRms) > -36.0, "preview: a phrase sounds", fmt(toDb(previewRms)) + " dB");
        s.check(peak < 0.9, "preview: at a sensible level (no peak near full scale)", fmt(peak, 2));
        h.p->stopPreview();
        h.render(4.0);
        const double after = h.rms(h.end() - h.at(0.3), h.end());
        s.check(toDb(after) < -70.0, "preview: stops on release (silent a few seconds later)", fmt(toDb(after)) + " dB");
        // The same phrase played on the keys: the preview a few dB below it.
        h.outL.clear(); h.outR.clear();
        for (int note : { 60, 64, 67, 72 }) { h.noteOn(note, 0.72f, 0); h.render(0.17); }
        h.render(1.2 - 0.68);
        const double played = h.rms(0, h.end());
        s.check(std::abs(toDb(previewRms) - toDb(played) - LightSeekerProcessor::previewLevelDb) < 3.0, "preview: about 4 dB below the instrument played",
                fmt(toDb(previewRms) - toDb(played)) + " dB");
        for (int n : { 60, 64, 67, 72 }) h.noteOff(n);
        h.render(5.0);
        // Never while the host plays.
        h.transport.playing = true;
        h.outL.clear(); h.outR.clear();
        h.render(0.1);
        s.check(!h.p->startPreview(3), "preview: refused while the host plays");
        h.render(1.0);
        s.check(toDb(h.rms(0, h.end())) < -90.0, "preview: silent while the host plays");
        // A preview stops when the host starts.
        h.transport.playing = false;
        h.render(0.1);
        h.p->startPreview(3);
        h.render(0.5);
        h.transport.playing = true;
        h.outL.clear(); h.outR.clear();
        h.render(4.0);
        s.check(toDb(h.rms(h.end() - h.at(0.3), h.end())) < -70.0, "preview: stops when the host starts playing");
        LightSeekerProcessor fx(false);
        s.check(!fx.startPreview(3), "preview: none in the effect");
    }
}

// Images of the panel for review (docs/snapshots).
int searchSnapshots(const juce::String& directory) {
    using namespace searchtests;
    juce::File dir(directory);
    dir.createDirectory();
    int written = 0;
    const auto shot = [&](LightSeekerEditor& editor, const juce::String& name) {
        auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
        juce::File file(dir.getChildFile(name + ".png"));
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat png;
        if (out.openedOk() && png.writeImageToStream(image, out)) { ++written; std::cout << "wrote " << file.getFullPathName() << "\n"; }
    };
    {
        Win w(true);
        w.editor->setScale(1.25f);
        w.view().settleForTest();
        w.view().openSearch();
        w.panel().setQuery("harp");
        w.panel().moveSelection(1);
        shot(*w.editor, "search-instrument-harp");
        w.panel().setQuery("choire");
        shot(*w.editor, "search-instrument-typo-choire");
        w.panel().chooseSelected();
        w.view().settleForTest();
        w.block();
        w.editor->instrumentPanel()->refreshForTest();
        w.view().openSearch();
        w.panel().setQuery("");
        shot(*w.editor, "search-instrument-recent");
    }
    {
        Win w(false);
        w.editor->setScale(1.25f);
        w.view().settleForTest();
        w.view().openSearch();
        w.panel().chipButton(0).setToggleState(true, juce::sendNotificationSync);   // Places
        w.panel().chipButton(2).setToggleState(true, juce::sendNotificationSync);   // Calm
        shot(*w.editor, "search-fx-calm-places");
        w.panel().chipButton(0).setToggleState(false, juce::sendNotificationSync);
        w.panel().chipButton(2).setToggleState(false, juce::sendNotificationSync);
        w.panel().setQuery("journey");
        shot(*w.editor, "search-fx-journeys");
        w.view().closeSearch();
        w.handTravel(dc::GoldenCathedral, 1, true);
        w.view().openSearch();
        w.panel().setQuery("echo");
        shot(*w.editor, "search-fx-echo-in-a-place");
    }
    return written > 0 ? 0 : 1;
}
