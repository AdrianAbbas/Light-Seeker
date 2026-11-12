# Changelog

LIGHT SEEKER and LIGHT SEEKER FX, by Windward Audio. Versions follow
[semantic versioning](https://semver.org): from 1.0.0 on, 1.0.x releases are bug fixes only.

## 1.0.0 (2026-11-12)

The first release, frozen: from here on only bug fixes; new ideas go to the "1.1 ideas" list in
[docs/ROADMAP.md](docs/ROADMAP.md).

**What is in it**

- Two plug-ins, AU and VST3, one universal download for macOS 10.15+ (Apple Silicon and Intel),
  tested in GarageBand and Logic Pro:
  - **LIGHT SEEKER**, a software instrument: 31 places in ten light-fantasy locations, each with
    its own instrument played inside its own space (real brass and woodwinds, recorded guitar,
    kalimba, violin and pad, modelled strings, voices, bells and plucked instruments); travel
    while notes ring on, or lock one instrument and carry it through every space.
  - **LIGHT SEEKER FX**, an audio effect: the same 31 places on any track, each with its own
    signature effect, level-matched.
- A painted world map that zooms into each location and place; search across instruments,
  places and presets.
- Journeys and Roam (the sound travels through places in time with the song), MIDI travel,
  automation of Location and Sub-location.
- The MIX section (reverb, echoes, modulation, character, atmosphere, low cut, pre-delay, duck)
  and Punch for the struck and plucked instruments; the Atmosphere layer; Shimmer.
- 50 instrument presets and 27 effect presets, your own presets, A/B compare, undo and redo,
  window sizes and three quality levels (Eco for laptops).

**Also in 1.0.0**

- The company name hosts show is now **Windward Audio** ("Windward Audio: LIGHT SEEKER",
  "Windward Audio: LIGHT SEEKER FX"); bundle IDs `com.windwardaudio.lightseeker` and
  `com.windwardaudio.lightseekerfx`. The Audio Unit codes and every parameter are unchanged:
  projects and presets from earlier builds load as they were.
- Your presets and drop-in scenes live in `~/Library/Windward Audio/LIGHT SEEKER/` and
  `~/Library/Windward Audio/LIGHT SEEKER FX/`; on first launch (and when you run the installer)
  anything in the old folders is copied there, and the old folders are kept.
- The installer removes copies left by earlier builds, so hosts list each plug-in once.
- An About screen (⚙ → About): version, company, where your data lives, credits.
- Fixed: the instrument's "Your presets" menu now lists the presets saved with it.
- The Windswept Isle's signature effect is now called "Wind Shroud"; it sounds the same and
  saved settings keep it.
