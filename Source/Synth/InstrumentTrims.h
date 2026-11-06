#pragma once
// Level trims of the 31 instruments (dB), written by `LightSeekerTests --calibrate-instruments
// Source/Synth/InstrumentTrims.h`: each brings its own kind of part (chords, melody or
// arpeggio; the loudness rebalance), played in its own place at the shipped defaults, to the
// same integrated loudness (ITU-R BS.1770). `standIn` is for builds without the recordings
// (their stand-ins play).
#include <array>

namespace ls::inst {
inline constexpr std::array<float, 31> instrumentTrims { -17.7f, -17.0f, -16.5f, -14.5f, -3.7f, -1.0f, -11.6f, 1.2f, 9.7f, -16.1f, -11.7f, -13.9f, -5.4f, 8.5f, 1.7f, -19.6f, -21.0f, -10.9f, -5.0f, -12.5f, -15.2f, -3.3f, -2.1f, -2.7f, -0.1f, -1.1f, -8.9f, 2.5f, -18.1f, -14.6f, -14.4f };
inline constexpr std::array<float, 31> standInTrims { -17.7f, -17.0f, -16.5f, -14.5f, -3.7f, -7.4f, -11.6f, 1.2f, 1.5f, -16.1f, -11.7f, -13.9f, -5.1f, 2.8f, 0.9f, -19.6f, -21.0f, -2.9f, -5.0f, -18.5f, -15.2f, -11.0f, -2.1f, -2.7f, -0.1f, 4.7f, -8.9f, 2.5f, -18.1f, -17.2f, -11.1f };
// The trims before the loudness rebalance (a dry held chord's loudest 400 ms): kept so a
// session saved before it, where Level had been moved, opens at the loudness it had.
inline constexpr std::array<float, 31> legacyInstrumentTrims { -16.9f, -11.9f, -11.7f, -16.9f, -3.4f, 2.8f, -11.4f, -1.3f, 7.8f, -16.3f, -10.6f, -9.1f, -5.2f, 7.8f, -0.3f, -21.7f, -16.2f, -5.9f, -6.2f, -7.3f, -16.0f, -3.1f, -4.9f, -2.3f, -1.3f, 0.9f, -6.7f, 0.5f, -17.4f, -9.0f, -12.0f };
inline constexpr std::array<float, 31> legacyStandInTrims { -16.9f, -11.9f, -12.3f, -16.9f, -3.4f, -5.6f, -11.4f, -1.3f, -1.0f, -16.3f, -10.6f, -9.1f, -5.7f, 0.4f, -1.5f, -21.7f, -16.2f, 2.8f, -6.2f, -13.4f, -16.0f, -10.5f, -4.9f, -2.3f, -1.3f, 6.2f, -6.7f, 0.5f, -17.4f, -11.9f, -10.4f };
// How much louder each instrument's own part (in its place, at the defaults) is than before
// the rebalance (LU; the trims, the library brass's re-fitted trims, the Fairy Harp's share of
// its court): an older session where Level had been moved opens that much lower.
inline constexpr std::array<float, 31> levelShift { -1.2f, -5.1f, -4.8f, 4.0f, -0.3f, -3.8f, -0.2f, 2.5f, 1.9f, 0.2f, -1.1f, -4.8f, -0.2f, 0.7f, 2.0f, 2.1f, -4.8f, -5.0f, 1.2f, -5.2f, 0.8f, -0.2f, 2.8f, -0.4f, 1.2f, -2.0f, -2.2f, 1.7f, -0.9f, -5.6f, -2.4f };
inline constexpr std::array<float, 31> standInLevelShift { -1.2f, -5.1f, -4.2f, 4.0f, -0.3f, -1.8f, -0.2f, 2.5f, 2.5f, 0.2f, -1.1f, -4.8f, 0.6f, 2.4f, 2.4f, 2.1f, -4.8f, -5.7f, 1.2f, -5.1f, 0.8f, -0.5f, 2.8f, -0.4f, 1.2f, -1.5f, -2.2f, 1.7f, -0.9f, -5.3f, -0.8f };
}
