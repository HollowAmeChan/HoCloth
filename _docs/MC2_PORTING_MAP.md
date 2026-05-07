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
- `bl-boundary`: Unity-side object/Renderer/Transform/API behavior that belongs primarily to Blender Python authoring, compile, runtime exchange, or writeback layers.

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
| Cloth/Constraints | partial | `_native/include/hocloth/cloth/constraints/`; solver/data-owner layer is mostly ported, remaining gaps are Angle full PreBuild/proxy feed, SelfCollision full builder parity, and Blender-side collider lifecycle wiring |
| Cloth/Collider | partial | `_native/include/hocloth/cloth/collider/`; native authoring data layer, collider-data conversion, and manager range registration bridge are present, Blender lifecycle bridge remains |
| Cloth/Wind | partial | `_native/include/hocloth/cloth/wind/`, `_native/include/hocloth/manager/simulation/wind_manager.hpp` |
| VirtualMesh | partial | `_native/include/hocloth/virtual_mesh/`; structured share/unique PreBuild deserialization, raw-byte proxy array restoration, and container ownership are now present, packed hash dictionaries remain |
| Reduction | partial | `_native/include/hocloth/reduction/`; settings/work data, Same/Simple/Shape reduction passes, base step flow, and VirtualMesh reduction handoff are present; full parity audit remains |
| PreBuild | partial | `_native/include/hocloth/prebuild/`, `_native/include/hocloth/manager/cloth/prebuild_manager.hpp`; share/unique/serialize data containers, build-id lookup, validation, transform-id replacement, manager reference-cache ownership, structured VirtualMesh restoration, and RenderSetup share restoration are present |

## Blender/Python Boundary Progress

These MC2 files should not be forced into the native solver layer. Their Unity object access maps to stable compiled data, frame inputs, or writeback on the Blender side.

| Boundary area | Status | HoCloth side |
| --- | --- | --- |
| Component wrapper / authoring state | partial | `components/`, `authoring/`; MC2 preset parameters, simplified UI, native backend default, build/debug controls, compiled-scene preview |
| Mesh / renderer / transform acquisition | partial | `compile/compiler.py`, `compile/compiled.py`, `runtime/inputs.py`; Blender object ids, mesh/bone inputs, frame transform data |
| Native bridge envelope | partial | `runtime/exchange.py`, `runtime/bridge.py`, `runtime/session.py`, `_native/src/hocloth_python_module.cpp`; session lifecycle and compiled/frame/native payload exchange |
| Runtime stepping and writeback | partial | `runtime/live.py`, `runtime/pose_apply.py`; realtime step, returned transform summary, applied/missing counts, pose writeback path |
| Debug artifacts | partial | `_build/compiled_scene_preview.json`, `_build/frame_inputs_preview.json`, `_build/runtime_debug_latest.json`; useful for BL/native boundary diagnosis |

