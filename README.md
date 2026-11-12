# LIGHT SEEKER

**Version 1.0.0, by Windward Audio.** **1.0 is frozen**: from now on only bug fixes go in;
new ideas go to the "1.1 ideas" list in [docs/ROADMAP.md](docs/ROADMAP.md). What is in 1.0:
[CHANGELOG.md](CHANGELOG.md).

**LIGHT SEEKER** is a light-fantasy **software instrument**, and **LIGHT SEEKER FX** the same
world as an **audio effect** (AU + VST3, macOS; GarageBand and Logic Pro). Both are built from
this repository: the bright, majestic twin of DUNGEON CRAWLER. A painterly, sunlit fantasy
**map**; click a **location** to zoom in, click again for its **places**. Sky castles,
crystal palaces, blossom meadows, golden cathedrals: uplifting, shimmering and majestic.

- **LIGHT SEEKER** (instrument; hosts list it as *Windward Audio: LIGHT SEEKER*): **every place has its own instrument**,
  played from your keyboard or the piano roll **inside that place's space**: fanfare
  trumpets at the Gates of Dawn, a glass harp at the Moonwell, a grand organ in the Nave of
  Light, a kalimba in the Butterfly Glade. Travel and the space morphs while held notes play
  on; **Lock instrument** carries one instrument through every space. One instrument per
  instance, one instance per track, like any software instrument.
- **LIGHT SEEKER FX** (effect; *Windward Audio: LIGHT SEEKER FX*): every place transforms any track's sound,
  with one signature effect no other place uses, level-matched and musical on guitar, synth,
  piano and voice. (This is the plug-in that was called LIGHT SEEKER before the instrument
  edition; projects that used it find it again.)

Progress, decisions and measurements: [docs/DEVLOG.md](docs/DEVLOG.md).

## The instruments (LIGHT SEEKER)

Each location is a family; each place has its own instrument, distinct from its siblings,
good dry (Intensity 0 %) and in its space. Some play the developer's own recordings
(classical guitar, kalimba, violin, Korg Minilogue soft pad), the rest are modelled with
WORLDBUILDER's sound engine (brass, bowed strings, voices, plucked strings, bells, winds).

