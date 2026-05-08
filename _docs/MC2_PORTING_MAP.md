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
| Define | complete | `_native/include/hocloth/core/define/` |
| Interface | complete | `_native/include/hocloth/core/interface/`; Unity `Transform` object interface remains a Blender/backend-record boundary |
| Utility/ResultCode | complete | `_native/include/hocloth/utility/result_code/` |
| Utility/Math | complete | `_native/include/hocloth/utility/math/` |
| Utility/Data | complete | `_native/include/hocloth/utility/data/` |
| Utility/NativeCollection | complete | `_native/include/hocloth/utility/native_collection/`; Unity allocator/job-transform wrappers remain boundary/adapted |
| Utility/Time | complete | `_native/include/hocloth/utility/time/`; `UnityTimeSpan` remains a Blender/native profiling boundary |
| Manager | partial | `_native/include/hocloth/manager/` |
| Cloth/Constraints | partial | `_native/include/hocloth/cloth/constraints/`; solver/data-owner layer is mostly ported, remaining gaps are Angle full PreBuild/proxy feed, SelfCollision full builder parity, and Blender-side collider lifecycle wiring |
| Cloth/Collider | partial | `_native/include/hocloth/cloth/collider/`; native authoring data layer, collider-data conversion, and manager range registration bridge are present, Blender lifecycle bridge remains |
| Cloth/Wind | partial | `_native/include/hocloth/cloth/wind/`, `_native/include/hocloth/manager/simulation/wind_manager.hpp` |
| VirtualMesh | partial | `_native/include/hocloth/virtual_mesh/`; structured share/unique PreBuild deserialization, raw-byte proxy array restoration, container ownership, and reduction transform remap storage are now present, packed hash dictionaries remain |
| Reduction | partial | `_native/include/hocloth/reduction/`; settings/work data, shared link/join helper surface, Same/Simple/Shape reduction passes, base step flow, and VirtualMesh reduction handoff are present; full parity audit remains |
| PreBuild | partial | `_native/include/hocloth/prebuild/`, `_native/include/hocloth/manager/cloth/prebuild_manager.hpp`; share/unique/serialize data containers, build-id lookup, validation, transform-id replacement, manager reference-cache ownership, structured VirtualMesh restoration, and RenderSetup share restoration are present |

## Blender/Python Boundary Progress

These MC2 files should not be forced into the native solver layer. Their Unity object access maps to authoring snapshots, frame inputs, native build outputs, or writeback on the Blender side.

| Boundary area | Status | HoCloth side |
| --- | --- | --- |
| Component wrapper / authoring state | partial | `components/mc2.py`, `authoring/`; MC2 preset parameters, simplified UI, native backend default, and first BoneCloth component creation/viewport path |
| Mesh / renderer / transform acquisition | partial | `runtime/authoring_snapshot.py`, `runtime/blender_bone_refs.py`, `runtime/inputs.py`; Blender object ids, mesh/bone references, frame transform data, Cache Output based `mesh_writeback_targets`, and explicit full custom-skinning/normal-adjustment `TransformRecord` feed remains a BL/native boundary item |
| Native bridge envelope | partial | `runtime/exchange.py`, `runtime/bridge.py`, `runtime/session.py`, `_native/src/hocloth_python_module.cpp`; session lifecycle plus authoring/frame/step payload exchange, C++ parsing of BoneCloth/BoneSpring `cloth_type`, active runtime conversion from native-transferred bones to MC2 Bone RenderSetupData, native RenderSetup fallback for BoneCloth custom-skinning/normal-adjustment records, `mesh_writeback_targets`, and native `get_mesh_outputs` export for later RenderData/Mapping output |
| Runtime stepping and writeback | partial | `runtime/live.py`, `runtime/pose_apply.py`; realtime/manual step, returned transform summary, applied/missing counts, pose writeback path, active runtime ProxyBoneMesh team tagging for MC2 post-writeback, object-local/world-space mesh output writeback, and manual mesh-output apply operator |

Current authoring boundary rule:

- Blender component properties must mirror MC2 Unity components. `BONE_CLOTH` / `SPRING_BONE` are authoring modes of `MagicaCloth` with `ClothSerializeData`; colliders are `MagicaSphereCollider` / `MagicaCapsuleCollider` / `MagicaPlaneCollider` style components.
- Old HoCloth `components/properties.py`, `components/registry.py`, and `compile/` package have been removed from the active frontend. Scene authoring now goes through MC2-style component collections plus an `authoring_snapshot`.
- New protocol work should extend `authoring_snapshot`, `frame_inputs`, `build_output`, or `step_output`. Python no longer generates a backend scene view or debug preview files; viewport and writeback consumers read native `build_output`.
- New frontend work lives in `components/mc2.py` and the simplified `authoring/panel.py`. The main panel creates MC2-native component containers and the build path sends `authoring_snapshot` to native first.

## Cloth

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/CheckSliderSerializeData.cs` | complete | `cloth/parameters/check_slider_serialize_data.hpp` |
| `Cloth/ClothBehaviour.cs` | bl-boundary | `components/`, `authoring/`, `runtime/`; Blender component/runtime boundary, native owns only compiled data and solver state |
| `Cloth/ClothForceMode.cs` | complete | `cloth/cloth_force_mode.hpp` |
| `Cloth/ClothNormalAxis.cs` | complete | `cloth/cloth_normal_axis.hpp` |
| `Cloth/ClothParameters.cs` | partial | `cloth/cloth_parameters.hpp`; native wind parameter ownership plus BoneCloth rotational/root-rotation parameters are present |
| `Cloth/ClothProcess.cs` | partial | `cloth/cloth_process.*`; native state/init shell, PreBuild construction, manager registration/release, StartUse/EndUse/DataUpdate, Bone RenderSetup runtime-build result creation, BoneCloth selection/bone-attribute application, custom-skinning/normal-adjustment record resolution into proxy conversion, runtime-build result handoff, registered-manager pending parameter refresh, skip-writing refresh, parameter sync, proxy/render mesh container ownership, and constraint-data handoff are present |
| `Cloth/ClothProcessData.cs` | partial | `cloth/cloth_process.*`; state flags, result/team id, render handles, render mesh info, proxy container, full custom skinning transform record input/storage, skip-writing flags, manager registration state, and transform replacement are present |
| `Cloth/ClothProcessGeneration.cs` | partial | `cloth/cloth_process.*`, `cloth/selection_data.hpp`; scale status check, initialization preflight, BoneCloth default selection generation from RenderSetup root/parent/pose data, PreBuild share/unique mesh restoration, and registration lifecycle are present; Unity renderer setup, async runtime mesh build, full proxy conversion, and mapping generation remain |
| `Cloth/ClothSerializeData.cs` | partial | `cloth/cloth_serialize_data.hpp`; MC2 authoring fields, source/root id lists, paint mode, settings blocks, runtime constraint params, validation, native-side editor hash surface, and `GetClothParameters()` are present |
| `Cloth/ClothSerializeData2.cs` | partial | `cloth/cloth_serialize_data.hpp`; selection data, BoneCloth transform-id attribute dictionary consumption, renderer attribute containers, PreBuild data ownership, MC2 zero-hash behavior, debug output, and transform-id replacement are present |
| `Cloth/ClothSerializeDataFunction.cs` | partial | `cloth/cloth_serialize_data.hpp`; `IsValid()`, `DataValidate()`, and parameter conversion are present; Unity Json import/export and object hash behavior remain boundary/deferred |
| `Cloth/ClothUpdateMode.cs` | complete | `cloth/cloth_parameters.hpp` |
| `Cloth/CullingSettings.cs` | complete | `cloth/parameters/culling_settings.hpp`; MC2 culling modes, distance culling validation, clone, and defaults are present; Renderer/GameObject references stay at the Blender viewport boundary |
| `Cloth/CurveSerializeData.cs` | complete | `cloth/parameters/curve_serialize_data.hpp`; value/linear curve/float4x4 curve-data evaluation, conversion, validation, and clone are present; Unity AnimationCurve object copying is represented by native curve samples |
| `Cloth/CustomSkinningSettings.cs` | complete | `cloth/custom_skinning_settings.hpp`; enable flag, validation, clone, and transform-list semantics are represented by backend transform ids |
| `Cloth/GizmoSerializeData.cs` | complete | `cloth/gizmo_serialize_data.hpp`; draw execution remains Blender authoring/gizmo layer |
| `Cloth/MagicaCloth.cs` | bl-boundary | Blender component wrapper; covered by HoCloth component properties, panel operators, compile entry points, and runtime session wiring |
| `Cloth/MagicaClothAnimationProperty.cs` | bl-boundary | animation-driven property wrapper; maps to Blender component properties/driver updates and runtime parameter sync, not native solver ownership |
| `Cloth/MagicaClothAPI.cs` | bl-boundary | native API + Python bridge; partially covered by `runtime/exchange.py`, `runtime/session.py`, `runtime/bridge.py`, and the nanobind module |
| `Cloth/NormalAlignmentSettings.cs` | complete | `cloth/normal_alignment_settings.hpp`; alignment mode, validation, clone, hash behavior, and transform reference semantics are represented by a backend transform id |
| `Cloth/SelectionData.cs` | complete | `cloth/selection_data.hpp`; data container/clone/compare/add/fill/merge, BoneCloth RenderSetup default selection generation, plus MC2-style GridMap nearest-attribute `ConvertFrom(...)` are present |

## Cloth/Collider

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Collider/ColliderComponent.cs` | partial | `cloth/collider/collider_component.hpp`; center/size/enabled/team+local-slot registration, validation hook, size/reverse virtuals, `ColliderData` conversion, and native UpdateParameters/Enable/Destroy notification helpers are present; Unity lifecycle dispatch remains at Blender/API boundary |
| `Cloth/Collider/MagicaCapsuleCollider.cs` | complete | `cloth/collider/capsule_collider.hpp`; direction/alignment/reverse/radius separation, size normalization, local dir/up, and collider type mapping are present |
| `Cloth/Collider/MagicaPlaneCollider.cs` | complete | `cloth/collider/plane_collider.hpp` |
| `Cloth/Collider/MagicaSphereCollider.cs` | complete | `cloth/collider/sphere_collider.hpp`; radius validation and type mapping are present |

