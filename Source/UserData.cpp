#include "UserData.h"
#include <atomic>

namespace ls::data {
namespace {

juce::String pluginName(bool instrument) { return instrument ? "LIGHT SEEKER" : "LIGHT SEEKER FX"; }

// The pre-1.0 folder both plug-ins shared (macOS: ~/Library/Application Support/LightSeeker).
juce::File oldSharedFolder(const juce::File& library) {
   #if JUCE_MAC
    return library.getChildFile("Application Support").getChildFile("LightSeeker");
   #else
    return library.getChildFile("LightSeeker");
   #endif
}

// What one plug-in takes from the old places: (old folder, new folder) pairs.
std::vector<std::pair<juce::File, juce::File>> sources(bool instrument, const juce::File& library) {
    const auto old = oldSharedFolder(library);
    std::vector<std::pair<juce::File, juce::File>> list {
        { old.getChildFile(instrument ? "Instrument Presets" : "Presets"), presetsFolder(instrument, library) },
        { old.getChildFile("scenes"), scenesFolder(instrument, library) },
    };
    // The presets hosts saved themselves (Logic and GarageBand keep .aupreset files under
    // the company name the plug-in reported, which was the old one). The folder names are
    // the plug-in's name in hosts and, from earlier builds, its file name.
    const auto audioPresets = library.getChildFile("Audio").getChildFile("Presets");
    const auto oldCompany = audioPresets.getChildFile("Veil Audio");   // the pre-1.0 company name (internal only)
    const auto newCompany = audioPresets.getChildFile(companyFolder);
    for (const juce::String folder : { pluginName(instrument), juce::String(instrument ? "LightSeeker" : "LightSeekerFX") })
        list.push_back({ oldCompany.getChildFile(folder), newCompany.getChildFile(folder) });
    return list;
}

void copyTree(const juce::File& from, const juce::File& to, Migration& m) {
    for (const auto& entry : juce::RangedDirectoryIterator(from, true, "*", juce::File::findFiles)) {
        const auto& file = entry.getFile();
        if (file.getFileName().startsWithChar('.')) continue;   // .DS_Store and the like
        const auto target = to.getChildFile(file.getRelativePathFrom(from));
        if (target.exists()) { ++m.kept; continue; }
        if (target.getParentDirectory().createDirectory() && file.copyFileTo(target)) ++m.copied;
        else m.ok = false;
    }
}

} // namespace

juce::File libraryFolder() {
    // JUCE's user data folder: ~/Library on macOS, ~/.config on Linux, AppData on Windows.
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
}

juce::File pluginFolder(bool instrument, const juce::File& library) {
    return library.getChildFile(companyFolder).getChildFile(pluginName(instrument));
}
juce::File presetsFolder(bool instrument, const juce::File& library) { return pluginFolder(instrument, library).getChildFile("Presets"); }
juce::File scenesFolder(bool instrument, const juce::File& library) { return pluginFolder(instrument, library).getChildFile("Scenes"); }

Migration migrate(bool instrument, const juce::File& library) {
    Migration m;
    const auto note = pluginFolder(instrument, library).getChildFile(migratedNote);
    if (note.existsAsFile()) return m;
    bool any = false;
    for (const auto& [from, to] : sources(instrument, library)) any = any || from.isDirectory();
    if (!any) return m;   // nothing old: create nothing
    m.ran = true;
    juce::String record;
    record << pluginName(instrument) << ": your data from earlier builds was copied here on "
           << juce::Time::getCurrentTime().toString(true, true) << ".\n"
           << "The originals were left where they were. Copied from:\n";
    for (const auto& [from, to] : sources(instrument, library)) {
        if (!from.isDirectory()) continue;
        const int before = m.copied;
        copyTree(from, to, m);
        record << "  " << from.getFullPathName() << "  ->  " << to.getFullPathName()
               << "  (" << (m.copied - before) << " files)\n";
    }
    // Only a complete copy is recorded; otherwise the next launch tries the rest again.
    if (m.ok) m.ok = note.getParentDirectory().createDirectory() && note.replaceWithText(record);
    return m;
}

void migrateOnce(bool instrument) {
    static std::atomic<bool> done[2] { false, false };
    if (done[instrument ? 1 : 0].exchange(true)) return;
    migrate(instrument);
}

} // namespace ls::data
