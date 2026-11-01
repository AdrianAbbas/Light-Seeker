#include "Scenes.h"
#include "Painted.h"

// Points of interest inside each location's scenes (the fallback positions; painted
// scenes carry their own, projected from the doors and features of their 3D scene).
namespace dc::art {
namespace {
using P = juce::Point<float>;
// Where each place of a location sits in that location's scenes. Index = sub-location
// (0 = the location's own scene).
const std::array<std::array<P, placesPerLocation>, LocationCount> hotspotPlaces {{
    {{ P { 480, 330 }, P { 200, 250 }, P { 760, 250 }, P { 480, 120 } }},   // Celestial Citadel
    {{ P { 200, 320 }, P { 480, 220 }, P { 760, 320 }, P {} }},             // Sky Temple
    {{ P { 480, 340 }, P { 200, 260 }, P { 760, 200 }, P {} }},             // Elven Sanctuary
    {{ P { 480, 260 }, P { 220, 340 }, P { 740, 300 }, P {} }},             // Crystal Palace
    {{ P { 480, 340 }, P { 220, 300 }, P { 760, 220 }, P {} }},             // Blossom Meadow
    {{ P { 480, 300 }, P { 250, 200 }, P { 730, 280 }, P {} }},             // Golden Cathedral
    {{ P { 480, 300 }, P { 200, 220 }, P { 760, 260 }, P {} }},             // Floating Isles
    {{ P { 260, 320 }, P { 480, 270 }, P { 720, 300 }, P {} }},             // Royal Gardens
    {{ P { 480, 340 }, P { 220, 260 }, P { 740, 280 }, P {} }},             // Fairy Glade
    {{ P { 260, 220 }, P { 520, 330 }, P { 760, 140 }, P {} }},             // Dragon's Peak
}};
}   // namespace

bool hasIllustration(SceneId id) {
    return id.isMap() || (id.location >= 0 && id.location < LocationCount);
}

std::vector<Hotspot> sceneHotspots(int location, int sub) {
    std::vector<Hotspot> spots;
    if (location < 0 || location >= LocationCount) return spots;
    // A painted scene has its own points of interest (doors, stairs, features).
    const SceneId id { location, clampSubLocation(location, sub) };
    if (const auto* s = paintedScene(id); s != nullptr && isPainted(id)) {
        for (int i = 0; i < s->anchorCount; ++i)
            if (s->anchors[i].kind == anchorHotspot)
                spots.push_back({ juce::roundToInt(s->anchors[i].value), { s->anchors[i].x, s->anchors[i].y }, 26.0f });
        if (!spots.empty()) {
            // Doors behind the viewer or beside the frame project outside it: keep every
            // point of interest in the picture, above the controls, and apart.
            const juce::Rectangle<float> area(40.0f, 70.0f, 880.0f, 310.0f);
            for (auto& h : spots) h.position = area.getConstrainedPoint(h.position);
            for (int pass = 0; pass < 8; ++pass)
                for (std::size_t a = 0; a < spots.size(); ++a)
                    for (std::size_t b = a + 1; b < spots.size(); ++b) {
                        auto d = spots[b].position - spots[a].position;
                        const float len = d.getDistanceFromOrigin();
                        if (len >= 90.0f) continue;
                        if (len < 1.0f) d = { 1.0f, 0.0f };
                        const auto push = d / juce::jmax(1.0f, d.getDistanceFromOrigin()) * ((90.0f - len) * 0.5f);
                        spots[a].position = area.getConstrainedPoint(spots[a].position - push);
                        spots[b].position = area.getConstrainedPoint(spots[b].position + push);
                    }
            return spots;
        }
    }
    for (int k = 0; k <= subLocationCount(location); ++k)
        spots.push_back({ k, hotspotPlaces[static_cast<std::size_t>(location)][static_cast<std::size_t>(k)], 26.0f });
    return spots;
}

std::vector<LightSource> sceneLights(SceneId id) {
    std::vector<LightSource> lights;
    if (!id.isMap()) renderScene(id, 4, 3, nullptr, &lights);   // tiny: only the lights are kept
    return lights;
}
}
