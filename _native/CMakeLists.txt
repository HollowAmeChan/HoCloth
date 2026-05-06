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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${HOCLOTH_RUNTIME_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${HOCLOTH_RUNTIME_DIR}")
foreach(_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_config}" _config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config_upper} "${HOCLOTH_RUNTIME_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config_upper} "${HOCLOTH_RUNTIME_DIR}")
endforeach()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${HOCLOTH_INTERMEDIATE_DIR}")
foreach(_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_config}" _config_upper)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config_upper} "${HOCLOTH_INTERMEDIATE_DIR}")
endforeach()

set(HOCLOTH_PYTHON_EXECUTABLE "" CACHE FILEPATH "Exact Python executable used to build the hocloth_native module")
set(HOCLOTH_PYTHON_VERSION "3.11" CACHE STRING "Required Python ABI version for Blender compatibility")

message(STATUS "HoCloth native module skeleton configured.")
message(STATUS "HoCloth runtime output directory: ${HOCLOTH_RUNTIME_DIR}")
message(STATUS "HoCloth intermediate output directory: ${HOCLOTH_INTERMEDIATE_DIR}")
message(STATUS "HoCloth solver backend: MC2 BoneSpring bootstrap")

set(HOCLOTH_NANOBIND_DIR "${HOCLOTH_PLUGIN_ROOT}/_third_party/nanobind")

add_library(hocloth_mc2_core STATIC
    "${HOCLOTH_NATIVE_ROOT}/src/api/mc2_backend.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/cloth/constraints/distance_constraint.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/cloth/constraints/inertia_constraint.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/cloth/constraints/motion_constraint.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/cloth/constraints/tether_constraint.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/cloth/constraints/triangle_bending_constraint.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/cloth/cloth_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/magica_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/simulation/collider_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/simulation/simulation_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/simulation/time_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/simulation/wind_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/team/team_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/transform/transform_data.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/transform/transform_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/manager/virtual_mesh/virtual_mesh_manager.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/utility/data/data_utility.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/utility/math/math_extensions.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/utility/math/math_utility.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/utility/native_collection/data_chunk.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/utility/native_collection/ex_processing_list.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/utility/result_code/result_code.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/virtual_mesh/virtual_mesh.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/virtual_mesh/virtual_mesh_container.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/virtual_mesh/virtual_mesh_transform.cpp"
)
target_include_directories(hocloth_mc2_core PUBLIC "${HOCLOTH_NATIVE_ROOT}/include")

add_executable(hocloth_mc2_core_smoke
    "${HOCLOTH_NATIVE_ROOT}/tests/native/mc2_core_smoke.cpp"
)
target_link_libraries(hocloth_mc2_core_smoke PRIVATE hocloth_mc2_core)

add_library(hocloth_runtime_core STATIC
    "${HOCLOTH_NATIVE_ROOT}/src/hocloth_collision_world.cpp"
    "${HOCLOTH_NATIVE_ROOT}/src/hocloth_runtime_api.cpp"
)
target_include_directories(hocloth_runtime_core PUBLIC "${HOCLOTH_NATIVE_ROOT}/include")
target_link_libraries(hocloth_runtime_core PUBLIC hocloth_mc2_core)

if (EXISTS "${HOCLOTH_NANOBIND_DIR}/CMakeLists.txt")
    if (HOCLOTH_PYTHON_EXECUTABLE)
        set(Python_EXECUTABLE "${HOCLOTH_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python executable for hocloth_native" FORCE)
    endif()

    set(Python_FIND_STRATEGY LOCATION)
    set(Python_FIND_REGISTRY FIRST)
    set(Python_FIND_IMPLEMENTATIONS CPython)

    find_package(Python ${HOCLOTH_PYTHON_VERSION} EXACT REQUIRED COMPONENTS Interpreter Development)

    message(STATUS "HoCloth Python executable: ${Python_EXECUTABLE}")
    message(STATUS "HoCloth Python include dir: ${Python_INCLUDE_DIRS}")
    message(STATUS "HoCloth Python libraries: ${Python_LIBRARIES}")
    message(STATUS "HoCloth Python SOABI: ${Python_SOABI}")

    add_subdirectory("${HOCLOTH_NANOBIND_DIR}" "${CMAKE_BINARY_DIR}/nanobind")

    nanobind_add_module(hocloth_native
        "${HOCLOTH_NATIVE_ROOT}/src/hocloth_python_module.cpp"
    )
    target_link_libraries(hocloth_native PRIVATE hocloth_runtime_core)
    target_include_directories(hocloth_native PRIVATE "${HOCLOTH_NATIVE_ROOT}/include")
    set_target_properties(hocloth_native PROPERTIES OUTPUT_NAME "hocloth_native")
else()
    message(WARNING "nanobind checkout not found at ${HOCLOTH_NANOBIND_DIR}; native Python module target was skipped.")
endif()
