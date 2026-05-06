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
| Utility/Math | skeleton | `_native/include/hocloth/utility/math/` |
| Utility/NativeCollection | partial | `_native/include/hocloth/utility/native_collection/` |
| Utility/Time | skeleton | `_native/include/hocloth/manager/simulation/time_manager.hpp` |
| Manager | partial | `_native/include/hocloth/manager/` |
| Cloth/Constraints | legacy-partial | `_native/include/hocloth/cloth/constraints/` |
| Cloth/Collider | planned | `_native/include/hocloth/cloth/collider/` |
| Cloth/Wind | skeleton | `_native/include/hocloth/manager/simulation/wind_manager.hpp` |
| VirtualMesh | planned | `_native/include/hocloth/virtual_mesh/` |
| Reduction | planned | `_native/include/hocloth/reduction/` |
| PreBuild | planned | `_native/include/hocloth/prebuild/` |

## Cloth

| MC2 file | Status | HoCloth target |
| --- | --- | --- |
| `Cloth/CheckSliderSerializeData.cs` | planned | `cloth/parameters/check_slider_serialize_data.*` |
| `Cloth/ClothBehaviour.cs` | defer | Blender component/runtime boundary |
| `Cloth/ClothForceMode.cs` | planned | `cloth/cloth_force_mode.hpp` |
| `Cloth/ClothNormalAxis.cs` | planned | `cloth/cloth_normal_axis.hpp` |
| `Cloth/ClothParameters.cs` | planned | `cloth/cloth_parameters.*` |
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
| `Cloth/Constraints/DistanceConstraint.cs` | legacy-partial | `cloth/constraints/distance_constraint.*` |
| `Cloth/Constraints/InertiaConstraint.cs` | legacy-partial | `cloth/constraints/inertia_constraint.*` |
| `Cloth/Constraints/MotionConstraint.cs` | planned | `cloth/constraints/motion_constraint.*` |
| `Cloth/Constraints/SelfCollisionConstraint.cs` | planned | `cloth/constraints/self_collision_constraint.*` |
| `Cloth/Constraints/SpringConstraint.cs` | legacy-partial | `cloth/constraints/spring_constraint.*` |
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
| `Manager/Team/TeamManager.cs` | partial | `manager/team/team_manager.*` |
| `Manager/Team/TeamWindData.cs` | planned | `manager/team/team_wind_data.*` |
| `Manager/TransformManager/TransformData.cs` | planned | `manager/transform/transform_data.*` |
| `Manager/TransformManager/TransformDataSerialization.cs` | planned | `manager/transform/transform_data_serialization.*` |
| `Manager/TransformManager/TransformManager.cs` | skeleton | `manager/transform/transform_manager.*` |
| `Manager/TransformManager/TransformRecord.cs` | planned | `manager/transform/transform_record.*` |
| `Manager/VirtualMesh/VirtualMeshManager.cs` | skeleton | `manager/virtual_mesh/virtual_mesh_manager.*` |

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
| `Utility/Data/DataUtility.cs` | planned | `utility/data/data_utility.*` |
| `Utility/Data/MultiDataBuilder.cs` | planned | `utility/data/multi_data_builder.*` |
| `Utility/Grid/GridMap.cs` | planned | `utility/grid/grid_map.*` |
| `Utility/Jobs/InterlockUtility.cs` | defer | C++ threading abstraction |
| `Utility/Jobs/JobUtility.cs` | defer | C++ scheduling abstraction |
| `Utility/Math/AABB.cs` | skeleton | `utility/math/math_types.hpp`, `utility/math/math_utility.*` |
| `Utility/Math/IntAABB.cs` | planned | `utility/math/int_aabb.*` |
| `Utility/Math/MathExtensions.cs` | planned | `utility/math/math_extensions.*` |
| `Utility/Math/MathUtility.cs` | skeleton | `utility/math/math_utility.*` |
| `Utility/Math/MinimumData.cs` | planned | `utility/math/minimum_data.*` |
| `Utility/Mesh/MeshUtility.cs` | planned | `utility/mesh/mesh_utility.*` |
| `Utility/Misc/Develop.cs` | planned | `utility/misc/develop.*` |
| `Utility/Misc/StaticStringBuilder.cs` | defer | C++ logging/dump utilities |
| `Utility/NativeCollection/DataChunk.cs` | partial | `utility/native_collection/data_chunk.*` |
| `Utility/NativeCollection/ExBitFlag16.cs` | planned | `utility/native_collection/ex_bit_flag16.*` |
| `Utility/NativeCollection/ExBitFlag8.cs` | planned | `utility/native_collection/ex_bit_flag8.*` |
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
| `VirtualMesh/VertexAttribute.cs` | planned | `virtual_mesh/vertex_attribute.*` |
| `VirtualMesh/VirtualMesh.cs` | planned | `virtual_mesh/virtual_mesh.*` |
| `VirtualMesh/VirtualMeshBoneWeight.cs` | planned | `virtual_mesh/virtual_mesh_bone_weight.*` |
| `VirtualMesh/VirtualMeshContainer.cs` | planned | `virtual_mesh/virtual_mesh_container.*` |
| `VirtualMesh/VirtualMeshPrimitive.cs` | planned | `virtual_mesh/virtual_mesh_primitive.*` |
| `VirtualMesh/VirtualMeshRaycastHit.cs` | planned | `virtual_mesh/virtual_mesh_raycast_hit.*` |
| `VirtualMesh/VirtualMeshTransform.cs` | planned | `virtual_mesh/virtual_mesh_transform.*` |
| `VirtualMesh/Function/VirtualMeshInputOutput.cs` | planned | `virtual_mesh/function/virtual_mesh_input_output.*` |
| `VirtualMesh/Function/VirtualMeshMapping.cs` | planned | `virtual_mesh/function/virtual_mesh_mapping.*` |
| `VirtualMesh/Function/VirtualMeshOptimization.cs` | planned | `virtual_mesh/function/virtual_mesh_optimization.*` |
| `VirtualMesh/Function/VirtualMeshProxy.cs` | planned | `virtual_mesh/function/virtual_mesh_proxy.*` |
| `VirtualMesh/Function/VirtualMeshReduction.cs` | planned | `virtual_mesh/function/virtual_mesh_reduction.*` |
| `VirtualMesh/Function/VirtualMeshSerialization.cs` | planned | `virtual_mesh/function/virtual_mesh_serialization.*` |
| `VirtualMesh/Function/VirtualMeshWork.cs` | planned | `virtual_mesh/function/virtual_mesh_work.*` |

## Next

Next priority: move `Manager/TransformManager/TransformData.cs`, `Manager/TransformManager/TransformRecord.cs`, `Cloth/Constraints/DistanceConstraint.cs`, and `Cloth/Constraints/InertiaConstraint.cs` from `planned/legacy-partial` into the new manager pipeline as `partial`.
