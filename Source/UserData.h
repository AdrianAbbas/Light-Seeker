#pragma once
// Where each plug-in keeps the user's data (1.0): its own folder under the company's,
//   macOS  ~/Library/Windward Audio/LIGHT SEEKER/     (the instrument)
//          ~/Library/Windward Audio/LIGHT SEEKER FX/  (the effect)
//   (elsewhere JUCE's user data folder, e.g. ~/.config, takes the place of ~/Library)
// each with Presets/ (the user's saved presets) and Scenes/ (drop-in art, see Scenes.h).
//
// Builds before 1.0 kept both plug-ins' data in one folder, ~/Library/Application
// Support/LightSeeker (Presets/ for the effect, Instrument Presets/ for the instrument,
// scenes/ for both), and hosts saved their own presets under the old company name in
// ~/Library/Audio/Presets. On its first launch each plug-in copies what it finds there
// into its new folders (migrate()): nothing is moved or deleted, a file already in the
// new place is never overwritten, and a note in the new folder (MIGRATED.txt) records
// that the copy was made so it happens once. install-mac.sh makes the same copy.
#include <juce_core/juce_core.h>

namespace ls::data {

inline constexpr const char* companyFolder = "Windward Audio";
inline constexpr const char* migratedNote = "MIGRATED.txt";

// The base everything is relative to: ~/Library on macOS (JUCE's user data folder).
juce::File libraryFolder();

// The plug-in's own folder and its parts, below `library` (normally libraryFolder()).
juce::File pluginFolder(bool instrument, const juce::File& library = libraryFolder());
juce::File presetsFolder(bool instrument, const juce::File& library = libraryFolder());
juce::File scenesFolder(bool instrument, const juce::File& library = libraryFolder());

struct Migration {
    bool ran = false;        // something old was found and this was the first launch since
    int copied = 0;          // files copied into the new folders
    int kept = 0;            // files left alone because the new folder already had them
    bool ok = true;          // every copy succeeded (otherwise it is tried again next launch)
};

// Copies the pre-1.0 data of one plug-in into its new folders, once (see above). Safe to
// call at every launch: it returns at once when the note exists or there is nothing old.
Migration migrate(bool instrument, const juce::File& library = libraryFolder());

// migrate() for the real library, at most once per process and edition.
void migrateOnce(bool instrument);

} // namespace ls::data
