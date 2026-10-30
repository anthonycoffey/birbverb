cmake_minimum_required(VERSION 3.22)

project(birbverb VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    # Universal (Apple Silicon + Intel) by default.
    # For a faster local/dev build, override with -DCMAKE_OSX_ARCHITECTURES=arm64
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
endif()

option(BIRBVERB_BUILD_TESTS "Build the offline DSP test suite" ON)
option(BIRBVERB_BUILD_TOOLS "Build the offline render/analyze tools" ON)

# --- The DSP core -------------------------------------------------------------
# Header-only, JUCE-free, namespace bv. An INTERFACE library so the test target
# can consume it without JUCE ever entering the link line — see ADR-004. If a
# JuceHeader include creeps into Source/dsp/, the test build breaks, which is
# exactly when we want to find out.
add_library(bv_dsp INTERFACE)
target_include_directories(bv_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_features(bv_dsp INTERFACE cxx_std_20)

# --- Pull JUCE in a reproducible, self-contained way --------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- Plugin formats -----------------------------------------------------------
# AU only exists on Apple; asking for it elsewhere fails the configure step.
set(BIRBVERB_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND BIRBVERB_FORMATS AU)
endif()

# --- The plugin ---------------------------------------------------------------
juce_add_plugin(birbverb
    PLUGIN_MANUFACTURER_CODE  Ynme
    PLUGIN_CODE               Bvrb
    FORMATS                   ${BIRBVERB_FORMATS}
    PRODUCT_NAME              "birbverb"
    COMPANY_NAME              "Anthony Coffey"
    BUNDLE_ID                 com.anthonycoffey.birbverb
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    VST3_CATEGORIES           Fx Reverb
    AU_MAIN_TYPE              kAudioUnitType_Effect)

juce_generate_juce_header(birbverb)

target_sources(birbverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(birbverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(birbverb
    PRIVATE
        bv_dsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Sprite assets ------------------------------------------------------------
# Compiled into the binary per ADR-003, so the plugin has no runtime file
# dependencies. Every control falls back to procedural drawing when its atlas is
# absent (SPEC-005 requirement 6), so this block stays dormant until art exists
# and the plugin builds and runs correctly in the meantime.
file(GLOB BIRBVERB_SPRITES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/sprites/*.png")
list(APPEND BIRBVERB_SPRITES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/sprites/sprites.json")
if(BIRBVERB_SPRITES)
    juce_add_binary_data(BirbAssets SOURCES ${BIRBVERB_SPRITES})
    target_link_libraries(birbverb PRIVATE BirbAssets)
    target_compile_definitions(birbverb PRIVATE BIRBVERB_HAS_ASSETS=1)
else()
    target_compile_definitions(birbverb PRIVATE BIRBVERB_HAS_ASSETS=0)
endif()

# --- Tests --------------------------------------------------------------------
if(BIRBVERB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Offline tools ------------------------------------------------------------
if(BIRBVERB_BUILD_TOOLS)
    add_executable(bv_render tools/render.cpp)
    target_link_libraries(bv_render PRIVATE bv_dsp)
endif()
