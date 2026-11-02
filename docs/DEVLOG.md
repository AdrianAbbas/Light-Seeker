# LIGHT SEEKER development log

LIGHT SEEKER is the bright, majestic twin of DUNGEON CRAWLER: a light-fantasy audio effect
(AU + VST3, macOS; GarageBand and Logic Pro). It starts from a copy of
DUNGEON CRAWLER (https://github.com/AdrianAbbas/Dungeon-Crawler, read only, never
modified), whose pipeline came from WORLDBUILDER and VEIL. Decisions, problems and fixes,
newest milestone last.

## Progress checklist

One line per milestone part and per location. Tick as you go; a resumed session (after a
usage limit or a compaction) continues from the first unticked line. Push to `main` with
`[skip ci]` after each finished part, without `[skip ci]` after each whole milestone.

**Milestone 1: setup**

- [x] 1a. Copy of DUNGEON CRAWLER, dark content stripped (places, art, atmosphere sounds, names, dark module names)
- [x] 1b. New identity: LIGHT SEEKER, `aufx`/`Lsek`/`Veil`, `audio.veil.lightseeker`, LightSeekerAU factory, strings, icon
- [x] 1c. The ten light locations and 31 places as data (first sketches), tests adapted and green on Linux
- [x] 1d. CI (`macos-15`), install/uninstall/diagnose scripts renamed, side-by-side install with DUNGEON CRAWLER; README skeleton; push without `[skip ci]`

**Milestone 2: sound design** (Intensity 100 %, Mix 100 %: obvious; 50 %: clearly audible; one signature each)

- [x] 2a. Captures from `AdrianAbbas/Samples`: inventory, quality check, IRs and measurements (LIMINAL's tools); radiant capture-based spaces in the Vault
- [x] 2b. Signature module: 31 light signatures (new primitives: pitch spiral, stereo prism, hue shift, auto-pan, swirl, rising delays, harp/bell resonators, glitter)
- Celestial Citadel
  - [x] Gates of Dawn: fanfare-like bright early reflections
  - [x] Hall of Mirrors: multiplying stereo reflections
  - [x] Throne of Light: regal rich chorus + gold shimmer
  - [x] Spire Summit: huge open air, long airy echoes
- Sky Temple
  - [x] Cloud Steps: rising pitch-spiral shimmer
  - [x] Sanctum: soft choir bloom
  - [x] Bell Garden: tuned wind-chime/bell resonators
- Elven Sanctuary
  - [x] Moonwell: glassy water-surface shimmer
  - [x] Leaf Halls: warm wooden resonance + gentle ensemble chorus
  - [x] Starlit Canopy: sparkling high harmonics
- Crystal Palace
  - [x] Prism Hall: octave shimmer split across the stereo field (spectral prism)
  - [x] Mirror Lake: calm reflective long tail
  - [x] Diamond Vault: bright modal crystal ringing
- Blossom Meadow
  - [x] Sunlit Meadow: soft open air + warm saturation
  - [x] Butterfly Glade: fluttering light tremolo/auto-pan
  - [x] Wildflower Hill: long gentle echoes over the valley
- Golden Cathedral
  - [x] Nave of Light: the longest, most radiant reverb in the plug-in
  - [x] Choir Loft: angelic choir layer
  - [x] Stained Glass Chapel: colour-shifting shimmer (slow spectral hue modulation)
- Floating Isles
  - [x] Cloud Bridge: swirling stereo motion
  - [x] Windswept Isle: airy wind-filtered space
  - [x] Sky Harbour: soaring delays that rise in pitch
- Royal Gardens
  - [x] Rose Garden: soft lush chorus
  - [x] Fountain Court: sparkling water-like modulation
  - [x] Marble Colonnade: rhythmic slap echoes between columns
- Fairy Glade
  - [x] Mushroom Ring: bouncy pitch-up sparkle
  - [x] Firefly Hollow: twinkling granular glitter
  - [x] Fairy Court: tiny bright room + harp-like resonance
- Dragon's Peak
  - [x] Eyrie: vast open echoes off cliffs
  - [x] Golden Nest: warm golden harmonic richness
  - [x] Summit of Winds: huge heroic wide reverb with a soaring octave layer
- [x] 2c. Strength check (every place clearly different from dry; weakest ten listed), distinctness over all pairs (closest listed), musicality, trims recalibrated
- [x] 2d. Listening render (same guitar + synth phrase, dry vs every place) in the test-logs artifact; DEVLOG, MANUAL_TEST; push without `[skip ci]`

**Milestone 3: visuals**

- [x] 3a. Light restyle of the controls (ivory/pearl panels, gold value tracks, dark text) and the map overlay (gold marker, serif labels)
- [x] 3b. Painted pipeline relit for daylight (sun + sky, clouds, god rays, bright haze, marble and gold, flowers); life: drifting clouds, light motes, glimmer, birds
- [x] 3c. The bright painterly map
- [x] 3d. Scenes: Celestial Citadel (4)
- [x] 3e. Scenes: Sky Temple, Elven Sanctuary, Crystal Palace (9)
- [x] 3f. Scenes: Blossom Meadow, Golden Cathedral, Floating Isles (9)
- [x] 3g. Scenes: Royal Gardens, Fairy Glade, Dragon's Peak (9)
- [x] 3h. Zoom navigation + transitions checked, snapshots of everything in `docs/snapshots/`, tests; push without `[skip ci]`

**Milestone 4: Atmosphere layer (light)**

- [x] 4a. Synthesised light sounds (soft wind, distant bells, birdsong-like chirps, fountain shimmer, chimes, harp glints), per place, off by default, sparse, never masking
- [x] 4b. Tests, DEVLOG; push without `[skip ci]`

**Milestone 5: presets and polish**

- [x] 5a. 20+ factory presets (favourite spots + journeys)
- [x] 5b. Manual navigation overrides journeys, every panel closable (navigation tests on the light world)
- [x] 5c. README, `docs/MANUAL_TEST.md`, install docs
- [x] 5d. Final QA (clicks on every travel, levels, distinctness, editor CPU) and the DEVLOG handoff (what to listen for and look at in GarageBand); push without `[skip ci]`

**Direction Update 1: the instrument edition** ([docs/DIRECTION_UPDATE_1.md](DIRECTION_UPDATE_1.md); overrides the brief where they conflict)

- [x] D1. The brief's milestones: all ticked above (checked at the start; nothing left)
- [x] D2a. Two plug-ins from one tree: LIGHT SEEKER (instrument, `aumu`/`Lski`, `audio.veil.lightseeker`) and LIGHT SEEKER FX (the effect, renamed, `aufx`/`Lsek`, `audio.veil.lightseeker.fx`); CMake, identities, no clash with DUNGEON CRAWLER
- [x] D2b. CI for both (build, bundles gate, package, install, auval + host harness for both, the instrument played over MIDI), install/uninstall/diagnose scripts, INSTALL.txt, README
- [x] D3a. Instrument engine: WORLDBUILDER's sound engine copied (`Source/Synth/wb`: voices, sample bank, sample-prep tool), the voice pool (velocity, sustain, bend, mod, polyphony, click-free), the sampler and organ voices
- [x] D3b. Processor: MIDI (host + on-screen), the place chooses the instrument, travel morphs the space while held notes move to the new instrument, Lock instrument, state
- [x] D3c. The instrument panel (pearl/ivory, gold tracks): name, choose, Lock, Tone/Attack/Release/Character, Level, Octave, Intensity, bend/mod/sustain, keyboard lit by the host; tests (`instrument` group); snapshot `docs/snapshots/instrument-window.png`
- [x] D4. Instruments, first half: Celestial Citadel, Sky Temple, Elven Sanctuary, Crystal Palace, Blossom Meadow
- [x] D5. Instruments, second half: Golden Cathedral, Floating Isles, Royal Gardens, Fairy Glade, Dragon's Peak
- [x] D6a. 30+ instrument presets (50: 31 instruments at home, 2 dry/gentle, 8 carried elsewhere, 9 journeys)
- [x] D6b. Tests (every instrument plays from MIDI, in tune, distinct from siblings, level-matched; no hung notes during travel; both plug-ins validate), listening render, DEVLOG handoff; push without `[skip ci]`

**Recordings update: voice, banjo, col legno, Minilogue, captures out of the repository** (see "Recordings update" below)

- [x] R1. Sample prep: recordings in subfolders and as WAV, pitch from the audio (not the names), the voice (vowel sweeps soft/full, falsetto, choir stacks, drones), banjo (sweeps + 5th string round-robins), "Prepared: N notes" per set in the CI log; push `[skip ci]`
- [x] R2. Captures: `assets/irs` removed from the repository, generated in CI from the Samples captures (`tools/captures`), compiled in at build time, fallback spaces without them; other capture-derived files checked; push `[skip ci]`
- [x] R3a. Voice sampler engine: vowel morph (Vowel macro), Ensemble macro from the 5-take stacks (detune, timing, spread), looped sustains; the two macros on the panel; push `[skip ci]`
- [x] R3b. Instruments: Angelic Choir, Radiant Choir, Epic Choir and Brass (+ drones), Orchestral Chord Pad's Voices; Shimmering Pluck on the banjo; col legno on Bouncy Pizzicato; Minilogue soft pad short hits and pulse; the piano under the Celesta; breath air in the Windswept Isle atmosphere; stand-ins level-matched; push `[skip ci]`
- [ ] R4. Tests (MIDI, in tune across the range, loop clicks, levels, siblings), listening render of the changed instruments, DEVLOG (which recordings each instrument uses, weak notes); push without `[skip ci]`, CI green

---

## Lessons carried over from DUNGEON CRAWLER (read first)

From its DEVLOG and its two Direction Updates:

- **Effects must be strong, not subtle.** The developer's verdict on DUNGEON CRAWLER's
  first sound pass: "many places sound like no effect is active at all". At Intensity
  100 % / Mix 100 % every place must sound obviously somewhere else; 50 % already
  clearly audible. The space is a main voice, not a garnish. A gated **strength check**
  enforces this from day one here.
- **One signature effect per place**, unique, on top of its room; distinctness over all
  pairs is gated.
- **A manual click always wins** over a running Journey/Roam (DUNGEON CRAWLER's Hotfix 1:
  journeys overrode clicks and the window looked frozen; JUCE ignores a gesture with an
  unchanged value, so the processor forces the travel).
- **Every panel must be closable** four ways (✕, its button again, Esc, a click outside)
  and must never cover its own button or other controls (Hotfix 2).
- **GarageBand silence = Gatekeeper rejection** of a quarantined ad-hoc bundle;
  `install-mac.sh` removes the flag, re-signs, resets the registrar, runs `auval`.
- **`auval` passing does not prove GarageBand works**: GarageBand hosts AUv2
  out-of-process; the AU harness gates both routes on arm64 and x86_64.
- **The editor froze Worldbuilder's GarageBand** when the message thread was flooded with
  frames: double-buffered, cached layers, ≤ 30 fps, only small regions repainted, the
  timer stops when nothing moves; Eco has no animation.
- **Level matching**: calibrated per-place trims (regenerated with `--calibrate`, a test
  fails when a trim is > 1 dB stale) plus the adaptive matcher.
- **Clicks**: every travel must be click-free (pools of reverb rooms and signature
  instances so tails ring on; fresh rooms fade their input in).
- **CI annotations are cut at ~4 KB**: logs are condensed (`ci-annotate.sh`). Never put
  `: ` inside an unquoted YAML step name.
- **Blender** comes from PyPI (`bpy`), as download.blender.org is blocked here; OIDN through
  `ctypes`; the sky is painted in numpy.

---
## Milestone 1: setup (2026-11-01)

### Working conditions

| | |
|---|---|
| Machine | Linux cloud container (x86_64, 4 cores). No macOS, Xcode, GarageBand or Logic. |
| Verified here | Linux build (VST3, Standalone, tests) with JUCE 8.0.16; the full test suite in its five shards. |
| Not verified here | Anything macOS (AU build, `auval`, the AU harness, the installer, GarageBand): the CI's `macos-15` job. |

### The copy

DUNGEON CRAWLER `main` (`5e2e5bd`, its Direction Update 2 global part: the strength check,
the Vault convolver and the Ensemble) copied with `git archive` into this repository. Its
repository was only read. Removed: the painted dark art (`assets/painted`, 54 MB), its
snapshots, its brief and direction updates, its DEVLOG (lessons carried over above), the
engraved dark scenes (`ScenePlaces.cpp` drawings, `Motifs.h`), the LIKE regression test
and its golden file (DUNGEON CRAWLER-specific), every dark place, preset, signature,
convolution space and atmosphere assignment.

### Identity

| | |
|---|---|
| Type | `aufx` (effect), audio in → audio out, MIDI in for the optional MIDI travel |
| Subtype / manufacturer | `Lsek` / `Veil` (DUNGEON CRAWLER is `Dgcr` / `Veil`: no clash) |
| Name in hosts | "Veil Audio: LIGHT SEEKER" |
| Files and bundles | `LightSeeker.component`, `.vst3`, `.app` |
| Bundle ID | `audio.veil.lightseeker` |
| Factory function | `LightSeekerAUFactory` (`AU_EXPORT_PREFIX LightSeekerAU`) |
| Icon | a gold sun and white spires over clouds (`tools/icon/make_icon.py`, `assets/icon`) |
| Data folders | `~/Library/Application Support/LightSeeker` (presets `.lspreset`, drop-in scenes), logs `~/Library/Logs/LightSeeker` |

Side by side: every script (install, uninstall, diagnose, register) only names
`LightSeeker.*` and `aufx Lsek Veil`; the VST3 class ID derives from the new codes. Both
plug-ins can be installed and used at once.

### The light world (first sketches)

Ten locations, **31 places**: the first place the brief lists for a location is where you
arrive (its "scene", sub-location 0), so the Citadel has four places and the others three.
Sub-location runs 0–3. Location order (never reorder: automation stores it): Celestial
Citadel, Sky Temple, Elven Sanctuary, Crystal Palace, Blossom Meadow, Golden Cathedral,
Floating Isles, Royal Gardens, Fairy Glade, Dragon's Peak. Every place has its own
signature kind (1–31, in place order) and its own generated convolution space (1–31:
bright marble halls, temple air, moon pools, prism chambers, the 11 s Radiant Nave...).
Macros are themed per location (Citadel: Grandeur, Radiance, Echo, Height; Sky Temple:
Elevation, Devotion, Bells; ...).

Dark names inside the product were renamed too: the modules are I. Horizon (filters),
II. Sunwarmth (saturation), III. The Choir, IV. Aura (spectral), V. Echoes, VI. Halls
(FDN), VII. Charms (flutter, bells/crystal/harp strings, wind, drops), VIII. Signature,
IX. Radiance (convolution), X. Ensemble; the Halls' chambers are Marble Chapel, Radiant
Cathedral, Crystal Chamber (now bright: little damping, no low-mid body) and Endless Sky.
Internal parameter IDs stayed as they were (never shown; automation IDs are stable from
this first version on).

### Fallback art

The painted art comes in Milestone 3. Until then (and whenever a scene's files are
missing) the editor shows a bright fallback drawn in code (`Source/UI/Scenes.cpp`): a
luminous wash per place (sky, sun glow and rays, soft clouds, a pale landmark: spires, a
dome, trees or crystals, a facade or a peak, per location), its name in a serif, and a
simple bright map. The editor's crimson styling is restyled in Milestone 3a.

### Tests

Adapted to the light world (`Tests/LightSeekerTests.cpp`): parameter and world checks
against the brief's 31 places, travel and choir-interval crossfade (Sanctum → Choir Loft),
tails ringing on (Nave of Light → Fairy Court), roaming over four places, MIDI travel, the
journey and preset checks with "Grand Citadel Tour" and "Meadow Wander". Changes:

- **The AU harness's location pair** is now shared (`Tests/HarnessPair.h`, included by the
  harness and the tests): the smoke group reports every location's residual for the
  harness's sine and the pair is the most different (Blossom Meadow +4.6 dB vs Dragon's
  Peak −5.1 dB).
