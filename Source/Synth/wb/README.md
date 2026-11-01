# WORLDBUILDER's sound engine (copied)

These files are copies of WORLDBUILDER's instrument engine
(https://github.com/AdrianAbbas/Worldbuilder, read only, never modified), taken from its
`main` at `ccce3e3`:

| Here | WORLDBUILDER |
|---|---|
| `Core.h`, `Music.h`, `Events.h`, `InstrumentData.h`, `Voices.h`, `Instruments.h`, `PlaceFx.h` | `Source/DSP/` |
| `SampleBank.h`, `SampleBank.cpp` | `Source/Samples/` |
| `../../tools/sample-prep/` | `tools/sample-prep/` |
| `../../scripts/ci-fetch-samples.sh` | `scripts/ci-fetch-samples.sh` (made non-fatal without the token) |

The only changes are mechanical: the namespace `wb` is `lswb` here (so the two plug-ins can
never share a symbol when a host loads both), and the sample folder's environment variable
is `LIGHTSEEKER_SAMPLES`. Everything that plays these voices as LIGHT SEEKER's instruments
(the voice pool, MIDI, the sampler voice, the organ, the patches) is in `Source/Synth/`.