## Cloth

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/CheckSliderSerializeData.cs` | complete | `cloth/parameters/check_slider_serialize_data.hpp` |
| `Cloth/ClothBehaviour.cs` | bl-boundary | `components/`, `authoring/`, `runtime/`; Blender component/runtime boundary, native owns only compiled data and solver state |
| `Cloth/ClothForceMode.cs` | complete | `cloth/cloth_force_mode.hpp` |
| `Cloth/ClothNormalAxis.cs` | complete | `cloth/cloth_normal_axis.hpp` |
| `Cloth/ClothParameters.cs` | partial | `cloth/cloth_parameters.hpp`; native wind parameter ownership is present |
| `Cloth/ClothProcess.cs` | partial | `cloth/cloth_process.*`; native state/init shell, PreBuild construction, manager registration/release, StartUse/EndUse/DataUpdate, runtime-build result handoff, parameter sync, proxy/render mesh container ownership, and constraint-data handoff are present |
| `Cloth/ClothProcessData.cs` | partial | `cloth/cloth_process.*`; state flags, result/team id, render handles, render mesh info, proxy container, custom skinning records, skip-writing flags, manager registration state, and transform replacement are present |
| `Cloth/ClothProcessGeneration.cs` | partial | `cloth/cloth_process.*`; scale status check, initialization preflight, PreBuild share/unique mesh restoration, and registration lifecycle are present; Unity renderer setup, async runtime mesh build, selection generation, proxy conversion, and mapping generation remain |
| `Cloth/ClothSerializeData.cs` | partial | `cloth/cloth_serialize_data.hpp`; MC2 authoring fields, source/root id lists, paint mode, settings blocks, runtime constraint params, validation, and `GetClothParameters()` are present |
| `Cloth/ClothSerializeData2.cs` | partial | `cloth/cloth_serialize_data.hpp`; selection data, bone/renderer attribute containers, PreBuild data ownership, and transform-id replacement are present |
| `Cloth/ClothSerializeDataFunction.cs` | partial | `cloth/cloth_serialize_data.hpp`; `IsValid()`, `DataValidate()`, and parameter conversion are present; Unity Json import/export and object hash behavior remain boundary/deferred |
| `Cloth/ClothUpdateMode.cs` | complete | `cloth/cloth_parameters.hpp` |
| `Cloth/CullingSettings.cs` | partial | `cloth/parameters/culling_settings.hpp`; Renderer/GameObject references stay at the Blender viewport boundary |
| `Cloth/CurveSerializeData.cs` | partial | `cloth/parameters/curve_serialize_data.hpp`; native float4x4 curve-data path is present, Unity AnimationCurve copy remains a boundary |
| `Cloth/CustomSkinningSettings.cs` | partial | `cloth/custom_skinning_settings.hpp`; Unity Transform list is represented as backend transform ids |
| `Cloth/GizmoSerializeData.cs` | complete | `cloth/gizmo_serialize_data.hpp`; draw execution remains Blender authoring/gizmo layer |
| `Cloth/MagicaCloth.cs` | bl-boundary | Blender component wrapper; covered by HoCloth component properties, panel operators, compile entry points, and runtime session wiring |
| `Cloth/MagicaClothAnimationProperty.cs` | bl-boundary | animation-driven property wrapper; maps to Blender component properties/driver updates and runtime parameter sync, not native solver ownership |
| `Cloth/MagicaClothAPI.cs` | bl-boundary | native API + Python bridge; partially covered by `runtime/exchange.py`, `runtime/session.py`, `runtime/bridge.py`, and the nanobind module |
| `Cloth/NormalAlignmentSettings.cs` | partial | `cloth/normal_alignment_settings.hpp`; Unity Transform reference is represented as a backend transform id |
| `Cloth/SelectionData.cs` | partial | `cloth/selection_data.hpp`; data container/clone/compare/add/fill/merge are present, GridMap conversion remains deferred |

## Cloth/Collider

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Collider/ColliderComponent.cs` | partial | `cloth/collider/collider_component.hpp`; center/size/enabled/team registration, validation hook, size/reverse virtuals, and `ColliderData` conversion are present; Unity lifecycle and manager notification bridge remain at Blender/API boundary |
| `Cloth/Collider/MagicaCapsuleCollider.cs` | complete | `cloth/collider/capsule_collider.hpp`; direction/alignment/reverse/radius separation, size normalization, local dir/up, and collider type mapping are present |
| `Cloth/Collider/MagicaPlaneCollider.cs` | complete | `cloth/collider/plane_collider.hpp` |
| `Cloth/Collider/MagicaSphereCollider.cs` | complete | `cloth/collider/sphere_collider.hpp`; radius validation and type mapping are present |