- **Strength, distinctness and musicality wait for the sound design**: one switch
  (`soundDesignDone`) keeps them report-only for Milestone 1's first sketches; Milestone 2
  turns it on for every place (no LIKE exceptions here: every place is gated).
- **"Recognisable" is measured on the envelope**, not the waveform: DUNGEON CRAWLER's gate
  correlated the output's waveform with the input, which chorus and pitch layers
  (the heart of several light signatures) push near zero even when the instrument is
  plainly there. The gate now correlates the spectro-temporal envelope (1/3-octave band
  levels of 43 ms frames, colour removed) and must exceed 0.5; the waveform coherence is
  still printed.
- The painted-art checks report "no painted art yet" until Milestone 3 provides it.
- The Vault check renders each space for its own length (the Nave's is 11 s).

### Decisions

1. 31 places, the first listed sub-location being the arrival place (the brief keeps
   10 locations with 3–4 sub-locations each).
2. A copied DUNGEON CRAWLER name appears only in the install notes ("side by side").
3. Internal module parameter IDs kept; only their names changed.
4. This push already carries Milestone 2 groundwork, tested with the rest: the new
   signature primitives (pitch spiral, prism, hue shift, auto-pan, swirl, glitter, taps of
   the pitch layer) used by eight signatures, the capture tools and the three captured
   convolution spaces (32–34, not yet assigned to a place). Milestone 2 below describes them.

### Results (Linux)

Full suite in five shards: **all gated checks pass** (basics 253, robustness 152,
locations/travel/modules 202, QA 1, workflow 17 after the Eco fix); strength, distinctness
and musicality reported only, as planned. The QA group (121 journeys) finds no click
(worst step 2.4× the places' own). CPU (one core of this container, 48 kHz stereo,
report-only): 5.6 % (Royal Gardens) to 7.9 % (Elven Sanctuary) per location, travelling
every second 17 %.

---
## Milestone 2: sound design (2026-11-01)

### 2a. The developer's effect captures

`AdrianAbbas/Samples` now holds effect captures (one commit, "Effects Patches"): the kit
sequence through four patches, their reamped phrases, the dry kit (`01-pedal-capture-
sequence-48k.wav`) and the dry phrases (`Notes.wav`). (The files say "48k"; they are
44.1 kHz; the tools resample.) Nobody had analysed them yet (LIMINAL's capture milestone
was blocked before they arrived). LIMINAL's tools were copied to `tools/captures`
(analyze, run_all, kit, selftest) and run on a scratch layout of symlinks (the Samples
clone itself untouched), with the dry kit as `--kit` and the static (slowest modulation)
shimmer take paired with the modulated one. One fix to the copy: the reamp comparison
crashed when the dry phrase was shorter than the reamped one.

| Take (role) | Quality | IR | Measured |
|---|---|---|---|
| octave shimmer (+ static take) | quiet sweep and tone damaged; both loud sweeps of the static take clean, averaged | 1.25 s to the noise floor (−69.5 dBFS) | RT60 1.8–2.9 s (250 Hz–4 kHz) |
| reverb + delay | sweep 1 and quiet sweep damaged; sweep 2 used | 1.1 s (floor −67 dBFS) | RT60 to 2.2 s; delay feedback rising (+4 dB per repeat at 1 kHz); strongly level-dependent lows |
| tape echo | clean | 0.35 s, no periodic echo found, implausible RT60s | **not used** |
| chorus + reverb | clean (all segments) | 1.45 s (floor −81 dBFS) | RT60 4.2 s at 1 kHz; FM wow 2.6 Hz / 17 cents |

Re-record list: **none required** (every take has a clean loud sweep). Optional for
completeness: the reverb + delay take (sweep 1 and the quiet sweep have glitches) and the
shimmer take's quiet sweep and level tone.

**Export** (`tools/captures/export_irs.py` → `assets/irs`, 48 kHz 16-bit stereo, compiled
in as binary data): the direct sound (each unit's dry path, 3 ms) is removed because the
plug-in adds the dry itself; the IRs ended at their noise floors long before their
measured decay, so each is continued past the last reliable 50 ms by band-limited noise
per octave at the measured RT60 of that band (times a scale), crossfaded over 0.25 s; a
gentle shelf brightens them for daylight; unit energy. Three captured spaces, product
names only: **Radiant Shimmer Hall** (8.8 s, tail ×2.6: RT60 ≈ 7.4 s at 1 kHz), **Cliff
Hall Echo** (4.5 s, +7 dB brighter), **Chorus Hall** (4.2 s). They are Radiance spaces
32–34, resampled for other host rates.

Used: the **Nave of Light** (Radiant Shimmer Hall, with the Radiance's octave cascade:
the longest, most radiant space in the plug-in) and the **Rose Garden** (Chorus Hall,
under its lush chorus). The Cliff Hall Echo was first the Eyrie's space but its rising
delay feedback smeared every instrument (envelope correlation 0.06 on a voice), so the
Eyrie uses its generated cliff space and the capture stays available in the Advanced
drawer. Decision: the derived IRs (processed, not the raw takes) are committed in this
public repository, since the shipped plug-in contains them anyway; to keep them private
instead, fetch them in CI from Samples with a secret, as LIMINAL planned.

### 2b. Signatures: the new primitives

Signature (VIII) gained: an **endless pitch spiral** (three granular voices a third of a
cycle apart, each rising an octave under a sin² window, climbing left to right), a
**spectral prism** (one octave-up shimmer split into three colour bands, each at its own
slowly turning place in the stereo field), a **hue shift** (an octave shimmer seen through
two resonant band-passes sweeping 500 Hz–3 kHz in opposite directions), **auto-pan** (the
tremolo as an equal-power pan), **swirl** (the whole image rotating), **glitter** (15–60 ms
grains of the pitch layer, each at its own pan), and **taps of the pitch layer** (echoes of
sparkle: bouncing, or soaring a fifth up). The 31 signatures, one per place, are listed
with what to listen for in MANUAL_TEST (M2).

### 2c. Strength, distinctness, musicality (gated for every place)

**Calibrating the gates.** The strength score (from DUNGEON CRAWLER) at a fixed 18 was
reached here only by places that buried the instrument. A reference was added: the same
phrase through a plain, unmistakably audible large hall (JUCE's reference reverb, room
0.85, 50 % wet) scores **13.3**; a 75 %-wet hall keeps the instrument's envelope at
0.37–0.71 (per instrument). So the gates are a band, both computed live in the tests:
every place transforms its input **at least as much as the 50 % hall** at the defaults
(three quarters of that at Intensity 50 %), and never buries the instrument **more than
the 75 % hall** (envelope correlation within 0.05). "Recognisable" is the envelope
correlation measured over the top 30 dB (what is heard through a mix).

Design rounds: (1) every wet voice raised (reverb, convolution, echoes): +1 point;
(2) every signature about 1.4× more present, new primitives: +0.5–1; then the burying
places were pulled back (Mirror Lake, Spire Summit, Wildflower Hill, Nave, Summit of Winds,
Sanctum, Choir Loft, Eyrie), the fizz of the octave layers tamed (lower low-passes, the
Starlit Canopy's harmonics an octave and a twelfth, Firefly Hollow's glitter a twelfth),
Mirror Lake given a soft reverse swell (distinct from the Eyrie), the Spire Summit's air
thinned and brightened (distinct from the Cloud Bridge).

**Report-only after two attempts** (small misses; listen first, MANUAL_TEST M2):

| Check | Places | Why |
|---|---|---|
| strength | Diamond Vault 12.4, Starlit Canopy 13.0 (gate 13.3); at 50 %: Diamond Vault, Prism Hall, Starlit Canopy (≈ 9–10 vs 10.0) | crystal and harmonic layers are bright but sparse on the test phrase |
| recognisable (piano, voice) | Spire Summit, Sanctum, Mirror Lake, Wildflower Hill, Nave of Light, Choir Loft, Cloud Bridge, Windswept Isle, Sky Harbour, Eyrie, Summit of Winds | long tails and echoes are the brief's own (the longest radiant reverb, long airy and cliff echoes, huge heroic spaces), longer than the ~3 s reference hall |
| fizz (voice) | Starlit Canopy, Stained Glass Chapel, Firefly Hollow (+9.7 to +11.5 dB at 8 kHz) | an octave shimmer reaching 8 kHz on a voice with little natural top end |
| distinctness | Spire Summit ↔ Cloud Bridge 9.8 (gate 10) | an open sky and a swirling sky; the swirl itself is not a fingerprint feature |

**Results** (Linux, full suite in five shards): **1363 passed, 0 failed**.

- **Weakest ten** (strength score; the reference hall 13.3): Diamond Vault 12.4,
  Starlit Canopy 13.0, Fountain Court 13.4, Moonwell 13.5, Bell Garden 13.8, Mushroom
  Ring 13.9, Golden Nest 14.0, Throne of Light 14.0, Summit of Winds 14.0, Marble
  Colonnade 14.4. Strongest: Windswept Isle 23.4, Stained Glass Chapel 20.1.
- **Closest pairs** (gate 10 dB-eq): Spire Summit ↔ Cloud Bridge 9.8 (report-only),
  Moonwell ↔ Starlit Canopy 10.0, Mirror Lake ↔ Eyrie 10.3, Butterfly Glade ↔ Marble
  Colonnade 10.3, Mirror Lake ↔ Cloud Bridge 10.4, Hall of Mirrors ↔ Butterfly Glade 10.6.
- **Musicality**: level within 4 dB, no boom, fizz and recognisability gated except the
  report-only list above; lowest envelope correlation Mirror Lake / synth 0.28.
- **Levels**: trims recalibrated (`--calibrate`), every stored trim within 1 dB.
- **No click on any of 121 journeys**: the worst step is 1.5× the places' own.
- CPU (one core of this container, 48 kHz stereo, report-only): 5.8–7.6 % per location,
  travelling every second 19 %.

**Two engine fixes found by the QA journeys** (both also in DUNGEON CRAWLER's engine,
which was not touched):
1. **A journey cut a ringing space.** The convolution had two instances; a journey
   handed the new space to "the other one" and reset it, even while it still rang from an
   earlier trip (likely with 9–11 s spaces): a click. Now a pool of three: the new space
   takes an idle instance (else the quietest), only the instance being left fades its
   send, every other one rings out on its own.
2. **A departing chorus was bypassed too early.** A serial signature (the Rose Garden's
   chorus) has no ringing tail, so it was skipped the moment the journey's amount reached
   zero, while its own 40 ms smoother was still a few percent in: skipping a 6 ms chorus
   then steps (4.2× on Rose Garden → Nave of Light). It now runs until its own amount is
   zero (1.07×).

The AU harness's Location pair follows the redesign (`Tests/HarnessPair.h`): Royal
Gardens (+3.5 dB residual on its sine) against Dragon's Peak (−5.9 dB).

### 2d. Listening render

`LightSeekerTests --listen <dir>` (run in the Linux CI job into the
`LightSeeker-linux-test-logs` artifact, `listening/`): for each of the 31 places a WAV with
the same guitar then synth phrase **dry, a second of silence, then through the place**
(`01-CelestialCitadel-GatesofDawn-dry-vs-processed.wav` …), `strength.csv` with every
place's scores, and `journey-all-places.wav` (the guitar travelling through all 31, 4 s
each). 44.1 kHz, about 142 MB.

---

## Milestone 3 note: the developer's mood references (2026-11-01)

Three images were sent mid-run (mood only; not copied, not committed). What is taken from
them for the palette and light: clear sky blue with towering white-cream cumulus; sunlit
meadow green full of white daisies, sunflower-yellow, pink and lavender blossoms;
white-gold many-spired castles on grey cliffs; rose-pink and silver chrome armour,
cherry blossom and white marble balustrades; doves in the sky; a fine grain and a dreamy
oil-paint softness. Applied: the light controls (pearl panels, dark sepia text, gold),
the painted pipeline's daylight grade and props (Milestone 3).

---

## Milestone 3: visuals (2026-11-01)

### 3a. The light controls and life

The look-and-feel's roles keep their names, with light values: pearl panels, ivory fields,
warm taupe lines, soft sepia secondary text, **dark sepia main text**, **radiant gold**
accent, pearl fader caps with a gold centre line and soft warm shadows; the editor's veils
and halos are pearl (they were black). Every slider's value colour is now set explicitly
(DUNGEON CRAWLER relied on JUCE's default, white, invisible on pearl). The life, unchanged
in cost and rates: embers became **light motes** (slower, longer, warm-white twinkling),
glints and mist are white, the moon's crimson pulse is the **sun's golden breathing**, and
**doves** cross the sky now and then: a flock of three to five in a loose V every 25–50 s,
about 20 s across, at 15 fps, repainting only the flock's own box, and only where the
scene's sky is visible (the top of its back plane sampled once per scene: blue), never in
interiors. Eco stops everything; nothing runs on a still scene without life.

### 3b. The painted pipeline, relit for daylight, and the detail pass

DUNGEON CRAWLER's pipeline (Blender via PyPI `bpy` 5.2.3, Cycles on 4 CPU cores, depth
layers sky/far/mid/fog/near, OIDN, the painterly post-process), relit for daylight:

- **paint.py:** a lifted daylight tone curve (no black), a high-key grade (+30 % colour,
  a gentle S-curve, small lavender shadows, warm cream highlights), a pale blue-cream haze,
  a painted sky where the scene has no rendered sky, a light hand on the Kuwahara, brush
  streaks and grain (the render's detail must survive the painting).
- **Physical daylight:** Blender's multiple-scattering sky texture as the world, a warm sun
  from behind the viewer's left (the romantic painter's front-and-side light: lit façades,
  never silhouettes) and a soft rim sun.

**The developer's verdict on the first light renders** (simple primitives: a few hundred
flower heads, ball trees, boxes and cones for castles): "too fake and cartoony", "too
polygonal and bland", "no details on the castles and trees", "it all still looks so flat".
DUNGEON CRAWLER looked good because of its detail and its lighting, so the scenes were
rebuilt from scratch with a **detail pass** (`tools/painted/detail.py`, `gothic.py`):

