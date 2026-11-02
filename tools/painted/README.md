# The painted art pipeline

LIGHT SEEKER's map and scenes are built in **Blender** from Python (no hand modelling),
rendered as **depth layers**, and then **painted** by a post-process. The plug-in moves the
layers at different speeds while zooming (parallax) and animates light motes, mist, glints,
the sun's glow and passing doves on top. The pipeline is DUNGEON CRAWLER's, relit for daylight.

```
scenes.py   one function per scene (the map and all 31 places): composition, camera, daylight,
            anchors (hotspots, landmarks, lights); v2 templates for meadows, skies, gothic
            interiors, isles and peaks (later registrations replace the first sketches)
detail.py   the detail builders, numpy-fast: grass (100-250 thousand blades), meadow flowers,
            natural trees (Direction Update 2: a recursive skeleton with golden-angle branching,
            tropism and pipe-model taper as continuous smooth tubes, a crown of alpha-cut leaf
            cards textured with a sprig drawn in numpy, every tree a little different),
            distant woods (a soft core and a fringe of leaf cards), the woven golden nest,
            cliff mesas, mountain ranges,
            sharp peaks, rolling valleys, distant groves, volumetric cumulus, floating isles,
            crystals, mushrooms, butterflies, sparkles; and the realistic materials (cliff
            stone, marble, gold, ashlar, stained glass, fields, polished floors, foliage)
gothic.py   a recursive gothic generator: towers with buttresses, string courses, lancets,
            pinnacles on pinnacles, crocketed spires with gold finials, halls with flying
            buttresses, whole citadels (~25 towers)
blib.py     the base builders and materials (from DUNGEON CRAWLER)
finish.py   the finishing pass before rendering (Direction Update 2): smooth shading with sharp
            edges kept above 40 degrees, small bevels on the architecture, subdivision on the
            soft shapes, rock and cliffs broken by Voronoi and cloud displacement
render.py   renders one scene's layers with Cycles (CPU) to multi-layer EXR + anchors.json
paint.py    denoise (OIDN), haze by layer (pale blue-cream air) and inside each layer towards the
            horizon (soft atmospheric depth), bloom, a lifted daylight tone
            curve, a high-key grade (cream highlights, lavender shadows), a light-handed
            Kuwahara (oil paint), brush streaks, dabs, fine grain; the sky where none is
            rendered; writes the finished layers
map_air.py  the map's air (final touches), after the light pass: atmospheric perspective on the
            land, haze layered on the far ranges (more at their foot), cloud banks behind the
            peaks (warm tops, lavender undersides), stratus collars, and a mist layer (horizon,
            valleys, river and sea) that keeps clear of the landmarks
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

Full size (1920 x 1120, 24 samples, OIDN-denoised) takes several minutes per scene on 4
CPU cores (`--jobs 2`); the 32 scenes a few hours. Every scene is built from scratch each
time (no saved .blend files): the scripts are the source. Renders go to `build-art/` (ignored
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
- Light: Blender's physical sky and a warm sun from behind the viewer's left, a soft rim
  sun; lavender shadows that never go black; aerial perspective into a pale blue-cream air;
  light shafts through haze in the interiors.
- Detail first: real geometry for every blade, petal, leaf, pinnacle and window; realistic
  materials (cracked, stained cliff stone with moss on the ledges; veined marble; coursed
  ashlar; leaded stained glass). The developer rejected the first, primitive renders as
  "too fake and cartoony" and approved this approach.
- Painting: a light hand (the detail must survive): Kuwahara, streaks, dabs, a fine grain.

## Drop-in art

See [docs/ART_SOURCES.md](../../docs/ART_SOURCES.md): a flat PNG or a folder of layers
replaces any scene without rebuilding.
