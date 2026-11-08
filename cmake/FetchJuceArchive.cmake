# SPDX-License-Identifier: AGPL-3.0-or-later
# Download the pinned JUCE archive with retries and SHA-256 check (used by CI before configure).
#   cmake -DDEST=<file.tar.gz> -P cmake/FetchJuceArchive.cmake
# Does nothing if DEST already exists with the right hash (e.g. restored from the CI cache).
include("${CMAKE_CURRENT_LIST_DIR}/JuceVersion.cmake")
if(NOT DEST)
    message(FATAL_ERROR "usage: cmake -DDEST=<file.tar.gz> -P FetchJuceArchive.cmake")
endif()

if(EXISTS "${DEST}")
    file(SHA256 "${DEST}" have)
    if(have STREQUAL LMW_JUCE_SHA256)
        message(STATUS "JUCE ${LMW_JUCE_VERSION} archive present and verified: ${DEST}")
        return()
    endif()
    message(STATUS "JUCE archive hash mismatch, downloading again")
    file(REMOVE "${DEST}")
endif()

set(attempts 6)
foreach(i RANGE 1 ${attempts})
    file(DOWNLOAD "${LMW_JUCE_URL}" "${DEST}" STATUS st TLS_VERIFY ON
         EXPECTED_HASH SHA256=${LMW_JUCE_SHA256})
    list(GET st 0 code)
    if(code EQUAL 0)
        message(STATUS "JUCE ${LMW_JUCE_VERSION} downloaded and verified (attempt ${i})")
        return()
    endif()
    list(GET st 1 why)
    message(STATUS "JUCE download attempt ${i}/${attempts} failed: ${why}")
    file(REMOVE "${DEST}")
    if(i LESS attempts)
        math(EXPR wait "10 * ${i}")                 # 10, 20, 30, ... s backoff
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep ${wait})
    endif()
endforeach()
message(FATAL_ERROR "JUCE download failed after ${attempts} attempts: ${LMW_JUCE_URL}")
