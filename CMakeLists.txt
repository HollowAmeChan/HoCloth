cmake_minimum_required(VERSION 3.24)

project(HoCloth LANGUAGES CXX)

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" HOCLOTH_SOURCE_DIR_NORM)
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}" HOCLOTH_BINARY_DIR_NORM)
set(HOCLOTH_ALLOWED_BUILD_ROOT "${HOCLOTH_SOURCE_DIR_NORM}/_native/build")

# Keep native build trees under _native/build so IDE defaults do not pollute the
# addon root with build/, cmake-build-*, or other generated directories.
if(HOCLOTH_BINARY_DIR_NORM MATCHES "^${HOCLOTH_SOURCE_DIR_NORM}/(build|cmake-build[^/]*)($|/)")
    message(FATAL_ERROR
        "Root-level CMake build directories are not supported for HoCloth.\n"
        "Use a build directory under: ${HOCLOTH_ALLOWED_BUILD_ROOT}\n"
        "Recommended presets live in CMakePresets.json.\n"
        "Example: cmake --preset ninja-release"
    )
endif()

add_subdirectory(_native)
