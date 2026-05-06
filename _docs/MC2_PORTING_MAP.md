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
| `Cloth/ClothUpdateMode.cs` | partial | `cloth/cloth_parameters.hpp` |
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
| `Cloth/Constraints/AngleConstraint.cs` | partial | `cloth/constraints/angle_constraint.*` |
| `Cloth/Constraints/ColliderCollisionConstraint.cs` | partial | `cloth/constraints/collider_collision_constraint.*` |
| `Cloth/Constraints/DistanceConstraint.cs` | partial | `cloth/constraints/distance_constraint.*` |
| `Cloth/Constraints/InertiaConstraint.cs` | partial | `cloth/constraints/inertia_constraint.*` |
| `Cloth/Constraints/MotionConstraint.cs` | partial | `cloth/constraints/motion_constraint.*` |
| `Cloth/Constraints/SelfCollisionConstraint.cs` | partial | `cloth/constraints/self_collision_constraint.*` |
| `Cloth/Constraints/SpringConstraint.cs` | partial | `cloth/cloth_parameters.hpp`, fixed-particle branch in `manager/simulation/simulation_manager.*` |
| `Cloth/Constraints/TetherConstraint.cs` | partial | `cloth/constraints/tether_constraint.*` |
| `Cloth/Constraints/TriangleBendingConstraint.cs` | partial | `cloth/constraints/triangle_bending_constraint.*` |

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
| `Manager/Simulation/ColliderManager.cs` | partial | `manager/simulation/collider_manager.*` |
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
- Added the first single-threaded `MotionConstraint` runtime path. The current verified smoke path covers the MC2 max-distance and backstop branches, `MotionConstraintParams` ownership in `ClothParameters`, `ClothParameters.radius_curve_data`, MC2-style normal-axis handling through `baseRot`, `ClothManager::Motion()`, and a separate motion-processing list in `SimulationManager`. The `MotionConstraint` curve reads now use `MC2EvaluateCurve(...)` for max distance and backstop distance to match the MC2 job; the friction/collision-normal block remains intentionally omitted because it is behind `#if false` in the reference source.
- Tightened `SimulationManager::RegisterParticleRange(...)` so newly allocated particle buffers are deterministically initialized to zero vectors, identity rotations, and zero friction values. The smoke `ExpectNear(...)` helper now rejects non-finite values, so NaN output cannot silently pass the native smoke chain.
- Added the C++ `TetherConstraint` module from `Cloth/Constraints/TetherConstraint.cs`: `TetherConstraintParams` now live in `ClothParameters`, `ClothManager::Tether()` owns the constraint instance, `SimulationManager` now carries the per-step basic position buffer used by tether solving, and `VirtualMeshManager` exposes `VertexRootIndices()` for root-particle lookup. This is a bottom-up code port and has not been routed through the smoke chain yet.
- Added the runtime-side C++ `TriangleBendingConstraint` module from `Cloth/Constraints/TriangleBendingConstraint.cs`: triangle pair/rest/sign/write-buffer arrays, `Register(...)` / `Exit(...)`, pair solve, volume solve, dihedral angle solve, and aggregate write-buffer application are now present. `TriangleBendingConstraintParams`, `TriangleBendingMethod`, `ClothManager::TriangleBending()`, and `SimulationManager` triangle-bending processing list accessors were added. The `CreateData(...)` mesh-topology builder remains deferred until the VirtualMesh/PreBuild data path is ported.
- Extended low-level `DataUtility` / `MathUtility` for later MC2 constraints: `int4`, sorted `PackInt4`, ushort `Pack64` / `Unpack64`, byte `Pack32` / `Unpack32`, `Cross(...)`, and `Clamp1(...)`.
- Added the runtime-side C++ `AngleConstraint` module from `Cloth/Constraints/AngleConstraint.cs`: angle restoration/limit parameters, work buffers, baseline processing-list access, step basic rotation buffer, baseline arrays on `VirtualMesh` / `VirtualMeshManager`, and the MC2-style baseline solve loop are now present. Baseline data generation remains deferred to the VirtualMesh/PreBuild port.
- Extended `MathUtility` for AngleConstraint and later constraints: `AxisAngle(...)`, vector `FromToRotation(...)`, and vector `ClampAngle(...)`.
- Added the first bottom-layer collider port: `ColliderManager` now owns MC2-style collider arrays, collider flags/types, `ColliderData`, `WorkData`, collider range registration/removal, and array accessors. `ColliderCollisionConstraintParams`, `ClothManager::ColliderCollision()`, and the `ColliderCollisionConstraint` ownership/work-buffer shell are present.
- Ported the Point/Edge paths of `ColliderCollisionConstraint`: MC2-style `ColliderCollisionMode`, per-step point collider loop, per-step edge collider loop, particle/edge AABB filtering, Sphere/Plane/Capsule detection helpers, fixed-point aggregate buffers for edge writeback, friction/collision-normal writeback, and BoneSpring velocity/max-distance handling are now in C++. Edge processing-list population remains a later VirtualMesh/update-pipeline integration point.
- Extended low-level `MathUtility` / `AABB` support for collider and later self-collision work: `ClampDistance(...)`, `Overlaps(...)`, `Expand(...)`, AABB encapsulation, `ClosestPtPointSegmentRatio(...)`, and `IntersectPointPlaneDist(...)`.
- Extended the bottom-layer `ColliderManager` lifecycle from `Manager/Simulation/ColliderManager.cs`: `SimulationManager` now exposes/marks `processingStepCollider`, and `ColliderManager` has C++ `PreSimulationUpdate(...)`, `CreateUpdateColliderList(...)`, `StartSimulationStep(...)`, `EndSimulationStep(...)`, and `PostSimulationUpdate(...)` entry points. The Start step now builds MC2-style Sphere/Capsule/Plane `WorkData` with interpolated frame poses, inertia-shifted old poses, radius/AABB data, capsule endpoints, and plane normals. PreSimulationUpdate now also applies the MC2 negative-scale teleport path through the newly ported matrix transform helpers.
- Extended low-level `MathUtility` with MC2 `ShiftPosition(...)` for inertia shift reuse.
- Extended low-level `MathUtility` matrix/rotation helpers from `Utility/Math/MathUtility.cs`: `ToNormalTangent(...)`, `LookRotation(...)`, `TransformPoint(...)`, `TransformVector(...)`, `TransformDirection(...)`, `TransformDistance(...)`, `TransformLength(...)`, `TransformRotation(...)`, and `InverseTransformPoint(...)`.
- Extended low-level `MathUtility` segment helpers from `Utility/Math/MathUtility.cs`: `ClosestPtSegmentSegment(...)` and `ClosestPtSegmentSegment2(...)`, used by edge-capsule collider collision and later self-collision work.
- Extended low-level `MathUtility` triangle/intersection helpers from `Utility/Math/MathUtility.cs`: point-triangle closest point/UVW, triangle center/normal/area/tangent/rotation, triangle-pair index/angle helpers, segment-triangle intersection, and ray-sphere intersection. These are prerequisite utilities for SelfCollision, VirtualMeshWork, and later topology builders.
- Added the first bottom-layer `SelfCollisionConstraint` C++ module from `Cloth/Constraints/SelfCollisionConstraint.cs`: `SelfCollisionMode`, `SelfCollisionConstraintParams`, primitive/sort/contact data structures, primitive counters, intersect flag ownership, work-buffer lifecycle, primitive registration/removal, primitive initialization, per-step primitive update, intersect primitive next-position update, per-team sort-and-sweep sorting, Self/Sync/ParentSync sort-and-sweep broad-phase contact generation, broad-phase contact refresh, EdgeEdge / PointTriangle XPBD solve, aggregate writeback/clear, Self/Sync/ParentSync edge-triangle intersect/tangle-release flagging, `SolveRuntimeSelfCollision(...)`, `ClothManager::SelfCollision()`, and CMake registration are now present.
- Extended step-list infrastructure for SelfCollision: `SimulationManager` now exposes/marks self particle, point-triangle, edge-edge, and triangle-point processing lists and can populate those lists from team self-collision flags. `BitFlag64::TestAny(...)` was added to mirror MC2 flag range checks.
- Started the fuller `TeamManager.TeamData` C++ port: `ClothUpdateMode`, proxy mesh type, negative-scale triangle/quaternion helper fields, MC2-style status query helpers, fixed-size sync parent team list, `ContainsTeamData(...)`, `SetSyncTeam(...)`, sync parent add/remove maintenance, `MappingData`, fixed-size team mapping index lists, mapping data registration/removal, and sync time/inertia-parameter copy helpers are now present. The broader MC2 team lifecycle jobs, culling updates, team wind data, and full process-driven parameter synchronization remain deferred.

