// The instrument edition's factory presets (Direction Update 1, milestone 6).
#include "Presets.h"
#include "Synth/Synth.h"
#include <string>

namespace dc {
namespace {
// An instrument in its own place.
FactoryPreset at(const char* name, int location, int sub, std::vector<std::pair<const char*, float>> extra = {}) {
    std::vector<std::pair<const char*, float>> values { { "location", float(location) }, { "subLocation", float(sub) } };
    values.insert(values.end(), extra.begin(), extra.end());
    return { name, "Instruments", values, {}, -1 };
}
// One place's instrument carried into another place (locked).
FactoryPreset carried(const char* name, int instLocation, int instSub, int location, int sub, std::vector<std::pair<const char*, float>> extra = {}) {
    std::vector<std::pair<const char*, float>> values { { "location", float(location) }, { "subLocation", float(sub) }, { "instLock", 1.0f } };
    values.insert(values.end(), extra.begin(), extra.end());
    return { name, "Carried elsewhere", values, {}, ls::inst::instrumentIndex(instLocation, instSub) };
}
} // namespace

const std::vector<FactoryPreset>& instrumentPresets() {
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> list {
            at("Fanfare at the Gates", CelestialCitadel, 0),
            at("Hall of Mirrors Strings", CelestialCitadel, 1),
            at("Throne Chords", CelestialCitadel, 2),
            at("Horn on the Spire", CelestialCitadel, 3),
            at("Harp up the Cloud Steps", SkyTemple, 0),
            at("Sanctum Voices", SkyTemple, 1),
            at("Bell Garden Chimes", SkyTemple, 2),
            at("Moonwell Glass Harp", ElvenSanctuary, 0),
            at("Lyre of the Leaf Halls", ElvenSanctuary, 1),
            at("Flute under the Stars", ElvenSanctuary, 2),
            at("Prism Bells", CrystalPalace, 0, { { "instCharacter", 0.65f } }),
            at("Glass Harmonica on the Lake", CrystalPalace, 1),
            at("Diamond Celesta", CrystalPalace, 2),
            at("Meadow Guitar", BlossomMeadow, 0),
            at("Butterfly Kalimba", BlossomMeadow, 1),
            at("Oboe on the Hill", BlossomMeadow, 2),
            at("Organ of the Nave", GoldenCathedral, 0),
            at("Choir Loft", GoldenCathedral, 1),
            at("Stained Glass Celesta and Harp", GoldenCathedral, 2),
            at("Cloud Bridge Pad", FloatingIsles, 0),
            at("Wind Flute on the Isle", FloatingIsles, 1),
            at("Harbour Pluck", FloatingIsles, 2),
            at("Rose Garden Harp", RoyalGardens, 0),
            at("Fountain Music Box", RoyalGardens, 1),
            at("Colonnade Harpsichord", RoyalGardens, 2),
            at("Mushroom Ring Pizzicato", FairyGlade, 0),
            at("Firefly Glockenspiel", FairyGlade, 1),
            at("Fairy Court Harp", FairyGlade, 2),
            at("Horns of the Eyrie", DragonsPeak, 0),
            at("Golden Nest Strings", DragonsPeak, 1),
            at("Summit Choir and Brass", DragonsPeak, 2),
            // Dry and close: the instrument on its own (Intensity 0).
            at("Dry Concert Harp", RoyalGardens, 0, { { "intensity", 0.0f } }),
            at("Gentle Strings (50 %)", DragonsPeak, 1, { { "intensity", 0.5f } }),
            // Carried elsewhere.
            carried("Organ in the Sky Temple", GoldenCathedral, 0, SkyTemple, 1),
            carried("Guitar by the Moonwell", BlossomMeadow, 0, ElvenSanctuary, 0),
            carried("Kalimba in the Crystal Halls", BlossomMeadow, 1, CrystalPalace, 0),
            carried("Celesta in the Nave", CrystalPalace, 2, GoldenCathedral, 0),
            carried("Trumpets from the Summit", CelestialCitadel, 0, DragonsPeak, 2),
            carried("Harp in the Hall of Mirrors", RoyalGardens, 0, CelestialCitadel, 1),
            carried("Pizzicato in the Bell Garden", FairyGlade, 0, SkyTemple, 2),
            carried("Cloud Pad on the Mirror Lake", FloatingIsles, 0, CrystalPalace, 1),
        };
        // The journeys of the effect, for the instrument too.
        for (const auto& p : factoryPresets())
            if (std::string(p.category) == "Journeys") list.push_back(p);
        return list;
    }();
    return presets;
}
}
