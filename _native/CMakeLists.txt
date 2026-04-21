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
foreach(_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_config}" _config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config_upper} "${HOCLOTH_RUNTIME_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config_upper} "${HOCLOTH_RUNTIME_DIR}")
endforeach()

# Static libraries and other non-runtime artifacts stay in the build tree.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${HOCLOTH_INTERMEDIATE_DIR}")
foreach(_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_config}" _config_upper)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config_upper} "${HOCLOTH_INTERMEDIATE_DIR}")
endforeach()
option(HOCLOTH_ENABLE_PHYSX "Enable PhysX-backed runtime path when dependencies are available" OFF)
set(HOCLOTH_PYTHON_EXECUTABLE "" CACHE FILEPATH "Exact Python executable used to build the hocloth_native module")
set(HOCLOTH_PYTHON_VERSION "3.11" CACHE STRING "Required Python ABI version for Blender compatibility")
set(HOCLOTH_PHYSX_ROOT "${HOCLOTH_PLUGIN_ROOT}/_third_party/PhysX/physx" CACHE PATH "PhysX source or SDK root")
set(HOCLOTH_PHYSX_INCLUDE_DIR "${HOCLOTH_PHYSX_ROOT}/include" CACHE PATH "PhysX include directory")
set(HOCLOTH_PHYSX_LIBRARY_DIR "" CACHE PATH "Directory containing PhysX libraries")
set(HOCLOTH_PHYSX_LIBRARIES "" CACHE STRING "Semicolon-separated list of PhysX libraries to link")
set(HOCLOTH_PHYSX_WORK_ROOT "${HOCLOTH_PLUGIN_ROOT}/_third_party/PhysX/physx" CACHE PATH "PhysX build workspace used by build_physx.ps1")

if (MSVC AND HOCLOTH_ENABLE_PHYSX AND HOCLOTH_PHYSX_LIBRARY_DIR MATCHES "\\.mt([/\\\\]|$)")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    message(STATUS "HoCloth MSVC runtime library: ${CMAKE_MSVC_RUNTIME_LIBRARY} (matched PhysX .mt build)")
endif()

message(STATUS "HoCloth native module skeleton configured.")
message(STATUS "HoCloth runtime output directory: ${HOCLOTH_RUNTIME_DIR}")
message(STATUS "HoCloth intermediate output directory: ${HOCLOTH_INTERMEDIATE_DIR}")
message(STATUS "HoCloth PhysX path enabled: ${HOCLOTH_ENABLE_PHYSX}")

set(HOCLOTH_NANOBIND_DIR "${HOCLOTH_PLUGIN_ROOT}/_third_party/nanobind")

