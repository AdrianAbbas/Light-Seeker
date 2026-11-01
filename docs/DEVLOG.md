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

- [ ] 2a. Captures from `AdrianAbbas/Samples`: inventory, quality check, IRs and measurements (LIMINAL's tools); radiant capture-based spaces in the Vault
- [ ] 2b. Signature module: 31 light signatures (new primitives: pitch spiral, stereo prism, hue shift, auto-pan, swirl, rising delays, harp/bell resonators, glitter)
- Celestial Citadel
  - [ ] Gates of Dawn: fanfare-like bright early reflections
  - [ ] Hall of Mirrors: multiplying stereo reflections
  - [ ] Throne of Light: regal rich chorus + gold shimmer
  - [ ] Spire Summit: huge open air, long airy echoes
- Sky Temple
  - [ ] Cloud Steps: rising pitch-spiral shimmer
  - [ ] Sanctum: soft choir bloom
  - [ ] Bell Garden: tuned wind-chime/bell resonators
- Elven Sanctuary
  - [ ] Moonwell: glassy water-surface shimmer
  - [ ] Leaf Halls: warm wooden resonance + gentle ensemble chorus
  - [ ] Starlit Canopy: sparkling high harmonics
- Crystal Palace
  - [ ] Prism Hall: octave shimmer split across the stereo field (spectral prism)
  - [ ] Mirror Lake: calm reflective long tail
  - [ ] Diamond Vault: bright modal crystal ringing
- Blossom Meadow
  - [ ] Sunlit Meadow: soft open air + warm saturation
  - [ ] Butterfly Glade: fluttering light tremolo/auto-pan
  - [ ] Wildflower Hill: long gentle echoes over the valley
- Golden Cathedral
  - [ ] Nave of Light: the longest, most radiant reverb in the plug-in
  - [ ] Choir Loft: angelic choir layer
  - [ ] Stained Glass Chapel: colour-shifting shimmer (slow spectral hue modulation)
- Floating Isles
  - [ ] Cloud Bridge: swirling stereo motion
  - [ ] Windswept Isle: airy wind-filtered space
  - [ ] Sky Harbour: soaring delays that rise in pitch
- Royal Gardens
  - [ ] Rose Garden: soft lush chorus
  - [ ] Fountain Court: sparkling water-like modulation
  - [ ] Marble Colonnade: rhythmic slap echoes between columns
- Fairy Glade
  - [ ] Mushroom Ring: bouncy pitch-up sparkle
  - [ ] Firefly Hollow: twinkling granular glitter
  - [ ] Fairy Court: tiny bright room + harp-like resonance
- Dragon's Peak
  - [ ] Eyrie: vast open echoes off cliffs
  - [ ] Golden Nest: warm golden harmonic richness
  - [ ] Summit of Winds: huge heroic wide reverb with a soaring octave layer
- [ ] 2c. Strength check (every place clearly different from dry; weakest ten listed), distinctness over all pairs (closest listed), musicality, trims recalibrated
- [ ] 2d. Listening render (same guitar + synth phrase, dry vs every place) in the test-logs artifact; DEVLOG, MANUAL_TEST; push without `[skip ci]`

**Milestone 3: visuals**

- [ ] 3a. Light restyle of the controls (ivory/pearl panels, gold value tracks, dark text) and the map overlay (gold marker, serif labels)
- [ ] 3b. Painted pipeline relit for daylight (sun + sky, clouds, god rays, bright haze, marble and gold, flowers); life: drifting clouds, light motes, glimmer, birds
- [ ] 3c. The bright painterly map
- [ ] 3d. Scenes: Celestial Citadel (4)
- [ ] 3e. Scenes: Sky Temple, Elven Sanctuary, Crystal Palace (9)
- [ ] 3f. Scenes: Blossom Meadow, Golden Cathedral, Floating Isles (9)
- [ ] 3g. Scenes: Royal Gardens, Fairy Glade, Dragon's Peak (9)
- [ ] 3h. Zoom navigation + transitions checked, snapshots of everything in `docs/snapshots/`, tests; push without `[skip ci]`

**Milestone 4: Atmosphere layer (light)**

- [ ] 4a. Synthesised light sounds (soft wind, distant bells, birdsong-like chirps, fountain shimmer, chimes, harp glints), per place, off by default, sparse, never masking
- [ ] 4b. Tests, DEVLOG; push without `[skip ci]`

**Milestone 5: presets and polish**

- [ ] 5a. 20+ factory presets (favourite spots + journeys)
- [ ] 5b. Manual navigation overrides journeys, every panel closable (navigation tests on the light world)
- [ ] 5c. README, `docs/MANUAL_TEST.md`, install docs
- [ ] 5d. Final QA (clicks on every travel, levels, distinctness, editor CPU) and the DEVLOG handoff (what to listen for and look at in GarageBand); push without `[skip ci]`

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
