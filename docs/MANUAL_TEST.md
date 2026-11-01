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