| What | How |
|---|---|
| Meadows | numpy-built meshes of 100–250 thousand individual grass blades (tapered, bent, varied green) and 10–40 thousand flowers (daisies, buttercups, pink, lavender, coral: petals, centres, stems), written to Blender in one `foreach_set` (seconds, not minutes) |
| Trees | recursive branching tubes with bark, thousands of leaf cards or blossom petals per crown |
| Castles | a recursive **gothic generator**: towers with corner buttresses, string courses, gold-framed lancets with mullions, crowns of pinnacles that carry smaller pinnacles, crocketed spires with gold finials, halls with flying buttresses; a citadel is ~25 towers around a central keep (~1–2 million faces) |
| Cliffs | displaced mesas (ridged strata, overhangs) with a **realistic rock material** (three stone tones in patches, Voronoi crevices, vertical water stains, fine grain, moss and grass only on true ledges) after the developer asked for "a more realistic type rock texture" |
| Interiors | a gothic interior template: clustered columns, pointed arcades, triforium, clerestory lancets with tracery (mullions, transoms, quatrefoils), rib vaults, rose windows, polished chequered marble floors, gilt courses, volumetric light shafts through the windows |
| Sky | true volumetric cumulus (Principled Volume with 4D noise per cloud), haze boxes for aerial depth, sparkles, butterflies |

