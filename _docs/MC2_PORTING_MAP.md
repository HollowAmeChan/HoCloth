# MC2 Core Porting Map

Reference root:

```text
_ReferenceProject/MagicaCloth2/Scripts/Core
```

Status labels:

- `planned`: not ported yet.
- `skeleton`: C++ target files or manager shell exist, but behavior is not ported.
- `partial`: some behavior or data ownership exists in the new MC2-style backend.
- `legacy-partial`: related logic exists only in old `_native/src/hocloth_*.cpp` bootstrap code.
- `defer`: Unity/editor/render specific code; reinterpret later at the Blender boundary.

## Module Summary

| MC2 module | Status | HoCloth target |
| --- | --- | --- |
| Define | partial | `_native/include/hocloth/core/define/` |
| Interface | skeleton | `_native/include/hocloth/manager/`, `_native/include/hocloth/core/` |
| Utility/ResultCode | skeleton | `_native/include/hocloth/utility/result_code/` |
| Utility/Math | partial | `_native/include/hocloth/utility/math/` |
| Utility/Data | partial | `_native/include/hocloth/utility/data/` |
| Utility/NativeCollection | partial | `_native/include/hocloth/utility/native_collection/` |
| Utility/Time | skeleton | `_native/include/hocloth/manager/simulation/time_manager.hpp` |
| Manager | partial | `_native/include/hocloth/manager/` |
| Cloth/Constraints | partial | `_native/include/hocloth/cloth/constraints/` |
| Cloth/Collider | planned | `_native/include/hocloth/cloth/collider/` |
| Cloth/Wind | skeleton | `_native/include/hocloth/manager/simulation/wind_manager.hpp` |
| VirtualMesh | partial | `_native/include/hocloth/virtual_mesh/` |
| Reduction | planned | `_native/include/hocloth/reduction/` |
| PreBuild | planned | `_native/include/hocloth/prebuild/` |

## Cloth

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/CheckSliderSerializeData.cs` | planned | `cloth/parameters/check_slider_serialize_data.*` |
| `Cloth/ClothBehaviour.cs` | defer | Blender component/runtime boundary |
| `Cloth/ClothForceMode.cs` | partial | `cloth/cloth_force_mode.hpp` |
| `Cloth/ClothNormalAxis.cs` | partial | `cloth/cloth_normal_axis.hpp` |
| `Cloth/ClothParameters.cs` | partial | `cloth/cloth_parameters.hpp` |
| `Cloth/ClothProcess.cs` | planned | `cloth/cloth_process.*` |
| `Cloth/ClothProcessData.cs` | planned | `cloth/cloth_process_data.*` |
| `Cloth/ClothProcessGeneration.cs` | planned | `cloth/cloth_process_generation.*` |
| `Cloth/ClothSerializeData.cs` | planned | `cloth/cloth_serialize_data.*` |
| `Cloth/ClothSerializeData2.cs` | planned | `cloth/cloth_serialize_data2.*` |
| `Cloth/ClothSerializeDataFunction.cs` | planned | `cloth/cloth_serialize_data_function.*` |
| `Cloth/ClothUpdateMode.cs` | planned | `cloth/cloth_update_mode.hpp` |
| `Cloth/CullingSettings.cs` | defer | Blender viewport/runtime culling boundary |
| `Cloth/CurveSerializeData.cs` | planned | `cloth/parameters/curve_serialize_data.*` |
| `Cloth/CustomSkinningSettings.cs` | planned | `cloth/custom_skinning_settings.*` |
| `Cloth/GizmoSerializeData.cs` | defer | Blender authoring/gizmo layer |
| `Cloth/MagicaCloth.cs` | defer | Blender component wrapper |
| `Cloth/MagicaClothAnimationProperty.cs` | planned | `cloth/animation_property.*` |
| `Cloth/MagicaClothAPI.cs` | defer | native API + Python bridge |
| `Cloth/NormalAlignmentSettings.cs` | planned | `cloth/normal_alignment_settings.*` |
| `Cloth/SelectionData.cs` | planned | `cloth/selection_data.*` |

## Cloth/Collider

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Collider/ColliderComponent.cs` | planned | `cloth/collider/collider_component.*` |
| `Cloth/Collider/MagicaCapsuleCollider.cs` | planned | `cloth/collider/capsule_collider.*` |
| `Cloth/Collider/MagicaPlaneCollider.cs` | planned | `cloth/collider/plane_collider.*` |
| `Cloth/Collider/MagicaSphereCollider.cs` | planned | `cloth/collider/sphere_collider.*` |