## Cloth/Constraints

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Constraints/AngleConstraint.cs` | partial | `cloth/constraints/angle_constraint.*`; runtime solver/work buffers are ported, native baseline arrays plus Mesh/Bone parent-generation feed are present, and restoration mass scaling is kept byte-for-byte with MC2 behavior; MC2 has no separate Angle data builder, so remaining closure depends on full VirtualMesh baseline/PreBuild feed parity |
| `Cloth/Constraints/ColliderCollisionConstraint.cs` | partial | `cloth/constraints/collider_collision_constraint.*`; point/edge solver, collider work-data path, native collider authoring data, manager registration bridge, collider transform synchronization, MC2-style edge temp-buffer lifecycle, active BoneSpring/BoneCloth particle writeback bridge, point and edge `DisableCollision`/invalid vertex filtering, and runtime collider scale feed into MC2 `frameScales` are present. Blender authoring now follows MC2's cloth-owned collider-list model via chain-level `collider_ids`, and the runtime bridge feeds world-space bone heads plus radius/limit-distance parameters into the MC2 collider solver; remaining work is broader Blender/API lifecycle parity and final numerical audit |
| `Cloth/Constraints/DistanceConstraint.cs` | complete | `cloth/constraints/distance_constraint.*`; params, data owner, `CreateData(...)`, register/exit, vertical/horizontal/shear runtime solver are present; shear builder now prefers MC2-style `VirtualMesh.edgeToTriangles` topology cache |
| `Cloth/Constraints/InertiaConstraint.cs` | partial | `cloth/constraints/inertia_constraint.*`, `manager/team/team_manager.*`, `manager/simulation/simulation_manager.*`; CenterData/fixed list/CreateData plus per-frame inertia lifecycle are structurally ported, but BoneCloth startup parity is still under audit because this path depends on exact `VirtualMeshProxy` fixed-list, adjacency, baseline, and transform-input semantics |
| `Cloth/Constraints/MotionConstraint.cs` | complete | `cloth/constraints/motion_constraint.*`; max-distance/backstop/stiffness runtime path is ported, MC2's disabled friction block remains intentionally omitted |
| `Cloth/Constraints/SelfCollisionConstraint.cs` | partial | `cloth/constraints/self_collision_constraint.*`; primitive ownership, MC2-style `Register`/`Exit` lifecycle, `UpdateTeam(...)` flag/primitive lifecycle, ClothProcess register/exit/parameter-dirty wiring, broad phase, XPBD contacts, intersect/tangle paths, Self/Sync/ParentSync processing lists, and debug/status reporting are present; remaining work is final numerical audit plus deeper PreBuild/VirtualMesh feed parity |
| `Cloth/Constraints/SpringConstraint.cs` | complete | `cloth/cloth_parameters.hpp`, fixed-particle branch in `manager/simulation/simulation_manager.*`; MC2 class has no active solver beyond params, BoneSpring runtime branch is ported |
| `Cloth/Constraints/TetherConstraint.cs` | complete | `cloth/constraints/tether_constraint.*`; params and runtime root-distance compression/stretch solver are ported |
| `Cloth/Constraints/TriangleBendingConstraint.cs` | complete | `cloth/constraints/triangle_bending_constraint.*`; params, data owner, `CreateData(...)`, register/exit, dihedral/volume solver, and aggregate writeback are present; pair builder now prefers MC2-style `VirtualMesh.edgeToTriangles` topology cache |

## Cloth/Wind

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/Wind/MagicaWindZone.cs` | partial | `cloth/wind/magica_wind_zone.hpp`; native data-form zone and direction helpers are present, TeamWind selection and particle force mixing are routed, Unity component lifecycle remains Blender boundary |
| `Cloth/Wind/WindParams.cs` | complete | `cloth/wind/wind_params.hpp` |
| `Cloth/Wind/WindSettings.cs` | complete | `cloth/wind/wind_settings.hpp` |