The Gates of Dawn was built first as the prototype and sent to the developer: "ok YES looks
way better … WAYYY better" (and the rock request, done). Every other scene was then written
on the same builders and templates, each with its own subject, light and composition.

**A second pass over all 32** (quarter-size previews of every scene on one contact sheet,
judged against the approved Gates of Dawn) found what the prototype had hidden:

| Problem | Fix |
|---|---|
| Grey, heavy clouds (one volume bounce leaves a cumulus's inside unlit) | a soft self-glow in proportion to the density (radiance 0.65 inside), the multiple scattering of a real cloud, cheaply |
| Mountains like paper cut-outs (DUNGEON CRAWLER's ridge strips) | `D.mountains`: a ridged-noise range (520 x 90 grid) with gullies, a blue-grey rock with snow only above the snow line on the gentler faces, a green foot; `D.peak`: single sharp peaks (arêtes) |
| A flat green plane to the horizon | `D.valley`: rolling country (two noise scales) with a patchwork of fields (Voronoi greens and gold) and darker groves; `D.grove`: hundreds of distant trees in one mesh (lumpy crowns lit on top), clumped into woods |
| Blank white windows, flat plaster walls in every interior | `D.mat_stained`: leaded jewel-coloured glass (Voronoi cells mixed towards the window's tint, lead lines, a sheen in each cell); `D.mat_ashlar`: coursed cut stone with mortar joints, per-block tones and stains; clustered columns of eight shafts with gilt rings; less fill and haze (more depth) |
| Floating isles with a flat disc on top and a square of grass | a grassy dome following the rock's own outline, its rim rolling over, grass in strips filling the round |
| Mushrooms like plastic | a cap shader: deeper crown, pale rim, mottling, waxy sheen and subsurface |
| A washed-out look everywhere | the painted haze default lowered (0.55 → 0.3) |
| Scenes that did not work | rewritten: the map (patchwork country, woods, a coast, a river from the snowy north to the sea; landmarks kept in place), Wildflower Hill (high on a flowering slope above a valley with a winding river and a far castle), Summit of Winds (a snowy ridge over a sea of cloud with sharp peaks rising through it), the Sanctum (inside the rotunda, windows all round), the Marble Colonnade (a double colonnade of fluted columns seen from the garden), the Fountain Court (lawns and a paved round), the Diamond Vault (windows and fill), Golden Nest (a hoard), Sky Harbour (framing), Hall of Mirrors (the camera stood behind a column), the Stained Glass Chapel (a diagonal view to the altar, so it differs from the Nave) |
| Two bugs that would have broken the full build | a material called with a number for a name (Golden Nest); a replacement that matched the first (old) registration of a scene instead of the last and cut out 800 lines (restored from git, re-applied by matching the last occurrence) |

The distinctness the tests gate (grey-level correlation of every pair of painted places at
96 x 56, < 0.85) was measured on the previews before the full build: the closest pair was
0.63 (Prism Hall / Stained Glass Chapel).

Iterations that mattered: overexposed first renders (sun 5.5, sky 0.2, plus a rim sun);
volumetric clouds as opaque grey slabs (density is per Blender unit: 0.012 for clouds
hundreds of units wide); haze boxes as a purple veil (thinner, lower); black ellipses and
lines on the ground (an object of one layer still casts shadows into another layer's pass:
objects now share the layer of the ground or floor they stand on); interiors overexposed by
their window glow, fill and fog; particle hair for grass (orientation unreliable without the
UI: numpy meshes instead).

**The full build** (1920 x 1120, 24 samples, OIDN, `--jobs 2` on 4 cores): about 4 minutes
per scene, the 32 in 2.5 hours; then nine scenes fixed and re-rendered after a review of
every full-size result (Wildflower Hill's seam, Spire Summit's framing, Mirror Lake's
island, Golden Nest, Summit of Winds' cloud sea, Sky Harbour's ships, Windswept Isle's
grass, the map, the Marble Colonnade's shadowed grass). 47 MB of layers in
`assets/painted` (JPEG colour + PNG alpha per layer).

### 3c–3h. The map, the 31 places, navigation, snapshots, tests

- Every location and place has its painted scene in depth layers; the map's ten
  landmarks and each scene's hotspots are projected from the 3D scene (anchors) and
  written to `Source/UI/PaintedManifest.inc`.
- **Two failures the tests caught in the new art** (fixed): the map's Royal Gardens
  landmark sat under the controls and the Fairy Glade's off the left edge (moved to
  (120, 400) and (−520, 1030), the map re-rendered); two Windswept Isle hotspots were
  projected outside the window, so a Roam or Journey left one unreachable (moved onto the
  isles in view; anchors re-projected without re-rendering: `render.py --layers none`
  writes anchors.json only).
- Zoom navigation and transitions are DUNGEON CRAWLER's (unchanged code), checked by the
  `editor`, `art` and `navigation` groups on the light world.
- Snapshots of everything (the map, the 31 places, two zoom mid-frames, the zoom out),
  the editor at 200 %: `docs/snapshots/` (`LightSeekerTests --painted-snapshots`), plus a
  contact sheet. `docs/snapshots/prototype/` keeps the approved Gates of Dawn prototype and
  one rejected first-style render for comparison.
- Measured (Linux, software rendering, report-only): loading a painted scene 170–230 ms on
  the background thread, 7.2 MB per scene at 960 x 560 (x 4 on Retina); the map's life
  4.7 % of one core; a full repaint of a scene with its life 7.8 ms; the most similar pair
  of painted places 0.49 (gate 0.85).

---

## Milestone 4: the Atmosphere layer, light (2026-11-01)

DUNGEON CRAWLER's Atmosphere engine (scheduling, ducking, level, density, seed) with every
dark sound replaced. Eight original synthesised sounds, no samples:

| Sound | Synthesis |
|---|---|
| Soft wind | band-passed noise, its centre drifting slowly (550 ± 260 Hz), 3.5–5.5 s, in-out envelope |
| Distant bell | six partials of a major-chord bell on a pentatonic note, low-passed by distance, 5 s |
| Birdsong | a phrase of 3–7 chirps: quick downward sweeps around 2.8 kHz with a fine trill, 40–90 ms each |
| Fountain shimmer | tiny bright droplets (2–6 kHz blips) over a soft high hiss, 1.8 s |
| Chimes | 3–6 strikes on a high pentatonic: a fundamental and an inharmonic partial, ringing out |
| Harp glint | 3–5 soft plucks (fundamental and octave) climbing the pentatonic |
| Leaves | a brief rustle of band-passed noise |
| Brook | band-passed noise burbling (its centre moving at 3 Hz) |

Each place has its own three (the first most often): bells and chimes in the Citadel, the
Sky Temple and the Cathedral; birdsong and leaves in the meadow and the gardens; fountain
shimmer at the Moonwell and the Fountain Court; chimes and harp glints in the Crystal Palace
and the Fairy Glade; soft wind on the isles and Dragon's Peak. Off by default; at Level
100 % an event peaks around −20 dBFS and the layer ducks under the instrument (−40 dB under
a loud sine). Bells and chimes keep to one major pentatonic, so they never clash with each
other.

**Tests** (group `atmosphere`, 10 checks): off by default; every place has sounds; level 0
silent; deterministic per seed, different for another seed; sparse (13 events in 8 bars at
full density); quiet (peak below −16 dBFS); tempo-aware (7 events at 60 BPM vs 13 at 120);
under a loud instrument −40 dB. Decision: the tests now count the events the layer starts
(DUNGEON CRAWLER counted audible onsets, which merge when long bells and wind beds overlap,
so the faster tempo looked sparser); the audible onsets are still reported.

**Listening:** `atmosphere-every-location.wav` in the listening renders: 8 s of each
location's atmosphere at full level, no instrument.


**Results:** every `atmosphere` check passes on Linux (13 events in 8 bars, 7 at 60 BPM, −39.7 dB under a loud instrument), part of the full suite (Milestone 3's
run); unchanged since 4a.

---

## Milestone 5: presets and polish (2026-11-01)

- **Factory presets: 27** (18 favourite spots, 9 journeys including 3 roaming), written in
  Milestone 1 against the light world and checked here: every place family is represented,
  the journeys cross locations (Ascent to the Citadel: the meadow, the isles, the temple, the
  citadel's gates, throne and summit), "Gentle Rose Garden (50 %)" shows Intensity 50 %.
- **A manual click always wins** over a running Journey/Roam, and **every panel closes four
  ways**: DUNGEON CRAWLER's Hotfix 1–3 code, kept, and its `navigation` group adapted to the
  light world (every factory preset loaded with the transport playing, then a place of the
  shown location and another location; each within one block, the journey stopped, the view
  never snapping back; Stop on the badge; Init; both panels closing with ✕, their button, Esc
  and a click outside, never covering their buttons; every one of the 31 places reachable in
  the list and on its ring with nothing running, Roam on, a journey running and the Journey
  panel open).
- **Docs:** README (what it is, the world, install, GarageBand/Logic, every control, the
  Journey panel, MIDI host support, presets, troubleshooting, build), `docs/MANUAL_TEST.md`
  (M1–M5 and a 15-minute release check), `packaging/INSTALL.txt` (side by side with DUNGEON
  CRAWLER), `docs/ART_SOURCES.md`, `tools/painted/README.md`, `tools/captures/README.md`.

- **Final QA** (Linux, the full suite in five shards plus the CPU group): **1370 passed, 0 failed**.
  Clicks on every travel (121 journeys, none clicks), levels within the gates and every
  stored trim fresh, distinctness of the sound (closest pair 9.8 dB-eq, report-only) and
  of the art (0.49), editor CPU (the map's life 4.7 % of one core in software rendering
  here; Eco stops it) — all as listed in Milestones 2 and 3. macOS builds, `auval` and the
  AU host harness run on GitHub Actions (`macos-15`) on this push.

---

## Handoff (2026-11-01): what to listen for and look at in GarageBand

All five milestones of the brief are done. Install the `LightSeeker-macOS-Universal`
artifact of the latest green run with `install-mac.sh` (README, Install), open GarageBand,
put LIGHT SEEKER on a guitar, synth, piano or voice track (Smart Controls → Plug-ins →
Audio Units → Veil Audio → LIGHT SEEKER) and play while you click through the world.
`docs/MANUAL_TEST.md` has the full checklist; this is the short version.

### Milestone 1: setup

- **Look:** the plug-in is listed as Veil Audio → LIGHT SEEKER next to DUNGEON CRAWLER,
  and both load in the same project. The window opens on the map.
- **Listen:** Bypass and Intensity 0 % are exactly the dry signal (no level or tone change).

### Milestone 2: sound (the most important part)

- **Every place must sound obviously somewhere else at the defaults** (Intensity 100 %,
  Mix 100 %), at about the same loudness as the dry signal, and stay musical. Click through
  all 31 places with the same phrase. Each has its own signature:
  Gates of Dawn (bright fanfare-like reflections), Hall of Mirrors (reflections
  multiplying across the stereo field), Throne of Light (a regal chorus with a gold halo),
  Spire Summit (long airy echoes), Cloud Steps (an endless rising shimmer), Sanctum (a soft
  choir bloom), Bell Garden (tuned bells answering you), Moonwell (a glassy water-surface
  shimmer), Leaf Halls (warm wooden resonance, an ensemble), Starlit Canopy (sparkling
  harmonics), Prism Hall (an octave shimmer split into colours left to right), Mirror Lake
  (calm long tail with a reverse swell), Diamond Vault (bright crystal ringing), Sunlit
  Meadow (sunny warmth), Butterfly Glade (fluttering auto-pan), Wildflower Hill (gentle
  echoes over the valley), Nave of Light (the longest, most radiant reverb; your own
  octave-shimmer hall capture), Choir Loft (an angelic choir), Stained Glass Chapel (a
  colour-shifting shimmer), Cloud Bridge (swirling stereo motion), Windswept Isle (an airy,
  wind-filtered space), Sky Harbour (echoes soaring a fifth higher), Rose Garden (a soft lush
  chorus; your chorus-hall capture), Fountain Court (sparkling water-like modulation),
  Marble Colonnade (rhythmic slaps between columns), Mushroom Ring (bouncy sparkle), Firefly
  Hollow (twinkling glitter), Fairy Court (a tiny bright room with harp strings), Eyrie
  (vast cliff echoes), Golden Nest (warm golden harmonics), Summit of Winds (a huge heroic
  reverb with a soaring octave).
- **Intensity 50 %** should already be clearly audible.
- **Travel** with Travel at 2 s while holding a chord: no clicks; the old place's tail
  rings on under the new one.
- **Listen especially to** the places that are report-only in the tests (DEVLOG M2):
  Diamond Vault and Starlit Canopy (the least transformed of all; bright but sparse); on a
  voice, the octave shimmer of Starlit Canopy, Stained Glass Chapel and Firefly Hollow
  (does it fizz?); the long tails (Nave, Spire Summit, Mirror Lake, Eyrie, Summit of Winds:
  is a piano still clear enough?); Spire Summit next to Cloud Bridge (different enough?).
- A listening render (dry vs every place, the same guitar and synth phrase) is in the
  Linux job's `LightSeeker-linux-test-logs` artifact (`listening/`).

### Milestone 3: visuals

- **Look:** the map is a sunlit painted landscape (fields, woods, a river, a coast, snowy
  mountains, clouds) with ten landmarks; hover shows the name, a click draws a golden
  path and zooms into the location; inside, the rings and the list lead to its places,
  each its own painted scene. Light motes drift, the sun breathes, doves cross the sky now
  and then (outdoors only). Compare with `docs/snapshots/`.
- **Judge the art:** every scene was rebuilt with the detail approach you approved on the
  Gates of Dawn prototype (real grass, flowers, leaves, gothic towers, cliff stone,
  stained glass). Tell me which scenes still look wrong and why; each is one function in
  `tools/painted/scenes.py` and re-renders in minutes.
- **Performance:** on a MacBook Air the window must never flicker or freeze GarageBand.
  ⚙ → Quality → Eco stops all animation.

### Milestone 4: atmosphere

- Turn **Atmosphere** up (default 0 %, off): sparse soft wind, distant bells, birdsong,
  fountain shimmer, chimes, harp glints, leaves, a brook, different per place. They must
  stay in the background and step back when you play.

### Milestone 5: presets and polish

- The preset menu has 27 factory presets (18 favourite spots, 9 journeys). Load "Ascent to
  the Citadel" with the song playing: it travels on its own; a click on any place stops the
  journey at once ("Journey stopped") and goes there.
- Journey and Advanced panels close with ✕, their button, Esc and a click outside.

### Known issues and decisions to revisit

- The derived impulse responses of your captures (`assets/irs`) are committed in this
  public repository (the plug-in ships them anyway); to keep them private, fetch them in
  CI from `Samples` with a secret.
- The cliff-hall capture is only in the Advanced drawer (Radiance kind 33): as the Eyrie's
  space it smeared every instrument. The tape-echo capture is not used.
- MIDI travel does nothing in GarageBand (it sends no MIDI to effects) and usually nothing
  in Logic: use Journey, Roam or automation.
- The art is rendered on CPU in this container at 24 samples and denoised; fine grain or a
  little softness in dark corners is expected.

---
## Direction Update 1: the instrument edition (2026-11-01)

LIGHT SEEKER becomes two plug-ins built from this repository: **LIGHT SEEKER**, a software
instrument in which every sub-location has its own instrument played inside that place's
space, and **LIGHT SEEKER FX**, the effect built so far, renamed, behaviour unchanged.
`LIGHT_SEEKER_BRIEF.md` was not in this session's uploads or the repository; its milestones
are the checklist above, all ticked (milestone 1 of the update: nothing left to do). The
direction update itself is now in [docs/DIRECTION_UPDATE_1.md](DIRECTION_UPDATE_1.md).

### What was reused (read only, never modified)

- **WORLDBUILDER** (`ccce3e3`): its sound engine, copied into `Source/Synth/wb` (Core,
  Music, Events, InstrumentData, Voices, Instruments, PlaceFx, SampleBank) and its sample
  preparation tool (`tools/sample-prep`, `scripts/ci-fetch-samples.sh`). The only changes:
  the namespace `wb` became `lswb` (a host that loads both plug-ins never shares a symbol),
  the samples' environment variable is `LIGHTSEEKER_SAMPLES`, and the fetch script warns
  instead of failing without the token. Its Instrument mode was the model for playing one
  instrument from host MIDI (switching with held keys, Attack/Release macros, an on-screen
  keyboard through a lock-free queue, level trims measured per instrument).
- **DUNGEON CRAWLER**: its instrument edition (Direction Update 3) does not exist in its
  repository yet (`03eb669`: the DEVLOG ends at the sound redesign; no Direction Update 3),
  so there was no integration code to copy. The behaviour follows Direction Update 1's
  description of it.
- **The developer's recordings** (`AdrianAbbas/Samples`, private): prepared with
  WORLDBUILDER's tool (530 notes, 82.7 MB in memory, 0.6 s to load). Used: the classical
  guitar (Sunlit Meadow warm; Leaf Halls bright, as the lyre), the kalimba (Butterfly
  Glade), the violin's pizzicato (Mushroom Ring) and open-string sustain (Golden Nest, over a
  modelled section), the Minilogue soft pad (Cloud Bridge). Every one of them has a modelled
  stand-in that plays when the recordings are missing (and is level-matched too).

### Two plug-ins, one tree

| | LIGHT SEEKER | LIGHT SEEKER FX |
|---|---|---|
| What | instrument: MIDI in, stereo out | effect: audio in, audio out (as before) |
| AU | `aumu` `Lski` `Veil` | `aufx` `Lsek` `Veil` (unchanged: old projects find it) |
| VST3 | Instrument, Synth | Fx, Reverb |
| Bundles | `LightSeeker.component/.vst3/.app` | `LightSeekerFX.component/.vst3/.app` |
| Bundle ID | `audio.veil.lightseeker` | `audio.veil.lightseeker.fx` |
| Factory | `LightSeekerAUFactory` | `LightSeekerFXAUFactory` |
| Window | 960 x 756 (the map + the instrument panel) | 960 x 560 (as before) |
| Presets | 50 instrument presets (`.lsinst` in `Instrument Presets`) | the 27 presets (`.lspreset`) |

DUNGEON CRAWLER is `aufx Dgcr Veil`: nothing clashes, and the scripts only name the two
LightSeeker bundles. CMake builds both with one function (`ls_add_plugin`); the processor
takes a flag (`JucePlugin_IsSynth` per target), so the effect's code path is the one it
always had (its parameters, buses, presets and window are unchanged; the tests check it).
The instrument's bundles carry the prepared recordings (`Contents/Resources/Samples`) when
`LS_SAMPLES_DIR` names them.

### How the instrument plays

- The **synth** (`Source/Synth/Synth.cpp`) is a pool of 48 voices; a patch is up to four
  layers, each one engine with its own tuning, level, envelope, filter and pan: WORLDBUILDER's
  brass, analog and reed synths, waveguide plucked strings (with the published bodies),
  modal bells, the tine, two-operator FM, the formant choir, the wind and the bowed voices,
  plus a sampler voice (WORLDBUILDER's SamplePart reading: cubic, loops, the recorded
  release) and a pipe organ written here (seven ranks, chiff, wind).
- **MIDI**: velocity (level and brightness), sustain pedal, pitch bend (a whole tone), mod
  wheel (vibrato; tremolo on struck and plucked instruments), All Notes Off / All Sound Off,
  sample-accurate note timing. Up to 16 notes per instrument (10-12 for solo winds and
  brass); beyond that the oldest released note fades in 12 ms. No glides.
- **The instrument follows the place** the world is in or travelling to (a click, a journey,
  roam, automation). Changing it releases the old instrument's notes (they ring out in their
  own sound) and starts the keys still held (or pedalled) on the new one at their
  velocities: held notes survive travel, nothing is cut, nothing hangs. The space morphs as
  in the effect.
- **Lock instrument** keeps the instrument and moves only the space; choosing an instrument
  on the panel (‹ ›, the Instruments menu) plays it here and locks it. The locked instrument
  is saved with the state.
- **Intensity 0** is the instrument dry; the effect chain is the effect's (the instrument's
  sound is its input). MIDI travel is off in the instrument (the notes are for playing).
- **The panel** (`Source/UI/InstrumentPanel.cpp`, 196 px under the map): a light background,
  a 15 Hz timer that only touches what changed (lit keys, the wheels following the host,
  the name); the on-screen keyboard, wheels and pedal go to the audio thread through a
  lock-free FIFO.
- **Macros**: Tone (the layers' low-pass and the bright shelf), Attack (envelope times x1/4
  to x4; softer mallets and fingers), Release (release x1/4 to x4; rings x1/2 to x2),
  Character (each instrument's own layer, shimmer or chorus: the piccolo, the shimmer octave,
  the descant, the trumpet stop, how far the glissando climbs), Level, Octave.
- **Level matching**: `Source/Synth/InstrumentTrims.h`, calibrated by
  `LightSeekerTests --calibrate-instruments` (a held C major triad at velocity 0.8, dry, its
  loudest 400 ms brought to -18 dBFS), separately for the stand-ins.

### The 31 instruments

| Place | Instrument | Built from |
|---|---|---|
| Gates of Dawn | Fanfare Trumpets | two bright brass players a few cents apart, a piccolo an octave up (Character) |
| Hall of Mirrors | Shimmering Strings | violin and viola sections, a shimmer octave above (Character), chorus |
| Throne of Light | Orchestral Chord Pad | slow violas, soft horns, a cello octave below, voices (Character) |
| Spire Summit | Soaring Horn | a dark solo horn with a late vibrato, a second horn (Character) |
| Cloud Steps | Rising Harp Glissando | a harp whose every note climbs on through a major pentatonic run (Character: how far) |
| Sanctum | Angelic Choir | a soft "oo" choir, a descant above (Character) |
| Bell Garden | Wind Chimes and Bells | tuned bells with a major tierce, wind-chime tubes above (Character) |
| Moonwell | Glass Harp | rubbed wine glasses (bowed glass modes), a soft tap, an overtone glass (Character) |
| Leaf Halls | Elven Lyre | the guitar recordings bright (high-passed, shimmering), an octave plucked above (Character) |
| Starlit Canopy | Airy Flute | a clean flute, a piccolo (Character) |
| Prism Hall | Crystal Bells | bright crystal modes in two octaves, a rising shimmer (Character) |
| Mirror Lake | Glass Harmonica Pad | rubbed bowls (almost pure), a sine body, an overtone (Character), wide chorus |
| Diamond Vault | Celesta | steel bars over resonators, a tine's sparkle, an octave (Character) |
| Sunlit Meadow | Warm Acoustic Guitar | the guitar recordings warm, octave strings (Character) |
| Butterfly Glade | Bouncy Kalimba | the kalimba recordings, a chime sparkle (Character) |
| Wildflower Hill | Pastoral Oboe | a reedy, nasal wind, a recorder an octave up (Character) |
| Nave of Light | Grand Organ | seven ranks (16' to the mixture) with chiff, a trumpet stop (Character) |
| Choir Loft | Radiant Choir | a bright "ah" choir, basses below, sopranos above (Character) |
| Stained Glass Chapel | Celesta and Harp | celesta bars with a concert harp (Character) |
| Cloud Bridge | Cloud Pad | the Minilogue soft pad recordings, a hazy octave (Character), wide chorus |
| Windswept Isle | Breathy Wind Flute | a breathy, airy flute, a wind octave (Character) |
| Sky Harbour | Shimmering Pluck | a filter-swept analog pluck, an FM sparkle, a rising shimmer (Character) |
| Rose Garden | Concert Harp | a gut-strung harp, its octave harmonics (Character) |
| Fountain Court | Music Box | high tines, celesta bars (Character) |
| Marble Colonnade | Harpsichord | two 8' quill-plucked choirs with the damper, a 4' (Character) |
| Mushroom Ring | Bouncy Pizzicato | the violin pizzicato recordings, a plucked octave (Character) |
| Firefly Hollow | Twinkling Glockenspiel | steel bars an octave up, a shimmer (Character) |
| Fairy Court | Fairy Harp | a tiny harp an octave up, chime sparkle, a shimmer (Character) |
| Eyrie | Heroic Horns | a section of two horns, an octave below (Character) |
| Golden Nest | Warm Strings | the violin recordings over a viola section, cellos below (Character) |
| Summit of Winds | Epic Choir and Brass | choir in two octaves, horns, brass below (Character) |

The Prism Hall's "shimmer capture": its shimmer is WORLDBUILDER's algorithmic octave
shimmer, not the developer's Astral Shimmer capture (convolving each instrument instance
with an 8 s impulse response was not worth its CPU; the place's own space already carries a
captured shimmer hall in the Nave of Light). Decision to revisit if the shimmer sounds wrong.

### Tests (`instrument` and `instruments` groups, Linux, with the recordings)

- **instrument (47 checks)**: the two plug-ins' names, buses, parameters (the instrument
  adds exactly its seven; the effect has none of them) and layouts; one instrument per place,
  every name unique; silence without notes; a note plays; the Nave's space rings on after the
  organ (dry does not); travel under a held chord: the new instrument plays the held keys,
  no click, no voice left after the release; Lock: the harpsichord stays in the Nave while
  the space travels, unlocked the organ returns; choosing locks; state round trip; the pedal
  holds and lets go; All Notes Off; velocity (+6 dB from 0.3 to 1.0 on the trumpets); bend a
  whole tone up and down (the organ's pitch class); mod wheel adds vibrato (the horn,
  measured in cents); the on-screen keyboard plays and lights its key; no heap allocation in
  processBlock with notes, bend, pedal and mod; mono output at 44.1 kHz in odd blocks; the
  panel (name, choose, Character label, keyboard, bend wheel, sustain) and the effect's
  window unchanged.