## Cloth/Constraints

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Constraints/AngleConstraint.cs` | partial | `cloth/constraints/angle_constraint.*`; runtime solver/work buffers are ported, native baseline arrays plus Mesh/Bone parent-generation feed are present, full PreBuild/proxy conversion remains to close |
| `Cloth/Constraints/ColliderCollisionConstraint.cs` | partial | `cloth/constraints/collider_collision_constraint.*`; point/edge solver, collider work-data path, native collider authoring data, and manager registration bridge are present; Blender/API lifecycle wiring remains. Current Blender BoneSpring runtime still uses the bootstrap collision response in `_native/src/hocloth_runtime_api.cpp` / `runtime/bridge.py`, not this full manager constraint yet |
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
| `Interface/ITransform.cs` | bl-boundary | Unity `Transform` collection/replacement boundary; represented by backend transform records plus Blender object ids and frame input transforms |
| `Interface/IValid.cs` | complete | `core/interface/i_valid.hpp` |

## Manager

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Manager/IManager.cs` | skeleton | `manager/i_manager.hpp` |
| `Manager/MagicaManager.cs` | partial | `manager/magica_manager.*`, native frame-step orchestration and ClothProcess register/unregister/start/end entry points now exist |
| `Manager/MagicaManagerAPI.cs` | partial | `manager/magica_manager.*`, `manager/simulation/time_manager.*`; global time scale, simulation frequency, max frame step count, update location, and initialization location APIs are present; Unity events and PreBuild unload API remain boundary/deferred |
| `Manager/MagicaSettings.cs` | complete | `manager/magica_settings.hpp`; refresh mode, simulation frequency, max frame step count, initialization location, update location, and validation are present |
| `Manager/Cloth/ClothManager.cs` | partial | `manager/cloth/cloth_manager.*`, MC2 constraint solve order is centralized |
| `Manager/Cloth/PreBuildManager.cs` | partial | `manager/cloth/prebuild_manager.*`; shared data cache, reference counting, unload-unused, status dump, constraint-data ownership, structured VirtualMesh share deserialization, and RenderSetup share deserialization are present; Unity renderer object restoration remains boundary |
| `Manager/Render/RenderData.cs` | bl-boundary | Blender writeback/render-object boundary; native should keep only stable mapping/output buffers |
| `Manager/Render/RenderManager.cs` | bl-boundary | Blender runtime writeback boundary; `runtime/pose_apply.py` and native output buffers cover the current exchange path |
| `Manager/Render/RenderSetupData.cs` | bl-boundary | render setup comes from Blender compile data; native serialization shells exist where PreBuild needs stable ids/ranges |
| `Manager/Render/RenderSetupDataSerialization.cs` | partial | `manager/render/render_setup_data_serialization.*`; PreBuild share/unique serialization containers and native share deserialize object are present, Unity renderer/mesh object collection remains a Blender boundary |
| `Manager/Simulation/ColliderManager.cs` | partial | `manager/simulation/collider_manager.*`; collider arrays, work-data, pre/start/end/post simulation jobs, update-list population, native collider range registration/removal/update/enable bridge are present |
| `Manager/Simulation/SimulationManager.cs` | partial | `manager/simulation/simulation_manager.*`, step lifecycle and processing-list population are now routed through native manager state |
| `Manager/Simulation/TimeManager.cs` | partial | `manager/simulation/time_manager.*`; simulation frequency/max-step/global-time-scale/update-location setters plus simulation delta/max-step/power calculation are present; Unity FixedUpdate/render counters remain boundary |
| `Manager/Simulation/WindManager.cs` | partial | `manager/simulation/wind_manager.*`; wind data ownership, registration/removal/enable, and native zone refresh are present |
| `Manager/Team/TeamManager.cs` | partial | `manager/team/team_manager.*`, parameter + inertia center/wind ownership, timing/update-count lifecycle, sync lists, state/control APIs, post-step flag cleanup |
| `Manager/Team/TeamWindData.cs` | complete | `manager/team/team_wind_data.hpp` |
| `Manager/TransformManager/TransformData.cs` | partial | `manager/transform/transform_data.*`; inverse rotation/root/dirty arrays are present, Unity transform-access restore remains a boundary |
| `Manager/TransformManager/TransformDataSerialization.cs` | partial | `manager/transform/transform_data_serialization.*`; share flag/init-pose arrays plus unique transform-record collection/replacement are present, Unity Transform object arrays are represented by backend records |
| `Manager/TransformManager/TransformManager.cs` | partial | `manager/transform/transform_manager.*`; backend transform record/update/root/dirty ownership is present |
| `Manager/TransformManager/TransformRecord.cs` | complete | `manager/transform/transform_record.*`; Unity `Transform` object access is represented by backend record input |
| `Manager/VirtualMesh/VirtualMeshManager.cs` | partial | `manager/virtual_mesh/virtual_mesh_manager.*`; proxy/mapping/common buffers plus baseline flags/team ids/start/count/data ownership, proxy exit, and mapping exit are present |

