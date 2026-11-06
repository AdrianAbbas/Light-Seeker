#pragma once
// The real woodwinds' level trims (dB, per instrument, on their real part), written by
// `LightSeekerTests --calibrate-woodwinds Source/Synth/WoodwindTrims.h`: with the library
// loaded, each woodwind instrument's melody from the loudness test (Tests/LoudnessTests.h:
// 16 bars of quarters, eighths and long notes, velocity 80, its own register, in its own
// place at the shipped defaults) as loud (BS.1770 integrated) as with the synthesis it
// replaces (the stand-in); the chord pad's woodwinds on its chords, with its Woodwinds macro
// against the macro at 0. `woodwindStandInTrims`: the same without the developer's recordings.
// 0 without the library.
#include <array>

namespace ls::inst {
inline constexpr std::array<float, 31> woodwindTrims { 0.0f, 0.0f, 17.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 14.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 20.0f, 0.0f, 0.0f, 0.0f, 0.0f, 15.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
inline constexpr std::array<float, 31> woodwindStandInTrims { 0.0f, 0.0f, 17.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 14.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 20.0f, 0.0f, 0.0f, 0.0f, 0.0f, 15.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
}