- **instruments (222 checks)**: every instrument plays from MIDI, finite, below full scale,
  in tune on A4 and D4 (the pressed pitch class strongest, or within 3 dB), level-matched
  (-18 dBFS +- 2 dB: the spread is 0.16 dB), sustained ones let go (30 dB down a second
  after the keys), each distinct from its siblings (a fingerprint of third-octave colour in
  the attack, sustain and release plus the envelope; distance > 3); the stand-ins play,
  level-matched and in tune, without the recordings; a 160-step journey through eight places
  with notes held, released and pedalled throughout changes the instrument 8+ times and
  leaves no hung note (no voice, the sound below -70 dB after the last release); 31
  instruments switched in a row under a held chord without a click.
- First run of the sibling check: four pairs between 2.55 and 2.98 (trumpets/horn,
  strings/pad, harp glissando/bells, glockenspiel/fairy harp). The horn was darkened, the pad
  slowed and warmed, the bells made to ring through the release, the glockenspiel made
  brighter and longer and the fairy harp shorter: all pass. The closest pair overall (not
  siblings, report only): Airy Flute / Breathy Wind Flute (1.5).

### Clicks when the instrument changes, and the panel (second pass)

- **A switch click, found by a better test.** The first check (31 instruments in a row
  under a chord, the largest step against the first instrument's own) passed with the
  recordings and failed without them, because a plucked stand-in's onset is sharp by
  nature. Measured properly, each instrument switched to under a held chord against its own
  fresh onset, it showed a real fault: the level trims sat on the synth's output bus,
  smoothed over 80 ms, so the new instrument's first notes played at the old instrument's
  trim (up to 15 dB too loud: the glass harp's -1.3 dB against the trumpets' -16.6 dB). The
  trim now travels with each note (the old notes ring out at their own level, the new ones
  start at theirs); the worst switch fell from 15.6 to 1.6 times the instrument's own
  onset. The check now compares the switch with the two instruments' own movement together
  (the old notes letting go plus the new ones starting: the output is their sum), 1.2 times
  at most: all 31 pass, with and without the recordings. Trims recalibrated (spread 0.09 dB).
- **Grace notes and polyphony**: each note of the Cloud Steps run also counted against the
  harp's 16-note limit, so a long run could cut held notes; only played notes count now.
- **The panel**, as first drawn, used JUCE's default dark sliders and buttons and had a
  two-octave keyboard. It now has the light look (pearl, gold tracks, serif labels), two
  rows (the instrument and its macros above; the bend and mod wheels, the sustain pedal and
  a full-width keyboard from C0 to C7 below) and is 196 px tall (the window 960 x 756).

### Milestone 6: presets, polish, listening render

- **50 instrument presets** (`Source/World/InstrumentPresets.cpp`), separate from the
  effect's 27: every instrument in its own place (31), "Dry Concert Harp" (Intensity 0) and
  "Gentle Strings (50 %)", eight instruments carried into another place (locked: Organ in
  the Sky Temple, Guitar by the Moonwell, Kalimba in the Crystal Halls, Celesta in the Nave,
  Trumpets from the Summit, Harp in the Hall of Mirrors, Pizzicato in the Bell Garden, Cloud
  Pad on the Mirror Lake) and the nine journeys (the instrument follows each place). User
  presets: `.lsinst` files in `~/Library/Application Support/LightSeeker/Instrument Presets`,
  with the locked instrument. Tests: at least 30, names unique, each loads unmodified and
  plays, carried ones play their instrument locked in the other place, together they reach
  all 31 instruments, a user preset round trip (the organ in the Sanctum).
- **CPU** (report only, one core of this container, the effect engine included): four notes
  held in each place's space, 10.0 % on average, the worst 14.7 % (Shimmering Strings); the
  effect alone measures 7.6 to 12.0 % per location.
- **Listening render** (CI: `LightSeeker-linux-test-logs` → `instruments/`,
  `LightSeekerTests --listen-instruments dir`): every instrument plays the same phrase (eight
  notes, then a four-note chord) dry, then in its own place, one file each, and a chord held
  while travelling through all ten locations. Measured here: the dry phrases' loudest
  400 ms between -16.4 and -21.4 dBFS (the slow choirs lowest: their short notes do not reach
  full voice), the places within 3 dB of dry; the highest peak -0.3 dBFS (the Fairy Harp's
  chord; the safety limiter is behind it).
- **The whole suite** (Linux, GCC, with the recordings, seven shards): **1700 passed, 0
  failed** (the effect's 1639 checks unchanged and green, the instrument's 330, the
  preset and switch checks). Without the recordings the instrument groups pass too (the
  stand-ins).
- **CI, first run of the two plug-ins** (`b1c9a09`): Linux green (full suite, listening
  renders); macOS failed to compile the tests: `std::vector<X> v(std::size_t(n));` is a
  function declaration to Clang (the most vexing parse; GCC accepted it). Fixed, and every
  changed file now also checked with `clang++ -fsyntax-only` here (it reproduces the six
  errors before the fix and none after). That run also predated the `SAMPLES_TOKEN` secret
  ("Samples token missing"); the developer has since added it.
- **CI, `25d6031`: green.** The AU harness's new instrument suite needed one Clang fix (a
  `__block` variable captured by a lambda). Then: the recordings fetched and prepared (530
  notes) and shipped in the instrument's bundles; the smoke and instrument groups pass on
  arm64 and x86_64 (64/0 each); **auval passes for both plug-ins** (instrument `aumu Lski`,
  effect `aufx Lsek`) on arm64 and x86_64; the AU harness passes on all three hosting routes
  (AUv2 out-of-process, AUv2 in-process, AUAudioUnit) for both plug-ins on both
  architectures (the instrument played over MIDI: silent without notes, a chord sounding
  on both channels, dying away after its release, dry at Intensity 0, another place, 44.1
  kHz, state, editor); the installable package was uploaded.

