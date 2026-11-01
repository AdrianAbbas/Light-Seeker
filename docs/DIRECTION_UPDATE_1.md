# LIGHT SEEKER — Direction Update 1: Instrument edition

Read after LIGHT_SEEKER_BRIEF.md. This file overrides it where they conflict.

## 0. The change

LIGHT SEEKER becomes **two plugins built from this same repository**:

1. **LIGHT SEEKER** (the main product): a **software instrument** (AU type `aumu`, new subtype, e.g. `Lski`; VST3 instrument), played from MIDI. **Every sub-location has its own unique instrument**, played *inside* that sub-location's space and effects.
2. **LIGHT SEEKER FX**: the effect plugin built so far (`aufx`), renamed, behaviour unchanged.

Both share the place engine, map, art, navigation, effects/captures, presets and tests; CI builds, validates (auval + host harness for both) and packages both; the install script installs both. Must not clash with DUNGEON CRAWLER's plugins.

**Scope (important):** this is a **normal, single-instrument plugin** — one instrument playing at a time per plugin instance, one instance per track, played from the host's MIDI/piano roll or a keyboard, like a typical software instrument. **Not** a WORLDBUILDER-style multi-instrument workstation: no pattern editor, no arrangement, no generated parts, no several instruments playing at once. From WORLDBUILDER, reuse only the **sound engine** (sampler/synthesis) and its **Instrument-mode** MIDI playing and keyboard view.

## 1. How the instrument works

Exactly as DUNGEON CRAWLER's instrument edition (see `AdrianAbbas/Dungeon-Crawler` → `DUNGEON_CRAWLER_DIRECTION_UPDATE_3.md`, read access only):
- choosing a sub-location selects its instrument and its space; travel morphs the space; held notes survive travel; **Lock instrument** toggle;
- light-styled instrument panel (ivory/pearl, gold tracks): macros, Intensity, on-screen keyboard, bend/mod/sustain;
- velocity, sustain, pitch bend, mod wheel, sensible polyphony, click-free; no forced glides; instrument presets plus journeys.

## 2. Engine — reuse

Reuse **WORLDBUILDER's instrument engine** (sampler, synthesis models, Instrument-mode MIDI and keyboard view) — copy from `AdrianAbbas/Worldbuilder`, never modify it. If DUNGEON CRAWLER's instrument edition already exists, reuse its integration code too (copy; never modify Dungeon-Crawler). Use the developer's recordings in the private `AdrianAbbas/Samples` repo where they fit (guitar, violin, kalimba, banjo, Minilogue, captures), processed bright and majestic.

## 3. Instruments (one per sub-location; each location a themed family)

Uplifting, radiant, majestic; each clearly distinct; good dry and in its space. Adjust names to the sub-locations that exist in the code.

| Location (family theme) | Sub-location → instrument |
|---|---|
| **Celestial Citadel** (radiant brass & strings) | Gates of Dawn → bright **fanfare trumpets** · Hall of Mirrors → shimmering **string ensemble** · Throne of Light → majestic **orchestral chord pad** · Spire Summit → soaring solo **horn** |
| **Sky Temple** (bells & voices) | Cloud Steps → rising **harp glissando** instrument · Sanctum → **angelic choir** · Bell Garden → **wind chimes / tuned bells** |
| **Elven Sanctuary** (harps & flutes) | Moonwell → **glass harp** · Leaf Halls → **elven lyre** (guitar samples, bright) · Starlit Canopy → airy **flute** |
| **Crystal Palace** (crystal) | Prism Hall → **crystal bells** (+ shimmer capture) · Mirror Lake → **glass harmonica** pad · Diamond Vault → **celesta** |
| **Blossom Meadow** (pastoral) | Sunlit Meadow → warm **acoustic guitar** (guitar samples) · Butterfly Glade → bouncy **kalimba** (kalimba samples) · Wildflower Hill → pastoral **oboe / recorder** |
| **Golden Cathedral** (sacred, golden) | Nave of Light → bright grand **organ** · Choir Loft → radiant **choir** · Stained Glass Chapel → **celesta + harp** |
| **Floating Isles** (airy) | Cloud Bridge → soft **cloud pad** (Minilogue soft pad) · Windswept Isle → breathy **wind flute** · Sky Harbour → shimmering **pluck** |
| **Royal Gardens** (courtly) | Rose Garden → concert **harp** · Fountain Court → **music box / celesta** · Marble Colonnade → **harpsichord** |
| **Fairy Glade** (twinkly) | Mushroom Ring → bouncy **pizzicato** (violin pizz samples) · Firefly Hollow → twinkling **glockenspiel** · Fairy Court → tiny **fairy harp** |
| **Dragon's Peak** (epic, heroic) | Eyrie → heroic **horns** · Golden Nest → warm **strings** (violin samples) · Summit of Winds → epic **choir + brass** |

## 4. Milestones (one long run, in order)

1. Finish any remaining Light Seeker brief milestones still unticked in the DEVLOG (do not skip them).
2. **Two-plugin setup:** instrument + FX targets, identities, CI for both, install script, README.
3. **Instrument engine:** port from WORLDBUILDER (and DUNGEON CRAWLER's integration if present), panel, Lock instrument, travel with held notes.
4. **Instruments, first half:** Celestial Citadel, Sky Temple, Elven Sanctuary, Crystal Palace, Blossom Meadow.
5. **Instruments, second half:** Golden Cathedral, Floating Isles, Royal Gardens, Fairy Glade, Dragon's Peak.
6. **Presets and polish:** at least 30 instrument presets; tests (every instrument plays from MIDI, in tune, distinct from siblings, level-matched; no hung notes during travel; both plugins validate); listening render; DEVLOG handoff.

**Rules:** never modify the Dungeon-Crawler, Worldbuilder or LIMINAL repos (copy what you need). DEVLOG checklist, resume after a limit. Push after each part with `[skip ci]`, without after each whole milestone. Small metric misses after 2 attempts → report-only; never leave the build broken. Don't wait for CI; don't ask questions. `runs-on: macos-15`. Never put `: ` inside an unquoted YAML step name. Editor double-buffered and cheap. No brand names in the product.
