#pragma once
// The library brass's level trims (dB, per instrument), written by `LightSeekerTests
// --calibrate-library Source/Synth/LibraryTrims.h`: with the library loaded, a held chord
// (C4 E4 G4, velocity 0.8, dry) as loud as the synthesis it replaces (the stand-in): its
// loudest 400 ms (weighted 50 %), its whole 1.5 s (20 %) and the mini-song's brass call
// (30 %), so the real brass hits harder without getting louder. `libraryStandInTrims`: the same without the developer's recordings
// (their stand-ins beside the brass). `libraryTilts`: the real part's register tilt (dB per
// octave from C4) that keeps it level with the synthesis up and down the range (fitted
// first). 0 without the library.
#include <array>

namespace ls::inst {
inline constexpr std::array<float, 31> libraryTilts { 1.8f, 0.0f, 0.7f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.4f, 0.0f, 1.0f };
inline constexpr std::array<float, 31> libraryTrims { 8.1f, 0.0f, 7.7f, 8.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 8.6f, 0.0f, 8.1f };
inline constexpr std::array<float, 31> libraryStandInTrims { 8.1f, 0.0f, 7.7f, 8.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 8.6f, 0.0f, 8.1f };
}
