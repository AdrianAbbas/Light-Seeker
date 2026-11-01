# LIGHT SEEKER

**LIGHT SEEKER** is a light-fantasy **audio effect** (AU + VST3, macOS; GarageBand and Logic
Pro): the bright, majestic twin of DUNGEON CRAWLER. A painterly, sunlit fantasy **map**;
click a **location** to zoom in and transform the sound, click again for its **places**.
Sky castles, crystal palaces, blossom meadows, golden cathedrals: uplifting, shimmering and
majestic, and always level-matched and musical on guitar, synth, piano and voice.

Every place transforms the sound obviously at the defaults (Intensity 100 %, Mix 100 %) and
has **one signature effect** no other place uses. Progress, decisions and measurements:
[docs/DEVLOG.md](docs/DEVLOG.md).

## The world

| Location | Places (the first is where you arrive) | Signatures |
|---|---|---|
| Celestial Citadel | Gates of Dawn · Hall of Mirrors · Throne of Light · Spire Summit | fanfare-like bright reflections · multiplying stereo reflections · regal chorus with a gold halo · long airy echoes |
| Sky Temple | Cloud Steps · Sanctum · Bell Garden | rising pitch-spiral shimmer · soft choir bloom · tuned chimes and bells |
| Elven Sanctuary | Moonwell · Leaf Halls · Starlit Canopy | glassy water-surface shimmer · warm wooden resonance and ensemble · sparkling high harmonics |
| Crystal Palace | Prism Hall · Mirror Lake · Diamond Vault | octave shimmer split into colours across the stereo field · calm long tail with a reverse swell · bright crystal ringing |
| Blossom Meadow | Sunlit Meadow · Butterfly Glade · Wildflower Hill | sunny warm saturation · fluttering auto-pan · gentle echoes over the valley |
| Golden Cathedral | Nave of Light · Choir Loft · Stained Glass Chapel | the longest, most radiant reverb · angelic choir · colour-shifting shimmer |
| Floating Isles | Cloud Bridge · Windswept Isle · Sky Harbour | swirling stereo motion · airy wind-filtered space · echoes soaring a fifth higher |
| Royal Gardens | Rose Garden · Fountain Court · Marble Colonnade | soft lush chorus · sparkling water-like modulation · rhythmic slaps between columns |
| Fairy Glade | Mushroom Ring · Firefly Hollow · Fairy Court | bouncy sparkle · twinkling glitter · tiny bright room and harp strings |
| Dragon's Peak | Eyrie · Golden Nest · Summit of Winds | vast echoes off the cliffs · warm golden harmonics · huge heroic reverb with a soaring octave |

Some spaces come from the developer's own effect captures (a long octave-shimmer hall in the
Nave of Light, a chorus hall in the Rose Garden), measured and rebuilt as impulse responses.

## Install (macOS 10.15+, Apple Silicon and Intel)

1. Download the `LightSeeker-macOS-Universal` artifact of a green run (GitHub → Actions →
   Native build → the run → Artifacts) or a release, and unzip it.
2. Quit GarageBand and Logic. In Terminal type `bash ` (with a space), drag
   `install-mac.sh` from the unzipped folder into the window, press Return.
3. Wait for "LIGHT SEEKER is installed and validated."

The script removes the browser's quarantine flag (without it macOS refuses to load an
unsigned plug-in and GarageBand shows a silent slot), re-signs the bundles for this Mac,
refreshes the Audio Unit list and runs Apple's validator. Details and a manual install:
[packaging/INSTALL.txt](packaging/INSTALL.txt). Uninstall: `bash uninstall-mac.sh`
(`--purge` also deletes your presets and drop-in scenes).

LIGHT SEEKER (`aufx Lsek Veil`, bundle ID `audio.veil.lightseeker`) installs **side by side
with DUNGEON CRAWLER** (`aufx Dgcr Veil`); neither's scripts touch the other's files.

## Using it

