# MC2 Core Porting Map

Reference root:

```text
_ReferenceProject/MagicaCloth2/Scripts/Core
```

Status labels:

- `complete`: MC2 file is ported at the file/API level for the native backend; Unity/editor-only decoration may be omitted when it has no C++ equivalent.
- `planned`: not ported yet.
- `skeleton`: C++ target files or manager shell exist, but behavior is not ported.
- `partial`: some behavior or data ownership exists in the new MC2-style backend.
- `legacy-partial`: related logic exists only in old `_native/src/hocloth_*.cpp` bootstrap code.
- `defer`: Unity/editor/render specific code; reinterpret later at the Blender boundary.

## Module Summary

| MC2 module | Status | HoCloth target |
| --- | --- | --- |
| Define | partial | `_native/include/hocloth/core/define/` |
| Interface | partial | `_native/include/hocloth/manager/`, `_native/include/hocloth/core/interface/` |
| Utility/ResultCode | complete | `_native/include/hocloth/utility/result_code/` |
| Utility/Math | partial | `_native/include/hocloth/utility/math/` |
| Utility/Data | partial | `_native/include/hocloth/utility/data/` |
| Utility/NativeCollection | partial | `_native/include/hocloth/utility/native_collection/` |
| Utility/Time | partial | `_native/include/hocloth/utility/time/`, `_native/include/hocloth/manager/simulation/time_manager.hpp` |
| Manager | partial | `_native/include/hocloth/manager/` |
| Cloth/Constraints | partial | `_native/include/hocloth/cloth/constraints/`; solver/data-owner layer is mostly ported, remaining gaps are collider components, Angle full PreBuild/proxy feed, SelfCollision full builder parity |
| Cloth/Collider | planned | `_native/include/hocloth/cloth/collider/` |
| Cloth/Wind | partial | `_native/include/hocloth/cloth/wind/`, `_native/include/hocloth/manager/simulation/wind_manager.hpp` |
| VirtualMesh | partial | `_native/include/hocloth/virtual_mesh/` |
| Reduction | planned | `_native/include/hocloth/reduction/` |
| PreBuild | planned | `_native/include/hocloth/prebuild/` |

