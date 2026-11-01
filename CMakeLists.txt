cmake_minimum_required(VERSION 3.22)
# macOS defaults: one universal binary that loads on Apple Silicon and Intel Macs
# back to 10.15. These must be set before project(); override on the command line.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()
project(LIGHT_SEEKER VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(LS_BUILD_TESTS "Build the offline processor tests" ON)
set(JUCE_PATH "" CACHE PATH "Existing JUCE 8.0.16 source (otherwise fetched)")
if(JUCE_PATH)
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ea77444750de45e036e0279bb0291144bf8e3688 # JUCE 8.0.16
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(JUCE)
endif()
set(LS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND LS_FORMATS AU)
endif()
# File and bundle names use "LightSeeker" (no space: simpler scripts); hosts show
# PLUGIN_NAME, "Veil Audio: LIGHT SEEKER".
juce_add_plugin(LightSeeker
    COMPANY_NAME "Veil Audio"
    BUNDLE_ID "audio.veil.lightseeker"
    PLUGIN_MANUFACTURER_CODE Veil
    PLUGIN_CODE Lsek
    PRODUCT_NAME "LightSeeker"
    PLUGIN_NAME "LIGHT SEEKER"
    AU_EXPORT_PREFIX LightSeekerAU
    DESCRIPTION "Light-fantasy location effect"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/icon-1024.png"
    FORMATS ${LS_FORMATS}
    # An audio effect: audio in, audio out. MIDI in (optional MIDI travel, Milestone 9)
    # where the host sends MIDI to effects; the AU stays an effect (aufx).
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    # Lets sandboxed hosts (GarageBand) load it in-process. It must still work
    # out-of-process, which the AU host harness checks.
    AU_SANDBOX_SAFE TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(LightSeeker PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Engine.cpp
    Source/DSP/Abyss.cpp Source/DSP/AncientBlood.cpp Source/DSP/Choir.cpp
    Source/DSP/Wraith.cpp Source/DSP/Echoes.cpp Source/DSP/Crypt.cpp
    Source/DSP/Relics.cpp Source/DSP/Signature.cpp Source/DSP/Vault.cpp Source/DSP/Convolver.cpp Source/DSP/Ensemble.cpp Source/DSP/Atmosphere.cpp Source/World/Locations.cpp
    Source/UI/EngravedLookAndFeel.cpp Source/UI/Engraving.cpp Source/UI/Scenes.cpp Source/UI/ScenePlaces.cpp Source/UI/SceneCache.cpp
    Source/UI/Painted.cpp)
# Fonts (SIL Open Font License, assets/fonts): Cinzel for titles, Cormorant Garamond for labels.
juce_add_binary_data(LightSeekerFonts NAMESPACE LightSeekerFonts SOURCES
    assets/fonts/CormorantGaramond-SemiBold.ttf assets/fonts/CormorantGaramond-Bold.ttf assets/fonts/Cinzel-SemiBold.ttf)
set_target_properties(LightSeekerFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Captured impulse responses (tools/captures/export_irs.py), 48 kHz 16-bit stereo.
juce_add_binary_data(LightSeekerIrs NAMESPACE LightSeekerIrs HEADER_NAME LightSeekerIrs.h SOURCES
    assets/irs/radiant-shimmer-hall.wav assets/irs/cliff-hall-echo.wav assets/irs/chorus-hall.wav)
set_target_properties(LightSeekerIrs PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(LightSeeker PUBLIC Source)
target_compile_definitions(LightSeeker PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(LightSeeker PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags LightSeekerFonts LightSeekerIrs)
# The painted art (tools/painted builds it into assets/painted) ships inside every bundle
# at Contents/Resources/Painted; development builds and the tests fall back to the
# source tree.
target_compile_definitions(LightSeeker PUBLIC LS_PAINTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/painted")
foreach(ls_format IN LISTS LS_FORMATS)
    set(ls_target LightSeeker_${ls_format})
    if(TARGET ${ls_target})
        if(ls_format STREQUAL "Standalone" AND NOT APPLE)
            set(ls_dest "$<TARGET_FILE_DIR:${ls_target}>/Painted")
        else()
            set(ls_dest "$<TARGET_FILE_DIR:${ls_target}>/../Resources/Painted")
        endif()
        add_custom_command(TARGET ${ls_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/painted" "${ls_dest}"
            VERBATIM)
    endif()
endforeach()
if(LS_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(LightSeekerTests PRODUCT_NAME "LightSeekerTests")
    # The linked plugin target exports the same setting using its format macro.
    get_target_property(ls_test_definitions LightSeekerTests COMPILE_DEFINITIONS)
    list(REMOVE_ITEM ls_test_definitions JUCE_STANDALONE_APPLICATION=1)
    set_target_properties(LightSeekerTests PROPERTIES COMPILE_DEFINITIONS "${ls_test_definitions}")
    target_sources(LightSeekerTests PRIVATE Tests/LightSeekerTests.cpp)
    target_link_libraries(LightSeekerTests PRIVATE LightSeeker juce::juce_audio_utils)
    target_compile_definitions(LightSeekerTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    add_test(NAME light_seeker_tests COMMAND LightSeekerTests)
endif()
