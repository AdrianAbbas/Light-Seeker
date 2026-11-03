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
# Two plug-ins from the same sources (Direction Update 1):
#   LIGHT SEEKER     the instrument (AU aumu Lski Veil, VST3 instrument): LightSeeker.*
#   LIGHT SEEKER FX  the effect     (AU aufx Lsek Veil, VST3 effect):     LightSeekerFX.*
# File and bundle names have no space (simpler scripts); hosts show "Veil Audio: LIGHT
# SEEKER" and "Veil Audio: LIGHT SEEKER FX". DUNGEON CRAWLER (Dgcr) never clashes.
set(LS_SAMPLES_DIR "" CACHE PATH "Prepared recordings (tools/sample-prep) to ship in the instrument's bundles")
set(LS_SHARED_SOURCES
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Engine.cpp
    Source/DSP/Abyss.cpp Source/DSP/AncientBlood.cpp Source/DSP/Choir.cpp
    Source/DSP/Wraith.cpp Source/DSP/Echoes.cpp Source/DSP/Crypt.cpp
    Source/DSP/Relics.cpp Source/DSP/Signature.cpp Source/DSP/Vault.cpp Source/DSP/Convolver.cpp Source/DSP/Ensemble.cpp Source/DSP/Atmosphere.cpp Source/World/Locations.cpp
    Source/World/InstrumentPresets.cpp Source/World/Search.cpp
    Source/Synth/Synth.cpp Source/Synth/Patches.cpp Source/Synth/wb/SampleBank.cpp
    Source/UI/EngravedLookAndFeel.cpp Source/UI/Engraving.cpp Source/UI/Scenes.cpp Source/UI/ScenePlaces.cpp Source/UI/SceneCache.cpp
    Source/UI/Painted.cpp Source/UI/InstrumentPanel.cpp Source/UI/SearchPanel.cpp Source/UI/MixPanel.cpp)
# Fonts (SIL Open Font License, assets/fonts): Cinzel for titles, Cormorant Garamond for labels.
juce_add_binary_data(LightSeekerFonts NAMESPACE LightSeekerFonts SOURCES
    assets/fonts/CormorantGaramond-SemiBold.ttf assets/fonts/CormorantGaramond-Bold.ttf assets/fonts/Cinzel-SemiBold.ttf)