## Cloth

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/CheckSliderSerializeData.cs` | complete | `cloth/parameters/check_slider_serialize_data.hpp` |
| `Cloth/ClothBehaviour.cs` | defer | Blender component/runtime boundary |
| `Cloth/ClothForceMode.cs` | complete | `cloth/cloth_force_mode.hpp` |
| `Cloth/ClothNormalAxis.cs` | complete | `cloth/cloth_normal_axis.hpp` |
| `Cloth/ClothParameters.cs` | partial | `cloth/cloth_parameters.hpp`; native wind parameter ownership is present |
| `Cloth/ClothProcess.cs` | planned | `cloth/cloth_process.*` |
| `Cloth/ClothProcessData.cs` | planned | `cloth/cloth_process_data.*` |
| `Cloth/ClothProcessGeneration.cs` | planned | `cloth/cloth_process_generation.*` |
| `Cloth/ClothSerializeData.cs` | planned | `cloth/cloth_serialize_data.*` |
| `Cloth/ClothSerializeData2.cs` | planned | `cloth/cloth_serialize_data2.*` |
| `Cloth/ClothSerializeDataFunction.cs` | planned | `cloth/cloth_serialize_data_function.*` |
| `Cloth/ClothUpdateMode.cs` | complete | `cloth/cloth_parameters.hpp` |
| `Cloth/CullingSettings.cs` | partial | `cloth/parameters/culling_settings.hpp`; Renderer/GameObject references stay at the Blender viewport boundary |
| `Cloth/CurveSerializeData.cs` | partial | `cloth/parameters/curve_serialize_data.hpp`; native float4x4 curve-data path is present, Unity AnimationCurve copy remains a boundary |
| `Cloth/CustomSkinningSettings.cs` | partial | `cloth/custom_skinning_settings.hpp`; Unity Transform list is represented as backend transform ids |
| `Cloth/GizmoSerializeData.cs` | complete | `cloth/gizmo_serialize_data.hpp`; draw execution remains Blender authoring/gizmo layer |
| `Cloth/MagicaCloth.cs` | defer | Blender component wrapper |
| `Cloth/MagicaClothAnimationProperty.cs` | planned | `cloth/animation_property.*` |
| `Cloth/MagicaClothAPI.cs` | defer | native API + Python bridge |
| `Cloth/NormalAlignmentSettings.cs` | partial | `cloth/normal_alignment_settings.hpp`; Unity Transform reference is represented as a backend transform id |
| `Cloth/SelectionData.cs` | partial | `cloth/selection_data.hpp`; data container/clone/compare/add/fill/merge are present, GridMap conversion remains deferred |

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
| `Cloth/Constraints/AngleConstraint.cs` | partial | `cloth/constraints/angle_constraint.*`; runtime solver/work buffers are ported, native baseline arrays plus Mesh/Bone parent-generation feed are present, full PreBuild/proxy conversion remains to close |
| `Cloth/Constraints/ColliderCollisionConstraint.cs` | partial | `cloth/constraints/collider_collision_constraint.*`; point/edge solver and collider work-data path are ported, collider component authoring/registration remains separate |
| `Cloth/Constraints/DistanceConstraint.cs` | complete | `cloth/constraints/distance_constraint.*`; params, data owner, `CreateData(...)`, register/exit, vertical/horizontal/shear runtime solver are present |
| `Cloth/Constraints/InertiaConstraint.cs` | complete | `cloth/constraints/inertia_constraint.*`, `manager/team/team_manager.*`, `manager/simulation/simulation_manager.*`; CenterData/fixed list/CreateData plus per-frame inertia lifecycle are ported |
| `Cloth/Constraints/MotionConstraint.cs` | complete | `cloth/constraints/motion_constraint.*`; max-distance/backstop/stiffness runtime path is ported, MC2's disabled friction block remains intentionally omitted |
| `Cloth/Constraints/SelfCollisionConstraint.cs` | partial | `cloth/constraints/self_collision_constraint.*`; primitive ownership, broad phase, XPBD contacts, intersect/tangle paths are present, full create/update-list parity remains to audit |
| `Cloth/Constraints/SpringConstraint.cs` | complete | `cloth/cloth_parameters.hpp`, fixed-particle branch in `manager/simulation/simulation_manager.*`; MC2 class has no active solver beyond params, BoneSpring runtime branch is ported |
| `Cloth/Constraints/TetherConstraint.cs` | complete | `cloth/constraints/tether_constraint.*`; params and runtime root-distance compression/stretch solver are ported |
| `Cloth/Constraints/TriangleBendingConstraint.cs` | complete | `cloth/constraints/triangle_bending_constraint.*`; params, data owner, `CreateData(...)`, register/exit, dihedral/volume solver, and aggregate writeback are present |

## Cloth/Wind

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Wind/MagicaWindZone.cs` | partial | `cloth/wind/magica_wind_zone.hpp`; native data-form zone and direction helpers are present, Unity component lifecycle remains Blender boundary |
| `Cloth/Wind/WindParams.cs` | complete | `cloth/wind/wind_params.hpp` |
| `Cloth/Wind/WindSettings.cs` | complete | `cloth/wind/wind_settings.hpp` |

