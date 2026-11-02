#pragma once
// Level trims of the 31 instruments (dB), written by `LightSeekerTests --calibrate-instruments
// Source/Synth/InstrumentTrims.h`: each brings a held chord (C4 E4 G4, velocity 0.8, dry) to
// the same loudness. `standIn` is for builds without the recordings (their stand-ins play).
#include <array>

namespace ls::inst {
inline constexpr std::array<float, 31> instrumentTrims { -16.5f, -11.9f, -11.7f, -16.9f, -3.4f, 2.8f, -11.4f, -1.3f, 7.8f, -16.3f, -10.6f, -9.1f, -5.2f, 7.8f, -0.3f, -21.7f, -16.2f, -5.9f, -6.2f, -7.3f, -16.0f, -3.1f, -4.9f, -2.3f, -1.3f, 0.9f, -6.7f, 2.0f, -17.4f, -9.0f, -11.4f };
inline constexpr std::array<float, 31> standInTrims { -16.5f, -11.9f, -12.3f, -16.9f, -3.4f, -5.6f, -11.4f, -1.3f, -1.0f, -16.3f, -10.6f, -9.1f, -5.7f, 0.4f, -1.5f, -21.7f, -16.2f, 2.8f, -6.2f, -13.4f, -16.0f, -10.5f, -4.9f, -2.3f, -1.3f, 6.2f, -6.7f, 2.0f, -17.4f, -11.9f, -9.3f };
}
