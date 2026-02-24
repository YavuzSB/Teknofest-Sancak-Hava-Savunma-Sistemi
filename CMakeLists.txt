cmake_minimum_required(VERSION 3.16)

project(TeknofestSancak LANGUAGES CXX)

# Enable CTest at the top-level so `ctest` can be run from the preset binaryDir.
include(CTest)
enable_testing()

if(WIN32)
	set(_SANCAK_BUILD_RASPI_DEFAULT OFF)
	set(_SANCAK_BUILD_PC_DEFAULT ON)
else()
	set(_SANCAK_BUILD_RASPI_DEFAULT ON)
	set(_SANCAK_BUILD_PC_DEFAULT OFF)
endif()

set(SANCAK_BUILD_RASPI ${_SANCAK_BUILD_RASPI_DEFAULT} CACHE BOOL "Build raspi vision/combat project")
set(SANCAK_BUILD_PC    ${_SANCAK_BUILD_PC_DEFAULT}    CACHE BOOL "Build PC GCS project")

# Root-level wrapper so CMake Presets can configure from repo root.
# Projects live under raspi/ (vision/combat) and PC/ (GCS).

if(SANCAK_BUILD_RASPI)
	add_subdirectory(raspi)
endif()

if(SANCAK_BUILD_PC)
	add_subdirectory(PC)
endif()