function(hocloth_configure_physx target_name)
    if (NOT EXISTS "${HOCLOTH_PHYSX_INCLUDE_DIR}/PxPhysicsAPI.h")
        message(WARNING "HOCLOTH_ENABLE_PHYSX is ON but PxPhysicsAPI.h was not found in ${HOCLOTH_PHYSX_INCLUDE_DIR}. Falling back to stub backend at compile time.")
        return()
    endif()

    set(_physx_library_dir "${HOCLOTH_PHYSX_LIBRARY_DIR}")
    if (NOT _physx_library_dir)
        set(_candidate_dirs
            "${HOCLOTH_PHYSX_WORK_ROOT}/bin/win.x86_64.vc143.mt/release"
            "${HOCLOTH_PHYSX_WORK_ROOT}/bin/win.x86_64.vc143.md/release"
            "${HOCLOTH_PHYSX_WORK_ROOT}/bin/win.x86_64.vc142.mt/release"
            "${HOCLOTH_PHYSX_WORK_ROOT}/bin/win.x86_64.vc142.md/release"
        )
        foreach(_candidate IN LISTS _candidate_dirs)
            if (EXISTS "${_candidate}/PhysX_64.lib")
                set(_physx_library_dir "${_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if (NOT _physx_library_dir)
        message(WARNING
            "HOCLOTH_ENABLE_PHYSX is ON but no PhysX library directory was found.\n"
            "Expected a build under ${HOCLOTH_PHYSX_WORK_ROOT}/bin/...\n"
            "Run build_physx.ps1 first or set HOCLOTH_PHYSX_LIBRARY_DIR manually."
        )
        return()
    endif()

    set(_physx_libraries ${HOCLOTH_PHYSX_LIBRARIES})
    if (NOT _physx_libraries)
        set(_default_physx_libraries
            PhysX_64.lib
            PhysXCommon_64.lib
            PhysXFoundation_64.lib
            PhysXCooking_64.lib
            PhysXExtensions_static_64.lib
            PhysXPvdSDK_static_64.lib
        )
        set(_resolved_physx_libraries "")
        foreach(_library IN LISTS _default_physx_libraries)
            if (EXISTS "${_physx_library_dir}/${_library}")
                list(APPEND _resolved_physx_libraries "${_physx_library_dir}/${_library}")
            else()
                message(WARNING "Expected PhysX library was not found: ${_physx_library_dir}/${_library}")
            endif()
        endforeach()
        set(_physx_libraries ${_resolved_physx_libraries})
    endif()

    if (NOT _physx_libraries)
        message(WARNING
            "HOCLOTH_ENABLE_PHYSX is ON but no PhysX libraries were resolved.\n"
            "Library dir: ${_physx_library_dir}"
        )
        return()
    endif()

    target_include_directories(${target_name} PRIVATE "${HOCLOTH_PHYSX_INCLUDE_DIR}")
    target_compile_definitions(${target_name} PUBLIC HOCLOTH_WITH_PHYSX=1)
    target_link_directories(${target_name} PRIVATE "${_physx_library_dir}")
    target_link_libraries(${target_name} PRIVATE ${_physx_libraries})

    set(_physx_runtime_dlls
        PhysX_64.dll
        PhysXCommon_64.dll
        PhysXFoundation_64.dll
        PhysXCooking_64.dll
    )
    foreach(_dll IN LISTS _physx_runtime_dlls)
        if (EXISTS "${_physx_library_dir}/${_dll}")
            add_custom_command(
                TARGET hocloth_native POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_physx_library_dir}/${_dll}"
                    "${HOCLOTH_RUNTIME_DIR}/${_dll}"
                VERBATIM
            )
        endif()
    endforeach()

    message(STATUS "HoCloth PhysX include dir: ${HOCLOTH_PHYSX_INCLUDE_DIR}")
    message(STATUS "HoCloth PhysX library dir: ${_physx_library_dir}")
    message(STATUS "HoCloth PhysX libraries: ${_physx_libraries}")
endfunction()

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
        if (NOT MSVC)
            message(FATAL_ERROR
                "HOCLOTH_ENABLE_PHYSX=ON currently requires the MSVC toolchain.\n"
                "Current compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER}\n"
                "If you are using CLion, switch the CMake profile to a Visual Studio/MSVC toolchain,\n"
                "then re-configure with a vs2022-* preset."
            )
        endif()
    endif()

    nanobind_add_module(hocloth_native
        "${HOCLOTH_NATIVE_ROOT}/src/hocloth_python_module.cpp"
    )
    target_link_libraries(hocloth_native PRIVATE hocloth_runtime_core)
    target_include_directories(hocloth_native PRIVATE "${HOCLOTH_NATIVE_ROOT}/include")
    set_target_properties(hocloth_native PROPERTIES OUTPUT_NAME "hocloth_native")

    if (HOCLOTH_ENABLE_PHYSX)
        hocloth_configure_physx(hocloth_runtime_core)
    endif()
else()
    message(WARNING "nanobind checkout not found at ${HOCLOTH_NANOBIND_DIR}; native Python module target was skipped.")
endif()