**The brass is real** (Fanfare Trumpets, Soaring Horn, Heroic Horns, the brass of Epic Choir
and Brass and the Orchestral Chord Pad's horns): the horn, trumpet, trombone and tuba of
VSCO 2 Community Edition (Versilian Studios, CC0 1.0 public domain; source, version and
licence in [`docs/LICENSES/VSCO-2-CE.txt`](docs/LICENSES/VSCO-2-CE.txt)), sustains with
their staccatos as the attack's front, every dynamic layer, shaped into LIGHT SEEKER's own
sound (a struck front, presence, bite, a touch of drive, a little of the synthesised brass
under them for weight and shine). The samples are not in this repository: the build fetches
them from their official source. A build without them plays the synthesised brass.

**The woodwinds are real too** (Airy Flute, Breathy Wind Flute, Pastoral Oboe, and a soft
woodwind choir in the Orchestral Chord Pad on its own Woodwinds macro): the same library's
flute, oboe, piccolo, clarinet and bassoon, looped sustains (with and without vibrato),
staccatos as a soft tongued front, every dynamic layer; the mod wheel is their expression (a
swell, brighter, and on the flutes more vibrato); a note played while the last is held joins
it legato. Bright and radiant: a little of the synthesised breath under them, presence and
an air band. A build without the library plays the synthesised woodwinds.

| Location (family) | Place → instrument |
|---|---|
| Celestial Citadel (radiant brass & strings) | Gates of Dawn → Fanfare Trumpets · Hall of Mirrors → Shimmering Strings · Throne of Light → Orchestral Chord Pad · Spire Summit → Soaring Horn |
| Sky Temple (bells & voices) | Cloud Steps → Rising Harp Glissando · Sanctum → Angelic Choir · Bell Garden → Wind Chimes and Bells |
| Elven Sanctuary (harps & flutes) | Moonwell → Glass Harp · Leaf Halls → Elven Lyre (guitar recordings, bright) · Starlit Canopy → Airy Flute |
| Crystal Palace (crystal) | Prism Hall → Crystal Bells (with a rising shimmer) · Mirror Lake → Glass Harmonica Pad · Diamond Vault → Celesta |
| Blossom Meadow (pastoral) | Sunlit Meadow → Warm Acoustic Guitar (guitar recordings) · Butterfly Glade → Bouncy Kalimba (kalimba recordings) · Wildflower Hill → Pastoral Oboe |
| Golden Cathedral (sacred, golden) | Nave of Light → Grand Organ · Choir Loft → Radiant Choir · Stained Glass Chapel → Celesta and Harp |
| Floating Isles (airy) | Cloud Bridge → Cloud Pad (Minilogue soft pad recordings) · Windswept Isle → Breathy Wind Flute · Sky Harbour → Shimmering Pluck |
| Royal Gardens (courtly) | Rose Garden → Concert Harp · Fountain Court → Music Box · Marble Colonnade → Harpsichord |
| Fairy Glade (twinkly) | Mushroom Ring → Bouncy Pizzicato (violin pizzicato recordings) · Firefly Hollow → Twinkling Glockenspiel · Fairy Court → Fairy Harp |
| Dragon's Peak (epic, heroic) | Eyrie → Heroic Horns · Golden Nest → Warm Strings (violin recordings) · Summit of Winds → Epic Choir and Brass |

**The instrument panel** (under the map):

| Control | What it does |
|---|---|
| **Instrument name, ‹ ›, Instruments** | The instrument playing now and its place. ‹ › and the Instruments menu play any place's instrument here (that locks it); "Follow the place" unlocks. |
| **Lock instrument** | Keeps the instrument while you travel: only the space changes. Off, the instrument follows the place (journeys too). |
| **Octave** | Shifts the keyboard by up to two octaves. |
| **Tone, Attack, Release** | Brightness, how the notes start (slower bows and breaths; softer mallets and fingers) and how long they ring after the key. 0 % = as designed. |
| **Character** | Each instrument's own: the trumpets' piccolo, the strings' shimmer octave, the choir's descant, the organ's trumpet stop, how long the harp's glissando sweeps (through the notes you hold, into the note you play; 0 % plays the note at once), the crystal bells' shimmer... Its name changes with the instrument. |
| **Vowel, Ensemble** | The voice instruments (the choirs, the chord pad's Voices). Vowel morphs between the sung vowels (mmm, ooh, oh, aah, ee: two either way from the instrument's own at 50 %). Ensemble brings the singers in one by one: one voice at 0 %, the instrument's own choir at 50 %, eight singers (each its own take, detune, timing and place in the stereo field) at 100 %. Dimmed on the other instruments. |
| **Level** | The instrument's level. At 0 dB every instrument is as loud as the others playing its own kind of part in its own place (pads, choirs and strings their chords, harps, plucks and bells their arpeggios, the solo winds and brass their melodies), so a mix starts balanced without touching a fader. |
| **Intensity** | How much of the place's space: 0 % is the instrument dry. |
| **Punch, Punch Time** | Gain on hit: each note starts louder (by up to +12 dB, less on soft notes) and settles back to its level over Punch Time (20-500 ms, default 120 ms). 50 % is the instrument's own: high on the brass (+6-8 dB), medium on plucks and harps (+3-3.5 dB), low on strings, bells and winds, off on pads and choirs; the text shows it in dB. A limiter keeps it from clipping. |
| **MIX** | Opens the MIX section (as the MIX button in Levels; Shimmer is there now). The instruments mostly played in a mix (brass, strings, plucks, harps, guitars) take a little less of the place's reverb by default; MIX Reverb at 150 % brings back the whole space. The brass's reverb also starts 15-20 ms later and dips under its hits, so the dry hit comes first. |
| **Bend, Mod, Sustain** | Pitch bend (a whole tone), modulation (vibrato; tremolo on struck and plucked instruments), the sustain pedal. Your MIDI keyboard's wheels and pedal move them. |
| **Keyboard** | Plays (click and drag); the keys light up with the notes the host plays. |

Velocity, sustain pedal, pitch bend and the mod wheel all work from MIDI; up to 16 notes per
instrument (10 to 12 for the solo winds and brass); nothing glides unless you bend. Travel
under held notes: the keys you hold carry on in the new place's instrument, the old one's
notes ring out, nothing hangs and nothing clicks.

## The world (LIGHT SEEKER FX: the places' signatures)

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

One download installs both plug-ins.

1. Download the `WindwardAudio-LightSeeker-1.0.0-macOS-Universal` artifact of a green run (GitHub → Actions →
   Native build → the run → Artifacts) or a release, and unzip it.
2. Quit GarageBand and Logic. In Terminal type `bash ` (with a space), drag
   `install-mac.sh` from the unzipped folder into the window, press Return.
3. Wait for "LIGHT SEEKER and LIGHT SEEKER FX are installed and validated."

The script removes the browser's quarantine flag (without it macOS refuses to load an
unsigned plug-in and GarageBand shows a silent slot), re-signs the bundles for this Mac,
refreshes the Audio Unit list and runs Apple's validator. Details and a manual install:
[packaging/INSTALL.txt](packaging/INSTALL.txt). Uninstall: `bash uninstall-mac.sh`
(`--purge` also deletes your presets and drop-in scenes).