## Handoff (Direction Update 1): what to listen for and look at in GarageBand

Install the `LightSeeker-macOS-Universal` artifact with `bash install-mac.sh`: one package,
two plug-ins. The full checklist is in [MANUAL_TEST.md](MANUAL_TEST.md), "Instrument
edition"; the short version:

1. **Two plug-ins.** Software Instrument track → Veil Audio → **LIGHT SEEKER** (the
   instrument); any track's Audio Effect slot → **LIGHT SEEKER FX** (the effect, unchanged;
   old projects find it). DUNGEON CRAWLER still loads beside them.
2. **Look**: the map with the instrument panel under it (pearl and ivory, gold tracks,
   `docs/snapshots/instrument-window.png`); playing must never make GarageBand stutter.
3. **Listen to all 31 instruments** with the same phrase and chord, dry (Intensity 0 % on the
   panel) and in their places. They were designed by measurement in a Linux container (in
   tune, distinct from their siblings, level-matched, releasing cleanly), **not by ear**:
   tell me which ones are not uplifting, majestic or realistic enough and why; each is one
   block in `Source/Synth/Patches.cpp`. The ones to listen to first: the brass (Fanfare
   Trumpets, Soaring Horn, Heroic Horns: modelled brass is the hardest), the Pastoral Oboe,
   the Glass Harp and Glass Harmonica Pad (rubbed glass is modelled with bowed modes), the
   Grand Organ, and the five that play your recordings (guitar twice, kalimba, violin
   pizzicato and sustain, the Minilogue soft pad).
