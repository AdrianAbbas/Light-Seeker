#pragma once
// 1.0.0 (the release freeze): what hosts and users see of the company and the version, the
// codes and IDs that must not change (old projects and presets), sessions saved by the
// build before 1.0 loading unchanged, and the user's data copied into its new folders.
#include "UserData.h"

namespace release {

// The sessions in Tests/fixtures were saved by the last build before 1.0 (getStateInformation,
// written as XML): the effect in a place with its controls moved, a journey and the window
// left open; the instrument with the organ locked and carried into the Sanctum, its sound
// shaped. The AU harness loads the same files through the Audio Unit route on macOS.
juce::File fixture(const char* name) { return juce::File(LS_TEST_FIXTURES).getChildFile(name); }

// Loads an old session into `p` and returns what `p` saves afterwards.
std::unique_ptr<juce::XmlElement> roundTrip(LightSeekerProcessor& p, const juce::XmlElement& old) {
    juce::MemoryBlock in, out;
    juce::AudioProcessor::copyXmlToBinary(old, in);
    p.setStateInformation(in.getData(), static_cast<int>(in.getSize()));
    p.getStateInformation(out);
    return juce::AudioProcessor::getXmlFromBinary(out.getData(), static_cast<int>(out.getSize()));
}

// The first difference between two saved states (attribute by attribute, child by child).
std::string firstDifference(const juce::XmlElement& a, const juce::XmlElement& b) {
    if (a.getTagName() != b.getTagName()) return "tag " + a.getTagName().toStdString() + " vs " + b.getTagName().toStdString();
    for (int i = 0; i < std::max(a.getNumAttributes(), b.getNumAttributes()); ++i) {
        const auto name = i < a.getNumAttributes() ? a.getAttributeName(i) : b.getAttributeName(i);
        if (a.getStringAttribute(name) != b.getStringAttribute(name) || a.hasAttribute(name) != b.hasAttribute(name))
            return a.getTagName().toStdString() + " " + (a.hasAttribute("id") ? a.getStringAttribute("id").toStdString() + " " : "") + name.toStdString()
                   + ": " + a.getStringAttribute(name, "(absent)").toStdString() + " vs " + b.getStringAttribute(name, "(absent)").toStdString();
    }
    if (a.getNumChildElements() != b.getNumChildElements())
        return a.getTagName().toStdString() + ": " + std::to_string(a.getNumChildElements()) + " vs " + std::to_string(b.getNumChildElements()) + " children";
    for (int i = 0; i < a.getNumChildElements(); ++i)
        if (auto d = firstDifference(*a.getChildElement(i), *b.getChildElement(i)); !d.empty()) return d;
    return {};
}

bool userFacingClean(const juce::String& text) {
    return !text.containsIgnoreCase("veil") && !text.containsIgnoreCase("elsewhere audio");
}

} // namespace release

