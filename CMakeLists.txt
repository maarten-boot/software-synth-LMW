cmake_minimum_required(VERSION 3.22)
project(LMWSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LMW_BUILD_PLUGIN "Build the JUCE standalone/VST3 targets" ON)
option(LMW_BUILD_TESTS  "Build unit tests" ON)

# Pure C++ libraries (no JUCE) -------------------------------------------
# lmw9p: small 9P client for trying the 9P server (Spec S10), tools/lmw9p.cpp
add_executable(lmw9p tools/lmw9p.cpp src/ninep/Transport.cpp)
target_include_directories(lmw9p PRIVATE src)
target_compile_features(lmw9p PRIVATE cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(lmw9p PRIVATE Threads::Threads $<$<PLATFORM_ID:Windows>:ws2_32>)

add_library(lmw_ui_core STATIC src/ui/Contrast.cpp)
target_include_directories(lmw_ui_core PUBLIC src)
# Linked into the VST3 shared library: must be position-independent. Ubuntu's GCC does this by
# default, Fedora's does not (ld: "failed to set dynamic section sizes: bad value").
set_target_properties(lmw_ui_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(lmw_dsp INTERFACE)          # header-only DSP engine
target_include_directories(lmw_dsp INTERFACE src)

# JUCE app / plugin ------------------------------------------------------
if(LMW_BUILD_PLUGIN)
    include(FetchContent)
    if(POLICY CMP0135)                   # extracted files get extraction time (rebuild-safe)
        cmake_policy(SET CMP0135 NEW)
    endif()
    # JUCE pinned to a commit and verified by SHA-256 (Spec S2.3); version in cmake/JuceVersion.cmake.
    # LMW_JUCE_ARCHIVE: optional local copy of the same archive (CI cache); the hash is checked either way.
    include(cmake/JuceVersion.cmake)
    set(LMW_JUCE_ARCHIVE "" CACHE FILEPATH "Local JUCE ${LMW_JUCE_VERSION} archive (optional)")
    if(LMW_JUCE_ARCHIVE)
        set(lmw_juce_source "${LMW_JUCE_ARCHIVE}")
    else()
        set(lmw_juce_source "${LMW_JUCE_URL}")
    endif()
    FetchContent_Declare(JUCE
        URL      "${lmw_juce_source}"
        URL_HASH SHA256=${LMW_JUCE_SHA256})
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
        src/app/PluginEditor.cpp
        src/app/MatrixView.cpp
        src/app/StandaloneApp.cpp
        src/app/NinePService.cpp
        src/ninep/Transport.cpp)

    target_compile_definitions(LMWSynth PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)   # src/app/StandaloneApp.cpp: themed window (Spec S6.2)

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
    if(WIN32)
        target_link_libraries(LMWSynth PRIVATE ws2_32)   # 9P sockets
    endif()

    # T-S9.2-host (Phase 8): loads the built VST3 through JUCE's VST3 hosting, as a DAW would.
    juce_add_console_app(LMWHostTest PRODUCT_NAME "LMWHostTest")
    target_sources(LMWHostTest PRIVATE tests/host/HostTest.cpp)
    target_compile_definitions(LMWHostTest PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(LMWHostTest PRIVATE
        juce::juce_audio_processors juce::juce_audio_utils
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_dependencies(LMWHostTest LMWSynth_VST3)
    if(LMW_BUILD_TESTS)
        add_test(NAME T-S9.2-host
                 COMMAND LMWHostTest "$<TARGET_GENEX_EVAL:LMWSynth_VST3,$<TARGET_PROPERTY:LMWSynth_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
    endif()
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
    add_executable(test_modules tests/test_modules.cpp)
    target_link_libraries(test_modules PRIVATE lmw_dsp)
    add_test(NAME T-S5-modules COMMAND test_modules)
    add_executable(test_rack tests/test_rack.cpp)
    target_link_libraries(test_rack PRIVATE lmw_ui_core)
    add_test(NAME T-S6.5-layout COMMAND test_rack)
    add_executable(test_patchtext tests/test_patchtext.cpp)
    target_link_libraries(test_patchtext PRIVATE lmw_dsp)
    add_test(NAME T-S5.4-text COMMAND test_patchtext)
    add_executable(test_keys tests/test_keys.cpp)
    target_link_libraries(test_keys PRIVATE lmw_dsp)
    add_test(NAME T-S5.8-keys COMMAND test_keys)
    add_executable(test_vcfmodes tests/test_vcfmodes.cpp)
    target_link_libraries(test_vcfmodes PRIVATE lmw_dsp)
    add_test(NAME T-S5.9-vcfmodes COMMAND test_vcfmodes)
    add_executable(test_vcos tests/test_vcos.cpp)
    target_link_libraries(test_vcos PRIVATE lmw_dsp)
    add_test(NAME T-S5.10-vcos COMMAND test_vcos)
    add_executable(test_vcf2 tests/test_vcf2.cpp)
    target_link_libraries(test_vcf2 PRIVATE lmw_dsp)
    add_test(NAME T-S5.11-vcf2 COMMAND test_vcf2)
    add_executable(test_lfo2 tests/test_lfo2.cpp)
    target_link_libraries(test_lfo2 PRIVATE lmw_dsp)
    add_test(NAME T-S5.12-lfo2 COMMAND test_lfo2)
    add_executable(test_vcfkey tests/test_vcfkey.cpp)
    target_link_libraries(test_vcfkey PRIVATE lmw_dsp)
    add_test(NAME T-S5.13-vcfkey COMMAND test_vcfkey)
    add_executable(test_pins tests/test_pins.cpp)
    target_link_libraries(test_pins PRIVATE lmw_dsp)
    add_test(NAME T-S5.14-pins COMMAND test_pins)
    add_executable(test_noise tests/test_noise.cpp)
    target_link_libraries(test_noise PRIVATE lmw_dsp)
    add_test(NAME T-S5.15-noise COMMAND test_noise)
    add_executable(test_patchfile tests/test_patchfile.cpp)
    target_include_directories(test_patchfile PRIVATE src)
    add_test(NAME T-S7.1-patchfile COMMAND test_patchfile)
    add_executable(test_ninep tests/test_ninep.cpp)
    target_include_directories(test_ninep PRIVATE src)
    add_test(NAME T-S10.1a-ninep COMMAND test_ninep)
    add_executable(test_ninep_socket tests/test_ninep_socket.cpp src/ninep/Transport.cpp)
    target_include_directories(test_ninep_socket PRIVATE src)
    find_package(Threads REQUIRED)
    target_link_libraries(test_ninep_socket PRIVATE Threads::Threads $<$<PLATFORM_ID:Windows>:ws2_32>)
    add_test(NAME T-S10.4-socket COMMAND test_ninep_socket)
endif()