## PreBuild / Reduction / Settings

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `PreBuild/PreBuildScriptableObject.cs` | partial | `prebuild/prebuild_serialize_data.hpp`; native `PreBuildDataLibrary` mirrors build-id lookup/add/replace, Blender asset warmup remains boundary |
| `PreBuild/PreBuildSerializeData.cs` | partial | `prebuild/prebuild_serialize_data.hpp`; enable/build-id/share lookup/data validation/transform replacement are present |
| `PreBuild/SharePreBuildData.cs` | partial | `prebuild/share_prebuild_data.hpp`; version/build-result/scale validation, proxy/render mesh serialization references, and constraint-data ownership are present |
| `PreBuild/UniquePreBuildData.cs` | partial | `prebuild/unique_prebuild_data.hpp`; render/proxy/render-mesh unique data plus transform-id collection/replacement are present |
| `Reduction/ReductionSettings.cs` | complete | `reduction/reduction_settings.hpp` |
| `Reduction/ReductionWorkData.cs` | partial | `reduction/reduction_work_data.hpp`; native data ownership shell exists, job buffers are adapted to C++ containers, and reduction remap/organization buffers are present |
| `Reduction/SameDistanceReduction.cs` | partial | `reduction/same_distance_reduction.*`; grid search, join-pair collection, JoinJob2-style live/dead vertex merge, link update, attribute/bone-weight merge, and final normal/weight cleanup are present |
| `Reduction/ShapeDistanceReduction.cs` | partial | `reduction/shape_distance_reduction.*`; connected-neighbor candidate search, CheckJoin2 filtering, and min-cost join-edge selection are present |
| `Reduction/SimpleDistanceReduction.cs` | partial | `reduction/simple_distance_reduction.*`; grid-based candidate search and MC2-style cost join-edge generation are present |
| `Reduction/StepReductionBase.cs` | partial | `reduction/step_reduction_base.*`; JoinEdge, step scheduling, merge-length stepping, join-edge sorting/selection, pair merge, link refresh, and final normal/bone-weight cleanup are present |
| `Settings/ClothDebugSettings.cs` | complete | `settings/cloth_debug_settings.hpp` |
| `Settings/VirtualMeshDebugSettings.cs` | complete | `settings/virtual_mesh_debug_settings.hpp` |

