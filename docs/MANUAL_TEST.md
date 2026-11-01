# LIGHT SEEKER: manual test in GarageBand (MacBook Air)

Each milestone adds its section. Install the `LightSeeker-macOS-Universal` artifact with
`bash install-mac.sh` first.

## M1: setup

1. The installer ends with "LIGHT SEEKER is installed and validated".
2. GarageBand → a track → an Audio Effect slot → Audio Units → **Veil Audio → LIGHT SEEKER**.
   If DUNGEON CRAWLER is installed, it is still listed beside it and still works.
3. The window opens on the map (Milestone 1: a simple bright placeholder map; the painted
   one is Milestone 3). Click a landmark: the sound travels there and the view zooms in.
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

1. **The map on opening:** a bright painterly world from above at an angle (meadows, a
   river, the sea in the south-west, snowy peaks, cloud banks), ten luminous landmarks with
   small serif names; the current one in gold with a diamond. Hover: a ring and the name.
2. **A click on a landmark:** a golden path, then the zoom into the place with parallax (the
   layers move at different speeds); "‹ Map" zooms back out.
3. **Every place** (31): its own painting, rings for the location's places and the list top
   right; a click on a ring crossfades to that place.
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
