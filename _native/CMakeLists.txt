cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HOCLOTH_PLUGIN_ROOT "${PROJECT_SOURCE_DIR}")
set(HOCLOTH_NATIVE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(HOCLOTH_RUNTIME_DIR "${HOCLOTH_PLUGIN_ROOT}/_bin" CACHE PATH "Runtime output directory used by the Blender addon")
set(HOCLOTH_INTERMEDIATE_DIR "${CMAKE_BINARY_DIR}/native_artifacts" CACHE PATH "Intermediate native build output directory")

file(MAKE_DIRECTORY "${HOCLOTH_RUNTIME_DIR}")
file(MAKE_DIRECTORY "${HOCLOTH_INTERMEDIATE_DIR}")

# Runtime-facing modules and DLLs should land in the addon root so Blender can
# load them without depending on the CMake build tree layout.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${HOCLOTH_RUNTIME_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${HOCLOTH_RUNTIME_DIR}")

# Static libraries and other non-runtime artifacts stay in the build tree.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${HOCLOTH_INTERMEDIATE_DIR}")

message(STATUS "HoCloth native module skeleton configured.")
message(STATUS "HoCloth runtime output directory: ${HOCLOTH_RUNTIME_DIR}")
message(STATUS "HoCloth intermediate output directory: ${HOCLOTH_INTERMEDIATE_DIR}")

# Placeholder target layout:
# - Find or add nanobind from third_party/nanobind
# - Find or add PhysX from third_party/PhysX
# - Build a narrow Python extension surface for build/runtime/cache APIs
#
# Any future MODULE/SHARED targets added here will inherit the runtime output
# path above and land in _bin/ by default.