4. **Hold a chord and travel** (click places, run a journey preset): the chord carries on in
   each new instrument, no clicks, nothing left hanging after you let go. **Lock instrument**:
   only the space moves.
5. **Your keyboard**: velocity, sustain pedal, pitch bend (a whole tone), mod wheel (vibrato;
   tremolo on struck and plucked instruments), and the panel's own wheels, pedal and keys.

### Known issues and decisions to revisit

- **The recordings in CI need the `SAMPLES_TOKEN` secret** (added by the developer during
  this session, after the first run). If a run's log ever shows "Samples token missing" or
  "Samples fetch failed" (an expired token), that package's recorded instruments play their
  modelled stand-ins and the panel says "synthesised (recordings not found)".
- **Nothing was heard or hosted on a Mac.** The Linux suite drives the real processBlock;
  the macOS job builds, validates both plug-ins with auval and the AU harness (the instrument
  played over MIDI on all three hosting routes: new code, first compiled by that job).
- **DUNGEON CRAWLER's instrument edition** did not exist yet, so nothing of it was copied;
  if it appears later, its integration can be compared with this one.
- **Prism Hall's shimmer** is WORLDBUILDER's algorithmic octave shimmer, not your Astral
  Shimmer capture (see above).
- **Recorded ranges**: the kalimba covers C4 to E6, the violin's sustain only its open
  strings (G3, D4, A4, E5: other notes are pitched up to a few semitones from them, the
  modelled section underneath carries the body), the pizzicato G3 to about A#5, the guitar
  E2 to A#5. Outside, the nearest note is resampled (in tune, duller far away).
- **Cloud Steps' glissando** climbs *after* each note (a run above it): a run before the
  note would delay what you play.
- The closest pair of instruments overall (not siblings) is Airy Flute / Breathy Wind Flute;
  they differ mainly in breath. Worth a listen.

## Recordings update: voice, banjo, piano, col legno, Minilogue; the captures out of the repository (2026-11-02)

New recordings arrived in `AdrianAbbas/Samples` after the instrument edition: the
developer's voice, a re-recorded banjo, two djembes, and (during this session) an 88-key
piano with its own `notes.txt` section. All of them are prepared in CI as before (the
`SAMPLES_TOKEN` secret, `scripts/ci-fetch-samples.sh`, never committed here).

### What the Samples repository holds (checked, not assumed)

- **No folders.** The brief mentioned `01-voice/notes.txt`; there is no `01-voice/` folder
  and no second `notes.txt`: every recording is still in the root (now mostly 24-bit
  44.1 kHz WAVs) and the one `notes.txt` has no voice section. Its new sections: the banjo
  (re-recorded through the interface; the old MacBook-microphone banjo files are gone), the
  djembes (template text, takes "3 per dynamic") and `[02-piano]` (88 keys A0-C8, fixed
  gain, short soft/medium/full sweeps and a long sweep, one take per key). The tool now
  finds a recording in any folder anyway (by its path or its name), so a later move into
  `01-voice/` etc. needs no manifest change.
- **The voice is named an octave below what was sung.** Every voice file was measured
  (YIN, then an octave check on the spectrum: no energy at half the candidate or at 1.5
  times it, every harmonic of the candidate present). The brief's example ("the '-E2'
  drones are actually on G1") does not match the audio: nothing is near G1 (49 Hz).

| File (name says) | Sounds | Notes |
|---|---|---|
| ooh / aah / ee / oh / mmm sweeps, chest "E2-A#3" | **E3 G3 A3 C#4 E4 G4 A#4** | the third note is A, not A#; most notes 10-45 cents flat |
| ooh / aah falsetto "C#4-E4" | **C#5 (C5 +45 for aah), E5** | |
| choir stacks "E2" (ooh, aah) | **E3** | 5 takes each |
| choir stacks "A2" (ooh, aah) | **A3** (ooh), **G#3 +40 = A3 -60** (aah) | ooh had a 6th, weaker take: not used |
| choir stack "E3" (ooh), (aah) | **D#4 +70 = E4 -30** (ooh), **E4** (aah) | |
| ooh choir "falsetto E4" | **E5** | |
| mmm choir "A2" | **A2** (the only voice file named right) | `_1`: two sliding hums near E3, not used |
| "low drone G1" (ooo, mmm) | **G2** | |
| "low drone A2" (aah) | **A3 -30** | |
| banjo sweeps | D3 F3 G#3 B3 D4 F4 G#4 B4 D5 F5 G#5, as named | 10-46 cents flat (open G tuned low) |
| banjo 5th string | G4 x 9 (3 soft, 3 medium, 3 hard), -11 to -16 cents | |
| piano (4 sweeps) | A0-C8 chromatic, as described | within 15 cents (the top octave a little sharp) |

Every note is mapped where it sounds and tuned by its detected pitch (the held pitch of its
loop for sustained notes); the CI log lists each renamed note.

### Sample prep (`tools/sample-prep`, `prepare.py` + `manifest.json`)

New in the tool (see its README): recordings in folders; WAVs at any rate (resampled);
`pitchFrom: audio` for the voice (notes sliced by the gaps between them, since a voice swells
and breathes inside a note; pitch on each note's steady middle; one label per stack or
drone file, a take more than a semitone off the others not used); `pitchFrom: track` for the
piano's chromatic sweeps (each onset looked for near the next key; re-onsets inside a ringing
bass note and repeated keys dropped: one take per key, as the notes say); layers that share a
name (two "ee full" takes: round-robins); `into` (the falsetto plays in both velocity layers);
`pick` (the banjo's 5th-string file holds its three dynamics in order); `gainGroup` (all the
voice sets keep their relative levels); unpitched `texture` sets (the breaths), also copied
into `fx/` for the effect's bundle; `ship: false` (prepared and reported, not loaded or
bundled). The CI log now prints `Prepared: N notes (set)` for every set and the total.

