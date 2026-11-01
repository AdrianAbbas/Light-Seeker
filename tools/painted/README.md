# The painted art pipeline

LIGHT SEEKER's map and scenes are built in **Blender** from Python (no hand modelling),
rendered as **depth layers**, and then **painted** by a post-process. The plug-in moves the
layers at different speeds while zooming (parallax) and animates light motes, mist, glints,
the sun's glow and passing doves on top. The pipeline is DUNGEON CRAWLER's, relit for daylight.

```
scenes.py   one function per scene (the map and all 31 places): composition from blib and the
            light builders (white-gold castles and spires, marble halls with gold, colonnades,
            balustrades, fountains, crystals, flower meadows, round and blossom trees, floating
            isles, cloud banks, mushrooms, bells, a golden dragon); daylight (a warm key from
            behind the viewer, a blue sky fill, a pale world); anchors (hotspots, landmarks, lights)
blib.py     the base builders and materials (from DUNGEON CRAWLER)
render.py   renders one scene's layers with Cycles (CPU) to multi-layer EXR + anchors.json
paint.py    denoise (OIDN), haze by layer (pale blue-cream air), bloom, a lifted filmic tone
            curve, a high-key grade (cream highlights, lavender shadows, pastels kept),
            generalised Kuwahara (oil paint), brush strokes, dabs, fine grain; paints the sky
            (blue to cream, the sun, sunlit cumulus, god rays); writes the finished layers
build.py    all of the above for every scene (or a list), then writes Source/UI/PaintedManifest.inc
```

## Running it

Blender runs as the `bpy` Python module (the same Blender, without its window):

```bash
pip install bpy==5.2.3 numpy scipy pillow OpenEXR
python3 tools/painted/build.py --samples 24         # every scene
python3 tools/painted/build.py sky-temple/sanctum   # one scene
python3 tools/painted/build.py --manifest-only      # only the manifest
```

A quick look while working on a scene (quarter size, a few seconds):

```bash
python3 tools/painted/render.py sky-temple/sanctum /tmp/r --scale 0.25 --samples 12
python3 tools/painted/paint.py /tmp/r /tmp/a --preview /tmp/sanctum.jpg
```

Full size (1920 x 1120, 24 samples, OIDN-denoised) takes about 3 minutes per scene on 4
CPU cores (the 32 scenes about 1.5 hours). Renders go to `build-art/` (ignored
by git); the finished layers to `assets/painted/<location>/<place>/` and are committed.

## The layers

| Layer | What | Format |
|---|---|---|
| `sky` | painted in paint.py: blue zenith to cream horizon, the sun and its glow, sunlit cumulus with lavender undersides, god rays | `sky.jpg` (opaque) |
| `far` | distant ranges (paler with distance), the outside beyond a gate | `far.jpg` + `far.png` (alpha) |
| `mid` | the subject: castles, halls, groves, isles, the map's land and landmarks | `mid.jpg` + `mid.png` |
| `fog` | mist (half resolution; most daylight scenes have none) | `fog.jpg` + `fog.png` |
| `near` | foreground rocks, pillars, chains, the parapet, cloud wisps | `near.jpg` + `near.png` |

Each layer is rendered with only its own objects visible to the camera; every other
object still casts shadows and blocks light, so the lighting stays whole. Colour and
alpha are separate files (JPEG colour bleeds outwards under transparent pixels, so no
fringes). The plug-in joins them back into premultiplied images.

## Look

- Palette: high-key and bright: clear sky blues, white and cream clouds, pearl and ivory
  marble, soft gold, blush pink, lavender and mint pastels, sunlit greens, white and yellow
  meadow flowers; radiant gold is the accent. (The developer's mood references informed the
  palette and the light only; they are never copied or committed.)
- Light: a warm sun from behind the viewer's left (the painted sun keeps its place in the
  sky), lavender shadows that never go black, aerial perspective into a pale blue-cream air.
- Painting: generalised Kuwahara, light brush streaks along the structure, soft dabs, a
  fine grain: a romantic oil-painting softness.

## Drop-in art

See [docs/ART_SOURCES.md](../../docs/ART_SOURCES.md): a flat PNG or a folder of layers
replaces any scene without rebuilding.