**Updating from a build before 1.0.** The company name hosts show is now *Windward Audio*.
Run `install-mac.sh` as above: it removes the earlier copies (so GarageBand and Logic list
each plug-in once, under Windward Audio) and copies your presets and drop-in scenes into the
new folders. Projects and presets made with earlier builds open unchanged: the Audio Unit
codes and every parameter are the same.

Your data: `~/Library/Windward Audio/LIGHT SEEKER/` (the instrument) and
`~/Library/Windward Audio/LIGHT SEEKER FX/` (the effect), each with `Presets/` and
`Scenes/`. On its first launch each plug-in copies anything it finds in the old folders
(`~/Library/Application Support/LightSeeker`, and the presets hosts saved under the old
company name in `~/Library/Audio/Presets`) into its new folder; the old folders are kept as
they were. ⚙ → About shows the version and where your data lives.

LIGHT SEEKER (`LightSeeker.component`, bundle ID `com.windwardaudio.lightseeker`) and LIGHT
SEEKER FX (`LightSeekerFX.component`, `com.windwardaudio.lightseekerfx`) install **side by
side with DUNGEON CRAWLER**; no script touches the other plug-ins' files.

## Using it

**The instrument.** GarageBand: a Software Instrument track → Smart Controls (B) → Plug-ins →
the instrument slot → Audio Units → Windward Audio → LIGHT SEEKER. Logic Pro: a Software
Instrument track → the Instrument slot → AU Instruments → Windward Audio → LIGHT SEEKER. Play
your keyboard or the piano roll; choose places on the map and play their instruments.

**The effect.** GarageBand: select a track → Smart Controls (B) → Plug-ins → an empty Audio
Effect slot → Audio Units → Windward Audio → LIGHT SEEKER FX. Logic Pro: an Audio FX slot →
Audio Units → Windward Audio → LIGHT SEEKER FX. Double-click the slot to open the window (if you
see plain sliders, switch the window's view menu from "Controls" to "Editor").

The map and the controls below are the same in both:

1. The window opens on the **map**. Hover a landmark to see its name; click it: a golden
   path shows the journey and the view zooms into the place.
2. Inside a location, the **rings** (and the list, top right) are its places: click one to
   travel there. The breadcrumb (top left) leads back: "‹ Map" returns to the map without
   changing the sound; the location's name travels back to its own first place.
3. Automate **Location** and **Sub-location** to move through the world over the song; or
   use a **Journey** (below).
4. **Search** (the magnifier at the top of the window, or Cmd+F): type a name, a sound or a
   mood and choose from the results (below).

### Search

Click the **magnifier** (top right of the picture) or press **Cmd+F** anywhere in the window.
A light panel opens over the scene; type and the results appear at once, grouped as
**Instruments** (LIGHT SEEKER only), **Places** (the ten locations and their 31 places) and
**Presets** (presets and journeys), each with a small icon, the location it belongs to and
its tags.

- **What you can type:** names ("Moonwell", "Concert Harp", "Ascent to the Citadel"), a
  location ("Golden Cathedral"), sounds and moods ("choir", "harp", "pluck", "pad",
  "strings", "brass", "bells", "shimmer", "reverb", "echo", "long tail", "bright", "airy",
  "epic", "calm", "magical", "pastoral", "sacred", "heroic", "warm", "sparkly"...) and which
  of the developer's recordings an instrument plays ("guitar", "banjo", "piano", "violin",
  "kalimba", "voice", "synth"). Case does not matter, the start of a word is enough ("cho"),
  small typos are forgiven ("harpe", "choire", "glockenspeil"), and several words narrow it
  down ("calm harp", "epic brass").