**GarageBand:** select a track → Smart Controls (B) → Plug-ins → an empty Audio Effect slot
→ Audio Units → Veil Audio → LIGHT SEEKER. Double-click the slot to open the window (if you
see plain sliders, switch the window's view menu from "Controls" to "Editor").
**Logic Pro:** an Audio FX slot → Audio Units → Veil Audio → LIGHT SEEKER.

1. The window opens on the **map**. Hover a landmark to see its name; click it: a golden
   path shows the journey and the view zooms into the place.
2. Inside a location, the **rings** (and the list, top right) are its places: click one to
   travel there. The breadcrumb (top left) leads back: "‹ Map" returns to the map without
   changing the sound; the location's name travels back to its own first place.
3. Automate **Location** and **Sub-location** to move through the world over the song; or
   use a **Journey** (below).

## Controls

| Control | What it does |
|---|---|
| **Character macros** (3–4, named per location) | The location's own controls, e.g. Celestial Citadel: Grandeur, Radiance, Echo, Height; Sky Temple: Elevation, Devotion, Bells. 0 % = as designed. |
| **Input** | How hard the place is driven (warmth, resonances); loudness stays matched. |
| **Intensity** | How much of the place: 0 % is the dry signal exactly, 50 % already clearly audible, 100 % as designed. |
| **Mix** | Dry/wet. |
| **Output** | Output level. |
| **Travel** (time + sync) | How long a journey takes: Instant to 10 s, or 1/4 note to 4 bars at the song's tempo. Tails ring on when you leave a place. |
| **Atmosphere** | Level of the place's sparse background sounds (soft wind, distant bells, birdsong, fountain shimmer, chimes, harp glints, leaves, a brook); 0 % = off (default). They step back when you play. |
| **Journey** (button) | The Journey panel: route, roaming, MIDI travel, atmosphere density/seed (below). Closes with ✕, a second click on Journey, Esc or a click outside it. |
| **Bypass** | Bypass (crossfaded). |
| **Advanced** | (Closes like the Journey panel.) Every module parameter as an offset from the place (0 % = as designed) or an override ("Place" = as designed), in ten groups: I Horizon (filters, resonator), II Sunwarmth (saturation), III Choir, IV Aura (spectral), V Echoes, VI Halls (reverb), VII Charms (flutter, bells, crystal, harp strings, wind, drops), VIII Signature (kind, amount, tone, rate), IX Radiance (convolution spaces, incl. the captured ones), X Ensemble (chorus, rising phaser, tape echo). Offsets travel with you. |
| **Preset name ▾, ◀ ▶** | Init / No preset, factory presets (favourite spots, journeys), your presets, Save as your preset… After any change the name shows a star ("Grand Citadel Tour *"). |
| **Journey active · Stop** (top centre) | Shown while a Journey or Roam runs (journey presets start one). Stop ends it where it is. |
| **A / B, A→B** | Compare two complete settings; copy the current one to the other slot. |
| **↶ ↷** | Undo / redo of your edits in the window. |
| **⚙** | Window size (75–200 %, or drag the corner) and Quality: Eco (lightest: no animation, a mono spectral layer), Standard, High (smoothest animation). |
| **▾** (above the controls) | Hide the controls to see the whole picture. |

**Journey panel:**

| Control | What it does |
|---|---|
| **Journey** + mode | Follow the route: Loop, One-way or Ping-pong. |
| **Bars per Stop** | How long each stop lasts (1–16 bars). |
| **Add this place / Remove last / Clear** | Build the route (up to 8 stops) from the places you visit. The map shows it with numbered stops. |
| **Roam** + rate | The plug-in wanders between the current location's places, every ½–16 bars. |
| **Randomness / Seed** | 0 % visits the places in order; more is more random. The same seed always gives the same wandering at the same bar. |
| **MIDI travel** + octave | Notes C–A of the chosen octave pick the ten locations, C–D♯ an octave higher the places (see host support below). |
| **Atmosphere Density / Seed** | How often background sounds come, and which. |

**A click always wins:** choosing a location on the map, a place in the list or a ring, or
the location in the breadcrumb takes you there at once and stops a running Journey or Roam
(a short note says "Journey stopped"; turn it on again in the panel or reload the preset).
Host automation of Location does not stop a journey: the journey steers while it is on.

Journeys, roaming and the atmosphere follow the song's position when it plays (the same bar
always gives the same result) and keep moving at the song's tempo when it is stopped.

**MIDI travel, host support (honestly):** the plug-in is an audio effect (`aufx`) so that it
loads in GarageBand. GarageBand does not send MIDI to audio effects, and Logic only offers
side-chain MIDI to "MIDI-controlled effects", so MIDI travel will usually do nothing there:
use Journey, Roam or automation instead. VST3 hosts that route MIDI to effects (Reaper,
Cubase, Bitwig) work.

## Factory presets

27: 18 favourite spots (Golden Gates at Dawn, Infinite Mirrors, Throne of Light, Above the
Clouds, Garden of Bells, Moonwell Glimmer, Under the Stars, Prism Light, Still Mirror Lake,
Summer Meadow, Butterfly Flutter, Nave of Light, Angel Choir, Soaring Harbour, Fountain
Sparkle, Fairy Court Harp, Heroic Summit, Gentle Rose Garden (50 %)) and 9 journeys
(Ascent to the Citadel, Grand Citadel Tour, Pilgrimage of Light, Through the Crystal Halls,
Flight of the Dragon, Elven Night; roaming: Fairy Dance, Drifting Isles, Meadow Wander).

## Troubleshooting

| Problem | Fix |
|---|---|
| Silent slot with plain sliders in GarageBand | The quarantine flag: run `install-mac.sh` again (from the unzipped folder), restart GarageBand. |
| Plug-in missing from the list | GarageBand: Settings → Audio/MIDI → tick "Audio Units", restart. Logic: Plug-in Manager → Reset & Rescan. |
| Plain sliders instead of the map | The window's view menu: "Editor", not "Controls". |
| "travelling…" for a moment | A scene is prepared the first time it is shown (a fraction of a second); after that it is cached. |
| CPU too high on a laptop | ⚙ → Quality → Eco. |
| Something else | Run `bash diagnose-mac.sh` (in the unzipped folder) and send the report it writes to the Desktop. |

Your own art can replace any scene: see [docs/ART_SOURCES.md](docs/ART_SOURCES.md). Manual
test checklist: [docs/MANUAL_TEST.md](docs/MANUAL_TEST.md).

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

```bash
build/LightSeekerTests_artefacts/Release/LightSeekerTests --listen build/test-logs/listening
```

macOS builds, `auval`, the AU host harness (in- and out-of-process, arm64 and x86_64) and
packaging run on GitHub Actions (`macos-15`); the Linux job runs the full suite and writes
the listening renders (every place dry vs processed, a journey, the atmosphere) into the
`LightSeeker-linux-test-logs` artifact. Art: `python3 tools/painted/build.py` (see
[tools/painted/README.md](tools/painted/README.md)); captures: [tools/captures/README.md](tools/captures/README.md).

## Layout

`Source/` (processor, engine, DSP modules, world, editor, art), `Tests/`, `tools/` (AU
harness, painted art pipeline, capture tools, icon), `scripts/` (CI, install), `packaging/`,
`assets/` (fonts, icon, captured impulse responses, painted art), `docs/` (DEVLOG, manual
test, art sources, snapshots).

Fonts: Cinzel and Cormorant Garamond (SIL Open Font License, `assets/fonts`).
