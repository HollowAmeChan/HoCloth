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
option(HOCLOTH_ENABLE_PHYSX "Enable PhysX-backed runtime path when dependencies are available" OFF)
set(HOCLOTH_PYTHON_EXECUTABLE "" CACHE FILEPATH "Exact Python executable used to build the hocloth_native module")
set(HOCLOTH_PYTHON_VERSION "3.11" CACHE STRING "Required Python ABI version for Blender compatibility")
set(HOCLOTH_PHYSX_ROOT "${HOCLOTH_PLUGIN_ROOT}/_third_party/PhysX/physx" CACHE PATH "PhysX source or SDK root")
set(HOCLOTH_PHYSX_INCLUDE_DIR "${HOCLOTH_PHYSX_ROOT}/include" CACHE PATH "PhysX include directory")
set(HOCLOTH_PHYSX_LIBRARY_DIR "" CACHE PATH "Directory containing PhysX libraries")
set(HOCLOTH_PHYSX_LIBRARIES "" CACHE STRING "Semicolon-separated list of PhysX libraries to link")

message(STATUS "HoCloth native module skeleton configured.")
message(STATUS "HoCloth runtime output directory: ${HOCLOTH_RUNTIME_DIR}")
message(STATUS "HoCloth intermediate output directory: ${HOCLOTH_INTERMEDIATE_DIR}")
message(STATUS "HoCloth PhysX path enabled: ${HOCLOTH_ENABLE_PHYSX}")

set(HOCLOTH_NANOBIND_DIR "${HOCLOTH_PLUGIN_ROOT}/_third_party/nanobind")

if (EXISTS "${HOCLOTH_NANOBIND_DIR}/CMakeLists.txt")
    if (HOCLOTH_PYTHON_EXECUTABLE)
        set(Python_EXECUTABLE "${HOCLOTH_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python executable for hocloth_native" FORCE)
    endif()

    set(Python_FIND_STRATEGY LOCATION)
    set(Python_FIND_REGISTRY FIRST)
    set(Python_FIND_IMPLEMENTATIONS CPython)

    # Ensure Python::Module exists before nanobind config runs. This avoids
    # generator-time failures on some Windows CMake/Python combinations where
    # nanobind's own Development.Module probe does not provide the target.
    find_package(Python ${HOCLOTH_PYTHON_VERSION} EXACT REQUIRED COMPONENTS Interpreter Development)

    message(STATUS "HoCloth Python executable: ${Python_EXECUTABLE}")
    message(STATUS "HoCloth Python include dir: ${Python_INCLUDE_DIRS}")
    message(STATUS "HoCloth Python libraries: ${Python_LIBRARIES}")
    message(STATUS "HoCloth Python SOABI: ${Python_SOABI}")

    add_subdirectory("${HOCLOTH_NANOBIND_DIR}" "${CMAKE_BINARY_DIR}/nanobind")

    add_library(hocloth_runtime_core STATIC
        "${HOCLOTH_NATIVE_ROOT}/src/hocloth_runtime_api.cpp"
        "${HOCLOTH_NATIVE_ROOT}/src/hocloth_physx_builder.cpp"
    )
    target_include_directories(hocloth_runtime_core PUBLIC "${HOCLOTH_NATIVE_ROOT}/include")
    if (HOCLOTH_ENABLE_PHYSX)
        if (EXISTS "${HOCLOTH_PHYSX_INCLUDE_DIR}/PxPhysicsAPI.h")
            target_include_directories(hocloth_runtime_core PRIVATE "${HOCLOTH_PHYSX_INCLUDE_DIR}")
            target_compile_definitions(hocloth_runtime_core PUBLIC HOCLOTH_WITH_PHYSX=1)

            if (HOCLOTH_PHYSX_LIBRARY_DIR)
                target_link_directories(hocloth_runtime_core PRIVATE "${HOCLOTH_PHYSX_LIBRARY_DIR}")
            endif()

            if (HOCLOTH_PHYSX_LIBRARIES)
                target_link_libraries(hocloth_runtime_core PRIVATE ${HOCLOTH_PHYSX_LIBRARIES})
            else()
                message(WARNING "HOCLOTH_ENABLE_PHYSX is ON but HOCLOTH_PHYSX_LIBRARIES is empty. The source path is enabled, but final linking may fail until PhysX libs are configured.")
            endif()
        else()
            message(WARNING "HOCLOTH_ENABLE_PHYSX is ON but PxPhysicsAPI.h was not found in ${HOCLOTH_PHYSX_INCLUDE_DIR}. Falling back to stub backend at compile time.")
        endif()
    endif()

    nanobind_add_module(hocloth_native
        "${HOCLOTH_NATIVE_ROOT}/src/hocloth_python_module.cpp"
    )
    target_link_libraries(hocloth_native PRIVATE hocloth_runtime_core)
    target_include_directories(hocloth_native PRIVATE "${HOCLOTH_NATIVE_ROOT}/include")
    set_target_properties(hocloth_native PROPERTIES OUTPUT_NAME "hocloth_native")
else()
    message(WARNING "nanobind checkout not found at ${HOCLOTH_NANOBIND_DIR}; native Python module target was skipped.")
endif()
