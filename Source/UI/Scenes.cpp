#include "Scenes.h"
#include "Painted.h"

// The fallback art: a luminous wash per place (sky, sun glow, soft clouds, a horizon in
// the location's colours) with the place's name, and a simple bright map. It is shown
// only where a scene has no painted layers (tools/painted) or its files are missing.
// Pure computation on the scene cache's thread.
namespace dc::art {
namespace {
// Map landmarks, virtual coordinates. Order = Location order.
const std::array<Landmark, LocationCount> landmarks {{
    { CelestialCitadel, { 480, 140 }, 50 },
    { SkyTemple, { 700, 120 }, 44 },
    { ElvenSanctuary, { 190, 230 }, 46 },
    { CrystalPalace, { 820, 260 }, 44 },
    { BlossomMeadow, { 370, 330 }, 46 },
    { GoldenCathedral, { 620, 250 }, 46 },
    { FloatingIsles, { 300, 120 }, 44 },
    { RoyalGardens, { 540, 350 }, 42 },
    { FairyGlade, { 170, 345 }, 40 },
    { DragonsPeak, { 860, 130 }, 46 },
}};

// Each location's palette: sky top, sky low, ground (high-key, pastel, sunlit).
struct Palette { juce::uint32 skyTop, skyLow, ground, accent; };
constexpr Palette palettes[LocationCount] = {
    { 0xff8fc4f0, 0xfffff4dc, 0xffeadfc8, 0xffe8b84a },   // Celestial Citadel: blue, cream, ivory marble
    { 0xffa9d4f5, 0xfffdf0f4, 0xfff4f1ec, 0xffe6c36a },   // Sky Temple: cloud white
    { 0xff9cc7d8, 0xffeaf3dc, 0xffa9c98f, 0xffcfe0f0 },   // Elven Sanctuary: silver and leaf green
    { 0xffb9dcf4, 0xfff2ecfb, 0xffdcd6f2, 0xffb6e3f4 },   // Crystal Palace: lavender glass
    { 0xff9fd0f2, 0xfffff6d8, 0xffb7d98a, 0xfff2b6c8 },   // Blossom Meadow: meadow green, blossom pink
    { 0xffc6d9ef, 0xfffff0cc, 0xfff0dcae, 0xffe8b84a },   // Golden Cathedral: soft gold
    { 0xff86c0f2, 0xffffffff, 0xffe8f2fa, 0xffd0e6f8 },   // Floating Isles: sky and cloud
    { 0xff9fcbea, 0xfffbf1e4, 0xffc4dca4, 0xfff0a8b8 },   // Royal Gardens: rose and lawn
    { 0xffb2d6c8, 0xfff6f0d8, 0xffa8cf98, 0xffe9d27a },   // Fairy Glade: mint and gold
    { 0xff92bce6, 0xfffde8c8, 0xffd9cbb0, 0xffe8a84a },   // Dragon's Peak: warm gold on stone
};

juce::Font serif(float height) { return juce::Font(juce::FontOptions("Georgia", height, juce::Font::plain)); }

void softEllipse(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    juce::ColourGradient grad(c, r.getCentre(), c.withAlpha(0.0f), { r.getRight(), r.getCentreY() }, true);
    g.setGradientFill(grad);
    g.fillEllipse(r);
}

void drawSky(juce::Graphics& g, const Palette& pal, float horizon, std::uint32_t seed) {
    g.setGradientFill(juce::ColourGradient(juce::Colour(pal.skyTop), 0, 0, juce::Colour(pal.skyLow), 0, horizon, false));
    g.fillRect(0.0f, 0.0f, virtualWidth, horizon + 2.0f);
    // The sun's glow and its rays.
    const float sx = 220.0f + static_cast<float>(seed % 520u), sy = 80.0f + static_cast<float>((seed * 7u) % 60u);
    softEllipse(g, { sx - 260, sy - 260, 520, 520 }, juce::Colours::white.withAlpha(0.55f));
    softEllipse(g, { sx - 60, sy - 60, 120, 120 }, juce::Colour(0xfffffbe8));
    for (int k = 0; k < 7; ++k) {
        const float a = -0.4f + 0.9f * static_cast<float>(k) / 6.0f + 1.5708f;
        juce::Path ray;
        ray.addTriangle(sx, sy, sx + std::cos(a - 0.05f) * 700.0f, sy + std::sin(a - 0.05f) * 700.0f,
                        sx + std::cos(a + 0.05f) * 700.0f, sy + std::sin(a + 0.05f) * 700.0f);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.fillPath(ray);
    }
    // Soft clouds.
    for (int k = 0; k < 9; ++k) {
        const float x = noise2(static_cast<float>(k), static_cast<float>(seed), seed) * virtualWidth;
        const float y = horizon * (0.25f + 0.6f * noise2(static_cast<float>(seed), static_cast<float>(k), seed + 3u));
        const float w = 140.0f + 160.0f * noise2(static_cast<float>(k), 2.0f, seed + 5u);
        softEllipse(g, { x - w * 0.5f, y - w * 0.12f, w, w * 0.24f }, juce::Colours::white.withAlpha(0.75f));
    }
}

void drawPlace(juce::Graphics& g, SceneId id) {
    const auto& pal = palettes[static_cast<std::size_t>(juce::jlimit(0, LocationCount - 1, id.location))];
    const auto seed = static_cast<std::uint32_t>(id.location * 13 + id.sub * 5 + 3);
    // Each place its own composition: horizon height, landmark position, size and form.
    const float horizon = 235.0f + 24.0f * static_cast<float>((id.location * 5 + id.sub * 3) % 7);
    g.fillAll(juce::Colour(pal.ground).darker(0.15f));   // no gap under the hills (and the horizon stays legible)
    drawSky(g, pal, horizon, seed);
    // Rolling ground, lighter in the distance (aerial perspective).
    for (int layer = 0; layer < 3; ++layer) {
        juce::Path hill;
        const float base = horizon + 20.0f + 70.0f * static_cast<float>(layer);
        hill.startNewSubPath(0, virtualHeight);
        for (int x = 0; x <= 960; x += 40)
            hill.lineTo(static_cast<float>(x), base - (30.0f + 20.0f * static_cast<float>(id.sub))
                                              * noise2(static_cast<float>(x) * (0.003f + 0.002f * static_cast<float>(id.location % 3)), static_cast<float>(layer), seed + static_cast<std::uint32_t>(layer)));
        hill.lineTo(virtualWidth, virtualHeight);
        hill.closeSubPath();
        g.setColour(juce::Colour(pal.ground).interpolatedWith(juce::Colour(pal.skyLow), 0.5f - 0.2f * static_cast<float>(layer)));
        g.fillPath(hill);
    }
    // A luminous landmark standing in the light: spires, a dome, crystals, trees or a
    // peak, one form per location, placed and sized per place.
    const float cx = 160.0f + 640.0f * static_cast<float>((id.sub * 37 + id.location * 23) % 100) / 100.0f;
    const float size = 0.7f + 0.25f * static_cast<float>(id.sub);
    const float base = horizon + 30.0f;
    const auto marble = juce::Colour(0xfff7f1e6).withAlpha(0.9f);
    softEllipse(g, { cx - 110 * size, base - 300 * size, 220 * size, 220 * size }, juce::Colour(pal.accent).withAlpha(0.5f));
    g.setColour(marble);
    juce::Path form;
    switch (id.location % 5) {
        case 0:   // spires
            for (int k = -2; k <= 2; ++k) {
                const float h = (200.0f - 50.0f * static_cast<float>(std::abs(k))) * size;
                form.addTriangle(cx + 34.0f * static_cast<float>(k) * size - 16 * size, base, cx + 34.0f * static_cast<float>(k) * size + 16 * size, base, cx + 34.0f * static_cast<float>(k) * size, base - h);
            }
            break;
        case 1:   // a domed temple
            form.addRectangle(cx - 80 * size, base - 70 * size, 160 * size, 70 * size);
            form.addEllipse(cx - 60 * size, base - 130 * size, 120 * size, 120 * size);
            break;
        case 2:   // crystals or great trees
            for (int k = 0; k < 4; ++k)
                form.addEllipse(cx - 90 * size + 50.0f * static_cast<float>(k) * size, base - (120.0f + 30.0f * static_cast<float>(k % 2)) * size, 70 * size, 130 * size);
            break;
        case 3:   // a tall facade
            form.addRectangle(cx - 60 * size, base - 160 * size, 120 * size, 160 * size);
            form.addTriangle(cx - 70 * size, base - 160 * size, cx + 70 * size, base - 160 * size, cx, base - 240 * size);
            break;
        default:  // a peak
            form.addTriangle(cx - 160 * size, base, cx + 160 * size, base, cx + 20 * size, base - 230 * size);
            break;
    }
    g.fillPath(form);
    // The name, in a small elegant serif.
    const float textY = juce::jlimit(60.0f, 420.0f, horizon - 120.0f);
    g.setColour(juce::Colour(0xff4a3b22));
    g.setFont(serif(30.0f));
    g.drawText(subLocationName(id.location, id.sub), juce::Rectangle<float>(200, textY, 560, 40), juce::Justification::centred);
    g.setFont(serif(16.0f));
    g.drawText(juce::String(locationNames[static_cast<std::size_t>(id.location)]).toUpperCase(), juce::Rectangle<float>(200, textY + 36, 560, 24),
               juce::Justification::centred);
}

void drawMap(juce::Graphics& g) {
    // A bright world from high above: the sea to the south-west, meadows, rivers, a
    // cloud bank to the north, snowy peaks in the north-east.
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffcfe9d0), 0, 0, juce::Colour(0xffe9e3c4), 0, virtualHeight, false));
    g.fillAll();
    juce::Path sea;
    sea.startNewSubPath(0, 400);
    sea.cubicTo(120, 420, 160, 500, 330, 560);
    sea.lineTo(0, 560);
    sea.closeSubPath();
    g.setColour(juce::Colour(0xff9fd0ea));
    g.fillPath(sea);
    juce::Path river;
    river.startNewSubPath(620, 120);
    river.cubicTo(560, 260, 420, 300, 260, 470);
    g.setColour(juce::Colour(0xffa9d6ef));
    g.strokePath(river, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (int k = 0; k < 6; ++k) {
        const float x = 700.0f + 45.0f * static_cast<float>(k), y = 100.0f + 12.0f * static_cast<float>(k % 2);
        juce::Path peak; peak.addTriangle(x - 40, y + 60, x + 40, y + 60, x, y - 10);
        g.setColour(juce::Colour(0xffc9c4cf)); g.fillPath(peak);
        juce::Path snow; snow.addTriangle(x - 13, y + 13, x + 13, y + 13, x, y - 10);
        g.setColour(juce::Colours::white); g.fillPath(snow);
    }
    for (int k = 0; k < 10; ++k)
        softEllipse(g, { 60.0f + 90.0f * static_cast<float>(k), 10.0f + 18.0f * static_cast<float>(k % 3), 220, 60 }, juce::Colours::white.withAlpha(0.8f));
    for (const auto& l : landmarks) {
        softEllipse(g, juce::Rectangle<float>(l.radius * 2.4f, l.radius * 2.4f).withCentre(l.position), juce::Colour(palettes[static_cast<std::size_t>(l.location)].accent).withAlpha(0.5f));
        g.setColour(juce::Colour(0xfffaf6ee));
        juce::Path tower; tower.addTriangle(l.position.x - 12, l.position.y + 14, l.position.x + 12, l.position.y + 14, l.position.x, l.position.y - 26);
        g.fillPath(tower);
    }
}
}

