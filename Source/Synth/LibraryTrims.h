#pragma once
// The library brass's level trims (dB, per instrument), written by `LightSeekerTests
// --calibrate-library Source/Synth/LibraryTrims.h`: with the library loaded, a held chord
// (C4 E4 G4, velocity 0.8, dry) as loud as the synthesis it replaces (the stand-in): its
// loudest 400 ms (weighted 70 %) and its whole 1.5 s (30 %), so the real brass hits harder
// without getting louder. `libraryStandInTrims`: the same without the developer's recordings
// (their stand-ins beside the brass). 0 without the library.
#include <array>

namespace ls::inst {
inline constexpr std::array<float, 31> libraryTrims { 8.2f, 0.0f, 6.1f, 6.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 6.4f, 0.0f, 8.7f };
inline constexpr std::array<float, 31> libraryStandInTrims { 8.2f, 0.0f, 6.2f, 6.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 6.4f, 0.0f, 7.5f };
}
