#==============================================================================
# FLUXCORE·12 — Transformer Core Modeler
# Top-level build for the JUCE audio plugin (VST3 + Standalone) plus the
# native, JUCE-free DSP test target.
#==============================================================================
cmake_minimum_required(VERSION 3.22)

project(FLUXCORE12 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FLUXCORE_BUILD_TESTS "Build the native JUCE-free DSP test target" ON)

#------------------------------------------------------------------------------
# JUCE
#
# By default JUCE is fetched from GitHub (tag 8.0.4). To build against a local
# checkout instead, drop a copy of JUCE into ./JUCE (e.g. a git submodule) and
# it will be preferred automatically — handy for offline / pinned CI builds.
#------------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    message(STATUS "FLUXCORE12: using local JUCE at ${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    add_subdirectory(JUCE)
else()
    message(STATUS "FLUXCORE12: fetching JUCE 8.0.4 from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

#------------------------------------------------------------------------------
# The plugin target
#------------------------------------------------------------------------------
juce_add_plugin(FLUXCORE12
    COMPANY_NAME                "TEKK Engineering"
    BUNDLE_ID                   "com.tekkaudio.fluxcore12"
    PRODUCT_NAME                "FLUXCORE-12"
    PLUGIN_MANUFACTURER_CODE    Tekk
    PLUGIN_CODE                 Fx12
    FORMATS                     VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    DESCRIPTION                 "Transformer Core Modeler")

# Only these two translation units are compiled; all DSP/GUI logic is header-only.
target_sources(FLUXCORE12 PRIVATE
    source/plugin/PluginProcessor.cpp
    source/plugin/PluginEditor.cpp)

target_compile_definitions(FLUXCORE12 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(FLUXCORE12
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

target_include_directories(FLUXCORE12 PRIVATE source)

#------------------------------------------------------------------------------
# Native DSP tests (no JUCE dependency).
#
# tests/test_main.cpp is completely JUCE-free: it includes ../source/dsp/Engine.h
# with the repo root on the include path. Keep this target independent of JUCE so
# the DSP can be verified without configuring/fetching the framework.
#------------------------------------------------------------------------------
if(FLUXCORE_BUILD_TESTS)
    enable_testing()

    add_executable(fluxcore_tests tests/test_main.cpp)
    target_include_directories(fluxcore_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(fluxcore_tests PRIVATE cxx_std_17)

    add_test(NAME fluxcore_tests COMMAND fluxcore_tests)
endif()
