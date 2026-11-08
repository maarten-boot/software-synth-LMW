# SPDX-License-Identifier: AGPL-3.0-or-later
# Download the pinned JUCE archive with retries and SHA-256 check (used by CI before configure).
#   cmake -DDEST=<file.tar.gz> -P cmake/FetchJuceArchive.cmake
# Does nothing if DEST already exists with the right hash (e.g. restored from the CI cache).
# Test hooks: -DURL=<other url>, -DATTEMPTS=<n>, -DWAIT=<seconds per attempt step>.
# Note: file(DOWNLOAD ... EXPECTED_HASH) aborts CMake on a failed download, so the hash is
# checked separately below — otherwise the retry loop would never retry.
include("${CMAKE_CURRENT_LIST_DIR}/JuceVersion.cmake")
if(URL)
    set(LMW_JUCE_URL "${URL}")
endif()
if(NOT ATTEMPTS)
    set(ATTEMPTS 6)
endif()
if(NOT DEFINED WAIT)
    set(WAIT 10)
endif()
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

foreach(i RANGE 1 ${ATTEMPTS})
    file(DOWNLOAD "${LMW_JUCE_URL}" "${DEST}" STATUS st TLS_VERIFY ON)
    list(GET st 0 code)
    list(GET st 1 why)
    if(code EQUAL 0)
        file(SHA256 "${DEST}" have)
        if(have STREQUAL LMW_JUCE_SHA256)
            message(STATUS "JUCE ${LMW_JUCE_VERSION} downloaded and verified (attempt ${i})")
            return()
        endif()
        set(why "SHA-256 mismatch (got ${have})")
    endif()
    message(STATUS "JUCE download attempt ${i}/${ATTEMPTS} failed: ${why}")
    file(REMOVE "${DEST}")
    if(i LESS ATTEMPTS)
        math(EXPR pause "${WAIT} * ${i}")           # 10, 20, 30, ... s backoff
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep ${pause})
    endif()
endforeach()
message(FATAL_ERROR "JUCE download failed after ${ATTEMPTS} attempts: ${LMW_JUCE_URL}")