## Define / Interface

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Define/ResultDefine.cs` | complete | `utility/result_code/result_code.hpp` |
| `Define/SystemDefine.cs` | complete | `core/define/system_define.hpp`; MC2 system constants and define symbol are present, time-setting usage is covered by `TimeManager` |
| `Interface/ICount.cs` | complete | `core/interface/i_count.hpp` |
| `Interface/IDataValidate.cs` | complete | `core/interface/i_data_validate.hpp` |
| `Interface/ITransform.cs` | bl-boundary | Unity `Transform` collection/replacement boundary; represented by backend transform records plus Blender object ids and frame input transforms |
| `Interface/IValid.cs` | complete | `core/interface/i_valid.hpp` |

## Manager

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Manager/IManager.cs` | complete | `manager/i_manager.hpp`; Initialize/Dispose/Status cover MC2 Initialize/Dispose/InformationLog roles, while EnterdEditMode is folded into native Dispose/Blender lifecycle |
| `Manager/MagicaManager.cs` | partial | `manager/magica_manager.*`, native frame-step orchestration, ClothProcess register/unregister/start/end entry points, and frame-start pending cloth parameter refresh now exist |
| `Manager/MagicaManagerAPI.cs` | partial | `manager/magica_manager.*`, `manager/simulation/time_manager.*`; global time scale, simulation frequency, max frame step count, update location, and initialization location APIs are present; Unity events and PreBuild unload API remain boundary/deferred |
| `Manager/MagicaSettings.cs` | complete | `manager/magica_settings.hpp`; refresh mode, simulation frequency, max frame step count, initialization location, update location, and validation are present |
| `Manager/Cloth/ClothManager.cs` | partial | `manager/cloth/cloth_manager.*`, MC2 constraint solve order is centralized |
| `Manager/Cloth/PreBuildManager.cs` | partial | `manager/cloth/prebuild_manager.*`; shared data cache, reference counting, unload-unused, status dump, constraint-data ownership, structured VirtualMesh share deserialization, and RenderSetup share deserialization are present; Unity renderer object restoration remains boundary |
| `Manager/Render/RenderData.cs` | bl-boundary | Blender writeback/render-object boundary; native should keep only stable mapping/output buffers |
| `Manager/Render/RenderManager.cs` | bl-boundary | Blender runtime writeback boundary; `runtime/pose_apply.py` and native output buffers cover the current exchange path |
| `Manager/Render/RenderSetupData.cs` | partial | `manager/render/render_setup_data_serialization.*`; render setup object acquisition remains a Blender compile boundary, while native now carries BoneCloth setup type, root/transform/parent/child id lists, BoneSpring collision bone indices, transform pose/name arrays, bone connection mode, render transform index, init render matrices/pose, MC2-style transform/parent lookup helpers, and full `TransformRecord` reconstruction by index/id |
| `Manager/Render/RenderSetupDataSerialization.cs` | partial | `manager/render/render_setup_data_serialization.*`; PreBuild share/unique serialization containers and native share deserialize object are present, Unity renderer/mesh object collection remains a Blender boundary |
| `Manager/Simulation/ColliderManager.cs` | partial | `manager/simulation/collider_manager.*`; collider arrays, work-data, pre/start/end/post simulation jobs, update-list population, native collider range registration/removal/update/enable bridge, runtime collider transform+scale sync, and single-slot invalidation are present |
| `Manager/Simulation/SimulationManager.cs` | partial | `manager/simulation/simulation_manager.*`, particle arrays, step lifecycle, processing-list population, MC2 `tempFloat3Buffer`, `FeedbackTempFloat3Buffer(...)`, WorkBufferUpdate-style processing/temp/count/sum buffer sizing, StartSimulationStep wind-force mixing, end-step static/dynamic collider-friction lifecycle, friction damping, speed clamp, and centrifugal acceleration are now routed through native manager state; remaining work is full solve-loop numerical parity, async/job scheduling adaptation, Unity `noise.cnoise` exact parity, and BoneCloth first-frame parity auditing against MC2 particle reset/base/old buffers |
| `Manager/Simulation/TimeManager.cs` | partial | `manager/simulation/time_manager.*`; simulation frequency/max-step/global-time-scale/update-location setters plus simulation delta/max-step/power calculation are present; Unity FixedUpdate/render counters remain boundary |
| `Manager/Simulation/WindManager.cs` | partial | `manager/simulation/wind_manager.*`; wind data ownership, registration/removal/enable, native zone refresh, TeamWindData selection feed, and particle force consumption are present; Unity `noise.cnoise` exact parity remains |
| `Manager/Team/TeamManager.cs` | partial | `manager/team/team_manager.*`, parameter + inertia center/wind ownership, timing/update-count lifecycle, sync lists, state/control APIs, process-driven parameter/skip-writing refresh, edge-collider counting, TeamWind time carry/update, and post-step flag cleanup; BoneCloth startup parity still depends on exact proxy fixed-list/center and transform input semantics |
| `Manager/Team/TeamWindData.cs` | complete | `manager/team/team_wind_data.hpp` |
| `Manager/TransformManager/TransformData.cs` | partial | `manager/transform/transform_data.*`; inverse rotation/root/dirty arrays are present, Unity transform-access restore remains a boundary |
| `Manager/TransformManager/TransformDataSerialization.cs` | partial | `manager/transform/transform_data_serialization.*`; share flag/init-pose arrays plus unique transform-record collection/replacement are present, Unity Transform object arrays are represented by backend records |
| `Manager/TransformManager/TransformManager.cs` | partial | `manager/transform/transform_manager.*`; backend transform record/update/root/dirty ownership is present |
| `Manager/TransformManager/TransformRecord.cs` | complete | `manager/transform/transform_record.*`; Unity `Transform` object access is represented by backend record input |
| `Manager/VirtualMesh/VirtualMeshManager.cs` | partial | `manager/virtual_mesh/virtual_mesh_manager.*`; proxy/mapping/common buffers plus baseline flags/team ids/start/count/data ownership, proxy exit, mapping exit, MC2-style move-only vertex-root/depth feed for baseline particles, triangle-derived local normal/binormal feed, registration-time vertex/edge array length guards, PreProxyMeshUpdate skinning, triangle normal/tangent rebuild, line-baseline rotation rebuild, and ProxyBoneMesh post-writeback to TransformData are present |