- **Choose:** ↑ ↓ and Enter, or click. It does exactly what choosing it by hand does: a
  place travels there (from the map the golden path is drawn and the view zooms in; held
  notes carry over and Lock instrument is respected), an instrument plays here and locks
  (as the Instruments menu does), a preset or journey loads (as the preset menu does).
  The panel closes.
- **Hover or arrow** over a result to read a short description at the bottom of the panel.
- **Preview** (LIGHT SEEKER): hold the small ▶ on an instrument's row to hear a short phrase
  on it, a little below its normal level, through the place you are in; it stops when you let
  go. It is unavailable while the song plays.
- **Chips** under the field show only Instruments, Places or Presets, or only what is Calm,
  Epic, Magical or Sacred (they combine; with nothing typed they list everything that fits).
- **Recent:** with nothing typed, your last eight searches and results (kept until the
  plug-in is unloaded).
- **Closing:** Esc, the ✕, the magnifier again, or a click anywhere outside the panel (that
  click still does what it does: the map is never blocked).

## Controls

| Control | What it does |
|---|---|
| **Character macros** (3–4, named per location) | The location's own controls, e.g. Celestial Citadel: Grandeur, Radiance, Echo, Height; Sky Temple: Elevation, Devotion, Bells. 0 % = as designed. |
| **Input** | How hard the place is driven (warmth, resonances); loudness stays matched. |
| **Intensity** | How much of the place: 0 % is the dry signal exactly, 50 % already clearly audible, 100 % as designed. |
| **Mix** | Dry/wet. |
| **MIX** (button, in place of Shimmer) | Opens the MIX section above the controls (below); a second click or its ✕ folds it away. It stays open while you play and the window remembers it. |
| **Output** | Output level. |
| **Travel** (time + sync) | How long a journey takes: Instant to 10 s, or 1/4 note to 4 bars at the song's tempo. Tails ring on when you leave a place. |
| **Atmosphere** | Level of the place's sparse background sounds (soft wind, distant bells, birdsong, fountain shimmer, chimes, harp glints, leaves, a brook); 0 % = off (default). They step back when you play. |
| **Journey** (button) | The Journey panel: route, roaming, MIDI travel, atmosphere density/seed (below). Closes with ✕, a second click on Journey, Esc or a click outside it. |
| **Bypass** | Bypass (crossfaded). |
| **Advanced** | (Closes like the Journey panel.) Every module parameter as an offset from the place (0 % = as designed) or an override ("Place" = as designed), in ten groups: I Horizon (filters, resonator), II Sunwarmth (saturation), III Choir, IV Aura (spectral), V Echoes, VI Halls (reverb), VII Charms (flutter, bells, crystal, harp strings, wind, drops), VIII Signature (kind, amount, tone, rate), IX Radiance (convolution spaces, incl. the captured ones), X Ensemble (chorus, rising phaser, tape echo). Offsets travel with you. |
| **Preset name ▾, ◀ ▶** | Init / No preset, factory presets (LIGHT SEEKER FX: favourite spots, journeys; LIGHT SEEKER: instruments, carried elsewhere, journeys), your presets, Save as your preset… After any change the name shows a star ("Grand Citadel Tour *"). |
| **Magnifier** (top right), Cmd+F | Search (above). |
| **Journey active · Stop** (top centre) | Shown while a Journey or Roam runs (journey presets start one). Stop ends it where it is. |
| **A / B, A→B** | Compare two complete settings; copy the current one to the other slot. |
| **↶ ↷** | Undo / redo of your edits in the window. |
| **⚙** | Window size (75–200 %, or drag the corner) and Quality: Eco (lightest: no animation, a mono spectral layer), Standard, High (smoothest animation). |
| **▾** (above the controls) | Hide the controls to see the whole picture. |

