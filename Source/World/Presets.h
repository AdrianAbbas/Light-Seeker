#pragma once
// Factory presets (Milestones 11 and 12): favourite spots and journeys. A preset sets
// the listed parameters on top of the defaults (everything else returns to its
// default) and, for journeys, the route. User presets are saved state files.
#include "Journey.h"
#include <utility>
#include <vector>

namespace dc {
struct FactoryPreset {
    const char* name;
    const char* category;   // "Favourite spots" or "Journeys"
    std::vector<std::pair<const char*, float>> values;   // parameter ID -> plain value
    std::vector<Stop> route;
    // The instrument edition: another sub-location's instrument played here (locked), or -1.
    int instrument = -1;
};

inline const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets {
        // Favourite spots.
        { "Golden Gates at Dawn", "Favourite spots", { { "location", CelestialCitadel }, { "subLocation", 0 } }, {} },
        { "Infinite Mirrors", "Favourite spots", { { "location", CelestialCitadel }, { "subLocation", 1 } }, {} },
        { "Throne of Light", "Favourite spots", { { "location", CelestialCitadel }, { "subLocation", 2 }, { "macro2", 0.7f } }, {} },
        { "Above the Clouds", "Favourite spots", { { "location", SkyTemple }, { "subLocation", 0 }, { "macro1", 0.7f } }, {} },
        { "Garden of Bells", "Favourite spots", { { "location", SkyTemple }, { "subLocation", 2 }, { "atmosLevel", 0.35f } }, {} },
        { "Moonwell Glimmer", "Favourite spots", { { "location", ElvenSanctuary }, { "subLocation", 0 } }, {} },
        { "Under the Stars", "Favourite spots", { { "location", ElvenSanctuary }, { "subLocation", 2 }, { "macro2", 0.65f } }, {} },
        { "Prism Light", "Favourite spots", { { "location", CrystalPalace }, { "subLocation", 0 } }, {} },
        { "Still Mirror Lake", "Favourite spots", { { "location", CrystalPalace }, { "subLocation", 1 }, { "atmosLevel", 0.3f } }, {} },
        { "Summer Meadow", "Favourite spots", { { "location", BlossomMeadow }, { "subLocation", 0 }, { "atmosLevel", 0.4f } }, {} },
        { "Butterfly Flutter", "Favourite spots", { { "location", BlossomMeadow }, { "subLocation", 1 } }, {} },
        { "Nave of Light", "Favourite spots", { { "location", GoldenCathedral }, { "subLocation", 0 }, { "macro1", 0.75f } }, {} },
        { "Angel Choir", "Favourite spots", { { "location", GoldenCathedral }, { "subLocation", 1 } }, {} },
        { "Soaring Harbour", "Favourite spots", { { "location", FloatingIsles }, { "subLocation", 2 } }, {} },
        { "Fountain Sparkle", "Favourite spots", { { "location", RoyalGardens }, { "subLocation", 1 }, { "atmosLevel", 0.35f } }, {} },
        { "Fairy Court Harp", "Favourite spots", { { "location", FairyGlade }, { "subLocation", 2 } }, {} },
        { "Heroic Summit", "Favourite spots", { { "location", DragonsPeak }, { "subLocation", 2 }, { "macro1", 0.7f } }, {} },
        { "Gentle Rose Garden (50 %)", "Favourite spots", { { "location", RoyalGardens }, { "subLocation", 0 }, { "intensity", 0.5f } }, {} },
        // Journeys.
        { "Ascent to the Citadel", "Journeys",
          { { "journeyOn", 1 }, { "journeyMode", JourneyOneWay }, { "journeyBars", 2 }, { "travelSync", 3 } },
          { { BlossomMeadow, 0 }, { FloatingIsles, 0 }, { SkyTemple, 0 }, { CelestialCitadel, 0 }, { CelestialCitadel, 2 }, { CelestialCitadel, 3 } } },
        { "Grand Citadel Tour", "Journeys",
          { { "location", CelestialCitadel }, { "journeyOn", 1 }, { "journeyMode", JourneyLoop }, { "journeyBars", 4 }, { "travelSync", 3 } },
          { { CelestialCitadel, 0 }, { CelestialCitadel, 1 }, { CelestialCitadel, 2 }, { CelestialCitadel, 3 } } },
        { "Pilgrimage of Light", "Journeys",
          { { "location", RoyalGardens }, { "journeyOn", 1 }, { "journeyMode", JourneyLoop }, { "journeyBars", 4 }, { "travelSync", 4 } },
          { { RoyalGardens, 2 }, { GoldenCathedral, 0 }, { GoldenCathedral, 1 }, { GoldenCathedral, 2 }, { SkyTemple, 1 } } },
        { "Through the Crystal Halls", "Journeys",
          { { "location", CrystalPalace }, { "journeyOn", 1 }, { "journeyMode", JourneyPingPong }, { "journeyBars", 2 }, { "travelSync", 3 } },
          { { CrystalPalace, 0 }, { CrystalPalace, 1 }, { CrystalPalace, 2 } } },
        { "Flight of the Dragon", "Journeys",
          { { "location", DragonsPeak }, { "journeyOn", 1 }, { "journeyMode", JourneyOneWay }, { "journeyBars", 4 }, { "travelSync", 4 } },
          { { DragonsPeak, 1 }, { DragonsPeak, 0 }, { FloatingIsles, 2 }, { DragonsPeak, 2 } } },
        { "Elven Night", "Journeys",
          { { "location", ElvenSanctuary }, { "journeyOn", 1 }, { "journeyMode", JourneyLoop }, { "journeyBars", 2 }, { "travelSync", 2 }, { "atmosLevel", 0.25f } },
          { { ElvenSanctuary, 0 }, { ElvenSanctuary, 1 }, { ElvenSanctuary, 2 }, { FairyGlade, 1 } } },
        { "Fairy Dance", "Journeys",
          { { "location", FairyGlade }, { "roamOn", 1 }, { "roamBars", 1 }, { "roamRandomness", 0.7f }, { "travelSync", 2 } }, {} },
        { "Drifting Isles", "Journeys",
          { { "location", FloatingIsles }, { "roamOn", 1 }, { "roamBars", 2 }, { "roamRandomness", 0.5f }, { "travelTime", 2.5f } }, {} },
        { "Meadow Wander", "Journeys",
          { { "location", BlossomMeadow }, { "roamOn", 1 }, { "roamBars", 3 }, { "roamRandomness", 0.3f }, { "travelTime", 3.0f }, { "atmosLevel", 0.3f } }, {} },
    };
    return presets;
}

// The instrument edition's presets (Direction Update 1): instruments in their own places,
// instruments carried into another place (Lock instrument), then the journeys (the
// instrument follows each place it passes, held notes carry over).
const std::vector<FactoryPreset>& instrumentPresets();
inline const std::vector<FactoryPreset>& presetsFor(bool instrument) { return instrument ? instrumentPresets() : factoryPresets(); }
}