## PreBuild / Reduction / Settings

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `PreBuild/PreBuildScriptableObject.cs` | partial | `prebuild/prebuild_serialize_data.hpp`, `manager/cloth/prebuild_manager.*`; native `PreBuildDataLibrary` mirrors build-id lookup/add/replace/remove/clear/validate/status dump, and `PreBuildManager::Warmup(...)` mirrors MC2 bulk pre-deserialization; Unity asset menu/Application.isPlaying remains Blender boundary |
| `PreBuild/PreBuildSerializeData.cs` | partial | `prebuild/prebuild_serialize_data.hpp`; enable/build-id/share lookup/data validation, 8-character build-id generation, HashSet-style transform collection, transform replacement, and debug status output are present |
| `PreBuild/SharePreBuildData.cs` | partial | `prebuild/share_prebuild_data.hpp`; version/build-result/scale validation, build-id check, ToString, proxy/render mesh serialization references, and constraint-data ownership are present |
| `PreBuild/UniquePreBuildData.cs` | partial | `prebuild/unique_prebuild_data.hpp`; render/proxy/render-mesh unique data, HashSet-style transform-id collection/replacement, validation, and ToString are present |
| `Reduction/ReductionSettings.cs` | complete | `reduction/reduction_settings.hpp`; includes MC2 editor hash behavior for simple/shape reduction distances |
| `Reduction/ReductionWorkData.cs` | partial | `reduction/reduction_work_data.hpp`; native data ownership shell exists, job buffers are adapted to C++ containers, reduction remap/organization buffers are present, and shared MC2-style link/join helper methods plus debug string output are now available |
| `Reduction/SameDistanceReduction.cs` | partial | `reduction/same_distance_reduction.*`; grid search, join-pair collection, JoinJob2-style live/dead vertex merge, shared link update helpers, attribute/bone-weight merge, and final normal/weight cleanup are present |
| `Reduction/ShapeDistanceReduction.cs` | partial | `reduction/shape_distance_reduction.*`; connected-neighbor candidate search, CheckJoin2 filtering, and min-cost join-edge selection are present |
| `Reduction/SimpleDistanceReduction.cs` | partial | `reduction/simple_distance_reduction.*`; grid-based candidate search and MC2-style cost join-edge generation are present |
| `Reduction/StepReductionBase.cs` | partial | `reduction/step_reduction_base.*`; JoinEdge, step scheduling, merge-length stepping, join-edge sorting/selection, pair merge, shared link refresh, and final normal/bone-weight cleanup are present |
| `Settings/ClothDebugSettings.cs` | complete | `settings/cloth_debug_settings.hpp` |
| `Settings/VirtualMeshDebugSettings.cs` | complete | `settings/virtual_mesh_debug_settings.hpp` |