## Cloth/Constraints

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Constraints/AngleConstraint.cs` | legacy-partial | `cloth/constraints/angle_constraint.*` |
| `Cloth/Constraints/ColliderCollisionConstraint.cs` | legacy-partial | `cloth/constraints/collider_collision_constraint.*` |
| `Cloth/Constraints/DistanceConstraint.cs` | partial | `cloth/constraints/distance_constraint.*` |
| `Cloth/Constraints/InertiaConstraint.cs` | partial | `cloth/constraints/inertia_constraint.*` |
| `Cloth/Constraints/MotionConstraint.cs` | partial | `cloth/constraints/motion_constraint.*` |
| `Cloth/Constraints/SelfCollisionConstraint.cs` | planned | `cloth/constraints/self_collision_constraint.*` |
| `Cloth/Constraints/SpringConstraint.cs` | partial | `cloth/cloth_parameters.hpp`, fixed-particle branch in `manager/simulation/simulation_manager.*` |
| `Cloth/Constraints/TetherConstraint.cs` | legacy-partial | `cloth/constraints/tether_constraint.*` |
| `Cloth/Constraints/TriangleBendingConstraint.cs` | planned | `cloth/constraints/triangle_bending_constraint.*` |

## Cloth/Wind

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Wind/MagicaWindZone.cs` | planned | `cloth/wind/magica_wind_zone.*` |
| `Cloth/Wind/WindParams.cs` | planned | `cloth/wind/wind_params.*` |
| `Cloth/Wind/WindSettings.cs` | planned | `cloth/wind/wind_settings.*` |

## Define / Interface

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Define/ResultDefine.cs` | skeleton | `utility/result_code/result_code.*` |
| `Define/SystemDefine.cs` | partial | `core/define/system_define.hpp`, `manager/simulation/time_manager.*` |
| `Interface/ICount.cs` | planned | `core/interface/i_count.hpp` |
| `Interface/IDataValidate.cs` | planned | `core/interface/i_data_validate.hpp` |
| `Interface/ITransform.cs` | planned | `core/interface/i_transform.hpp` |
| `Interface/IValid.cs` | planned | `core/interface/i_valid.hpp` |

## Manager

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Manager/IManager.cs` | skeleton | `manager/i_manager.hpp` |
| `Manager/MagicaManager.cs` | skeleton | `manager/magica_manager.*` |
| `Manager/MagicaManagerAPI.cs` | planned | `api/magica_manager_api.*` |
| `Manager/MagicaSettings.cs` | planned | `manager/magica_settings.*` |
| `Manager/Cloth/ClothManager.cs` | skeleton | `manager/cloth/cloth_manager.*` |
| `Manager/Cloth/PreBuildManager.cs` | planned | `manager/cloth/prebuild_manager.*` |
| `Manager/Render/RenderData.cs` | defer | `manager/render/render_data.*` |
| `Manager/Render/RenderManager.cs` | defer | `manager/render/render_manager.*` |
| `Manager/Render/RenderSetupData.cs` | defer | `manager/render/render_setup_data.*` |
| `Manager/Render/RenderSetupDataSerialization.cs` | defer | `manager/render/render_setup_data_serialization.*` |
| `Manager/Simulation/ColliderManager.cs` | skeleton | `manager/simulation/collider_manager.*` |
| `Manager/Simulation/SimulationManager.cs` | partial | `manager/simulation/simulation_manager.*` |
| `Manager/Simulation/TimeManager.cs` | skeleton | `manager/simulation/time_manager.*` |
| `Manager/Simulation/WindManager.cs` | skeleton | `manager/simulation/wind_manager.*` |
| `Manager/Team/TeamManager.cs` | partial | `manager/team/team_manager.*`, parameter + inertia center ownership |
| `Manager/Team/TeamWindData.cs` | planned | `manager/team/team_wind_data.*` |
| `Manager/TransformManager/TransformData.cs` | partial | `manager/transform/transform_data.*` |
| `Manager/TransformManager/TransformDataSerialization.cs` | planned | `manager/transform/transform_data_serialization.*` |
| `Manager/TransformManager/TransformManager.cs` | partial | `manager/transform/transform_manager.*` |
| `Manager/TransformManager/TransformRecord.cs` | partial | `manager/transform/transform_record.*` |
| `Manager/VirtualMesh/VirtualMeshManager.cs` | partial | `manager/virtual_mesh/virtual_mesh_manager.*` |