set_target_properties(LightSeekerFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Captured impulse responses (48 kHz 16-bit stereo). They are derived from the developer's
# private effect captures, so they are not in this repository: CI makes them from the
# Samples repository (tools/captures/ci-build-irs.sh) and names the folder with
# LS_IRS_DIR; they are compiled into both plug-ins. Without them the Vault's captured
# spaces use generated fallback spaces of the same character (Vault.cpp).
set(LS_IRS_DIR "" CACHE PATH "Captured impulse responses (tools/captures/ci-build-irs.sh) to compile in")
set(LS_IR_FILES radiant-shimmer-hall.wav cliff-hall-echo.wav chorus-hall.wav)
set(ls_have_irs FALSE)
if(LS_IRS_DIR)
    set(ls_have_irs TRUE)
    set(ls_ir_sources "")
    foreach(ls_ir IN LISTS LS_IR_FILES)
        if(EXISTS "${LS_IRS_DIR}/${ls_ir}")
            list(APPEND ls_ir_sources "${LS_IRS_DIR}/${ls_ir}")
        else()
            set(ls_have_irs FALSE)
        endif()
    endforeach()
endif()
add_library(LightSeekerIrsConfig INTERFACE)
if(ls_have_irs)
    message(STATUS "Captured spaces: compiled in from ${LS_IRS_DIR}")
    juce_add_binary_data(LightSeekerIrs NAMESPACE LightSeekerIrs HEADER_NAME LightSeekerIrs.h SOURCES ${ls_ir_sources})
    set_target_properties(LightSeekerIrs PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(LightSeekerIrsConfig INTERFACE LightSeekerIrs)
    target_compile_definitions(LightSeekerIrsConfig INTERFACE LS_HAVE_CAPTURED_IRS=1)
else()
    message(STATUS "Captured spaces: not found (LS_IRS_DIR), the fallback spaces are built in")
    target_compile_definitions(LightSeekerIrsConfig INTERFACE LS_HAVE_CAPTURED_IRS=0)
endif()

function(ls_add_plugin target)
    cmake_parse_arguments(LS "SYNTH" "PRODUCT;NAME;CODE;BUNDLE;PREFIX;DESCRIPTION;AUTYPE" "CATEGORIES" ${ARGN})
    if(LS_SYNTH)
        set(ls_is_synth TRUE)
    else()
        set(ls_is_synth FALSE)
    endif()
    juce_add_plugin(${target}
        COMPANY_NAME "Veil Audio"
        BUNDLE_ID "${LS_BUNDLE}"
        PLUGIN_MANUFACTURER_CODE Veil
        PLUGIN_CODE ${LS_CODE}
        PRODUCT_NAME "${LS_PRODUCT}"
        PLUGIN_NAME "${LS_NAME}"
        AU_EXPORT_PREFIX ${LS_PREFIX}
        DESCRIPTION "${LS_DESCRIPTION}"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/icon-1024.png"
        FORMATS ${LS_FORMATS}
        # The effect: audio in, audio out, MIDI in for the optional MIDI travel (the AU
        # stays an effect, aufx). The instrument: MIDI in, audio out (aumu).
        IS_SYNTH ${ls_is_synth}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES ${LS_CATEGORIES}
        AU_MAIN_TYPE ${LS_AUTYPE}
        # Lets sandboxed hosts (GarageBand) load it in-process. It must still work
        # out-of-process, which the AU host harness checks.
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(${target} PRIVATE ${LS_SHARED_SOURCES})
    target_include_directories(${target} PUBLIC Source)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(${target} PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags LightSeekerFonts LightSeekerIrsConfig)
    # The painted art (tools/painted builds it into assets/painted) ships inside every
    # bundle at Contents/Resources/Painted; development builds and the tests fall back to
    # the source tree. The instrument's bundles also carry the prepared recordings
    # (Contents/Resources/Samples) when LS_SAMPLES_DIR names them.
    target_compile_definitions(${target} PUBLIC LS_PAINTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/painted")
    foreach(ls_format IN LISTS LS_FORMATS)
        set(ls_target ${target}_${ls_format})
        if(TARGET ${ls_target})
            if(ls_format STREQUAL "Standalone" AND NOT APPLE)
                set(ls_res "$<TARGET_FILE_DIR:${ls_target}>")
            else()
                set(ls_res "$<TARGET_FILE_DIR:${ls_target}>/../Resources")
            endif()
            add_custom_command(TARGET ${ls_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/painted" "${ls_res}/Painted"
                VERBATIM)
            if(LS_SYNTH AND LS_SAMPLES_DIR)
                add_custom_command(TARGET ${ls_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${LS_SAMPLES_DIR}" "${ls_res}/Samples"
                    COMMAND ${CMAKE_COMMAND} -E rm -rf "${ls_res}/Samples/fx"
                    VERBATIM)
            elseif(LS_SAMPLES_DIR AND EXISTS "${LS_SAMPLES_DIR}/fx/index.json")
                # The effect carries only what its Atmosphere layer plays (the breaths).
                add_custom_command(TARGET ${ls_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${LS_SAMPLES_DIR}/fx" "${ls_res}/Samples"
                    VERBATIM)
            endif()
        endif()
    endforeach()
endfunction()

ls_add_plugin(LightSeeker SYNTH
    PRODUCT "LightSeeker" NAME "LIGHT SEEKER" CODE Lski BUNDLE "audio.veil.lightseeker"
    PREFIX LightSeekerAU DESCRIPTION "Light-fantasy location instrument"
    AUTYPE kAudioUnitType_MusicDevice CATEGORIES Instrument Synth)
ls_add_plugin(LightSeekerFX
    PRODUCT "LightSeekerFX" NAME "LIGHT SEEKER FX" CODE Lsek BUNDLE "audio.veil.lightseeker.fx"
    PREFIX LightSeekerFXAU DESCRIPTION "Light-fantasy location effect"
    AUTYPE kAudioUnitType_Effect CATEGORIES Fx Reverb)
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
