# LIGHT SEEKER

**LIGHT SEEKER** is a light-fantasy **audio effect** (AU + VST3, macOS; GarageBand and Logic
Pro): the bright, majestic twin of DUNGEON CRAWLER. A painterly, sunlit fantasy **map**;
click a **location** to zoom in and transform the sound, click again for its
**sub-locations**. Sky castles, crystal palaces, blossom meadows, celestial cathedrals:
uplifting, shimmering and majestic, and always level-matched and musical.

> Work in progress: this README is the Milestone 1 skeleton. Milestone 5 completes it
> (every control, troubleshooting). Progress and decisions: [docs/DEVLOG.md](docs/DEVLOG.md).

## The world

Ten locations, 31 places (the first place of each location is where you arrive). Every
place has one **signature effect** no other place uses.

| Location | Places |
|---|---|
| Celestial Citadel | Gates of Dawn · Hall of Mirrors · Throne of Light · Spire Summit |
| Sky Temple | Cloud Steps · Sanctum · Bell Garden |
| Elven Sanctuary | Moonwell · Leaf Halls · Starlit Canopy |
| Crystal Palace | Prism Hall · Mirror Lake · Diamond Vault |
| Blossom Meadow | Sunlit Meadow · Butterfly Glade · Wildflower Hill |
| Golden Cathedral | Nave of Light · Choir Loft · Stained Glass Chapel |
| Floating Isles | Cloud Bridge · Windswept Isle · Sky Harbour |
| Royal Gardens | Rose Garden · Fountain Court · Marble Colonnade |
| Fairy Glade | Mushroom Ring · Firefly Hollow · Fairy Court |
| Dragon's Peak | Eyrie · Golden Nest · Summit of Winds |

## Install (macOS)

Download the `LightSeeker-macOS-Universal` artifact of a green run (Actions → Native build
→ Artifacts), unzip it and run `bash install-mac.sh` from Terminal. Details:
[packaging/INSTALL.txt](packaging/INSTALL.txt). LIGHT SEEKER (`aufx Lsek Veil`,
`audio.veil.lightseeker`) installs side by side with DUNGEON CRAWLER (`aufx Dgcr Veil`).

- **GarageBand:** an Audio Effect slot → Audio Units → Veil Audio → LIGHT SEEKER.
- **Logic Pro:** an Audio FX slot → Audio Units → Veil Audio → LIGHT SEEKER.

## Controls (summary)

Location and Sub-location (the map and the scenes), Intensity (how far you travel from the
dry sound), Mix, Input, Output, Travel Time / Sync, three or four themed macros per
location, the Advanced drawer (every module parameter), Journeys and Roam, the optional
Atmosphere layer, presets with A/B and undo/redo, resizable window, Eco/Standard/High
quality.

## Build and test

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --parallel 4
```

```bash
bash scripts/run-tests-parallel.sh build/LightSeekerTests_artefacts/Release/LightSeekerTests build/test-logs
```

macOS builds, `auval`, the AU host harness (in- and out-of-process, arm64 and x86_64) and
packaging run on GitHub Actions (`macos-15`); the Linux job runs the full suite and writes
the listening renders into the `LightSeeker-linux-test-logs` artifact.

## Layout

`Source/` (processor, engine, DSP modules, world, editor, art), `Tests/`, `tools/`
(AU harness, painted art pipeline, capture tools, icon), `scripts/` (CI, install),
`packaging/`, `assets/` (fonts, icon, painted art), `docs/` (DEVLOG, manual test, art
sources, snapshots).

Fonts: Cinzel and Cormorant Garamond (SIL Open Font License, `assets/fonts`).
