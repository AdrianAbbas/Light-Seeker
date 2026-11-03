#pragma once
// Search (both plug-ins): one index of everything you can go to or load, built once per
// edition at first use and only read afterwards. Every location, place (sub-location),
// instrument (the instrument edition), preset and journey is an entry with its name, where
// it belongs, its sound and mood tags, the developer's recordings it uses, and a short
// description. Searching is instant (about 130 entries of ~30 tokens): case-insensitive,
// partial words (prefixes), small typos (one edit, two in long words; transpositions count
// as one), every query word must match something; results grouped by type.
//
// Pure data and functions (no UI, no allocation on the audio thread: never called there).
#include <array>
#include <string>
#include <vector>

namespace dc::search {

enum class Kind { Instrument, Place, Location, Preset, Journey };
// The three groups the panel shows (and its kind chips filter).
enum class Group { Instruments, Places, Presets };
inline constexpr int groupCount = 3;
inline constexpr std::array<const char*, groupCount> groupNames { "Instruments", "Places", "Presets" };
inline Group groupOf(Kind k) noexcept {
    return k == Kind::Instrument ? Group::Instruments : (k == Kind::Place || k == Kind::Location ? Group::Places : Group::Presets);
}

struct Entry {
    Kind kind = Kind::Place;
    std::string name;          // as shown
    std::string where;         // the location it belongs to ("Celestial Citadel · Gates of Dawn")
    std::string family;        // instruments: their family; presets: their category
    std::vector<std::string> tags;   // sound, mood and recordings ("choir", "calm", "guitar"...)
    std::string description;   // a sentence or two (hover / selection)
    std::string key;           // stable identity (Recent): "inst:3", "place:2:1", "loc:2", "preset:12"
    int location = -1, sub = 0;      // places and locations: where to travel
    int instrument = -1;             // instruments: which (0..30)
    int preset = -1;                 // presets and journeys: the factory index of this edition
    // Normalised words with the weight of the field they come from.
    struct Token { std::string text; float weight; };
    std::vector<Token> tokens;
    std::string normalisedName;
};

struct Filter {
    int group = -1;                    // -1: every group, else a Group
    std::vector<std::string> moods;    // every one of these tags (exact, lower case)
    bool accepts(const Entry&) const;
};

struct Result { int entry; float score; };
// Results in display order: grouped (the group with the best match first), best first
// within each group, at most `perGroup` of each.
struct Results {
    std::vector<Result> items;
    std::array<int, groupCount> groupTotals {};   // matches per group before the cap
};

class Index {
public:
    explicit Index(bool instrumentEdition);
    const std::vector<Entry>& entries() const noexcept { return list; }
    const Entry& entry(int k) const { return list[static_cast<std::size_t>(k)]; }
    bool instrumentEdition() const noexcept { return instrument; }
    // An empty query lists everything the filter accepts (in index order); with no filter
    // either, nothing.
    Results search(const std::string& query, const Filter& = {}, int perGroup = 8) const;
    int find(const std::string& key) const;   // -1 when unknown
private:
    void add(Entry);
    std::vector<Entry> list;
    bool instrument;
};

// The index of an edition: built on first use (message thread), then shared.
const Index& index(bool instrumentEdition);

// The building blocks (tests).
std::string normalise(const std::string&);   // lower case, letters and digits, single spaces
std::vector<std::string> words(const std::string&);
int editDistance(const std::string& a, const std::string& b, int limit);   // optimal string alignment, capped at limit + 1
// How well one query word matches one indexed word: 1 exact, < 1 prefix / inside / typo, 0 none.
float matchWord(const std::string& query, const std::string& word);
// The mood chips under the field.
inline constexpr std::array<const char*, 4> moodChips { "calm", "epic", "magical", "sacred" };
}
