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

# JUCE app / plugin ------------------------------------------------------
if(LMW_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(LMWSynth
        COMPANY_NAME             "LMW"
        PRODUCT_NAME             "LMW Synth"
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
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(LMWSynth
        PRIVATE
            lmw_ui_core
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
endif()