Prepared and shipped (534 notes, 44 MB of FLAC, 123.6 MB in memory, loaded in about a
second): guitar 45, kalimba 51, violin sustain 11, pizzicato 42, col legno 11, **banjo 42**
(11 notes x 3 dynamics + G4 x 3 per dynamic), **piano long notes 88**, **voice** ooh 18, oh 7,
aah 18, ee 21, mmm 14 (soft and full chest sweeps, the falsetto in both layers, a second "ee
full" take), **choir stacks** ooh 20, aah 15, mmm 5, **drones** 4, **breaths** 11, Minilogue
pulse (long) 37, soft pad short 37 and long 37. Prepared but not shipped (no instrument
plays them): the piano's short-note sweeps (261 notes) and the harsh square, cyberpunk,
static-y bass and pulse (short) patches.

### Which instruments use which recordings now

| Instrument (place) | Recordings | Stand-in without them |
|---|---|---|
| **Orchestral Chord Pad** (Throne of Light) | voice: the soft ooh/aah choir (vowel between them, four singers, the soft takes and stacks) on the **Voices** macro | the formant choir |
| **Angelic Choir** (Sanctum) | voice: soft takes and falsetto ("ooh"), ooh stacks (four singers); above E5 the recordings go on pitched up (formants rising with them: younger, not lower) and fade over an octave into the formant choir; light shimmer, bright air | the formant choir |
| **Celesta** (Diamond Vault) | piano: the long notes, high-passed under the bars (the hammered body) | the modelled bars alone |
| **Leaf Halls, Sunlit Meadow** | guitar (unchanged) | modelled lute / guitar |
| **Butterfly Glade** | kalimba (unchanged) | the tine |
| **Radiant Choir** (Choir Loft) | voice: full chest and falsetto ("aah"), aah stacks (five singers), swelling in; the formant choir above E5; synthesised basses and sopranos | the formant choir |
| **Cloud Pad** (Cloud Bridge) | Minilogue soft pad (long) and, quietly inside it, the pulsating patch (long) | the analog pad |
| **Shimmering Pluck** (Sky Harbour) | **banjo** (3 dynamics, the 5th string's three hits as round-robins), high-passed (no drum boom, no low strings), its ring shortened, the Minilogue soft pad's short hits as a synth body, chorus and a rising shimmer: a shimmer, not a twang | the analog pluck it had |
| **Bouncy Pizzicato** (Mushroom Ring) | violin pizzicato and now its **col legno** (the bow's wood: a woody tick on each note) | modelled pluck |
| **Warm Strings** (Golden Nest) | violin open-string sustain (unchanged) | modelled violins |
| **Epic Choir and Brass** (Summit of Winds) | voice: full chest "aah" with the aah stacks (seven singers), an upper octave from the falsetto/soft takes (three singers), the **G2/A3 drones** an octave under the brass; the choir and brass swell in together | the formant choir |
| Windswept Isle's **Atmosphere** (both plug-ins) | the slow breaths, pitched up 2-3 semitones, only their air (1.8-7 kHz), very soft, one of the place's three sounds | a synthesised gust |

**Vowel and Ensemble** (two new instrument parameters, added last so older sessions load
unchanged; on the panel between Character and Level, dimmed on non-voice instruments):
Vowel moves two vowels either way along mmm-ooh-oh-aah-ee (equal-power crossfades between
neighbouring vowel sweeps; the formant stand-ins follow it too); Ensemble goes from one
voice (0 %) through the instrument's own choir (50 %) to eight singers (100 %): each
singer a different take of the 5-take stacks where a stack lies within a fifth of the note
(else the sweeps again), its own detune (up to 12 cents), slow drift, timing offset (up to
45 ms) and place in the stereo field; level held (+3 dB at most for eight). Every weight is
ramped per sample (30 ms), and a vowel turned to while a note sounds joins in its loop:
no clicks (tested). Instruments now have up to eight layers (the Epic choir has seven).

**Not used** (in the manifest's `unused` list with the reasons): the **djembes**: LIGHT
SEEKER has no percussion, and a hand drum in an atmosphere or under a pastoral instrument
would read as rhythm, not as the light world's air; skipped. The banjo's strumming, palm
muting and drum-head knocks, the hummed melody, the grungy "eeeh", the ghostly falsetto
(eerie, unpitched), all whispers and the "shh/hhh/fff/sss" (creepy in this world), the
"mmm" drone (the "ooo" drone of the same pitch is used), the effect captures and phrases
(the captures make the IRs), and the Minilogue's harsh square, cyberpunk and static-y bass
(not bright or majestic).

### The captures: out of the repository, made in CI

`assets/irs/` (the three IRs derived from the pedal captures, and their measurement JSONs)
is no longer in the repository (the deletion landed in `3e767d1`, one commit earlier than
its message says; history not rewritten, as asked). `tools/captures/ci-build-irs.sh` lays
the captures out as run_all.py expects (symlinks; the Samples checkout untouched), runs
`run_all.py` and `export_irs.py` (about 50 s; the result is bit-identical to the files that
were committed) while the Samples checkout exists in `ci-fetch-samples.sh`, and CMake
compiles them into both plug-ins when `LS_IRS_DIR` names them (`LS_HAVE_CAPTURED_IRS`).
Without them the Vault's three captured kinds are generated fallback halls of the same
character (long bright shimmer hall; bright hall with far echoes; lush hall), and the four
capture-tuned effect checks on the Nave of Light and the Rose Garden (strength at 100 % and
50 %, recognisability, trims; and the AU harness pair) are reported, not gated, in such a
build. With the token, CI now **requires** both the captures and the recordings
(`LS_REQUIRE_CAPTURES`, `LS_REQUIRE_SAMPLES` in the smoke test), so a broken pipeline
cannot hide behind the fallbacks. Other capture-derived files: none left (`docs/` has the
measurements as prose, the code a few measured numbers such as RT60s in comments and the
calibrated trims; no audio or IR data).

### Tests

- **recorded** (new group, 93 checks, with the recordings): every changed instrument plays
  from MIDI across its range (a note every 4 semitones, e.g. the Angelic Choir E3-E6 through
  the pitch-shifted top); the played pitch class strongest on every note; **in tune within
  25 cents on every note** (harmonics' peaks); sustained ones hold a chord 8 s with **no
  click at the loop points** (a second-difference spike detector) and steady level;
  **level-matched with their siblings** (within 3 dB; the `instruments` group keeps the
  ±2 dB target); Vowel changes the colour at the same level, Ensemble widens (L/R
  correlation 1.00 -> about 0.6) without getting much louder; turning both under a held
  chord, no click; the breath atmosphere plays the recordings; without them every changed
  instrument's stand-in plays in tune and the breath is a gust.
- **instrument / instruments** (342): the instrument now has nine parameters; distinct from
  siblings: two new close pairs on the first run, Grand Organ / Radiant Choir (2.27) and
  Heroic Horns / Epic Choir and Brass (2.25), almost all of it in the envelope (third-octave
  colours of a choir chord and an organ chord nearly coincide). Fixed by what a choir does:
  the Radiant Choir swells in (0.95 s) and dies away on its recorded tail, the Epic choir
  and its brass swell in together (the Eyrie's horns strike), the Epic choir gets its upper
  octave and more air. Trims recalibrated (with and without the recordings).
- **The whole suite** (Linux, eight shards): with the recordings and the captured spaces
  (the CI configuration) **1807 passed, 0 failed**; without either (a build with no token)
  1760 passed, 0 failed after one real fix: the fallback hall reported the captured hall's
  length (its tail was cut). CPU (report only): four notes in each place 8.2 % on average,
  the worst 14.9 % (Epic Choir and Brass: up to 7 x 8 sample reads per note).
- **Listening render** (CI: `LightSeeker-linux-test-logs` → `changed-instruments/`,
  `LightSeekerTests --listen-changed dir`): the same melody (E4 G4 C5 B4 A4 G4 F4 E4) and
  chord progression (C, Am, F, G, C) on each of the eight changed instruments, dry, then in
  its place: 16 files. Here: loudest 400 ms between -15.3 and -18.7 dBFS, peaks -7.1 to
  -1.4 dBFS.

### Notes that sound weak or out of tune (to re-record)

All of these play in tune (the sampler corrects them by their measured pitch), but the
further a note is off, the more the correction colours it and the more the ensemble's
takes disagree. Worth re-recording, worst first:

- **Choir stacks**: ooh "E3" (sung around E4 -30: two takes +78 and +88 cents from D#4, i.e.
  the stack wavers between D#4 and E4), aah "A2" (sung near A3 -60: takes from +63 to +79
  cents over G#3), ooh "falsetto E4" take 3 (-56 cents), aah "E3" takes 4-5 (-40, -43),
  mmm "A2" take 2 (-38). ooh "A2" had a sixth, weak take (not used); mmm "A2_1" slides (not
  used).
- **Sweeps** 35-45 cents off: ooh soft C#4 (-42), ooh full G3 (-41), A3 (-41), C#4 (-35),
  aah falsetto C5 (+45) and E5 (-41), ee full A3 (-43). Loops that match poorly (crossfaded,
  no click, but the vibrato's phase turns): oh soft A#4, aah full G4, aah falsetto C5, ee
  full (2nd take) E3 and A3.
- **mmm**: the "full" hums are 5-6 dB *quieter* than the "soft" ones (kept as named; each is
  levelled to the others in the morph, so only the colour differs).
- **Drones**: the aah "A2" drone is A3 -39 cents (both takes): re-sing on pitch if it should
  sit under the brass exactly.
- **Banjo**: D3 is 46 cents flat in the soft and medium sweeps (the 4th string), G#4 hard
  -35; the soft and medium sweeps have a higher noise floor (-68 and -70 dBFS) than the hard
  sweep and the 5th-string file (-85): hiss, partly taken off by the gentle noise reduction.
  The dynamics were not in loudness order on most notes (the soft takes often louder than
  the medium), so the layers were set 9/4/0 dB apart per note, as for the guitar.
- **Piano** (long notes): the top notes G#6, G#7, A7, A#7, C8 are 10-15 dB quieter than the
  rest, A7 +35 cents. (The short soft sweep's top notes are up to 30 dB down, but that
  sweep is not used.)
- **Minilogue pulse** (long): its pulsing makes every loop a loose match (crossfaded).

### Known issues and decisions to revisit

- **Nothing was heard.** As before, everything was designed by measurement here; listen
  first to the three choirs (are the vowels and the ensemble convincing? is the Angelic
  Choir's pitch-shifted top angelic or chipmunk-like above C6?), the Shimmering Pluck (does
  the banjo still read as a banjo?) and the Celesta's piano body.
- The piano is used only under the Celesta: no place calls for a piano as such. Its short
  sweeps are prepared (and checked) but not shipped; a piano instrument would be a new
  place's job.
- Memory: the recordings now take 123.6 MB (82.7 before) in every process that loads the
  instrument.