void testRelease(Suite& s) {
    s.section("release 1.0.0: identity, version, old sessions, user data");
    // What hosts show (Info.plist, the AU registry, VST3 vendor) comes from these.
    s.check(juce::String(JucePlugin_VersionString) == "1.0.0", "version 1.0.0", JucePlugin_VersionString);
    s.check(JucePlugin_VersionCode == 0x10000, "version code 0x10000 (1.0.0, what AU hosts read)", std::to_string(JucePlugin_VersionCode));
    s.check(juce::String(JucePlugin_Manufacturer) == "Windward Audio", "company shown in hosts: Windward Audio", JucePlugin_Manufacturer);
    // Unchanged: the AU manufacturer and subtype codes (old projects find the plug-ins by them).
    s.check(JucePlugin_ManufacturerCode == 0x5665696c, "AU manufacturer code unchanged (Veil)");
    s.check(JucePlugin_PluginCode == 0x4c736b69, "the instrument's AU subtype unchanged (Lski)");
    {
        LightSeekerProcessor fx, inst(true);
        s.check(fx.getName() == "LIGHT SEEKER FX" && inst.getName() == "LIGHT SEEKER", "names in hosts",
                (fx.getName() + " / " + inst.getName()).toStdString());
        // Every text the user can read from the processors: names, labels, choices, presets.
        std::string dirty;
        for (auto* p : { &fx, &inst }) {
            for (auto* parameter : p->getParameters()) {
                juce::StringArray texts { parameter->getName(100), parameter->getLabel() };
                texts.addArray(parameter->getAllValueStrings());
                for (const auto& t : texts) if (!release::userFacingClean(t)) dirty += t.toStdString() + " | ";
            }
            for (const auto& preset : dc::presetsFor(p->isInstrument()))
                for (const juce::String t : { juce::String(preset.name), juce::String(preset.category) })
                    if (!release::userFacingClean(t)) dirty += t.toStdString() + " | ";
        }
        for (bool instrument : { true, false })
            if (!release::userFacingClean(LightSeekerView::aboutText(instrument).replace(ls::data::pluginFolder(instrument).getFullPathName(), {})))
                dirty += "About text | ";
        s.check(dirty.empty(), "no old company name in any parameter, choice, preset or About text", dirty);
        for (bool instrument : { true, false }) {
            const auto about = LightSeekerView::aboutText(instrument);
            s.check(about.startsWith(instrument ? "LIGHT SEEKER  version 1.0.0\n" : "LIGHT SEEKER FX  version 1.0.0\n") && about.contains("Windward Audio"),
                    std::string("About screen: name, version 1.0.0, company (") + (instrument ? "instrument" : "effect") + ")", about.toStdString());
        }
    }
    // Sessions saved before 1.0 load unchanged: every parameter, the journey, the locked
    // instrument, the window, saved again exactly as they were saved.
    for (bool instrument : { false, true }) {
        const char* name = instrument ? "old-session-instrument.xml" : "old-session-fx.xml";
        const std::string kind = instrument ? "instrument" : "effect";
        auto old = juce::XmlDocument::parse(release::fixture(name));
        if (!s.check(old != nullptr, "old session fixture readable (" + kind + ")", release::fixture(name).getFullPathName().toStdString())) continue;
        auto p = std::make_unique<LightSeekerProcessor>(instrument);
        p->setRateAndBufferSizeDetails(48000, 512);
        p->prepareToPlay(48000, 512);
        auto saved = release::roundTrip(*p, *old);
        {   // a few blocks, as a host plays after loading (the instrument's lock lands here)
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            for (int k = 0; k < 8; ++k) { buffer.clear(); p->processBlock(buffer, midi); }
        }
        const auto difference = saved == nullptr ? std::string("no state") : release::firstDifference(*old, *saved);
        s.check(difference.empty(), "an old session of the " + kind + " loads unchanged (state compare)", difference);
        int params = 0, wrong = 0;
        for (auto* child : old->getChildWithTagNameIterator("PARAM")) {
            ++params;
            const auto v = p->parameters.getRawParameterValue(child->getStringAttribute("id"));
            if (v == nullptr || std::abs(v->load() - static_cast<float>(child->getDoubleAttribute("value"))) > 1.0e-5f) ++wrong;
        }
        s.check(params > 100 && wrong == 0, "... every parameter ID still exists and has its saved value (" + kind + ")",
                std::to_string(wrong) + " of " + std::to_string(params));
        if (instrument)
            s.check(p->instrumentLocked() && p->instrumentIndex() == ls::inst::instrumentIndex(dc::GoldenCathedral, 0) && p->route().size() == 2,
                    "... with its locked instrument (the organ) and its journey");
        else
            s.check(p->route().size() == 3 && p->presetName == "My old hall" && std::abs(p->editorScale.load() - 1.25f) < 1.0e-6f,
                    "... with its journey, preset name and window size");
    }
    // The user's folders, and the copy of the data from earlier builds.
    s.check(ls::data::presetsFolder(true).getFullPathName().endsWith("Windward Audio/LIGHT SEEKER/Presets")
                && ls::data::presetsFolder(false).getFullPathName().endsWith("Windward Audio/LIGHT SEEKER FX/Presets"),
            "presets live in Windward Audio/LIGHT SEEKER and /LIGHT SEEKER FX", ls::data::presetsFolder(true).getFullPathName().toStdString());
   #if JUCE_MAC
    s.check(ls::data::pluginFolder(true) == juce::File("~/Library/Windward Audio/LIGHT SEEKER"), "macOS: ~/Library/Windward Audio/LIGHT SEEKER",
            ls::data::pluginFolder(true).getFullPathName().toStdString());
   #endif
    {
        LightSeekerProcessor fx, inst(true);
        s.check(fx.userPresetFolder() == ls::data::presetsFolder(false) && inst.userPresetFolder() == ls::data::presetsFolder(true),
                "each plug-in saves its presets in its own folder");
    }
    {
        const auto library = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ls-migration-test");
        library.deleteRecursively();
       #if JUCE_MAC
        const auto old = library.getChildFile("Application Support/LightSeeker");
       #else
        const auto old = library.getChildFile("LightSeeker");
       #endif
        const auto write = [](const juce::File& f, const juce::String& text) { f.getParentDirectory().createDirectory(); f.replaceWithText(text); };
        write(old.getChildFile("Presets/Warm Hall.lspreset"), "<LIGHT_SEEKER/>");
        write(old.getChildFile("Instrument Presets/My Harp.lsinst"), "<LIGHT_SEEKER/>");
        write(old.getChildFile("Instrument Presets/Kept.lsinst"), "old");
        write(old.getChildFile("scenes/sky-temple/sanctum.png"), "png");
        const auto hostOld = library.getChildFile("Audio/Presets/Veil Audio/LIGHT SEEKER/Song.aupreset");
        write(hostOld, "aupreset");
        write(library.getChildFile("Audio/Presets/Veil Audio/DUNGEON CRAWLER/Other.aupreset"), "not ours");
        // A preset the user already has in the new place is never overwritten.
        write(ls::data::presetsFolder(true, library).getChildFile("Kept.lsinst"), "new");
        const auto first = ls::data::migrate(true, library), fxFirst = ls::data::migrate(false, library);
        const auto again = ls::data::migrate(true, library);
        const auto newInst = ls::data::pluginFolder(true, library), newFx = ls::data::pluginFolder(false, library);
        s.check(first.ran && first.ok && first.copied == 3 && first.kept == 1, "presets migrate: the instrument's copied on first launch",
                std::to_string(first.copied) + " copied, " + std::to_string(first.kept) + " kept");
        s.check(newInst.getChildFile("Presets/My Harp.lsinst").existsAsFile() && newInst.getChildFile("Scenes/sky-temple/sanctum.png").existsAsFile()
                    && library.getChildFile("Audio/Presets/Windward Audio/LIGHT SEEKER/Song.aupreset").existsAsFile()
                    && newInst.getChildFile("Presets/Kept.lsinst").loadFileAsString() == "new",
                "... into LIGHT SEEKER/Presets and /Scenes, and the host's presets under the new company name");
        s.check(fxFirst.ran && fxFirst.ok && newFx.getChildFile("Presets/Warm Hall.lspreset").existsAsFile()
                    && newFx.getChildFile("Scenes/sky-temple/sanctum.png").existsAsFile() && !newFx.getChildFile("Presets/My Harp.lsinst").exists(),
                "... the effect's into LIGHT SEEKER FX (and only its own)");
        s.check(old.getChildFile("Presets/Warm Hall.lspreset").existsAsFile() && old.getChildFile("Instrument Presets/My Harp.lsinst").existsAsFile()
                    && hostOld.existsAsFile(),
                "... and a copy is kept in the old folders");
        s.check(newInst.getChildFile(ls::data::migratedNote).existsAsFile() && !again.ran, "... once (a note records it; the next launch does nothing)");
        s.check(!library.getChildFile("Audio/Presets/Windward Audio/DUNGEON CRAWLER").exists(), "... other plug-ins' presets are left alone");
        const auto empty = library.getChildFile("empty");
        s.check(!ls::data::migrate(true, empty).ran && !empty.exists(), "nothing old: nothing is created");
        library.deleteRecursively();
    }
}
