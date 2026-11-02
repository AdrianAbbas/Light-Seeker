# LIGHT SEEKER — Direction Update 2: notes after testing in GarageBand

The sound is excellent now: **no instrument or place sound changes except item 1.** Mostly UI.

1. **Rising Harp Glissando** (Sky Temple → Cloud Steps) sounds badly out of tune. Cause: each
   note's grace-note run climbs a fixed major pentatonic above that note (`Synth.cpp`
   `runSteps`), so the run plays notes outside the key, and chords make every note run its own
   scale and clash. Fix: the glissando only uses notes that fit what is played. The run is built
   from the notes currently held (an upward harp sweep through the held chord tones and their
   octaves, rising *into* the played note rather than away from it). A single held note runs
   only octaves and its fifth. Also check the harp pluck itself is in tune across the range
   (cents report). Keep it a radiant rising harp.
2. **Birds** flying across the screen leave an opaque white rectangle. Birds drawn with proper
   alpha on the scene, the dirty region repainted from the cached scene underneath
   (double-buffered, no white fill), at every window size and in GarageBand. A snapshot test of
   a frame with birds over the scene.
3. **Closing and reopening the window** brings it back exactly as it was left: the same location
   / sub-location view and zoom, the same instrument drawer state, the same panels open. Never
   reset to the world map. The view state is stored in the processor (and in the saved
   session/project state) and restored when the editor is created.
4. **Place names, top right:** after clicking one, hovering over others leaves them highlighted
   after the mouse leaves. Hover clears on mouse exit, the selected place shows a distinct,
   stable "selected" state; every other hoverable control checked for the same bug.
5. **Dragons** removed completely, from every scene, the map and any animation. Dragon's Peak
   keeps its name and peaks (golden cliffs, nest, sky), with no dragon anywhere.
6. **Visuals:** less polygonal, a little more realistic, keeping the bright painterly style. In
   the art pipeline: smooth shading and subdivision at render time; proper trees (natural
   branching, leaf cards / alpha foliage, varied sizes); grass and flower scattering; displaced,
   naturally broken rock and cliffs; better materials (marble, stone, bark, leaves with subtle
   texture and roughness variation); soft atmospheric depth. External assets or textures only
   CC0 (source and licence logged); nothing copied from the reference images. Image and bundle
   sizes about the same. Every location, sub-location and the map re-rendered; `docs/snapshots/`
   before/after.

**Tests:** no regression in the sound tests; glissando in-tune/in-key check (held chords in
several keys); editor state restore test; hover-clear test; the snapshots. DEVLOG checklist,
handoff with what to look at in GarageBand.

**Rules:** never modify the Dungeon-Crawler, Worldbuilder, LIMINAL or Samples repos. Push each
part with `[skip ci]`; after the final push (without `[skip ci]`) wait for CI and fix until
green (wall-clock-only timing failures: re-run once, then report). Small metric misses after 2
attempts → report-only; never weaken real checks. Editor double-buffered, cheap, never freezing
a MacBook Air. `runs-on: macos-15`. Never put `: ` inside an unquoted YAML step name. No brand
names in the product.