Latest verification:

```text
package_addon.ps1 -Version motion-backstop-init-verified -IncludeNativeBuild -RunNativeSmoke -FreshConfigure
native smoke: team_id=1 mesh_id=0 transforms=1 meshes=1 proxy_vertices=2 distance_connections=2 fixed=1 center_data=2 center_transform=0 inertia_x=0.198333 gravity_dot=1 force_mode=10 particles=2 steps=1 p0.x=0.0998836 p0.y=-0.000422865 motion_p1.x=1.449 p1.x=1.42373 old_p1.x=1.42373 real_v1.x=10.4236 disp_p1.x=1.42373 proxy_p1.x=1.42373
```

The native smoke test now contains finite numeric assertions for the fixed spring particle, Motion max-distance/backstop stage, moving particle, old position writeback, real velocity, display position, and proxy position writeback.

VirtualMesh scope note:

- `VirtualMesh` / `VirtualMeshManager` are intentionally partial for now. Full proxy generation, mapping mesh, optimization/reduction/work functions, normal/tangent update jobs, transform/skinning update jobs, and serialization/prebuild restore are not current XPBD-core blockers.
- Do not spend deep test effort on VirtualMesh-specific behavior yet. Current tests only need to cover the lower-level infrastructure it depends on: `ExNativeArray` / `ExSimpleNativeArray`, `DataChunk`, packed index helpers, `VertexAttribute`, transform chunk registration, and simple proxy array ownership needed by the solver smoke path.
- When the core XPBD flow is stable, return to VirtualMesh as a separate integration/prebuild phase.

Wind / VirtualMesh test boundary:

- Do not route current smoke coverage through `WindManager`, `TeamWindData`, or full `VirtualMesh` behavior. These modules should stay as low-level construction/ownership targets until the main XPBD solver chain is stable.
- For now, wind and virtual mesh work should be limited to compile-safe data structures and manager skeletons. Avoid behavior assertions that depend on wind zone scanning, moving wind, proxy generation, mapping, reduction, or skinning.

Next priority: continue the bottom-up XPBD core port. `TetherConstraint`, runtime-side `TriangleBendingConstraint`, runtime-side `AngleConstraint`, collider collision, SelfCollision Self/Sync/ParentSync runtime paths, and the first TeamData/mapping ownership layer are now present; next targets are the remaining data builders/update-list population needed to feed these constraints, plus more TeamManager lifecycle/culling/parameter synchronization pieces.