## PreBuild / Reduction / Settings

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `PreBuild/PreBuildScriptableObject.cs` | defer | Blender asset/compiled scene boundary |
| `PreBuild/PreBuildSerializeData.cs` | planned | `prebuild/prebuild_serialize_data.*` |
| `PreBuild/SharePreBuildData.cs` | planned | `prebuild/share_prebuild_data.*` |
| `PreBuild/UniquePreBuildData.cs` | planned | `prebuild/unique_prebuild_data.*` |
| `Reduction/ReductionSettings.cs` | planned | `reduction/reduction_settings.*` |
| `Reduction/ReductionWorkData.cs` | planned | `reduction/reduction_work_data.*` |
| `Reduction/SameDistanceReduction.cs` | planned | `reduction/same_distance_reduction.*` |
| `Reduction/ShapeDistanceReduction.cs` | planned | `reduction/shape_distance_reduction.*` |
| `Reduction/SimpleDistanceReduction.cs` | planned | `reduction/simple_distance_reduction.*` |
| `Reduction/StepReductionBase.cs` | planned | `reduction/step_reduction_base.*` |
| `Settings/ClothDebugSettings.cs` | planned | `settings/cloth_debug_settings.*` |
| `Settings/VirtualMeshDebugSettings.cs` | planned | `settings/virtual_mesh_debug_settings.*` |

