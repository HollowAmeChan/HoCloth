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

set(HOCLOTH_NANOBIND_DIR "${HOCLOTH_PLUGIN_ROOT}/_third_party/nanobind")

if (EXISTS "${HOCLOTH_NANOBIND_DIR}/CMakeLists.txt")
    # Ensure Python::Module exists before nanobind config runs. This avoids
    # generator-time failures on some Windows CMake/Python combinations where
    # nanobind's own Development.Module probe does not provide the target.
    find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development)

    add_subdirectory("${HOCLOTH_NANOBIND_DIR}" "${CMAKE_BINARY_DIR}/nanobind")

    add_library(hocloth_runtime_core STATIC
        "${HOCLOTH_NATIVE_ROOT}/src/hocloth_runtime_api.cpp"
    )
    target_include_directories(hocloth_runtime_core PUBLIC "${HOCLOTH_NATIVE_ROOT}/include")

    nanobind_add_module(hocloth_native
        "${HOCLOTH_NATIVE_ROOT}/src/hocloth_python_module.cpp"
    )
    target_link_libraries(hocloth_native PRIVATE hocloth_runtime_core)
    target_include_directories(hocloth_native PRIVATE "${HOCLOTH_NATIVE_ROOT}/include")
    set_target_properties(hocloth_native PROPERTIES OUTPUT_NAME "hocloth_native")
else()
    message(WARNING "nanobind checkout not found at ${HOCLOTH_NANOBIND_DIR}; native Python module target was skipped.")
endif()
