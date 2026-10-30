cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel), runs on macOS 11 and later.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(HalcyonReverb VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HALCYON_BUILD_TESTS "Build the JUCE-free engine tests" ON)

# ---- Engine (no JUCE) -------------------------------------------------------

add_library(halcyon_engine STATIC Source/dsp/ReverbEngine.cpp)
target_include_directories(halcyon_engine PUBLIC Source)

if(HALCYON_BUILD_TESTS)
    add_executable(engine_tests tests/EngineTests.cpp)
    target_link_libraries(engine_tests PRIVATE halcyon_engine)
    enable_testing()
    add_test(NAME engine_tests COMMAND engine_tests)
endif()

# ---- Plugin ------------------------------------------------------------------

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(WARNING "JUCE not found. Run: git submodule update --init --recursive")
    return()
endif()

add_subdirectory(JUCE)

juce_add_plugin(HalcyonReverb
    PRODUCT_NAME "Halcyon Reverb"
    COMPANY_NAME "creativespxce"
    BUNDLE_ID "com.creativespxce.halcyonreverb"
    PLUGIN_MANUFACTURER_CODE Cspx
    PLUGIN_CODE Hlcy
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(HalcyonReverb PRIVATE
    Source/Parameters.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(HalcyonReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(HalcyonReverb
    PRIVATE
        halcyon_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