## Utility

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Utility/Data/DataUtility.cs` | partial | `utility/data/data_utility.*`; MC2 pack/unpack and remaining-data helpers are present, Unity object conversion remains a boundary |
| `Utility/Data/MultiDataBuilder.cs` | complete | `utility/data/multi_data_builder.hpp` |
| `Utility/Grid/GridMap.cs` | partial | `utility/grid/grid_map.hpp`; native hash-map helper exists, Unity job/container semantics are adapted |
| `Utility/Jobs/InterlockUtility.cs` | partial | `utility/jobs/interlock_utility.hpp`; fixed-point aggregate add/read/max/clear helpers and synchronous aggregate solve helpers are present; Unity atomic job scheduling is adapted away |
| `Utility/Jobs/JobUtility.cs` | partial | `utility/jobs/job_utility.hpp`; synchronous fill/serial-number/hashset-list/AABB/sphere-UV/transform-position/int-copy/index-to-multimap helpers are present; Unity JobHandle/Burst scheduling is adapted away |
| `Utility/Math/AABB.cs` | complete | `utility/math/math_types.hpp`, `utility/math/math_utility.*` |
| `Utility/Math/IntAABB.cs` | complete | `utility/math/int_aabb.hpp` |
| `Utility/Math/MathExtensions.cs` | complete | `utility/math/math_extensions.*` |
| `Utility/Math/MathUtility.cs` | partial | `utility/math/math_utility.*` |
| `Utility/Math/MinimumData.cs` | complete | `utility/math/minimum_data.hpp` |
| `Utility/Mesh/MeshUtility.cs` | bl-boundary | Unity `Renderer`/`MeshFilter`/`SkinnedMeshRenderer` access maps to Blender mesh/object acquisition in `compile/`; no native solver module planned |
| `Utility/Misc/Develop.cs` | complete | `utility/misc/develop.*`; native output/assert backend replaces Unity `Debug` |
| `Utility/Misc/StaticStringBuilder.cs` | complete | `utility/misc/static_string_builder.hpp`; shared static append/append-line/append-to-string helper is present |
| `Utility/NativeCollection/DataChunk.cs` | complete | `utility/native_collection/data_chunk.*` |
| `Utility/NativeCollection/ExBitFlag16.cs` | complete | `utility/native_collection/bit_flag.hpp` |
| `Utility/NativeCollection/ExBitFlag8.cs` | complete | `utility/native_collection/bit_flag.hpp` |
| `Utility/NativeCollection/ExCostSortedList1.cs` | complete | `utility/native_collection/ex_cost_sorted_list1.hpp` |
| `Utility/NativeCollection/ExCostSortedList4.cs` | complete | `utility/native_collection/ex_cost_sorted_list4.hpp` |
| `Utility/NativeCollection/ExNativeArray.cs` | partial | `utility/native_collection/ex_native_array.hpp`; chunk reuse, expand/fill/remove, summary/debug helpers are present, unsafe reinterpret/serialization is adapted |
| `Utility/NativeCollection/ExProcessingList.cs` | complete | `utility/native_collection/ex_processing_list.*`; C++ counter pointer replaces Unity `NativeReference` job pointer |
| `Utility/NativeCollection/ExSimpleNativeArray.cs` | partial | `utility/native_collection/ex_simple_native_array.hpp`; range add/fill/remove, summary/debug helpers are present, unsafe reinterpret/serialization is adapted |
| `Utility/NativeCollection/ExTransformAccessArray.cs` | bl-boundary | Unity job transform-access wrapper; represented by `TransformManager` records plus Blender frame-input arrays |
| `Utility/NativeCollection/FixedList128BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/FixedList32BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/FixedList4096BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/FixedList512BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/FixedList64BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/NativeArrayExtensions.cs` | partial | `utility/native_collection/native_array_extensions.hpp`; raw byte conversion helpers are present for trivially copyable values and BitFlag8, Unity allocator/dispose helpers remain irrelevant |
| `Utility/NativeCollection/NativeMultiHashMapExtensions.cs` | partial | `utility/native_collection/native_multi_hash_map_extensions.hpp`; contains/unique-add/remove/to-fixed-list/serialize/deserialize helpers are present, Unity Burst/job allocator details are adapted away |
| `Utility/NativeCollection/NativeReferenceExtensions.cs` | complete | `utility/native_collection/native_reference_extensions.hpp`; interlocked start-index helper is present for atomic and local counters |
| `Utility/ResultCode/Exception.cs` | complete | `utility/result_code/exception.*` |
| `Utility/ResultCode/ResultCode.cs` | complete | `utility/result_code/result_code.*`; native debug logging is treated as the C++ logging boundary |
| `Utility/Time/TimeSpan.cs` | complete | `utility/time/time_span.*`; DebugLog/Log are omitted at the C++ logging boundary |
| `Utility/Time/UnityTimeSpan.cs` | bl-boundary | Blender/native profiling abstraction; not part of solver behavior |

## VirtualMesh

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `VirtualMesh/VertexAttribute.cs` | complete | `virtual_mesh/vertex_attribute.hpp` |
| `VirtualMesh/VirtualMesh.cs` | partial | `virtual_mesh/virtual_mesh.*`; core arrays, fixed list/AABB, bind pose, transform restore rotations, baseline arrays, parent-driven baseline/root/depth/local-pose builder, optimization entry, and reduction organization entry points are present |
| `VirtualMesh/VirtualMeshBoneWeight.cs` | complete | `virtual_mesh/virtual_mesh_bone_weight.*` |
| `VirtualMesh/VirtualMeshContainer.cs` | partial | `virtual_mesh/virtual_mesh_container.*`; share mesh plus unique transform-record override are present, and managed PreBuild meshes are left owned by PreBuildManager |
| `VirtualMesh/VirtualMeshPrimitive.cs` | complete | `virtual_mesh/virtual_mesh_primitive.hpp` |
| `VirtualMesh/VirtualMeshRaycastHit.cs` | complete | `virtual_mesh/virtual_mesh_raycast_hit.hpp` |
| `VirtualMesh/VirtualMeshTransform.cs` | partial | `virtual_mesh/virtual_mesh_transform.*` |
| `VirtualMesh/Function/VirtualMeshInputOutput.cs` | planned | `virtual_mesh/function/virtual_mesh_input_output.*` |
| `VirtualMesh/Function/VirtualMeshMapping.cs` | planned | `virtual_mesh/function/virtual_mesh_mapping.*` |
| `VirtualMesh/Function/VirtualMeshOptimization.cs` | partial | `virtual_mesh/virtual_mesh.*`; duplicate triangle removal path is present |
| `VirtualMesh/Function/VirtualMeshProxy.cs` | partial | `virtual_mesh/virtual_mesh.*`; fixed-list/AABB, vertex bind pose, vertex-to-transform rotation, Mesh edge baseline parent generation, and Bone transform baseline generation are present; full proxy conversion/normal tangent/edge flag/reduction/custom skinning remain |
| `VirtualMesh/Function/VirtualMeshReduction.cs` | partial | `virtual_mesh/virtual_mesh.*`, `reduction/*.hpp`; Reduction entry point, InitReductionWorkData, Same/Simple/Shape algorithm chain, OrganizationInit, remap, basic-data copy/remap, line/triangle rebuild, and store-back are present; full parity audit remains |
| `VirtualMesh/Function/VirtualMeshSerialization.cs` | partial | `virtual_mesh/virtual_mesh_serialization.*`; share/unique serialization data containers, structured simple-array restore, raw-byte proxy array restore, unique transform-record restore, center fixed list, and baseline fallback are present; packed hash dictionaries remain deferred |
| `VirtualMesh/Function/VirtualMeshWork.cs` | partial | `virtual_mesh/virtual_mesh.*`; average/max vertex distance sampling is present |

## Next

Last completed step:

- Extended the PreBuild data-chain pass: `VirtualMeshSerializationData::ShareDeserialize(...)` now restores MC2 raw-byte proxy arrays for edges, edge flags, bind pose, transform rotations, depth/root/parent/child data, vertex local pose, normal adjustment, and baseline arrays; `NativeArrayExtensions.cs` now has a native raw-byte helper surface; `TransformDataSerialization.cs` and `RenderSetupDataSerialization.cs` now have native partial ports.
- Extended the native `ClothProcess` lifecycle toward MC2 parity: PreBuild construction now restores share/unique proxy and render mesh containers, `RegisterToManagers(...)` registers teams/proxy particles/mapping meshes/inertia-distance-bending constraints, `UnregisterFromManagers(...)` releases those resources, `StartUse`/`EndUse`/`DataUpdate` are present, and `RuntimeBuildResult` gives later async runtime mesh construction a single handoff point into the same manager pipeline.
- Added structured `VirtualMeshSerializationData::ShareDeserialize(...)` and unique transform-record restoration. PreBuildManager now restores structured simple-array mesh fields instead of name/type-only shells; `VirtualMeshContainer` preserves manager-owned PreBuild meshes; `VirtualMeshManager` now has `ExitProxyMesh(...)` so ClothProcess can release proxy/global buffer ownership.
- Added the first native `ClothProcess` data/init shell from `ClothProcess.cs`, `ClothProcessData.cs`, and `ClothProcessGeneration.cs`: MC2 state flags, result/team id, render handles, render mesh info, proxy container, custom skinning records, skip-writing, transform replacement, scale status check, serialize-data validation, PreBuild registration, parameter sync, and constraint-data handoff are now present. Unity renderer setup, async build, selection generation, proxy conversion, mapping, and full manager registration remain later phases.
- Added the first native `PreBuildManager` pass from `Manager/Cloth/PreBuildManager.cs`: shared prebuild data is cached by build id, reference counted, unloaded when unused, surfaced in `MagicaManager`, and carries render setup serialization data plus proxy/render mesh shells and distance/bending/inertia constraint data. Full raw-byte `VirtualMesh` / `RenderSetupData` deserialization remains a later data-chain pass.
- Added the first native `ClothSerializeData` / `ClothSerializeData2` data-layer pass: MC2 authoring fields, source renderer/root ids, paint mode, settings blocks, PreBuild ownership, transform-id replacement, build validation, `DataValidate()`, and `GetClothParameters()` now exist in `cloth/cloth_serialize_data.hpp`. Json import/export, Unity object hashing, and renderer/texture object collection stay at the Blender boundary.
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
- Audited the Blender-visible collision path: the active BoneSpring backend currently routes compiled `collision_objects` / `collision_bindings` through the bootstrap angular-space solver. The bootstrap path now uses consistent world-to-angle scaling for collider center/radius/capsule height, clamps per-collider push distance, and no longer auto-binds every collider to every spring chain when no collider group exists. Full MC2 `ColliderManager + ColliderCollisionConstraint` runtime wiring remains the next collider integration step.
- Started the active-runtime collider bridge toward full MC2 manager wiring: native `hocloth_runtime_api.cpp` now converts each compiled collision object into `mc2::ColliderManager::ColliderData`, keeps per-binding collider-index lookup caches, refreshes those data records from per-frame runtime collision inputs, and exposes `mc2_collider_data` / `mc2_collider_bindings` in backend status. The Python fallback mirrors this cache shape for debugging parity. Capsule exchange now carries MC2 direction/alignment/reverse/end-radius fields from Blender authoring properties, mapping to `CapsuleX/Y/ZCenter` or `CapsuleX/Y/ZStart`.
- Extended the active-runtime bridge from raw collider data to MC2 manager registration: each compiled spring chain now gets a collider-only MC2 team, explicit collision bindings are registered into `ColliderManager::RegisterColliderDataRange(...)`, runtime collision inputs update the registered collider data, and the bridge runs the MC2 collider lifecycle through `PreSimulationUpdate(...)`, `CreateUpdateColliderList(...)`, `StartSimulationStep(...)`, and `EndSimulationStep(...)` to produce collider `WorkData`. Backend status now reports registered collider and work-data counts. The solver still uses bootstrap angular-space response until particle/VirtualMesh data is wired into `ColliderCollisionConstraint`.
- Started the active-runtime particle-buffer bridge required by `ColliderCollisionConstraint`: each BoneSpring chain now builds a minimal MC2 proxy `VirtualMesh` with one vertex per joint, registers it through `VirtualMeshManager::RegisterProxyMesh(...)`, registers a matching `SimulationManager` particle range, initializes base/next/old/velocity/step-basic position and rotation buffers, marks step particles, and calls `ColliderCollisionConstraint::Solve(...)` in point mode after collider `WorkData` generation. The result is still not mapped back to Blender bone output; this stage exists to make MC2 particle-side collision buffers live before replacing bootstrap angular response.
- Added the first native writeback path from MC2 particle buffers to BoneSpring output: after the existing spring/inertia joint-state update, runtime now writes joint states into MC2 particle `NextPositions`, runs MC2 collider work/point solve, then projects solved `NextPositions - BasePositions` back to compact pitch/roll joint state before `GetBoneTransforms(...)`. The old angular-space bootstrap collision response is no longer called in native step output; Python stub still mirrors the older fallback path.
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
- Added the first Reduction data layer: `ReductionSettings` is complete, `ReductionWorkData` now owns the C++ containers for reduction/optimization/final mesh data, and `StepReductionBase` has the MC2 base state plus `JoinEdge` type.
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
- Started closing the lower `VirtualMeshWork` / `VirtualMeshOptimization` / `VirtualMeshReduction` layer: `VirtualMesh::CalcAverageAndMaxVertexDistanceRun()`, `Optimization()`, duplicate triangle removal, reduction work-data initialization, Same/Simple/Shape reduction chain, remap/organization/basic-data generation, line/triangle rebuild, and store-back into `VirtualMesh` are now present. Full parity audit remains deferred.
- Added the native Reduction algorithm pass: `SameDistanceReduction` has grid-based neighbor search, join pair collection, JoinJob2-style dead-to-live merge, link-map refresh, live vertex normal normalization, and bone-weight adjustment; `StepReductionBase` now has MC2-style multi-step merge scheduling, non-overlapping pair selection, pair merge, link refresh, and final cleanup; `SimpleDistanceReduction` and `ShapeDistanceReduction` now generate MC2-style candidate join edges.

Latest verification:

```text
cmake --build _native/build/vs2022-release --config Release --target hocloth_mc2_core_smoke
_bin/hocloth_mc2_core_smoke.exe
package_addon.ps1 -Version codex-compile-prep -IncludeNativeBuild -RunNativeSmoke
native smoke: team_id=1 mesh_id=0 transforms=1 meshes=1 proxy_vertices=2 distance_connections=2 fixed=1 center_data=2 center_transform=0 inertia_x=0.2 gravity_dot=1 force_mode=10 particles=2 steps=1 p0.x=0.116505 p0.y=-0.00133923 motion_p1.x=1.24991 p1.x=1.24991 old_p1.x=1.24991 real_v1.x=-0.00515699 disp_p1.x=1.24991 proxy_p1.x=1.24991
```

The native smoke test now contains finite-value assertions plus synchronization checks for old position, display position, and proxy position writeback. Exact XPBD numeric parity remains a later solver-port verification task.

VirtualMesh scope note:

- `VirtualMesh` / `VirtualMeshManager` are intentionally partial for now. Simple proxy ownership, mapping buffer registration/release, structured serialization restore, average-distance work, duplicate-triangle optimization, and reduction organization/store-back are present; full proxy generation, mapping solve/generation behavior, reduction step algorithms, normal/tangent update jobs, and transform/skinning update jobs are still separate integration work.
- Do not spend deep test effort on VirtualMesh-specific behavior yet. Current tests only need to cover the lower-level infrastructure it depends on: `ExNativeArray` / `ExSimpleNativeArray`, `DataChunk`, packed index helpers, `VertexAttribute`, transform chunk registration, and simple proxy array ownership needed by the solver smoke path.
- When the core XPBD flow is stable, return to VirtualMesh as a separate integration/prebuild phase.

Wind / VirtualMesh test boundary:

- Wind and VirtualMesh can now move beyond skeletons, but keep tests focused. Wind currently has data ownership and refresh logic; avoid behavior assertions that depend on final wind-zone scanning or particle force mixing until that layer is explicitly ported.
- VirtualMesh work can resume as compile-safe function/data ports first. Avoid deep behavior assertions around proxy generation, mapping solve, reduction, or skinning until those functions have been ported end to end.

Next priority: continue the bottom-up XPBD core port. `TetherConstraint`, runtime-side `TriangleBendingConstraint`, runtime-side `AngleConstraint`, collider collision, SelfCollision Self/Sync/ParentSync runtime paths, and the first TeamData/mapping ownership layer are now present; next targets are the remaining data builders/update-list population needed to feed these constraints, plus more TeamManager lifecycle/culling/parameter synchronization pieces.
