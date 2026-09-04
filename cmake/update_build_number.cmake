# Script to auto-increment BUILD_NUMBER in src/BuildNumber.h
if(NOT DEFINED HEADER_FILE)
    message(FATAL_ERROR "HEADER_FILE not defined")
endif()

set(CURRENT_BUILD 83)

if(EXISTS "${HEADER_FILE}")
    file(READ "${HEADER_FILE}" CONTENT)
    string(REGEX MATCH "BUILD_NUMBER ([0-9]+)" MATCH_RES "${CONTENT}")
    if(CMAKE_MATCH_1)
        set(CURRENT_BUILD ${CMAKE_MATCH_1})
    endif()
endif()

math(EXPR NEW_BUILD "${CURRENT_BUILD} + 1")

file(WRITE "${HEADER_FILE}"
"// Generated automatically by cmake/update_build_number.cmake
#pragma once

#define BUILD_NUMBER ${NEW_BUILD}
#define BUILD_NUMBER_STR \"${NEW_BUILD}\"
"
)

message(STATUS "Build number auto-incremented to: ${NEW_BUILD}")
