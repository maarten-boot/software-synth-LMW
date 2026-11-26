cmake_minimum_required(VERSION 3.22)
project(LMWSynth VERSION 1.6.0 LANGUAGES C CXX)   # Spec S6.10: raised with every release
include(cmake/TutorialText.cmake)   # Spec S7.9: docs/tutorial/Tutorial.md -> generated/TutorialText.h
include(cmake/GuideText.cmake)      # Spec S11.5: docs/ai/SoundDesign.md -> generated/GuideText.h
include(cmake/ChangesText.cmake)    # Spec S6.11: architecture/Changes.md -> generated/ChangesText.h
include(cmake/TermsText.cmake)      # Spec S7.10: docs/Terms.md -> generated/TermsText.h

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
# lmw-mcp: the MCP server for AI assistants (Spec S11), tools/lmw-mcp.cpp
add_executable(lmw-mcp tools/lmw-mcp.cpp src/ninep/Transport.cpp)
target_include_directories(lmw-mcp PRIVATE src ${CMAKE_BINARY_DIR}/generated)
target_compile_features(lmw-mcp PRIVATE cxx_std_20)
target_compile_definitions(lmw-mcp PRIVATE LMW_MCP_VERSION="${PROJECT_VERSION}")
target_link_libraries(lmw-mcp PRIVATE Threads::Threads $<$<PLATFORM_ID:Windows>:ws2_32 shell32 ole32>)

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
        NEEDS_CURL               TRUE       # Spec S12.2: HTTPS for the Assistant tab on Linux
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        PLUGIN_MANUFACTURER_CODE Lmwb
        PLUGIN_CODE              Lmw1
        FORMATS                  Standalone VST3
        COPY_PLUGIN_AFTER_BUILD  FALSE)

    # Spec S6.10: version and git commit, shown in the title bar, the About dialog and 9P /version.
    add_custom_target(lmw_version
        COMMAND ${CMAKE_COMMAND} -DVERSION=${PROJECT_VERSION} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                -DOUT=${CMAKE_BINARY_DIR}/generated/LMWVersion.h -P ${CMAKE_SOURCE_DIR}/cmake/LMWVersion.cmake
        BYPRODUCTS ${CMAKE_BINARY_DIR}/generated/LMWVersion.h
        COMMENT "LMW-Synth version" VERBATIM)
    add_dependencies(LMWSynth lmw_version)
    target_include_directories(LMWSynth PRIVATE ${CMAKE_BINARY_DIR}/generated)

    target_sources(LMWSynth PRIVATE
        src/app/PluginProcessor.cpp
        src/app/PluginEditor.cpp
        src/app/MatrixView.cpp
        src/app/SchematicView.cpp
        src/app/TermsView.cpp
        src/app/StandaloneApp.cpp
        src/app/NinePService.cpp
        src/app/AssistantSession.cpp
        src/app/AssistantView.cpp
        src/app/KeyStore.cpp
        src/ninep/Transport.cpp)

    target_compile_definitions(LMWSynth PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1      # Spec S12.2: libcurl (NEEDS_CURL) loaded when the Assistant first uses HTTPS
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
    if(APPLE)
        target_link_libraries(LMWSynth PRIVATE "-framework Security")   # Spec S12.6: the key in the Keychain
    endif()
    target_link_libraries(LMWSynth PRIVATE ${CMAKE_DL_LIBS})           # Spec S12.6: libsecret loaded at run time
    if(WIN32)
        target_link_libraries(LMWSynth PRIVATE ws2_32 advapi32)   # 9P sockets; the key in the Credential Manager (S12.6)
        # Spec S9.5: Windows installer, `cmake --build <build> --config Release --target installer` when
        # NSIS 3 is found. makensis gets native paths (backslashes; see installer/LMW-Synth.nsi).
        find_program(LMW_MAKENSIS makensis PATHS "C:/Program Files (x86)/NSIS" "C:/Program Files/NSIS")
        find_package(Python3 COMPONENTS Interpreter)   # Spec S9.8: the documents as HTML
        if(LMW_MAKENSIS AND Python3_Interpreter_FOUND)
            set(lmw_stage ${CMAKE_BINARY_DIR}/installer-stage)
            set(lmw_setup ${CMAKE_BINARY_DIR}/LMW-Synth-${PROJECT_VERSION}-setup.exe)
            file(TO_NATIVE_PATH "${lmw_stage}" lmw_stage_native)
            file(TO_NATIVE_PATH "${lmw_setup}" lmw_setup_native)
            add_custom_target(installer
                COMMAND ${CMAKE_COMMAND} -E rm -rf ${lmw_stage}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${lmw_stage}
                COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:LMWSynth_Standalone> $<TARGET_FILE:lmw9p> $<TARGET_FILE:lmw-mcp> ${lmw_stage}/
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "$<TARGET_GENEX_EVAL:LMWSynth_VST3,$<TARGET_PROPERTY:LMWSynth_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
                        ${lmw_stage}/LMW-Synth.vst3
                COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${lmw_stage}/LICENSE.txt
                COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/scripts/md2html.py ${lmw_stage}/docs
                COMMAND ${LMW_MAKENSIS} -V2 -DVERSION=${PROJECT_VERSION} -DSTAGE=${lmw_stage_native}
                        -DOUTFILE=${lmw_setup_native} ${CMAKE_SOURCE_DIR}/installer/LMW-Synth.nsi
                DEPENDS LMWSynth_Standalone LMWSynth_VST3 lmw9p lmw-mcp
                COMMENT "Building ${lmw_setup}"
                VERBATIM)
        endif()
    endif()

    # T-S9.2-host (Phase 8): loads the built VST3 through JUCE's VST3 hosting, as a DAW would.
    juce_add_console_app(LMWHostTest PRODUCT_NAME "LMWHostTest")
    target_sources(LMWHostTest PRIVATE tests/host/HostTest.cpp src/ninep/Transport.cpp)   # 9P client (T-S10.12)
    target_include_directories(LMWHostTest PRIVATE src)
    # JUCE_MODAL_LOOPS_PERMITTED: the test runs the message loop for a while (runDispatchLoopUntil, T-S10.12).
    target_compile_definitions(LMWHostTest PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        LMW_MCP_PATH="$<TARGET_FILE:lmw-mcp>")              # Spec S11: lmw-mcp end to end
    add_dependencies(LMWHostTest lmw-mcp)
    target_link_libraries(LMWHostTest PRIVATE $<$<PLATFORM_ID:Windows>:ws2_32>
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
    add_executable(test_schematic tests/test_schematic.cpp)
    target_link_libraries(test_schematic PRIVATE lmw_ui_core)
    add_test(NAME T-S6.7-schematic COMMAND test_schematic)
    add_executable(test_rack tests/test_rack.cpp)
    target_link_libraries(test_rack PRIVATE lmw_ui_core)
    add_test(NAME T-S6.5-layout COMMAND test_rack)
    add_executable(test_history tests/test_history.cpp)
    target_link_libraries(test_history PRIVATE lmw_ui_core)
    add_test(NAME T-S6.14-history COMMAND test_history)
    add_executable(test_scale tests/test_scale.cpp)
    target_link_libraries(test_scale PRIVATE lmw_ui_core)
    add_test(NAME T-S6.17-scale COMMAND test_scale)
    add_executable(test_ensemble tests/test_ensemble.cpp)
    target_include_directories(test_ensemble PRIVATE src)
    add_test(NAME T-S7.11-ensemble COMMAND test_ensemble)
    add_executable(test_terms tests/test_terms.cpp)
    target_include_directories(test_terms PRIVATE src ${CMAKE_BINARY_DIR}/generated)
    add_test(NAME T-S7.10-terms COMMAND test_terms)
    find_package(Python3 COMPONENTS Interpreter)    # Spec S9.8: the documents as HTML (no other packages)
    if(Python3_Interpreter_FOUND)
        add_test(NAME T-S9.8-html COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tests/test_md2html.py
                 ${CMAKE_BINARY_DIR}/test-html)
    endif()
    if(NOT WIN32)                                   # Spec S9.7: the cleanup monitor's selection (bash)
        add_test(NAME T-S9.7-cleanup COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_cleanup.sh)
    endif()
    add_executable(test_presets tests/test_presets.cpp)
    target_include_directories(test_presets PRIVATE src)
    add_test(NAME T-S7.5-presets COMMAND test_presets)
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
    add_executable(test_formant tests/test_formant.cpp)
    target_link_libraries(test_formant PRIVATE lmw_dsp)
    add_test(NAME T-S5.16-formant COMMAND test_formant)
    add_executable(test_routing tests/test_routing.cpp)
    target_link_libraries(test_routing PRIVATE lmw_dsp)
    add_test(NAME T-S5.17-routing COMMAND test_routing)
    add_executable(test_lforate tests/test_lforate.cpp)
    target_link_libraries(test_lforate PRIVATE lmw_dsp)
    add_test(NAME T-S5.19-lforate COMMAND test_lforate)
    add_executable(test_lfodelay tests/test_lfodelay.cpp)
    target_link_libraries(test_lfodelay PRIVATE lmw_dsp)
    add_test(NAME T-S5.24-lfodelay COMMAND test_lfodelay)
    add_executable(test_pan tests/test_pan.cpp)
    target_link_libraries(test_pan PRIVATE lmw_dsp)
    add_test(NAME T-S5.25-pan COMMAND test_pan)
    add_executable(test_pm tests/test_pm.cpp)
    target_link_libraries(test_pm PRIVATE lmw_dsp)
    add_test(NAME T-S5.20-pm COMMAND test_pm)
    add_executable(test_comb tests/test_comb.cpp)
    target_link_libraries(test_comb PRIVATE lmw_dsp)
    add_test(NAME T-S7.6-comb COMMAND test_comb)
    add_executable(test_delay tests/test_delay.cpp)
    target_link_libraries(test_delay PRIVATE lmw_dsp)
    add_test(NAME T-S7.8-delay COMMAND test_delay)
    add_executable(test_instruments tests/test_instruments.cpp)
    target_include_directories(test_instruments PRIVATE src ${CMAKE_BINARY_DIR}/generated)
    target_link_libraries(test_instruments PRIVATE lmw_dsp)
    add_test(NAME T-S7.7-instruments COMMAND test_instruments)
    add_executable(test_tutorial tests/test_tutorial.cpp)
    target_include_directories(test_tutorial PRIVATE src ${CMAKE_BINARY_DIR}/generated)
    add_test(NAME T-S7.9-tutorial COMMAND test_tutorial)
    add_executable(test_mcp tests/test_mcp.cpp)          # Spec S11: lmw-mcp against a fake synth
    target_include_directories(test_mcp PRIVATE src)
    add_test(NAME T-S11-mcp COMMAND test_mcp)
    add_executable(test_assistant tests/test_assistant.cpp)  # Spec S12: the Assistant tab's Claude client, fake service
    target_include_directories(test_assistant PRIVATE src)
    add_test(NAME T-S12-client COMMAND test_assistant)
    add_executable(test_keystore tests/test_keystore.cpp src/app/KeyStore.cpp)   # Spec S12.6: the system keychain
    target_include_directories(test_keystore PRIVATE src)
    target_link_libraries(test_keystore PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(test_keystore PRIVATE advapi32)
    elseif(APPLE)
        target_link_libraries(test_keystore PRIVATE "-framework Security" "-framework CoreFoundation")
    endif()
    add_test(NAME T-S12-key COMMAND test_keystore)
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