## Define / Interface

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Define/ResultDefine.cs` | complete | `utility/result_code/result_code.hpp` |
| `Define/SystemDefine.cs` | partial | `core/define/system_define.hpp`, `manager/simulation/time_manager.*` |
| `Interface/ICount.cs` | complete | `core/interface/i_count.hpp` |
| `Interface/IDataValidate.cs` | complete | `core/interface/i_data_validate.hpp` |
| `Interface/ITransform.cs` | defer | Unity `Transform` collection/replacement boundary; reinterpret through Blender object ids later |
| `Interface/IValid.cs` | complete | `core/interface/i_valid.hpp` |

## Manager

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Manager/IManager.cs` | skeleton | `manager/i_manager.hpp` |
| `Manager/MagicaManager.cs` | partial | `manager/magica_manager.*`, native frame-step orchestration now exists |
| `Manager/MagicaManagerAPI.cs` | planned | `api/magica_manager_api.*` |
| `Manager/MagicaSettings.cs` | planned | `manager/magica_settings.*` |
| `Manager/Cloth/ClothManager.cs` | partial | `manager/cloth/cloth_manager.*`, MC2 constraint solve order is centralized |
| `Manager/Cloth/PreBuildManager.cs` | planned | `manager/cloth/prebuild_manager.*` |
| `Manager/Render/RenderData.cs` | defer | `manager/render/render_data.*` |
| `Manager/Render/RenderManager.cs` | defer | `manager/render/render_manager.*` |
| `Manager/Render/RenderSetupData.cs` | defer | `manager/render/render_setup_data.*` |
| `Manager/Render/RenderSetupDataSerialization.cs` | defer | `manager/render/render_setup_data_serialization.*` |
| `Manager/Simulation/ColliderManager.cs` | partial | `manager/simulation/collider_manager.*` |
| `Manager/Simulation/SimulationManager.cs` | partial | `manager/simulation/simulation_manager.*`, step lifecycle and processing-list population are now routed through native manager state |
| `Manager/Simulation/TimeManager.cs` | partial | `manager/simulation/time_manager.*`, simulation delta/max-step/power calculation is present |
| `Manager/Simulation/WindManager.cs` | partial | `manager/simulation/wind_manager.*`; wind data ownership, registration/removal/enable, and native zone refresh are present |
| `Manager/Team/TeamManager.cs` | partial | `manager/team/team_manager.*`, parameter + inertia center/wind ownership, timing/update-count lifecycle, sync lists, state/control APIs, post-step flag cleanup |
| `Manager/Team/TeamWindData.cs` | complete | `manager/team/team_wind_data.hpp` |
| `Manager/TransformManager/TransformData.cs` | partial | `manager/transform/transform_data.*`; inverse rotation/root/dirty arrays are present, Unity transform-access restore remains a boundary |
| `Manager/TransformManager/TransformDataSerialization.cs` | planned | `manager/transform/transform_data_serialization.*` |
| `Manager/TransformManager/TransformManager.cs` | partial | `manager/transform/transform_manager.*`; backend transform record/update/root/dirty ownership is present |
| `Manager/TransformManager/TransformRecord.cs` | complete | `manager/transform/transform_record.*`; Unity `Transform` object access is represented by backend record input |
| `Manager/VirtualMesh/VirtualMeshManager.cs` | partial | `manager/virtual_mesh/virtual_mesh_manager.*`; proxy/mapping/common buffers plus baseline flags/team ids/start/count/data ownership are present |

## PreBuild / Reduction / Settings

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `PreBuild/PreBuildScriptableObject.cs` | defer | Blender asset/compiled scene boundary |
| `PreBuild/PreBuildSerializeData.cs` | planned | `prebuild/prebuild_serialize_data.*` |
| `PreBuild/SharePreBuildData.cs` | planned | `prebuild/share_prebuild_data.*` |
| `PreBuild/UniquePreBuildData.cs` | planned | `prebuild/unique_prebuild_data.*` |
| `Reduction/ReductionSettings.cs` | complete | `reduction/reduction_settings.hpp` |
| `Reduction/ReductionWorkData.cs` | partial | `reduction/reduction_work_data.hpp`; native data ownership shell exists, job buffers are adapted to C++ containers |
| `Reduction/SameDistanceReduction.cs` | planned | `reduction/same_distance_reduction.*` |
| `Reduction/ShapeDistanceReduction.cs` | planned | `reduction/shape_distance_reduction.*` |
| `Reduction/SimpleDistanceReduction.cs` | planned | `reduction/simple_distance_reduction.*` |
| `Reduction/StepReductionBase.cs` | skeleton | `reduction/step_reduction_base.*`; JoinEdge and base state are present, reduction algorithm/jobs remain deferred |
| `Settings/ClothDebugSettings.cs` | complete | `settings/cloth_debug_settings.hpp` |
| `Settings/VirtualMeshDebugSettings.cs` | complete | `settings/virtual_mesh_debug_settings.hpp` |