**The MIX section** (both plug-ins; for sitting a place in a song with other instruments). Each
group control runs 0–150 %, 100 % = as designed; Intensity stays the master on top. Moving
any of them is click-free and keeps the level steady (turning the reverb down does not make
the sound jump); all are automatable and saved with presets and projects. Older projects open
with everything at 100 % and sound exactly as before.

| Control | What it does |
|---|---|
| **Reverb** | Every reverb and captured space (the halls, the Radiance spaces, the aura's wash). |
| **Echoes** | The delays, multi-taps, slap echoes, rising and soaring delays, the tape echo. |
| **Modulation** | Chorus, flutter, tremolo and auto-pan, the swirl, pitch waver (not the shimmer). |
| **Shimmer** | Every shimmer, octave shimmer and sparkle (the places' and the instrument's own): 0 % none, 100 % the default (a little below the original design), 150 % more. Unchanged, only moved here. |
| **Character** | Resonators, bells and chimes, harp strings, harmonic richness (warmth), the choir layers: the place's signature sounds. |
| **Atmosphere** | The Atmosphere layer's level, 0–150 % of its own Level (turn it on in the toolbar or in Advanced). |
| **Low Cut** | Takes the mud out of the reverbs: everything below 80–600 Hz (Off: as designed). |
| **Pre-delay** | Starts the reverbs later (up to +150 ms; earlier, to -50 ms, where a hall's own pre-delay allows): the dry note comes first. |
| **Duck** | The reverbs dip (to -15 dB at 100 %) while you play and bloom in the gaps. 0 % = off (default). |
| **As designed** | Every MIX control back to 100 % / off. |

**Journey panel:**

| Control | What it does |
|---|---|
| **Journey** + mode | Follow the route: Loop, One-way or Ping-pong. |
| **Bars per Stop** | How long each stop lasts (1–16 bars). |
| **Add this place / Remove last / Clear** | Build the route (up to 8 stops) from the places you visit. The map shows it with numbered stops. |
| **Roam** + rate | The plug-in wanders between the current location's places, every ½–16 bars. |
| **Randomness / Seed** | 0 % visits the places in order; more is more random. The same seed always gives the same wandering at the same bar. |
| **MIDI travel** + octave | Notes C–A of the chosen octave pick the ten locations, C–D♯ an octave higher the places (see host support below). |
| **Atmosphere Density / Seed** | How often background sounds come, and which. (On/off, Level and Density are also at the top of Advanced.) |

**A click always wins:** choosing a location on the map, a place in the list or a ring, or
the location in the breadcrumb takes you there at once and stops a running Journey or Roam
(a short note says "Journey stopped"; turn it on again in the panel or reload the preset).
Host automation of Location does not stop a journey: the journey steers while it is on.

Journeys, roaming and the atmosphere follow the song's position when it plays (the same bar
always gives the same result) and keep moving at the song's tempo when it is stopped.

**MIDI travel, host support (honestly):** LIGHT SEEKER FX is an audio effect (`aufx`) so that
it loads in GarageBand (in the instrument the notes are for playing, so MIDI travel is off). GarageBand does not send MIDI to audio effects, and Logic only offers
side-chain MIDI to "MIDI-controlled effects", so MIDI travel will usually do nothing there:
use Journey, Roam or automation instead. VST3 hosts that route MIDI to effects (Reaper,
Cubase, Bitwig) work.

## Factory presets

**LIGHT SEEKER (instrument): 50.** 31 instruments in their own places (one per place), two
dry or gentle (Dry Concert Harp, Gentle Strings (50 %)), 8 instruments carried into another
place (Organ in the Sky Temple, Guitar by the Moonwell, Kalimba in the Crystal Halls, Celesta
in the Nave, Trumpets from the Summit, Harp in the Hall of Mirrors, Pizzicato in the Bell
Garden, Cloud Pad on the Mirror Lake) and the 9 journeys below (the instrument follows each
place a journey passes; your held notes carry on).

**LIGHT SEEKER FX (effect): 27:** 18 favourite spots (Golden Gates at Dawn, Infinite Mirrors, Throne of Light, Above the
Clouds, Garden of Bells, Moonwell Glimmer, Under the Stars, Prism Light, Still Mirror Lake,
Summer Meadow, Butterfly Flutter, Nave of Light, Angel Choir, Soaring Harbour, Fountain
Sparkle, Fairy Court Harp, Heroic Summit, Gentle Rose Garden (50 %)) and 9 journeys
(Ascent to the Citadel, Grand Citadel Tour, Pilgrimage of Light, Through the Crystal Halls,
Flight of the Dragon, Elven Night; roaming: Fairy Dance, Drifting Isles, Meadow Wander).

## Troubleshooting

| Problem | Fix |
|---|---|
| An instrument says "synthesised (recordings not found)" | That build was made without the private recordings (see Build); the instrument plays its modelled stand-in. |
| Silent slot with plain sliders in GarageBand | The quarantine flag: run `install-mac.sh` again (from the unzipped folder), restart GarageBand. |
| Plug-in missing from the list | GarageBand: Settings → Audio/MIDI → tick "Audio Units", restart. Logic: Plug-in Manager → Reset & Rescan. |
| A plug-in listed twice, or still under its old company name | Run `install-mac.sh` again: it removes the copies from before 1.0. A copy in `/Library` needs the `sudo rm` command it prints. Then restart GarageBand. |
| Plain sliders instead of the map | The window's view menu: "Editor", not "Controls". |
| "travelling…" for a moment | A scene is prepared the first time it is shown (a fraction of a second); after that it is cached. |
| CPU too high on a laptop | ⚙ → Quality → Eco. |
| Something else | Run `bash diagnose-mac.sh` (in the unzipped folder) and send the report it writes to the Desktop. |

Your own art can replace any scene: see [docs/ART_SOURCES.md](docs/ART_SOURCES.md). Manual
test checklist: [docs/MANUAL_TEST.md](docs/MANUAL_TEST.md).

## Build and test

The build makes both plug-ins (`LightSeeker` and `LightSeekerFX` targets). The instrument's
recordings come from the private `AdrianAbbas/Samples` repository, prepared with
`tools/sample-prep` (from WORLDBUILDER) and copied into the instrument's bundles; CI fetches
them with the `SAMPLES_TOKEN` secret (a fine-grained token with Contents read access to
that repository). Without them the instruments that use them play modelled stand-ins.

```bash
python3 tools/sample-prep/prepare.py --source ../Samples --out build/samples
```

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLS_SAMPLES_DIR=build/samples
```

```bash
cmake --build build --parallel 4
```

```bash
LIGHTSEEKER_SAMPLES=build/samples bash scripts/run-tests-parallel.sh build/LightSeekerTests_artefacts/Release/LightSeekerTests build/test-logs
```

```bash
build/LightSeekerTests_artefacts/Release/LightSeekerTests --listen build/test-logs/listening
```

macOS builds, `auval`, the AU host harness (in- and out-of-process, arm64 and x86_64, both
plug-ins: the instrument played over MIDI, the effect on a sine) and packaging run on GitHub
Actions (`macos-15`); the Linux job runs the full suite and writes the listening renders
(every place dry vs processed, a journey, the atmosphere; every instrument dry and in its
place, a chord travelling through the world) into the `WindwardAudio-LightSeeker-linux-test-logs` artifact.
The `release` test group (both jobs) and the install test check the 1.0 freeze: the company
and version 1.0.0 hosts see, the registry listing each plug-in once, sessions saved before
1.0 (`Tests/fixtures`) loading unchanged, and the presets' migration. Art: `python3 tools/painted/build.py` (see
[tools/painted/README.md](tools/painted/README.md)); captures: [tools/captures/README.md](tools/captures/README.md).

## Layout

`Source/` (processor, engine, DSP modules, world, editor, art; `Synth/` the instruments and
`Synth/wb` WORLDBUILDER's sound engine, copied), `Tests/`, `tools/` (AU harness, painted art
pipeline, capture tools, sample preparation, icon), `scripts/` (CI, install), `packaging/`,
`assets/` (fonts, icon, captured impulse responses, painted art), `docs/` (DEVLOG, roadmap,
manual test, art sources, snapshots), `CHANGELOG.md`.

Fonts: Cinzel and Cormorant Garamond (SIL Open Font License, `assets/fonts`).