const std::array<Landmark, LocationCount>& mapLandmarks() {
    // The painted map's landmarks where it is installed (its manifest), else the
    // fallback map's.
    static const std::array<Landmark, LocationCount> painted = [] {
        auto marks = landmarks;
        if (const auto* s = paintedScene({ -1, 0 }))
            for (int i = 0; i < s->anchorCount; ++i)
                if (s->anchors[i].kind == anchorLandmark) {
                    const int index = juce::roundToInt(s->anchors[i].value);
                    if (index >= 0 && index < LocationCount)
                        marks[static_cast<std::size_t>(index)].position = { s->anchors[i].x, s->anchors[i].y };
                }
        return marks;
    }();
    return isPainted({ -1, 0 }) ? painted : landmarks;
}

juce::Image renderScene(SceneId id, int widthPixels, int heightPixels, const std::atomic<bool>* abort, std::vector<LightSource>* lights) {
    juce::Image image(juce::Image::RGB, std::max(1, widthPixels), std::max(1, heightPixels), true);
    if (abort != nullptr && abort->load()) return image;
    juce::Graphics g(image);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(widthPixels) / virtualWidth, static_cast<float>(heightPixels) / virtualHeight));
    if (id.isMap()) drawMap(g);
    else drawPlace(g, id);
    if (lights != nullptr && !id.isMap()) {
        // Glints for the editor's gentle life (light motes catching the sun).
        for (int k = 0; k < 5; ++k)
            lights->push_back({ { 200.0f + 140.0f * static_cast<float>(k), 150.0f + 30.0f * static_cast<float>(k % 2) }, 14.0f, 2 });
    }
    return image;
}

juce::String sceneSlug(SceneId id) {
    const auto slug = [](juce::String s) { return s.toLowerCase().removeCharacters("'").replaceCharacter(' ', '-'); };
    if (id.isMap()) return "map";
    return slug(locationNames[static_cast<std::size_t>(juce::jlimit(0, LocationCount - 1, id.location))]) + "/"
         + slug(subLocationName(id.location, id.sub));
}

juce::File overrideFile(SceneId id) {
    // macOS: ~/Library/Application Support/LightSeeker/scenes (JUCE's user data
    // folder there is ~/Library itself).
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile("Application Support");
   #endif
    return base.getChildFile("LightSeeker").getChildFile("scenes").getChildFile(sceneSlug(id) + ".png");
}
}