## Utility

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Utility/Data/DataUtility.cs` | partial | `utility/data/data_utility.*`; MC2 pack/unpack and remaining-data helpers are present, Unity object conversion remains a boundary |
| `Utility/Data/MultiDataBuilder.cs` | complete | `utility/data/multi_data_builder.hpp` |
| `Utility/Grid/GridMap.cs` | partial | `utility/grid/grid_map.hpp`; native hash-map helper exists, Unity job/container semantics are adapted |
| `Utility/Jobs/InterlockUtility.cs` | defer | C++ threading abstraction |
| `Utility/Jobs/JobUtility.cs` | defer | C++ scheduling abstraction |
| `Utility/Math/AABB.cs` | complete | `utility/math/math_types.hpp`, `utility/math/math_utility.*` |
| `Utility/Math/IntAABB.cs` | complete | `utility/math/int_aabb.hpp` |
| `Utility/Math/MathExtensions.cs` | complete | `utility/math/math_extensions.*` |
| `Utility/Math/MathUtility.cs` | partial | `utility/math/math_utility.*` |
| `Utility/Math/MinimumData.cs` | complete | `utility/math/minimum_data.hpp` |
| `Utility/Mesh/MeshUtility.cs` | planned | `utility/mesh/mesh_utility.*` |
| `Utility/Misc/Develop.cs` | complete | `utility/misc/develop.*`; native output/assert backend replaces Unity `Debug` |
| `Utility/Misc/StaticStringBuilder.cs` | defer | C++ logging/dump utilities |
| `Utility/NativeCollection/DataChunk.cs` | complete | `utility/native_collection/data_chunk.*` |
| `Utility/NativeCollection/ExBitFlag16.cs` | complete | `utility/native_collection/bit_flag.hpp` |
| `Utility/NativeCollection/ExBitFlag8.cs` | complete | `utility/native_collection/bit_flag.hpp` |
| `Utility/NativeCollection/ExCostSortedList1.cs` | complete | `utility/native_collection/ex_cost_sorted_list1.hpp` |
| `Utility/NativeCollection/ExCostSortedList4.cs` | complete | `utility/native_collection/ex_cost_sorted_list4.hpp` |
| `Utility/NativeCollection/ExNativeArray.cs` | partial | `utility/native_collection/ex_native_array.hpp`; chunk reuse, expand/fill/remove, summary/debug helpers are present, unsafe reinterpret/serialization is adapted |
| `Utility/NativeCollection/ExProcessingList.cs` | complete | `utility/native_collection/ex_processing_list.*`; C++ counter pointer replaces Unity `NativeReference` job pointer |
| `Utility/NativeCollection/ExSimpleNativeArray.cs` | partial | `utility/native_collection/ex_simple_native_array.hpp`; range add/fill/remove, summary/debug helpers are present, unsafe reinterpret/serialization is adapted |
| `Utility/NativeCollection/ExTransformAccessArray.cs` | planned | `utility/native_collection/ex_transform_access_array.*` |
| `Utility/NativeCollection/FixedList128BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/FixedList32BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/FixedList4096BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/FixedList512BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/FixedList64BytesExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/NativeArrayExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/NativeMultiHashMapExtensions.cs` | defer | C++ container compatibility |
| `Utility/NativeCollection/NativeReferenceExtensions.cs` | defer | C++ container compatibility |
| `Utility/ResultCode/Exception.cs` | complete | `utility/result_code/exception.*` |
| `Utility/ResultCode/ResultCode.cs` | complete | `utility/result_code/result_code.*`; native debug logging is treated as the C++ logging boundary |
| `Utility/Time/TimeSpan.cs` | complete | `utility/time/time_span.*`; DebugLog/Log are omitted at the C++ logging boundary |
| `Utility/Time/UnityTimeSpan.cs` | defer | Blender/native profiling abstraction |

## VirtualMesh

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `VirtualMesh/VertexAttribute.cs` | complete | `virtual_mesh/vertex_attribute.hpp` |
| `VirtualMesh/VirtualMesh.cs` | partial | `virtual_mesh/virtual_mesh.*`; core arrays, fixed list/AABB, bind pose, transform restore rotations, baseline arrays, and parent-driven baseline/root/depth/local-pose builder are present |
| `VirtualMesh/VirtualMeshBoneWeight.cs` | complete | `virtual_mesh/virtual_mesh_bone_weight.*` |
| `VirtualMesh/VirtualMeshContainer.cs` | partial | `virtual_mesh/virtual_mesh_container.*` |
| `VirtualMesh/VirtualMeshPrimitive.cs` | complete | `virtual_mesh/virtual_mesh_primitive.hpp` |
| `VirtualMesh/VirtualMeshRaycastHit.cs` | complete | `virtual_mesh/virtual_mesh_raycast_hit.hpp` |
| `VirtualMesh/VirtualMeshTransform.cs` | partial | `virtual_mesh/virtual_mesh_transform.*` |
| `VirtualMesh/Function/VirtualMeshInputOutput.cs` | planned | `virtual_mesh/function/virtual_mesh_input_output.*` |
| `VirtualMesh/Function/VirtualMeshMapping.cs` | planned | `virtual_mesh/function/virtual_mesh_mapping.*` |
| `VirtualMesh/Function/VirtualMeshOptimization.cs` | planned | `virtual_mesh/function/virtual_mesh_optimization.*` |
| `VirtualMesh/Function/VirtualMeshProxy.cs` | partial | `virtual_mesh/virtual_mesh.*`; fixed-list/AABB, vertex bind pose, vertex-to-transform rotation, Mesh edge baseline parent generation, and Bone transform baseline generation are present; full proxy conversion/normal tangent/edge flag/reduction/custom skinning remain |
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
- Extended `VirtualMeshManager` with the MC2 mapping buffer ownership layer from `Manager/VirtualMesh/VirtualMeshManager.cs`: mapping id/reference/attribute/local position/local normal/bone weight/position/normal arrays, mapping vertex count/accessors, `RegisterMappingMesh(...)`, `ExitMappingMesh(...)`, center-transform ownership, `mappingIndex + 1` id storage, `TeamManager::MappingData`回写, and mapping mesh `mapping_id` assignment are now present. This is only the low-level registration/release substrate; `VirtualMesh/Function/VirtualMeshMapping.cs` mapping solve/generation behavior remains deferred.
- Added file-level completion tracking to this map and marked the small fully ported MC2 files explicitly. `Define/ResultDefine.cs`, `Interface/ICount.cs`, `Interface/IDataValidate.cs`, `Interface/IValid.cs`, `ClothForceMode.cs`, `ClothNormalAxis.cs`, `ClothUpdateMode.cs`, `DataChunk.cs`, `VertexAttribute.cs`, and `VirtualMeshBoneWeight.cs` are now tracked as `complete`. `DataChunk` gained MC2 constructor parity, `VertexAttribute` gained the missing disable-collision static/set overload, and `VirtualMeshBoneWeight` now has the MC2 validity/count/add/normalize/string helpers.
- Continued lightweight utility/interface migration: `IntAABB.cs`, `MinimumData.cs`, `ExBitFlag16.cs`, `ExCostSortedList1.cs`, and `ExCostSortedList4.cs` are now ported and marked `complete`. `float4` and `int4` gained mutable index accessors needed by these MC2-style fixed containers.
- Added another lightweight type pass: MC2 processing/cancel exceptions, `VirtualMeshPrimitive`, `VirtualMeshRaycastHit`, and `TimeSpan` are now ported. `TimeSpan` keeps the native timing/string behavior; MC2 `Develop.DebugLog/Log` calls are intentionally treated as the C++ logging boundary.
- Added native cloth parameter helpers: `CheckSliderSerializeData` is complete, `CurveSerializeData` now supports value/linear-curve/float4x4 curve-data evaluation and conversion, and `CullingSettings` carries the MC2 culling enums plus distance-culling values/validation. Unity `AnimationCurve`, `Renderer`, and `GameObject` references remain integration-boundary concerns.
- Added native data-layer ports for `SelectionData`, `NormalAlignmentSettings`, and `CustomSkinningSettings`. These now cover the MC2 value ownership, validation, clone/compare/fill/merge style helpers, and transform references are represented as backend ids; `SelectionData.ConvertFrom(...)` / GridMap search and Unity object replacement remain later integration/data-builder work.
- Added `MultiDataBuilder` and a lightweight native `GridMap` utility. `MultiDataBuilder` preserves the MC2 key-ordered flattening and `Pack12_20(count,start)` index output; `GridMap` provides native grid hashing/add/remove/move/area helpers for later `SelectionData` and spatial builder work. Also added data ports for `ClothDebugSettings`, `VirtualMeshDebugSettings`, and `GizmoSerializeData`.
- Added the first Reduction data layer: `ReductionSettings` is complete, `ReductionWorkData` now owns the C++ containers for reduction/optimization/final mesh data, and `StepReductionBase` has the MC2 base state plus `JoinEdge` type. Actual step reduction algorithms/jobs remain deferred.
- Fixed the native compile-check blocker found while preparing a full build pass: `DataChunk` is now a header-inline value type for constructors, validity, range helpers, `Clear()`, and `Empty()`. WinDbg/cdb showed the old external `DataChunk::Empty()` return path could fault inside `ExNativeArray<float>::GetEmptyChunk(...)` during proxy registration. This keeps `Utility/NativeCollection/DataChunk.cs` marked `complete` and makes `ExNativeArray` smoke-safe again.
- Reframed `hocloth_mc2_core_smoke` as a compile/run preflight check for the current porting phase. It still exercises MagicaManager initialization, team/proxy registration, particle registration, inertia/motion/distance solving, display position calculation, and proxy writeback synchronization, but no longer blocks on the previous narrow numerical constants while XPBD parity is still being ported.
- Started the large-module closure pass for `TeamManager` and `SimulationManager`: `TeamManager` now has MC2-style enable/skip/reset/time-reset controls, create-time reset flags, `AlwaysTeamUpdate(...)` timing/update-count scheduling, and `PostTeamUpdate(...)` post-frame flag/force/time cleanup. `SimulationManager::CreateStepParticleList(...)` now ports the MC2 `CreateUpdateParticleList` population path for step particles, baselines, triangle bending pairs, edge collider collision, motion particles, and self-collision lists from team state and cloth parameters.
- The native smoke path now enters the solver through the new lifecycle sequence: `AlwaysTeamUpdate -> BeginSimulationStep -> SimulationStepTeamUpdate -> CreateStepParticleList -> StartSimulationStep -> constraints -> EndSimulationStepSolve -> CalcDisplayPosition -> PostTeamUpdate`. This keeps Wind and full VirtualMesh behavior outside the current smoke boundary while exercising the manager orchestration needed for larger XPBD module integration.
- Added the next manager-orchestration layer: `TimeManager` now computes MC2-style `SimulationPower` and exposes max substep count, `SimulationManager::PreSimulationUpdate(...)` ports the reset/inertia-shift/negative-scale particle preparation job, `ClothManager::SolveStepConstraints(...)` centralizes the MC2 constraint order (`Tether -> Distance -> Angle -> TriangleBending -> Collider -> Distance -> Motion -> SelfCollision`), and `MagicaManager::StepFrame(...)` provides a native frame-step skeleton for Blender-side driving.
- Closed another Team/Simulation orchestration gap from `TeamManager.CalcCenterAndInertiaAndWindJob` and `SimulationManager.UpdateStepBasicPotureJob`: `TeamManager::UpdateCenterAndInertia(...)` now prepares component/center frame poses, fixed-point-derived center fallback, negative-scale flags/sign/quaternion/triangle helper values, negative-scale teleport matrices, teleport reset/keep decisions, smoothing, world-inertia shift, frame moving speed/direction, and stabilization weights before the step loop. `MagicaManager::StepFrame(...)` now calls this before `PreSimulationUpdate(...)`. Wind-zone collection remains intentionally outside the current runtime path.
- Extended `MathUtility` with MC2-style `ToRotation(normal,tangent)`, affine matrix multiply, and affine inverse helpers. `SimulationManager::UpdateStepBasicPosture(...)` now applies negative-scale local rotation and fixed/baseline rotation rewrite, and `CalcDisplayPosition(...)` now applies the MC2 negative-scale display-rotation conversion instead of leaving that block deferred. `VirtualMeshManager` exposes `VertexBindPoseRotations()` plus the local position/rotation accessors needed by these paths.
- Closed another bottom-layer utility pass: `ResultCode.cs` now maps to a native `ResultStatus` wrapper with result/warning state, merge/process helpers, static success/error presets, and result/warning information strings; this module is now marked `complete` with debug logging handled by the C++ logging boundary.
- Extended the Transform data layer with MC2-style inverse rotation storage, root-id tracking, dirty state, `InverseTransformDirection(...)`, and manager accessors for inverse rotation/root/dirty state. `TransformRecord.cs` is now file-level complete for backend record semantics, while Unity transform access/restore jobs stay at the Blender boundary.
- Completed the C++ `AABB.cs` value-type port: center/extents/half-extents/max-side/valid/surface-area helpers, point/AABB containment, overlap, expand, encapsulate, matrix transform, equality, and string dump are now present while preserving the existing lowercase `min/max` fields used by current constraints.
- Added more MC2-compatible NativeCollection surface area for later large-module ports: `ExNativeArray` now has vector/count range adds, whole-array add, `ExpandAndFill(...)`, `GetRef(...)`, `ToString()`, and MC2-order `RemoveAndFill(...)`; `ExSimpleNativeArray` gained counted range add and debug dump helpers. Unsafe reinterpret/serialization remains intentionally adapted to C++ containers.
- Extended `DataUtility.cs` parity with packed int2/int3/int4 helpers, `Pack32(int4)`, unpack helpers, and `RemainingData(...)` so later builder and constraint code can follow MC2 call shapes without local workarounds.
- Extended the `TeamManager.cs` C++ state/control surface for the next large-module pass: native active/true team counts, process query, update mode/time scale setters, sync suspend, camera/distance culling flags, anchor state, external force injection/clear, restore-transform-once query/clear, and edge-collider collision counting now exist without pulling in Unity renderer or Wind behavior.
- Closed three small Utility files at file/API level: `MathExtensions.cs` is now marked complete for float4x4 element access and curve evaluation helpers; `Develop.cs` has a native logging/assert backend with MC2 prefixes and debug macro gates; `ExProcessingList.cs` now exposes counter/buffer ownership plus a C++ counter pointer equivalent to the Unity job schedule pointer.
- Started the Wind module proper. `WindSettings.cs` and `WindParams.cs` are now complete native data ports; `MagicaWindZone.cs` has a native data-form zone with direction/radial/addition helpers and local/world direction conversion; `WindManager.cs` now owns MC2-style `WindData`, wind arrays, registration/removal/enable, and per-frame native zone refresh including volume, direction, attenuation, and addition flags. `TeamWindData.cs` is complete and is now owned by `TeamManager`. The actual team wind-zone selection and particle wind force solve remain the next Wind behavior layer.
- Ported the next Wind behavior layer from `TeamManager.CalcCenterAndInertiaAndWindJob`: `ClothParameters` now owns `WindParams`, `MagicaManager::StepFrame(...)` refreshes `WindManager` every frame, and `TeamManager::UpdateCenterAndInertia(...)` now builds each team's `TeamWindData` from enabled wind zones. The selection logic follows MC2's rules: non-addition zones choose the smallest containing volume, addition zones can contribute up to three entries, sphere radial zones evaluate attenuation, and previous wind time is preserved through `AddOrReplaceWindZone(...)`. Moving wind and final particle wind-force mixing remain pending.
- Closed a large chunk of `Cloth/Constraints` at file/API level. `DistanceConstraint` now has a native `CreateData(...)` builder for MC2 vertical/horizontal/shear connections from current `VirtualMesh` edges/triangles; `TriangleBendingConstraint` now has a native `CreateData(...)` builder for dihedral and volume pairs plus write-buffer indices; `InertiaConstraint` now has `CreateData(...)` for CenterData, local gravity direction, and fixed-center list ownership. `MotionConstraint`, `TetherConstraint`, `SpringConstraint`, `DistanceConstraint`, `InertiaConstraint`, and `TriangleBendingConstraint` are now marked `complete`; `AngleConstraint`, `ColliderCollisionConstraint`, and `SelfCollisionConstraint` remain intentionally `partial` because their remaining work belongs to baseline/PreBuild generation, collider component registration, or full self-collision builder parity.
- Added a parent-driven native baseline feed for `AngleConstraint` and other baseline consumers. `VirtualMesh::BuildBaseLinesFromParents()` now builds MC2-style child arrays, baseline flags/start/count/data, vertex local position/rotation, root indices, and depth values from existing `vertex_parent_indices`; `VirtualMeshManager` now owns baseline flags and team ids in addition to start/count/data. This does not replace the full `VirtualMeshProxy.cs` Mesh/Bone baseline generation path, but it closes the low-level feed needed once parent indices are available.

Latest verification:

```text
cmake --build _native/build/vs2022-release --config Release --target hocloth_mc2_core_smoke
_bin/hocloth_mc2_core_smoke.exe
package_addon.ps1 -Version codex-compile-prep -IncludeNativeBuild -RunNativeSmoke
native smoke: team_id=1 mesh_id=0 transforms=1 meshes=1 proxy_vertices=2 distance_connections=2 fixed=1 center_data=2 center_transform=0 inertia_x=0.2 gravity_dot=1 force_mode=10 particles=2 steps=1 p0.x=0.116505 p0.y=-0.00133923 motion_p1.x=1.24991 p1.x=1.24991 old_p1.x=1.24991 real_v1.x=-0.00515699 disp_p1.x=1.24991 proxy_p1.x=1.24991
```

The native smoke test now contains finite-value assertions plus synchronization checks for old position, display position, and proxy position writeback. Exact XPBD numeric parity remains a later solver-port verification task.

VirtualMesh scope note:

- `VirtualMesh` / `VirtualMeshManager` are intentionally partial for now. Simple proxy ownership and mapping buffer registration/release are present; full proxy generation, mapping solve/generation behavior, optimization/reduction/work functions, normal/tangent update jobs, transform/skinning update jobs, and serialization/prebuild restore are not current XPBD-core blockers.
- Do not spend deep test effort on VirtualMesh-specific behavior yet. Current tests only need to cover the lower-level infrastructure it depends on: `ExNativeArray` / `ExSimpleNativeArray`, `DataChunk`, packed index helpers, `VertexAttribute`, transform chunk registration, and simple proxy array ownership needed by the solver smoke path.
- When the core XPBD flow is stable, return to VirtualMesh as a separate integration/prebuild phase.

Wind / VirtualMesh test boundary:

- Wind and VirtualMesh can now move beyond skeletons, but keep tests focused. Wind currently has data ownership and refresh logic; avoid behavior assertions that depend on final wind-zone scanning or particle force mixing until that layer is explicitly ported.
- VirtualMesh work can resume as compile-safe function/data ports first. Avoid deep behavior assertions around proxy generation, mapping solve, reduction, or skinning until those functions have been ported end to end.

Next priority: continue the bottom-up XPBD core port. `TetherConstraint`, runtime-side `TriangleBendingConstraint`, runtime-side `AngleConstraint`, collider collision, SelfCollision Self/Sync/ParentSync runtime paths, and the first TeamData/mapping ownership layer are now present; next targets are the remaining data builders/update-list population needed to feed these constraints, plus more TeamManager lifecycle/culling/parameter synchronization pieces.
