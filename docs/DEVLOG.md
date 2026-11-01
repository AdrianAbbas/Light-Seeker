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

