cmake_minimum_required(VERSION 3.16)

project(TeknofestSancak LANGUAGES CXX)

# Enable CTest at the top-level so `ctest` can be run from the preset binaryDir.
include(CTest)
enable_testing()

# Root-level wrapper so CMake Presets can configure from repo root.
# The actual C++ vision project lives under raspi/.
add_subdirectory(raspi)