## Utility

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Utility/Data/DataUtility.cs` | partial | `utility/data/data_utility.*` |
| `Utility/Data/MultiDataBuilder.cs` | planned | `utility/data/multi_data_builder.*` |
| `Utility/Grid/GridMap.cs` | planned | `utility/grid/grid_map.*` |
| `Utility/Jobs/InterlockUtility.cs` | defer | C++ threading abstraction |
| `Utility/Jobs/JobUtility.cs` | defer | C++ scheduling abstraction |
| `Utility/Math/AABB.cs` | partial | `utility/math/math_types.hpp`, `utility/math/math_utility.*` |
| `Utility/Math/IntAABB.cs` | planned | `utility/math/int_aabb.*` |
| `Utility/Math/MathExtensions.cs` | partial | `utility/math/math_extensions.*` |
| `Utility/Math/MathUtility.cs` | partial | `utility/math/math_utility.*` |
| `Utility/Math/MinimumData.cs` | planned | `utility/math/minimum_data.*` |
| `Utility/Mesh/MeshUtility.cs` | planned | `utility/mesh/mesh_utility.*` |
| `Utility/Misc/Develop.cs` | planned | `utility/misc/develop.*` |
| `Utility/Misc/StaticStringBuilder.cs` | defer | C++ logging/dump utilities |
| `Utility/NativeCollection/DataChunk.cs` | partial | `utility/native_collection/data_chunk.*` |
| `Utility/NativeCollection/ExBitFlag16.cs` | planned | `utility/native_collection/ex_bit_flag16.*` |
| `Utility/NativeCollection/ExBitFlag8.cs` | partial | `utility/native_collection/bit_flag.hpp` |
| `Utility/NativeCollection/ExCostSortedList1.cs` | planned | `utility/native_collection/ex_cost_sorted_list1.*` |
| `Utility/NativeCollection/ExCostSortedList4.cs` | planned | `utility/native_collection/ex_cost_sorted_list4.*` |
| `Utility/NativeCollection/ExNativeArray.cs` | partial | `utility/native_collection/ex_native_array.hpp` |
| `Utility/NativeCollection/ExProcessingList.cs` | partial | `utility/native_collection/ex_processing_list.*` |
| `Utility/NativeCollection/ExSimpleNativeArray.cs` | partial | `utility/native_collection/ex_simple_native_array.hpp` |
| `Utility/NativeCollection/ExTransformAccessArray.cs` | planned | `utility/native_collection/ex_transform_access_array.*` |
| `Utility/NativeCollection/FixedList128BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/FixedList32BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/FixedList4096BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/FixedList512BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/FixedList64BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/NativeArrayExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/NativeMultiHashMapExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/NativeReferenceExtensions.cs` | defer | C++ container compatibility |
| `Utility/ResultCode/Exception.cs` | planned | `utility/result_code/exception.*` |
| `Utility/ResultCode/ResultCode.cs` | skeleton | `utility/result_code/result_code.*` |
| `Utility/Time/TimeSpan.cs` | planned | `utility/time/time_span.*` |
| `Utility/Time/UnityTimeSpan.cs` | defer | Blender/native profiling abstraction |

## VirtualMesh

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `VirtualMesh/VertexAttribute.cs` | partial | `virtual_mesh/vertex_attribute.hpp` |
| `VirtualMesh/VirtualMesh.cs` | partial | `virtual_mesh/virtual_mesh.*` |
| `VirtualMesh/VirtualMeshBoneWeight.cs` | partial | `virtual_mesh/virtual_mesh_bone_weight.hpp` |
| `VirtualMesh/VirtualMeshContainer.cs` | partial | `virtual_mesh/virtual_mesh_container.*` |
| `VirtualMesh/VirtualMeshPrimitive.cs` | planned | `virtual_mesh/virtual_mesh_primitive.*` |
| `VirtualMesh/VirtualMeshRaycastHit.cs` | planned | `virtual_mesh/virtual_mesh_raycast_hit.*` |
| `VirtualMesh/VirtualMeshTransform.cs` | partial | `virtual_mesh/virtual_mesh_transform.*` |
| `VirtualMesh/Function/VirtualMeshInputOutput.cs` | planned | `virtual_mesh/function/virtual_mesh_input_output.*` |
| `VirtualMesh/Function/VirtualMeshMapping.cs` | planned | `virtual_mesh/function/virtual_mesh_mapping.*` |
| `VirtualMesh/Function/VirtualMeshOptimization.cs` | planned | `virtual_mesh/function/virtual_mesh_optimization.*` |
| `VirtualMesh/Function/VirtualMeshProxy.cs` | planned | `virtual_mesh/function/virtual_mesh_proxy.*` |
| `VirtualMesh/Function/VirtualMeshReduction.cs` | planned | `virtual_mesh/function/virtual_mesh_reduction.*` |
| `VirtualMesh/Function/VirtualMeshSerialization.cs` | planned | `virtual_mesh/function/virtual_mesh_serialization.*` |
| `VirtualMesh/Function/VirtualMeshWork.cs` | planned | `virtual_mesh/function/virtual_mesh_work.*` |

## Next

Last completed step:

- Added MC2-style `DataUtility` packing helpers, `MathExtensions` curve sampling helpers, corrected `VertexAttribute` flag semantics, exposed solver arrays from `SimulationManager` / `VirtualMeshManager`, and added the first single-threaded `DistanceConstraint::Solve(...)` port against the new manager pipeline.
- Moved `InertiaConstraint.CenterData` ownership into `TeamManager`, kept `InertiaConstraint` responsible for fixed-point data, added quaternion/math helpers, and ported the first single-threaded `SimulationStepTeamUpdate(...)` stage for center interpolation, local inertia ratios, gravity dot, scale ratio, and blend weight.
- Ported the first single-threaded particle simulation stages from `SimulationManager.cs`: `StartSimulationStepJob` now computes interpolated base pose, local inertia shift, gravity integration, and predicted positions; `EndSimulationStepJob` now writes velocity, real velocity, and `oldPos` back after constraints. The native smoke path now runs `SimulationStepTeamUpdate -> StartSimulationStep -> DistanceConstraint::Solve -> EndSimulationStepSolve`.
- Added `ClothForceMode`, `ClothParameters.damping_curve_data`, and `TeamData.force_mode/impact_force`; `StartSimulationStep` now applies MC2-style damping curve evaluation and external force modes before prediction.
- Ported the first single-threaded `CalcDisplayPositionJob` path: `SimulationManager::CalcDisplayPosition(...)` now computes display prediction from `oldPos` / `realVelocity`, stores `dispPos`, and writes blended positions back into `VirtualMeshManager::positions`.
- Added `ClothNormalAxis` and the fixed-particle spring branch from `SimulationManager.StartSimulationStepJob`: `SpringConstraintParams` live in `ClothParameters`, and fixed BoneSpring particles now clamp toward their base position using the MC2 spring math. This is the runtime particle branch only; the full MC2 `SpringConstraint` class remains mostly a parameter/serialization boundary in this backend.
- Added the first single-threaded `MotionConstraint` runtime path. The current port covers the MC2 max-distance branch, `MotionConstraintParams` ownership in `ClothParameters`, `ClothManager::Motion()`, and a separate motion-processing list in `SimulationManager`; backstop remains planned.

Latest verification:

```text
package_addon.ps1 -Version motion-final-check -IncludeNativeBuild -RunNativeSmoke -FreshConfigure
native smoke: team_id=1 mesh_id=0 transforms=1 meshes=1 proxy_vertices=2 distance_connections=2 fixed=1 center_data=2 center_transform=0 inertia_x=0.2 gravity_dot=1 force_mode=10 particles=2 steps=1 p0.x=0.0992764 p0.y=-0.000416909 motion_p1.x=1.25 p1.x=1.16816 old_p1.x=1.16816 real_v1.x=-4.91048 disp_p1.x=1.16816 proxy_p1.x=1.16816
```

The native smoke test now contains numeric assertions for the fixed spring particle, Motion max-distance stage, moving particle, old position writeback, real velocity, display position, and proxy position writeback.

VirtualMesh scope note:

- `VirtualMesh` / `VirtualMeshManager` are intentionally partial for now. Full proxy generation, mapping mesh, optimization/reduction/work functions, normal/tangent update jobs, transform/skinning update jobs, and serialization/prebuild restore are not current XPBD-core blockers.
- Do not spend deep test effort on VirtualMesh-specific behavior yet. Current tests only need to cover the lower-level infrastructure it depends on: `ExNativeArray` / `ExSimpleNativeArray`, `DataChunk`, packed index helpers, `VertexAttribute`, transform chunk registration, and simple proxy array ownership needed by the solver smoke path.
- When the core XPBD flow is stable, return to VirtualMesh as a separate integration/prebuild phase.

Wind / VirtualMesh test boundary:

- Do not route current smoke coverage through `WindManager`, `TeamWindData`, or full `VirtualMesh` behavior. These modules should stay as low-level construction/ownership targets until the main XPBD solver chain is stable.
- For now, wind and virtual mesh work should be limited to compile-safe data structures and manager skeletons. Avoid behavior assertions that depend on wind zone scanning, moving wind, proxy generation, mapping, reduction, or skinning.

Next priority: continue the XPBD core flow, but keep tests focused on reusable low-level infrastructure rather than full VirtualMesh or Wind features. The next core-side targets are the remaining constraint solvers that sit after prediction: Motion backstop, Tether, and TriangleBending.
