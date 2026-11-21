# SPDX-License-Identifier: AGPL-3.0-or-later
# Spec S7.9: docs/tutorial/Tutorial.md as C++ text, so the lesson presets carry their lesson's steps in
# their notes and the document stays the one source. Written at configure time; a change to the document
# re-runs the configure step on the next build. The text is split at its "---" rulers (never inside a
# UTF-8 character) into raw string literals, each well below MSVC's 16 KB limit per literal.
set(_lmw_tut_src "${CMAKE_SOURCE_DIR}/docs/tutorial/Tutorial.md")
set(_lmw_tut_out "${CMAKE_BINARY_DIR}/generated/TutorialText.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_lmw_tut_src}")
file(READ "${_lmw_tut_src}" _lmw_rest)
set(_lmw_out "// Generated from docs/tutorial/Tutorial.md by cmake/TutorialText.cmake - do not edit.\n#pragma once\nnamespace lmw::tutorial {\ninline constexpr const char* textParts[] = {\n")
while(TRUE)
    string(FIND "${_lmw_rest}" "\n---\n" _lmw_at)
    if(_lmw_at EQUAL -1)
        string(APPEND _lmw_out "R\"lmwtut(${_lmw_rest})lmwtut\",\n")
        break()
    endif()
    math(EXPR _lmw_cut "${_lmw_at} + 5")
    string(SUBSTRING "${_lmw_rest}" 0 ${_lmw_cut} _lmw_part)
    string(SUBSTRING "${_lmw_rest}" ${_lmw_cut} -1 _lmw_rest)
    string(APPEND _lmw_out "R\"lmwtut(${_lmw_part})lmwtut\",\n")
endwhile()
string(APPEND _lmw_out "};\n} // namespace lmw::tutorial\n")
if(EXISTS "${_lmw_tut_out}")
    file(READ "${_lmw_tut_out}" _lmw_old)
endif()
if(NOT "${_lmw_old}" STREQUAL "${_lmw_out}")
    file(WRITE "${_lmw_tut_out}" "${_lmw_out}")
endif()
