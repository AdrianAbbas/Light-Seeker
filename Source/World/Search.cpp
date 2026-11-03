// Search: the index and the matching (see Search.h).
#include "Search.h"
#include "Locations.h"
#include "Presets.h"
#include "Synth/Synth.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <set>

namespace dc::search {
namespace {

using Tags = std::vector<std::string>;

// ---- The data --------------------------------------------------------------------------

struct LocationInfo { Tags tags; const char* description; };
const std::array<LocationInfo, LocationCount>& locationInfo() {
    static const std::array<LocationInfo, LocationCount> info {{
        { { "majestic", "regal", "bright", "heroic", "golden", "castle" }, "A citadel of white stone and gold above the clouds: fanfares, mirrors, a throne and a spire." },
        { { "sacred", "airy", "heavenly", "ethereal", "calm", "temple" }, "A temple among the clouds: rising steps, a sanctum of voices, a garden of bells." },
        { { "magical", "forest", "calm", "night", "mysterious", "elven" }, "An elven refuge under silver trees: a moonlit well, wooden halls, a starlit canopy." },
        { { "crystal", "glassy", "bright", "shimmer", "sparkly", "clear" }, "A palace of crystal: prisms, a still mirror lake, a ringing diamond vault." },
        { { "pastoral", "warm", "calm", "sunny", "natural", "flowers" }, "Sunlit fields: open air, butterflies, long gentle echoes over the valley." },
        { { "sacred", "reverb", "choir", "cathedral", "epic", "golden" }, "A cathedral of gold and light: the longest reverb, an angelic choir, stained glass." },
        { { "airy", "dreamy", "wide", "sky", "clouds", "movement" }, "Islands drifting in the sky: swirling bridges of cloud, wind, soaring delays." },
        { { "elegant", "romantic", "courtly", "garden", "calm", "classical" }, "Royal gardens: a lush rose garden, a sparkling fountain, a marble colonnade." },
        { { "magical", "playful", "sparkly", "whimsical", "fairy", "small" }, "A fairy glade: a bouncy mushroom ring, twinkling fireflies, a tiny harp court." },
        { { "epic", "heroic", "vast", "mountain", "cinematic", "huge" }, "A high peak above the world: echoes off the cliffs, a golden nest, the summit's winds." },
    }};
    return info;
}

struct PlaceInfo { Tags tags; const char* description; };
// In place order (Celestial Citadel's four, then three per location).
const std::array<PlaceInfo, placeCount>& placeInfo() {
    static const std::array<PlaceInfo, placeCount> info {{
        // Celestial Citadel
        { { "bright", "fanfare", "reflections", "heroic", "majestic", "epic", "regal", "room" }, "Bright, fanfare-like early reflections at the citadel's gates: a heroic, majestic entrance." },
        { { "reflections", "mirror", "stereo", "wide", "echo", "bright", "magical", "shimmer" }, "Reflections that multiply across the stereo field, as in a hall of mirrors." },
        { { "regal", "chorus", "shimmer", "golden", "rich", "majestic", "sacred", "reverb", "epic" }, "A regal, rich chorus with a golden shimmer halo: the throne room's warm grandeur." },
        { { "airy", "open", "echo", "long tail", "vast", "epic", "wind", "high" }, "Huge open air at the top of the spire, with long airy echoes." },
        // Sky Temple
        { { "shimmer", "rising", "spiral", "magical", "ethereal", "dreamy", "airy", "sparkly" }, "A shimmer that spirals upwards in pitch, step after step into the clouds." },
        { { "choir", "voice", "sacred", "calm", "soft", "bloom", "angelic", "reverb" }, "A soft choir blooms out of the sound: the temple's quiet, sacred heart." },
        { { "bells", "chimes", "resonant", "sparkly", "bright", "magical", "calm", "garden" }, "Tuned wind chimes and bells ring in sympathy with what you play." },
        // Elven Sanctuary
        { { "glassy", "water", "shimmer", "calm", "magical", "night", "mysterious", "reverb" }, "A glassy shimmer like moonlight on the surface of a well." },
        { { "warm", "wooden", "chorus", "forest", "calm", "pastoral", "natural", "room" }, "Warm wooden resonance with a gentle ensemble chorus: elven halls of living wood." },
        { { "sparkly", "stars", "harmonics", "night", "magical", "airy", "bright", "long tail" }, "Sparkling high harmonics under a canopy of stars, with a long tail." },
        // Crystal Palace
        { { "shimmer", "octave", "prism", "stereo", "wide", "crystal", "bright", "magical" }, "An octave shimmer split across the stereo field like light through a prism." },
        { { "calm", "reflective", "long tail", "water", "lake", "reverb", "still", "ambient" }, "A calm, reflective space with a long tail, still as a mirror lake." },
        { { "crystal", "ringing", "bright", "resonant", "glassy", "sparkly", "metallic" }, "Bright modal ringing, as if every note struck a diamond." },
        // Blossom Meadow
        { { "warm", "open", "airy", "pastoral", "sunny", "saturation", "calm", "natural" }, "Soft open air and a warm saturation: a sunlit summer meadow." },
        { { "tremolo", "auto pan", "flutter", "playful", "movement", "pastoral", "light" }, "A light fluttering tremolo and auto-pan, like butterfly wings." },
        { { "echo", "delay", "gentle", "long tail", "pastoral", "calm", "open", "valley" }, "Long, gentle echoes rolling over the valley from the hill." },
        // Golden Cathedral
        { { "reverb", "long tail", "huge", "cathedral", "sacred", "radiant", "epic", "hall" }, "The longest, most radiant reverb in the plug-in: a nave filled with light." },
        { { "choir", "voice", "angelic", "sacred", "heavenly", "reverb", "epic" }, "An angelic choir rises from the loft around every note." },
        { { "shimmer", "colourful", "sacred", "chapel", "magical", "modulation", "reverb" }, "A shimmer whose colour shifts slowly, like light through stained glass." },
        // Floating Isles
        { { "swirl", "stereo", "movement", "wide", "dreamy", "airy", "clouds" }, "Swirling stereo motion on a bridge of clouds." },
        { { "airy", "wind", "breathy", "open", "calm", "ethereal", "breath" }, "An airy, wind-filtered space on a lonely isle." },
        { { "delay", "echo", "rising", "soaring", "pitch", "heroic", "epic", "sky" }, "Soaring delays that rise in pitch as they fly out from the harbour." },
        // Royal Gardens
        { { "lush", "chorus", "soft", "romantic", "warm", "calm", "garden" }, "A soft, lush chorus among the roses." },
        { { "water", "sparkly", "modulation", "fountain", "playful", "bright" }, "Sparkling, water-like modulation around the fountain." },
        { { "slapback", "echo", "rhythmic", "stone", "room", "classical", "regal" }, "Rhythmic slap echoes bouncing between marble columns." },
        // Fairy Glade
        { { "bouncy", "playful", "pitch", "sparkly", "whimsical", "magical", "fairy" }, "A bouncy, pitched-up sparkle inside the mushroom ring." },
        { { "glitter", "granular", "twinkling", "sparkly", "magical", "night", "whimsical" }, "Twinkling granular glitter, like fireflies in a hollow." },
        { { "small room", "harp", "resonance", "bright", "fairy", "magical", "intimate" }, "A tiny bright room with a harp-like resonance: the fairies' court." },
        // Dragon's Peak
        { { "echo", "vast", "open", "cliffs", "epic", "heroic", "mountain" }, "Vast open echoes thrown back by the cliffs." },
        { { "warm", "golden", "harmonics", "rich", "saturation", "cosy" }, "Warm, golden harmonic richness in the nest." },
        { { "heroic", "epic", "huge", "wide", "reverb", "octave", "soaring", "shimmer" }, "A huge, heroic, wide reverb with a soaring octave above it." },
    }};
    return info;
}

struct InstrumentInfo { Tags tags; Tags recordings; const char* description; };
// In instrument order (= place order). `recordings`: the developer's recordings it plays
// (without them, its stand-in plays).
const std::array<InstrumentInfo, ls::inst::instrumentCount>& instrumentInfo() {
    static const std::array<InstrumentInfo, ls::inst::instrumentCount> info {{
        { { "brass", "trumpet", "fanfare", "bright", "heroic", "regal", "epic", "majestic" }, {}, "Two bright trumpets a few cents apart, a piccolo an octave up (Character)." },
        { { "strings", "violin", "viola", "ensemble", "shimmer", "bright", "lush", "magical" }, {}, "Violin and viola sections with a shimmer octave above (Character) and a chorus." },
        { { "pad", "strings", "horns", "chords", "warm", "regal", "epic", "lush", "choir" }, { "voice" }, "Slow violas, soft horns, a cello an octave below and voices (Character)." },
        { { "brass", "horn", "solo", "heroic", "soaring", "epic", "warm" }, {}, "A dark solo horn with a late vibrato, a second horn (Character)." },
        { { "harp", "glissando", "pluck", "rising", "magical", "sparkly", "arpeggio" }, {}, "A harp whose every note climbs through a run built from the notes you hold (Character: how far)." },
        { { "choir", "angelic", "sacred", "calm", "soft", "heavenly", "pad", "ooh" }, { "voice" }, "A soft \"ooh\" choir with a descant above (Character); Vowel and Ensemble shape the voices." },
        { { "bells", "chimes", "metallic", "sparkly", "bright", "magical", "calm" }, {}, "Tuned bells with a major tierce, wind-chime tubes above (Character)." },
        { { "glass", "glassy", "bowed", "pad", "calm", "magical", "ethereal", "crystal" }, {}, "Rubbed wine glasses, a soft tap, an overtone glass (Character)." },
        { { "lyre", "pluck", "strings", "bright", "magical", "elven", "shimmer" }, { "guitar" }, "The guitar recordings made bright and shimmering, an octave plucked above (Character)." },
        { { "flute", "woodwind", "airy", "breathy", "soft", "calm", "pastoral", "magical" }, {}, "A clean flute with a piccolo (Character)." },
        { { "bells", "crystal", "shimmer", "bright", "sparkly", "glassy", "magical" }, {}, "Bright crystal modes in two octaves with a rising shimmer (Character)." },
        { { "pad", "glass", "glassy", "calm", "ethereal", "airy", "long tail", "crystal" }, {}, "Rubbed bowls, almost pure, a sine body, an overtone (Character), a wide chorus." },
        { { "celesta", "bells", "keys", "mallet", "sparkly", "bright", "magical" }, { "piano" }, "Steel bars over resonators with the piano recordings under them, a tine's sparkle (Character)." },
        { { "guitar", "acoustic", "pluck", "strings", "warm", "pastoral", "calm", "natural" }, { "guitar" }, "The guitar recordings, warm, with octave strings (Character)." },
        { { "kalimba", "pluck", "tine", "bouncy", "playful", "warm", "pastoral" }, { "kalimba" }, "The kalimba recordings with a chime sparkle (Character)." },
        { { "oboe", "woodwind", "reed", "pastoral", "calm", "warm", "recorder" }, {}, "A reedy, nasal oboe with a recorder an octave up (Character)." },
        { { "organ", "keys", "sacred", "epic", "majestic", "church", "huge" }, {}, "Seven ranks from 16' to the mixture, with chiff, and a trumpet stop (Character)." },
        { { "choir", "aah", "sacred", "radiant", "epic", "bright", "heavenly" }, { "voice" }, "A bright \"aah\" choir, basses below, sopranos above (Character); Vowel and Ensemble shape the voices." },
        { { "celesta", "harp", "pluck", "bells", "sacred", "sparkly", "magical", "golden" }, {}, "Celesta bars with a concert harp (Character)." },
        { { "pad", "synth", "analog", "airy", "dreamy", "warm", "wide", "ambient" }, { "synth" }, "The analog synth's soft pad recordings, a hazy octave (Character), a wide chorus." },
        { { "flute", "woodwind", "breathy", "airy", "wind", "calm", "ethereal" }, {}, "A breathy, airy flute with a wind octave (Character)." },
        { { "pluck", "synth", "shimmer", "bright", "sparkly", "rising" }, { "banjo", "synth" }, "The banjo recordings, high-passed, with a synth body, chorus and a rising shimmer (Character)." },
        { { "harp", "pluck", "strings", "classical", "elegant", "calm", "romantic", "warm" }, {}, "A gut-strung concert harp, its octave harmonics (Character)." },
        { { "music box", "tines", "bells", "sparkly", "nostalgic", "magical", "playful", "calm" }, {}, "High music-box tines with celesta bars (Character)." },
        { { "harpsichord", "keys", "pluck", "baroque", "courtly", "classical", "bright", "regal" }, {}, "Two quill-plucked 8' choirs with the damper, a 4' (Character)." },
        { { "pizzicato", "strings", "pluck", "bouncy", "playful", "col legno", "whimsical" }, { "violin" }, "The violin pizzicato recordings with their col legno tick, a plucked octave (Character)." },
        { { "glockenspiel", "bells", "mallet", "twinkling", "sparkly", "bright", "magical", "playful" }, {}, "Steel bars an octave up with a shimmer (Character)." },
        { { "harp", "pluck", "fairy", "sparkly", "shimmer", "magical", "whimsical", "bright" }, {}, "A tiny harp an octave up, a chime sparkle and a shimmer (Character)." },
        { { "brass", "horns", "heroic", "epic", "majestic", "warm", "cinematic" }, {}, "A section of two horns with an octave below (Character)." },
        { { "strings", "viola", "cello", "warm", "lush", "romantic", "epic", "sustained" }, { "violin" }, "The violin recordings over a viola section, cellos below (Character)." },
        { { "choir", "brass", "horns", "epic", "heroic", "huge", "cinematic", "sacred" }, { "voice" }, "A choir in two octaves, horns and brass below, swelling in together (Character)." },
    }};
    return info;
}

// Words the query ignores (unless it has nothing else).
bool stopWord(const std::string& w) {
    static const std::set<std::string> words { "the", "of", "and", "in", "a", "an", "on", "at", "to", "up", "by", "from", "under", "with" };
    return words.count(w) > 0;
}

int placeIndex(int location, int sub) { return ls::inst::instrumentIndex(location, sub); }

void addTags(Tags& to, const Tags& from, std::size_t limit = 100) {
    std::size_t added = 0;
    for (const auto& t : from) {
        if (added >= limit) break;
        if (std::find(to.begin(), to.end(), t) == to.end()) { to.push_back(t); ++added; }
    }
}

std::string placeLabel(int location, int sub) {
    return std::string(locationNames[static_cast<std::size_t>(location)]) + " \xc2\xb7 " + subLocationName(location, sub);
}

float valueOf(const FactoryPreset& p, const char* id, float fallback) {
    for (const auto& [key, value] : p.values) if (std::string(key) == id) return value;
    return fallback;
}

} // namespace

// ---- Matching ----------------------------------------------------------------------------

std::string normalise(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool space = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == '\'') continue;   // "Dragon's" -> "dragons"
        if (std::isalnum(c)) { out.push_back(static_cast<char>(std::tolower(c))); space = false; }
        else if (!space) { out.push_back(' '); space = true; }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    const auto n = normalise(s);
    std::size_t start = 0;
    while (start < n.size()) {
        auto end = n.find(' ', start);
        if (end == std::string::npos) end = n.size();
        if (end > start) out.push_back(n.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

int editDistance(const std::string& a, const std::string& b, int limit) {
    const int n = static_cast<int>(a.size()), m = static_cast<int>(b.size());
    if (std::abs(n - m) > limit) return limit + 1;
    // Optimal string alignment (Damerau-Levenshtein with adjacent transpositions); the
    // words are short, three rows are plenty.
    std::vector<int> prev2(static_cast<std::size_t>(m + 1)), prev(static_cast<std::size_t>(m + 1)), row(static_cast<std::size_t>(m + 1));
    for (int j = 0; j <= m; ++j) prev[static_cast<std::size_t>(j)] = j;
    for (int i = 1; i <= n; ++i) {
        row[0] = i;
        int best = row[0];
        for (int j = 1; j <= m; ++j) {
            const auto J = static_cast<std::size_t>(j);
            const int cost = a[static_cast<std::size_t>(i - 1)] == b[J - 1] ? 0 : 1;
            int v = std::min({ prev[J] + 1, row[J - 1] + 1, prev[J - 1] + cost });
            if (i > 1 && j > 1 && a[static_cast<std::size_t>(i - 1)] == b[J - 2] && a[static_cast<std::size_t>(i - 2)] == b[J - 1])
                v = std::min(v, prev2[J - 2] + 1);
            row[J] = v;
            best = std::min(best, v);
        }
        if (best > limit) return limit + 1;
        std::swap(prev2, prev);
        std::swap(prev, row);
    }
    return std::min(prev[static_cast<std::size_t>(m)], limit + 1);
}

float matchWord(const std::string& q, const std::string& w) {
    if (q.empty() || w.empty()) return 0.0f;
    if (q == w) return 1.0f;
    const auto ql = q.size(), wl = w.size();
    if (wl > ql && w.compare(0, ql, q) == 0)   // a partial word: "cho" -> "choir"
        return 0.7f + 0.25f * static_cast<float>(ql) / static_cast<float>(wl);
    if (ql < 3) return 0.0f;
    if (w.find(q) != std::string::npos) return 0.5f;   // inside a word: "light" in "starlight"
    // Typos: one edit (two in long words); "harpe" -> "harp", "choire" -> "choir".
    const int allowed = ql >= 8 ? 2 : 1;
    const int d = editDistance(q, w, allowed);
    if (d <= allowed) return 0.62f - 0.1f * static_cast<float>(d - 1);
    // A partial word with a typo: "glokc" -> "glockenspiel".
    if (ql >= 4 && wl > ql) {
        const int dp = std::min(editDistance(q, w.substr(0, ql), allowed), editDistance(q, w.substr(0, ql + 1), allowed));
        if (dp <= allowed) return 0.45f;
    }
    return 0.0f;
}

bool Filter::accepts(const Entry& e) const {
    if (group >= 0 && static_cast<int>(groupOf(e.kind)) != group) return false;
    for (const auto& mood : moods)
        if (std::find(e.tags.begin(), e.tags.end(), mood) == e.tags.end()) return false;
    return true;
}

// ---- The index ---------------------------------------------------------------------------

void Index::add(Entry e) {
    const auto addWords = [&e](const std::string& text, float weight) {
        for (auto& w : words(text)) {
            auto found = std::find_if(e.tokens.begin(), e.tokens.end(), [&w](const Entry::Token& t) { return t.text == w; });
            if (found == e.tokens.end()) e.tokens.push_back({ w, weight });
            else found->weight = std::max(found->weight, weight);
        }
    };
    addWords(e.name, 1.0f);
    for (const auto& t : e.tags) addWords(t, 0.8f);
    addWords(e.family, 0.6f);
    addWords(e.where, 0.6f);
    static const char* kindWords[] { "instrument", "place", "location", "preset", "journey" };
    addWords(kindWords[static_cast<int>(e.kind)], 0.5f);
    if (e.kind == Kind::Location) addWords("place", 0.5f);
    e.normalisedName = normalise(e.name);
    list.push_back(std::move(e));
}

Index::Index(bool instrumentEdition) : instrument(instrumentEdition) {
    // Instruments (the instrument edition).
    if (instrument)
        for (int k = 0; k < ls::inst::instrumentCount; ++k) {
            int l = 0, s = 0;
            ls::inst::instrumentPlace(k, l, s);
            const auto& info = instrumentInfo()[static_cast<std::size_t>(k)];
            Entry e;
            e.kind = Kind::Instrument;
            e.name = ls::inst::patch(k).name;
            e.family = ls::inst::patch(k).family;
            e.where = placeLabel(l, s);
            e.tags = info.tags;
            addTags(e.tags, info.recordings);
            if (!info.recordings.empty()) e.tags.push_back("recorded");
            e.description = std::string(info.description) + " Home: " + subLocationName(l, s) + ", " + locationNames[static_cast<std::size_t>(l)] + ".";
            e.key = "inst:" + std::to_string(k);
            e.location = l;
            e.sub = s;
            e.instrument = k;
            add(std::move(e));
        }
    // Locations and their places.
    for (int l = 0; l < LocationCount; ++l) {
        const auto& li = locationInfo()[static_cast<std::size_t>(l)];
        Entry loc;
        loc.kind = Kind::Location;
        loc.name = locationNames[static_cast<std::size_t>(l)];
        loc.where = "Location";
        loc.tags = li.tags;
        std::string places;
        for (int s = 0; s <= subLocationCount(l); ++s) places += std::string(s > 0 ? ", " : "") + subLocationName(l, s);
        loc.description = std::string(li.description) + " Places: " + places + ".";
        loc.key = "loc:" + std::to_string(l);
        loc.location = l;
        loc.sub = 0;
        add(std::move(loc));
        for (int s = 0; s <= subLocationCount(l); ++s) {
            const int p = placeIndex(l, s);
            const auto& pi = placeInfo()[static_cast<std::size_t>(p)];
            Entry e;
            e.kind = Kind::Place;
            e.name = subLocationName(l, s);
            e.where = locationNames[static_cast<std::size_t>(l)];
            e.tags = pi.tags;
            e.description = pi.description;
            if (instrument) {
                // The place chooses its instrument: its name and recordings find the place too.
                const auto& ii = instrumentInfo()[static_cast<std::size_t>(p)];
                addTags(e.tags, ii.recordings);
                e.family = ls::inst::patch(p).name;
                e.description += std::string(" Instrument: ") + ls::inst::patch(p).name + ".";
            }
            e.key = "place:" + std::to_string(l) + ":" + std::to_string(s);
            e.location = l;
            e.sub = s;
            add(std::move(e));
        }
    }
    // Presets and journeys, in the order of the preset menu.
    const auto& presets = presetsFor(instrument);
    for (int k = 0; k < static_cast<int>(presets.size()); ++k) {
        const auto& p = presets[static_cast<std::size_t>(k)];
        const bool journey = std::string(p.category) == "Journeys";
        Entry e;
        e.kind = journey ? Kind::Journey : Kind::Preset;
        e.name = p.name;
        e.family = p.category;
        e.preset = k;
        e.key = "preset:" + std::to_string(k);
        // Where it starts: its Location parameter, else the route's first stop.
        int l = 0, s = 0;
        if (valueOf(p, "location", -1.0f) >= 0.0f) { l = static_cast<int>(valueOf(p, "location", 0)); s = static_cast<int>(valueOf(p, "subLocation", 0)); }
        else if (!p.route.empty()) { l = p.route.front().location; s = p.route.front().sub; }
        l = std::clamp(l, 0, LocationCount - 1);
        s = clampSubLocation(l, s);
        e.location = l;
        e.sub = s;
        if (journey) {
            const bool roam = valueOf(p, "roamOn", 0) >= 0.5f;
            e.where = roam ? std::string(locationNames[static_cast<std::size_t>(l)]) : "Journey";
            e.tags = { "journey", "travel" };
            if (roam) { e.tags.push_back("roam"); e.tags.push_back("wander"); }
            std::string text;
            if (roam) {
                const int bars = static_cast<int>(valueOf(p, "roamBars", 1));
                text = "Roams between the places of " + std::string(locationNames[static_cast<std::size_t>(l)]) + ", a new place every few bars ("
                       + std::to_string(std::max(1, bars)) + ").";
                addTags(e.tags, locationInfo()[static_cast<std::size_t>(l)].tags, 4);
                addTags(e.tags, placeInfo()[static_cast<std::size_t>(placeIndex(l, 0))].tags, 2);
            } else {
                const int mode = static_cast<int>(valueOf(p, "journeyMode", JourneyLoop));
                e.tags.push_back(mode == JourneyOneWay ? "one way" : (mode == JourneyPingPong ? "ping pong" : "loop"));
                text = std::string(mode == JourneyOneWay ? "A one-way journey" : (mode == JourneyPingPong ? "A journey there and back" : "A looping journey"))
                       + ", " + std::to_string(static_cast<int>(valueOf(p, "journeyBars", 4))) + " bars per stop: ";
                std::set<int> locations;
                for (std::size_t i = 0; i < p.route.size(); ++i) {
                    const auto& st = p.route[i];
                    text += std::string(i > 0 ? " \xe2\x86\x92 " : "") + subLocationName(st.location, st.sub);
                    locations.insert(st.location);
                    addTags(e.tags, placeInfo()[static_cast<std::size_t>(placeIndex(st.location, st.sub))].tags, 2);
                }
                text += ".";
                // The locations it passes through find it too.
                std::string through;
                for (int loc : locations) through += std::string(through.empty() ? "" : ", ") + locationNames[static_cast<std::size_t>(loc)];
                e.where = through;
            }
            if (instrument) text += " The instrument follows each place; held notes carry over.";
            e.description = text;
        } else {
            const int p0 = placeIndex(l, s);
            const int inst = p.instrument >= 0 ? p.instrument : p0;
            e.where = placeLabel(l, s);
            std::string text;
            if (instrument) {
                const auto& ii = instrumentInfo()[static_cast<std::size_t>(inst)];
                addTags(e.tags, ii.tags, 5);
                addTags(e.tags, ii.recordings);
                addTags(e.tags, placeInfo()[static_cast<std::size_t>(p0)].tags, 3);
                const std::string name = ls::inst::patch(inst).name;
                if (p.instrument >= 0) {
                    int il = 0, is = 0;
                    ls::inst::instrumentPlace(inst, il, is);
                    text = name + " (from " + subLocationName(il, is) + ") carried into " + subLocationName(l, s) + ", " + locationNames[static_cast<std::size_t>(l)]
                           + "; Lock instrument on.";
                    e.tags.push_back("carried");
                } else
                    text = name + " in its own place, " + subLocationName(l, s) + " (" + locationNames[static_cast<std::size_t>(l)] + ").";
                e.family = std::string(p.category) + " \xc2\xb7 " + name;
            } else {
                addTags(e.tags, placeInfo()[static_cast<std::size_t>(p0)].tags);
                text = std::string(subLocationName(l, s)) + " in " + locationNames[static_cast<std::size_t>(l)] + ".";
            }
            const float intensity = valueOf(p, "intensity", 1.0f);
            if (intensity <= 0.0f) { text += " Dry: Intensity 0."; e.tags.insert(e.tags.begin(), "dry"); }
            else if (intensity < 1.0f) { text += " Gentle: Intensity " + std::to_string(static_cast<int>(intensity * 100.0f + 0.5f)) + " %."; e.tags.insert(e.tags.begin(), "gentle"); }
            for (int m = 0; m < macroCount; ++m) {
                const float v = valueOf(p, ("macro" + std::to_string(m + 1)).c_str(), 0.5f);
                if (std::abs(v - 0.5f) > 0.01f && m < macroCountFor(l))
                    text += std::string(" More ") + macroName(l, m) + ".";
            }
            if (valueOf(p, "atmosLevel", 0.0f) > 0.0f) { text += " With the place's Atmosphere."; e.tags.push_back("atmosphere"); }
            e.description = text;
        }
        add(std::move(e));
    }
}

int Index::find(const std::string& key) const {
    for (std::size_t k = 0; k < list.size(); ++k) if (list[k].key == key) return static_cast<int>(k);
    return -1;
}

Results Index::search(const std::string& query, const Filter& filter, int perGroup) const {
    Results out;
    auto q = words(query);
    std::vector<std::string> meaningful;
    for (const auto& w : q) if (!stopWord(w)) meaningful.push_back(w);
    if (!meaningful.empty()) q = meaningful;
    const auto nq = normalise(query);
    std::array<std::vector<Result>, groupCount> groups;
    const bool listAll = q.empty();
    if (listAll && filter.group < 0 && filter.moods.empty()) return out;
    for (int k = 0; k < static_cast<int>(list.size()); ++k) {
        const auto& e = list[static_cast<std::size_t>(k)];
        if (!filter.accepts(e)) continue;
        float score = 0;
        if (!listAll) {
            bool all = true;
            for (const auto& w : q) {
                float best = 0;
                for (const auto& t : e.tokens) best = std::max(best, matchWord(w, t.text) * t.weight);
                if (best <= 0.0f) { all = false; break; }
                score += best;
            }
            if (!all) continue;
            // The whole name typed (or most of its start): first.
            if (e.normalisedName == nq) score += 1.5f;
            else if (nq.size() >= 2 && e.normalisedName.compare(0, nq.size(), nq) == 0
                     && (e.normalisedName.size() == nq.size() || e.normalisedName[nq.size()] == ' '))   // whole words: "harp" is not "harpsichord"
                score += 0.6f * static_cast<float>(nq.size()) / static_cast<float>(e.normalisedName.size());
            // Instruments and places before presets of the same strength.
            if (e.kind == Kind::Instrument || e.kind == Kind::Place) score += 0.01f;
        }
        groups[static_cast<std::size_t>(groupOf(e.kind))].push_back({ k, score });
    }
    std::array<int, groupCount> order { 0, 1, 2 };
    for (auto& g : groups)
        std::stable_sort(g.begin(), g.end(), [](const Result& a, const Result& b) { return a.score > b.score; });
    // The group with the best match first; presets (built from the instruments and places)
    // come after an instrument or place that matches about as well.
    if (!listAll)
        std::stable_sort(order.begin(), order.end(), [&groups](int a, int b) {
            const auto top = [&groups](int g) {
                const auto& list2 = groups[static_cast<std::size_t>(g)];
                return list2.empty() ? -1.0f : list2.front().score * (g == static_cast<int>(Group::Presets) ? 0.85f : 1.0f);
            };
            return top(a) > top(b) + 1.0e-4f;
        });
    for (int g : order) {
        const auto& list2 = groups[static_cast<std::size_t>(g)];
        out.groupTotals[static_cast<std::size_t>(g)] = static_cast<int>(list2.size());
        for (int i = 0; i < static_cast<int>(list2.size()) && i < perGroup; ++i) out.items.push_back(list2[static_cast<std::size_t>(i)]);
    }
    return out;
}

const Index& index(bool instrumentEdition) {
    static const Index effect(false), instrumentIndex(true);
    return instrumentEdition ? instrumentIndex : effect;
}

} // namespace dc::search
