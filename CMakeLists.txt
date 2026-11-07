cmake_minimum_required(VERSION 3.22)
project(LMWSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LMW_BUILD_PLUGIN "Build the JUCE standalone/VST3 targets" ON)
option(LMW_BUILD_TESTS  "Build unit tests" ON)

# Pure C++ libraries (no JUCE) -------------------------------------------
add_library(lmw_ui_core STATIC src/ui/Contrast.cpp)
target_include_directories(lmw_ui_core PUBLIC src)

add_library(lmw_dsp INTERFACE)          # header-only DSP engine
target_include_directories(lmw_dsp INTERFACE src)

# JUCE app / plugin ------------------------------------------------------
if(LMW_BUILD_PLUGIN)
    include(FetchContent)
    if(POLICY CMP0135)                   # extracted files get extraction time (rebuild-safe)
        cmake_policy(SET CMP0135 NEW)
    endif()
    # JUCE 9.0.5, pinned to its commit and verified by SHA-256 (Spec S2.3).
    # A shallow git clone cannot fetch a bare commit hash, so the commit's source archive is used.
    FetchContent_Declare(JUCE
        URL      https://github.com/juce-framework/JUCE/archive/d60b7a78030661574a615b586d9325ba7a7fa7dd.tar.gz
        URL_HASH SHA256=8237b2976871982eb9107fd67b576d95bf02355d99540a0127c4e7c001ecd9f5)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(LMWSynth
        COMPANY_NAME             "LMW"
        PRODUCT_NAME             "LMW-Synth"
        IS_SYNTH                 TRUE
        NEEDS_MIDI_INPUT         TRUE
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        PLUGIN_MANUFACTURER_CODE Lmwb
        PLUGIN_CODE              Lmw1
        FORMATS                  Standalone VST3
        COPY_PLUGIN_AFTER_BUILD  FALSE)

    target_sources(LMWSynth PRIVATE
        src/app/PluginProcessor.cpp
        src/app/PluginEditor.cpp)

    target_compile_definitions(LMWSynth PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(LMWSynth
        PRIVATE
            lmw_ui_core
            lmw_dsp
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# Tests ------------------------------------------------------------------
if(LMW_BUILD_TESTS)
    enable_testing()
    add_executable(test_contrast tests/test_contrast.cpp)
    target_link_libraries(test_contrast PRIVATE lmw_ui_core)
    add_test(NAME T-S6.2-contrast COMMAND test_contrast)
    add_executable(test_engine tests/test_engine.cpp)
    target_link_libraries(test_engine PRIVATE lmw_dsp)
    add_test(NAME T-S5-engine COMMAND test_engine)
    add_executable(test_paramtext tests/test_paramtext.cpp)
    target_link_libraries(test_paramtext PRIVATE lmw_ui_core)
    add_test(NAME T-S6.1-text COMMAND test_paramtext)
    add_executable(test_spsc tests/test_spsc.cpp)
    target_link_libraries(test_spsc PRIVATE lmw_dsp)
    add_test(NAME T-S10.2-spsc COMMAND test_spsc)
endif()
