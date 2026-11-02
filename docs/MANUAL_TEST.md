# LIGHT SEEKER: manual test in GarageBand (MacBook Air)

Each milestone adds its section. Install the `LightSeeker-macOS-Universal` artifact with
`bash install-mac.sh` first. Since Direction Update 1 the download holds two plug-ins:
**LIGHT SEEKER** (the instrument, section "Instrument edition" at the end) and **LIGHT SEEKER
FX** (the effect: sections M1 to M5, which name it LIGHT SEEKER as it was then called).

## M1: setup

1. The installer ends with "LIGHT SEEKER is installed and validated".
2. GarageBand → a track → an Audio Effect slot → Audio Units → **Veil Audio → LIGHT SEEKER**.
   If DUNGEON CRAWLER is installed, it is still listed beside it and still works.
3. The window opens on the painted map. Click a landmark: the sound travels there and the
   view zooms in.
4. Play guitar or a synth: every place sounds clearly different from the dry sound.
5. Bypass, Mix 0 % and Intensity 0 % give the dry sound.

## M2: sound design (listen on guitar, synth, piano and voice)

Intensity 100 %, Mix 100 % (the defaults). Every place should sound obviously somewhere
else, and at 50 % still clearly. Levels should not jump between places. Listen for each
place's **signature**:

| Place | Listen for |
|---|---|
| Celestial Citadel · Gates of Dawn | bright, brassy first reflections thrown left and right, like a fanfare, in a big marble hall |
| Hall of Mirrors | reflections multiplying outwards across the stereo field |
| Throne of Light | a regal, rich chorus with a halo of gold an octave up |
| Spire Summit | thin, bright air; long airy echoes falling away |
| Sky Temple · Cloud Steps | an endless spiral of shimmer climbing, drifting from left to right |
| Sanctum | a soft sung "ah" blooming under the instrument |
| Bell Garden | tuned chimes and a bell ringing with each note |
| Elven Sanctuary · Moonwell | a glassy shimmer moving like light on water |
| Leaf Halls | warm wooden body, a gentle doubling |
| Starlit Canopy | twinkling high harmonics in a wide starry space |
| Crystal Palace · Prism Hall | an octave shimmer split into colours, each colour at its own place left to right, slowly turning |
| Mirror Lake | a calm, long tail; each phrase mirrored back in a soft reverse swell |
| Diamond Vault | bright crystal ringing and a high sparkle on every attack |
| Blossom Meadow · Sunlit Meadow | warm, sunny saturation in soft open air |
| Butterfly Glade | the sound flutters from side to side |
| Wildflower Hill | gentle echoes coming back over the valley |
| Golden Cathedral · Nave of Light | the longest, most radiant reverb (the developer's captured shimmer hall) |
| Choir Loft | an angelic choir an octave and a fifth above |
| Stained Glass Chapel | a shimmer slowly changing colour |
| Floating Isles · Cloud Bridge | the whole image swirling around you |
| Windswept Isle | thin, wind-blown, gusting |
| Sky Harbour | echoes that soar a fifth higher as they fly |
| Royal Gardens · Rose Garden | a soft, lush, wide chorus (the developer's captured chorus hall) |
| Fountain Court | sparkling, water-like modulation |
| Marble Colonnade | rhythmic slaps left-right between columns |
| Fairy Glade · Mushroom Ring | every attack bounces up in sparkles, left and right |
| Firefly Hollow | tiny grains of light blinking on and off around you |
| Fairy Court | a tiny bright room and harp strings ringing in sympathy |
| Dragon's Peak · Eyrie | vast echoes thrown back off the cliffs |
| Golden Nest | warm golden harmonic richness |
| Summit of Winds | a huge, wide, heroic reverb with an octave soaring above |

Check especially (report-only in the tests, see DEVLOG): the long-tailed places with a
voice or piano (Nave of Light, Spire Summit, Summit of Winds, Eyrie, Mirror Lake) stay
usable in a mix; Firefly Hollow, Stained Glass Chapel and Starlit Canopy are not too
fizzy on a voice; Diamond Vault and Starlit Canopy are obvious enough.

## M3: visuals (look at)

1. **The map on opening:** a bright painterly world from above at an angle (a patchwork of
   fields and woods, a river from the snowy mountains in the north to the sea in the
   south-west, cloud banks), ten landmarks with
   small serif names; the current one in gold with a diamond. Hover: a ring and the name.
2. **A click on a landmark:** a golden path, then the zoom into the place with parallax (the
   layers move at different speeds); "‹ Map" zooms back out.
3. **Every place** (31): its own painting, rings for the location's places and the list top
   right; a click on a ring crossfades to that place. Judge the detail against the Gates of
   Dawn (the approved prototype): grass blades and flowers, leaves and blossom, the gothic
   towers' windows and pinnacles, cracked and stained cliff stone, leaded stained glass.
   Compare with `docs/snapshots/` (and `contact-sheet.jpg`); note any scene that looks
   wrong, flat or fake.
4. **Life, gently:** light motes drifting up from glowing spots, glints twinkling on gold,
   crystal and water, white mist drifting low, the sun's glow breathing; now and then a few
   doves cross the sky (exterior scenes and the map). Nothing flickers; the MacBook Air stays
   cool (Activity Monitor: GarageBand's CPU barely changes with the window open). ⚙ → Eco
   stops all animation.
5. **The controls:** pearl and ivory panel, dark sepia text, gold value tracks and caps'
   centre lines, readable values under every fader; menus and tooltips pearl.

## M4: Atmosphere (listen for)

Off by default. Raise **Atmosphere** to 50–100 % (and Journey panel → Density): sparse,
quiet, synthesised sounds of the place: soft wind (Spire Summit, the isles, Dragon's Peak),
distant bells (the Citadel, the Sky Temple, the Cathedral), birdsong-like chirps (the
meadow, the gardens), fountain shimmer (the Moonwell, the Fountain Court), chimes and harp
glints (the Crystal Palace, the Fairy Glade), leaves and a brook. Never busy, never a riser;
they step back when you play loud; the same seed gives the same sounds at the same bars.

## M5: presets and polish

1. **Presets:** the preset name ▾ → Favourite spots / Journeys: 27 factory presets. ◀ ▶ step
   through them. Each sounds as named; "Init / No preset" returns everything to default.
2. **A click always wins:** load "Grand Citadel Tour" (a journey: the badge says "Journey
   active"), press play, then click "‹ Map" and another landmark: you go there at once, the
   note says "Journey stopped", the name shows a star ("Grand Citadel Tour *"). Load
   "Meadow Wander" (roaming) and click each Blossom Meadow place in the list: each one stays.
   The badge's **Stop** ends a journey where it is.
3. **Every panel closes four ways:** the Journey panel and the Advanced drawer close with ✕,
   a second click on their button, Esc (when the window has focus) and a click outside them;
   while open they never cover their own button, Journey, Advanced or ▾.
4. **Workflow:** A/B and A→B, undo/redo of your edits, Save as your preset… (appears under
   Your presets), window size 75–200 % (⚙ or the corner), Eco/Standard/High.
5. **State:** save the GarageBand project, close and reopen it: the place, every control,
   the route and the window size come back; duplicate the track: the copy is identical.

## Release check (15 minutes)

- [ ] Install from a fresh browser download with `install-mac.sh`; DUNGEON CRAWLER (if
      installed) still loads.
- [ ] GarageBand: insert on a guitar, a synth, a piano and a voice track; play each through
      a few places of every location: obvious, beautiful, no loudness jumps, no clicks when
      travelling, tails ring on.
- [ ] Intensity 50 %: still clearly audible; 0 %, Mix 0 % and Bypass: the dry sound.
- [ ] The map, every location's scene, the zooms; the life is gentle; Eco stops it.
- [ ] Atmosphere at 60 %: sparse and quiet.
- [ ] A journey preset with the transport playing, a click on another place, Stop.
- [ ] Save, reopen, duplicate.

## Instrument edition (Direction Update 1)

Install with `bash install-mac.sh`: it should end with "LIGHT SEEKER and LIGHT SEEKER FX are
installed and validated" (two auval PASS lines).

1. **Both plug-ins.** GarageBand: a new Software Instrument track → the instrument slot →
   Audio Units → Veil Audio → **LIGHT SEEKER**. On an audio track, an Audio Effect slot →
   Veil Audio → **LIGHT SEEKER FX**: it behaves exactly as before (an old project that used
   LIGHT SEEKER finds LIGHT SEEKER FX). DUNGEON CRAWLER, if installed, still loads.
2. **The window**: the map with the instrument panel underneath (pearl and ivory, gold
   tracks). It must not flicker or slow GarageBand when you play.
3. **Play** a keyboard or the piano roll (or click the panel's keyboard): the arrival place
   (Celestial Citadel, Gates of Dawn) plays its fanfare trumpets in its space. The keys light
   up with the notes you play.
4. **Every place's instrument.** Click through all 31 places and play the same phrase and a
   chord in each. Each should sound uplifting and clearly different from its siblings, at
   about the same loudness, good dry (Intensity 0 % on the panel) and in its space:

   | Place | Instrument | Listen for |
   |---|---|---|
   | Gates of Dawn | Fanfare Trumpets | bright, brassy, louder and brighter when you play harder |
   | Hall of Mirrors | Shimmering Strings | a bowed section with a shimmer an octave above |
   | Throne of Light | Orchestral Chord Pad | strings, soft horns and voices swelling in slowly |
   | Spire Summit | Soaring Horn | one dark, round horn with a late vibrato |
   | Cloud Steps | Rising Harp Glissando | every note climbs on in a quick harp run (Character: how far) |
   | Sanctum | Angelic Choir | a soft "oo" choir, a descant above |
   | Bell Garden | Wind Chimes and Bells | tuned bells ringing long, chimes above them |
   | Moonwell | Glass Harp | rubbed wine glasses: soft onset, pure and glassy |
   | Leaf Halls | Elven Lyre | your guitar recordings made bright and shimmering |
   | Starlit Canopy | Airy Flute | a clean flute (a piccolo with Character up) |
   | Prism Hall | Crystal Bells | bright crystal bells with a rising shimmer |
   | Mirror Lake | Glass Harmonica Pad | almost pure rubbed bowls, wide and slow |
   | Diamond Vault | Celesta | a celesta's bell-like bars |
   | Sunlit Meadow | Warm Acoustic Guitar | your guitar recordings, warm |
   | Butterfly Glade | Bouncy Kalimba | your kalimba recordings |
   | Wildflower Hill | Pastoral Oboe | a reedy, nasal oboe |
   | Nave of Light | Grand Organ | a full organ with a breath of wind; a trumpet stop with Character up |
   | Choir Loft | Radiant Choir | a bright "ah" choir with basses and sopranos |
   | Stained Glass Chapel | Celesta and Harp | the two together |
   | Cloud Bridge | Cloud Pad | your Minilogue soft pad recordings, hazy and wide |
   | Windswept Isle | Breathy Wind Flute | a lot of breath and air |
   | Sky Harbour | Shimmering Pluck | a plucked synth sweeping open, a shimmer rising |
   | Rose Garden | Concert Harp | a concert harp, ringing |
   | Fountain Court | Music Box | high tines like a music box |
   | Marble Colonnade | Harpsichord | quill-plucked, the same at any velocity, stops with the key |
   | Mushroom Ring | Bouncy Pizzicato | your violin pizzicato recordings |
   | Firefly Hollow | Twinkling Glockenspiel | high steel bars twinkling |
   | Fairy Court | Fairy Harp | a tiny, bright harp an octave up |
   | Eyrie | Heroic Horns | a horn section with an octave below |
   | Golden Nest | Warm Strings | your violin recordings over a warm section |
   | Summit of Winds | Epic Choir and Brass | voices and horns together |

   The panel says "recorded" under the instruments that use your recordings; "synthesised
   (recordings not found)" means the build had no access to them (the SAMPLES_TOKEN secret).
5. **Held notes through travel.** Hold a chord and click another location: the space morphs,
   the chord carries on in the new place's instrument without a click, the old instrument's
   notes ring out. Release: nothing keeps sounding (no hung notes). Do the same with a
   journey preset (e.g. "Ascent to the Citadel") running while you play.
6. **Lock instrument.** Tick it, travel: only the space changes. Untick: the place's
   instrument returns. ‹ › and "Instruments" play any instrument here (locked).
7. **Controls.** Velocity, the sustain pedal, the pitch-bend wheel (a whole tone) and the mod
   wheel (vibrato; tremolo on bells and plucked instruments) from your keyboard and on the
   panel. Tone, Attack, Release and Character change the instrument as their names say (the
   Character's name changes with the instrument). Octave shifts the keyboard. On the choirs
   (Angelic, Radiant, Epic) and the Orchestral Chord Pad: Vowel sweeps mmm, ooh, oh, aah, ee
   (turn it while holding a chord: smooth, no click); Ensemble from one singer (0 %) to a
   wide choir of eight (100 %). Both are dimmed on the other instruments.
8. **Presets** (the preset menu): 31 instruments in their places, two dry or gentle ones,
   eight "carried elsewhere" (e.g. "Celesta in the Nave": the celesta in the Nave of Light's
   space, locked), and the journeys.
9. **Save and reopen** the project: the place, the locked instrument and the panel come back.
10. Logic Pro: the same from an AU Instruments slot; four instances on four tracks with
    chords held should stay well within the MacBook Air's CPU.