## Utility

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Utility/Data/DataUtility.cs` | complete | `utility/data/data_utility.*`; MC2 pack/unpack, remaining-data, curve evaluation, and collider flag helper surface are present; Unity `AnimationCurve` object conversion and generic array-copy semantics are intentionally represented by native curve data / C++ value copies |
| `Utility/Data/MultiDataBuilder.cs` | complete | `utility/data/multi_data_builder.hpp` |
| `Utility/Grid/GridMap.cs` | complete | `utility/grid/grid_map.hpp`; grid hashing, area enumeration, add/remove/move, data count, and MC2 map accessor aliases are present; Unity native container lifetime is represented by C++ containers |
| `Utility/Jobs/InterlockUtility.cs` | complete | `utility/jobs/interlock_utility.hpp`; fixed-point aggregate add/read/max/clear helpers and synchronous aggregate solve helpers are present; Unity atomic/job scheduling is adapted away |
| `Utility/Jobs/JobUtility.cs` | complete | `utility/jobs/job_utility.hpp`; synchronous fill/reference-fill/serial-number/hashset-list/AABB/sphere-UV/transform-position/int-copy/index-to-multimap helpers are present; Unity JobHandle/Burst scheduling is adapted away |
| `Utility/Math/AABB.cs` | complete | `utility/math/math_types.hpp`, `utility/math/math_utility.*` |
| `Utility/Math/IntAABB.cs` | complete | `utility/math/int_aabb.hpp` |
| `Utility/Math/MathExtensions.cs` | complete | `utility/math/math_extensions.*` |
| `Utility/Math/MathUtility.cs` | complete | `utility/math/math_utility.*`; MC2 public math helper surface is ported, including vector/quaternion angle helpers, normal/tangent/binormal helpers, matrix transform/inverse-transform helpers, geometry intersection helpers, NaN checks, and mass/inverse-mass utilities |
| `Utility/Math/MinimumData.cs` | complete | `utility/math/minimum_data.hpp` |
| `Utility/Mesh/MeshUtility.cs` | bl-boundary | Unity `Renderer`/`MeshFilter`/`SkinnedMeshRenderer` access maps to Blender mesh/object acquisition in `compile/`; no native solver module planned |
| `Utility/Misc/Develop.cs` | complete | `utility/misc/develop.*`; native output/assert backend replaces Unity `Debug` |
| `Utility/Misc/StaticStringBuilder.cs` | complete | `utility/misc/static_string_builder.hpp`; shared static append/append-line/append-to-string helper is present |
| `Utility/NativeCollection/DataChunk.cs` | complete | `utility/native_collection/data_chunk.*` |
| `Utility/NativeCollection/ExBitFlag16.cs` | complete | `utility/native_collection/bit_flag.hpp` |
| `Utility/NativeCollection/ExBitFlag8.cs` | complete | `utility/native_collection/bit_flag.hpp` |
| `Utility/NativeCollection/ExCostSortedList1.cs` | complete | `utility/native_collection/ex_cost_sorted_list1.hpp` |
| `Utility/NativeCollection/ExCostSortedList4.cs` | complete | `utility/native_collection/ex_cost_sorted_list4.hpp` |
| `Utility/NativeCollection/ExNativeArray.cs` | complete | `utility/native_collection/ex_native_array.hpp`; chunk reuse, expand/fill/remove, copy/native-array accessors, raw-byte serialization, summary/debug helpers, and MC2-style prefix copy from simple arrays are present; unsafe reinterpret is adapted through typed vector/raw-byte helpers |
| `Utility/NativeCollection/ExProcessingList.cs` | complete | `utility/native_collection/ex_processing_list.*`; C++ counter pointer replaces Unity `NativeReference` job pointer |
| `Utility/NativeCollection/ExSimpleNativeArray.cs` | complete | `utility/native_collection/ex_simple_native_array.hpp`; range add/fill/remove, copy/native-array accessors, raw-byte serialization, and summary/debug helpers are present; unsafe reinterpret is adapted through typed vector/raw-byte helpers |
| `Utility/NativeCollection/ExTransformAccessArray.cs` | bl-boundary | Unity job transform-access wrapper; represented by `TransformManager` records plus Blender frame-input arrays |
| `Utility/NativeCollection/FixedList128BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/FixedList32BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/FixedList4096BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/FixedList512BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/FixedList64BytesExtensions.cs` | complete | `utility/native_collection/fixed_list.hpp`; MC2 set/remove-swapback/stack/queue helper surface is present |
| `Utility/NativeCollection/NativeArrayExtensions.cs` | complete | `utility/native_collection/native_array_extensions.hpp`; raw byte conversion, MC2 raw-byte aliases, resize/dispose-safe vector equivalents, and BitFlag8 helpers are present; Unity allocator ownership is represented by RAII containers |
| `Utility/NativeCollection/NativeMultiHashMapExtensions.cs` | complete | `utility/native_collection/native_multi_hash_map_extensions.hpp`; contains/unique-add/remove/to-fixed-list/serialize/deserialize helpers are present; Unity Burst/job allocator details are adapted away |
| `Utility/NativeCollection/NativeReferenceExtensions.cs` | complete | `utility/native_collection/native_reference_extensions.hpp`; interlocked start-index helper is present for atomic and local counters |
| `Utility/ResultCode/Exception.cs` | complete | `utility/result_code/exception.*` |
| `Utility/ResultCode/ResultCode.cs` | complete | `utility/result_code/result_code.*`; native debug logging is treated as the C++ logging boundary |
| `Utility/Time/TimeSpan.cs` | complete | `utility/time/time_span.*`; DebugLog/Log are omitted at the C++ logging boundary |
| `Utility/Time/UnityTimeSpan.cs` | bl-boundary | Blender/native profiling abstraction; not part of solver behavior |

## VirtualMesh

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `VirtualMesh/VertexAttribute.cs` | complete | `virtual_mesh/vertex_attribute.hpp` |
| `VirtualMesh/VirtualMesh.cs` | partial | `virtual_mesh/virtual_mesh.*`; core arrays, Bone RenderSetup import entry, fixed list/AABB, bind pose, transform restore rotations, vertex-to-triangle/vertex-to-vertex/edge-to-triangle proxy adjacency data, custom skinning index storage, local/mapping center pose fields, baseline arrays, MC2-style compressed-adjacency Mesh baseline parent generation, parent-driven Bone baseline/root/depth/local-pose builder, deterministic Dispose reset, derived-topology refresh after topology mutations, optimization entry, and reduction organization entry points are present |
| `VirtualMesh/VirtualMeshBoneWeight.cs` | complete | `virtual_mesh/virtual_mesh_bone_weight.*` |
| `VirtualMesh/VirtualMeshContainer.cs` | complete | `virtual_mesh/virtual_mesh_container.*`; share mesh, unique transform-record override, dispose ownership, transform count/index lookup, and center-transform lookup are present; Unity `Transform` objects are represented by backend records |
| `VirtualMesh/VirtualMeshPrimitive.cs` | complete | `virtual_mesh/virtual_mesh_primitive.hpp` |
| `VirtualMesh/VirtualMeshRaycastHit.cs` | complete | `virtual_mesh/virtual_mesh_raycast_hit.hpp` |
| `VirtualMesh/VirtualMeshTransform.cs` | complete | `virtual_mesh/virtual_mesh_transform.*`; name/index/parent/matrix data, origin, clone/update, point/vector/direction/rotation transforms, inverse transforms, and local-to-local transform composition are present |
| `VirtualMesh/Function/VirtualMeshInputOutput.cs` | partial | `compile/`, `runtime/`, `virtual_mesh/virtual_mesh.*`; Unity Mesh/Renderer/Transform acquisition/export belongs mostly to Blender compile/writeback, while native now has BoneCloth/BoneSpring RenderSetup import, transform-to-bone-vertex conversion, skin bind-pose generation, custom skinning bone registration, shared center-space helpers, mesh buffers, BoneCloth root/default selection feed, MC2 child-id driven BoneConnectionMode Line/Sequential/Automatic topology construction, Automatic root ordering/root-count parity guards, duplicate bone traversal guards, and BoneSpring DisableCollision plus collision-bone-index attribute setup |
| `VirtualMesh/Function/VirtualMeshMapping.cs` | partial | `virtual_mesh/virtual_mesh.*`; direct mapping from reduction join/reference indices, search mapping via GridMap, proxy-neighbor weight generation, mapping transform/rotation storage, and mapping mesh type transition are present; exact center-space comparison, proxy reference-index parity, and runtime render-mapping writeback integration remain |
| `VirtualMesh/Function/VirtualMeshOptimization.cs` | partial | `virtual_mesh/virtual_mesh.*`; duplicate triangle removal path is present |
| `VirtualMesh/Function/VirtualMeshProxy.cs` | partial | `virtual_mesh/virtual_mesh.*`; fixed-list/AABB, vertex bind pose, vertex-to-transform rotation, vertex-to-triangle lists, triangle direction optimization, triangle-derived normal/binormal feed, vertex-to-vertex compressed adjacency storage, edge-to-triangle topology cache, edge cut-flag generation, invalid-to-fixed conversion, selection attribute application, BoneCloth default root-fixed selection, MC2-style Mesh baseline parent generation from compressed adjacency, Bone transform baseline generation, runtime proxy baseline generation, normal adjustment, V2 custom skinning weight creation, runtime proxy skinning/writeback entry points, and explicit `RefreshDerivedTopology()` after import/reduction mutations are present; BoneCloth startup parity remains under active audit for fixed-list/baseline ordering, root/depth, transform flags, and runtime transform-record space; remaining work is full proxy conversion/reduction parity audit plus explicit BL-side full `TransformRecord` feed |
| `VirtualMesh/Function/VirtualMeshReduction.cs` | partial | `virtual_mesh/virtual_mesh.*`, `reduction/*.hpp`; Reduction entry point, InitReductionWorkData, Same/Simple/Shape algorithm chain, OrganizationInit, remap, basic-data copy/remap, line/triangle rebuild, reduction transform/skin-bone index remap, store-back, and post-store derived topology refresh are present; full parity audit remains |
| `VirtualMesh/Function/VirtualMeshSerialization.cs` | partial | `virtual_mesh/virtual_mesh_serialization.*`; share/unique serialization data containers, structured simple-array restore, raw-byte proxy array restore including `FixedList32Bytes<uint>` vertex-to-triangle decode, vertex-to-vertex compressed adjacency arrays, edge-to-triangle map restoration, custom skinning indices, center pose fields, unique transform-record restore, center fixed list, and baseline fallback are present; remaining packed hash dictionaries stay deferred |
| `VirtualMesh/Function/VirtualMeshWork.cs` | partial | `virtual_mesh/virtual_mesh.*`; average/max vertex distance sampling, vertex-index GridMap creation, and triangle/edge ray intersection are present; Unity center-transform ray conversion remains Blender/boundary context |

## Next

Keep this section short. The module tables above are the source of truth; do not append per-update completion logs here.

Current priorities:

- Collision parity: continue auditing ColliderCollision/SelfCollision behavior against MC2 using the current VirtualMesh and ParticleBuffer data flow.
- VirtualMesh parity: close the remaining proxy conversion, mapping, reduction, and runtime writeback gaps listed in the VirtualMesh table.
- Particle/runtime buffers: keep collision friction, static friction, display position, old/current particle state, and reset lifecycle aligned with MC2 before adding Blender-side compensation.
- Blender boundary: Python should pass raw component/object snapshots and consume native build output; topology, prebuild conversion, and MC2 data ownership stay in C++.

When a large investigation produces durable rules, move them to a dedicated architecture/debug note and link it from the relevant module row instead of expanding this section.
