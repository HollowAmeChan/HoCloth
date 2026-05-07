# Blender / Native 数据交换契约

本文档定义 Blender Python 侧与 C++ native backend 的数据边界。当前目标不是立刻重写 native 解析器，而是先把 Python 侧所有 build / frame / output / debug 数据统一放进同一套 envelope，避免继续让临时 dict 字段散落在 panel、operator、session 和 bridge 中。

## 1. 契约入口

Python 契约模块：

```text
runtime/exchange.py
```

当前协议：

```text
schema: hocloth.exchange
schema_version: 1
coordinate_space: blender_world
length_unit: meter
quaternion_order: wxyz
```

所有面向调试、导出、未来 native v2 的数据都使用 `ExchangeEnvelope`：

```python
{
    "schema": "hocloth.exchange",
    "schema_version": 1,
    "payload_type": "authoring_snapshot | compiled_scene | build_output | frame_inputs | step_output | runtime_debug",
    "coordinate_space": "blender_world",
    "length_unit": "meter",
    "quaternion_order": "wxyz",
    "payload": {...}
}
```

## 2. Native 入口策略

当前 C++ nanobind 入口已经可以识别 `hocloth.exchange` envelope：

```text
compiled_scene envelope -> payload.scene
frame_inputs envelope   -> payload
```

为了过渡期安全，native parser 仍保留直接读取 root dict 的兼容路径：

```text
build_scene(root)        -> root.spring_bones / root.bone_chains / root.colliders ...
set_runtime_inputs(root) -> root.bone_chains / root.collision_objects ...
```

Python bridge 调用 native 时优先传 envelope：

```text
NativeModuleBridge.build_scene()
NativeModuleBridge.set_runtime_inputs()
```

Python stub backend 仍会在本地拆成旧 payload，因为 stub 使用的是 Python dataclass/字典逻辑。等 C++ 侧全链路稳定后，可以再删除 native root dict 兼容路径。

## 3. Authoring Snapshot

`payload_type = "authoring_snapshot"`

这是下一阶段的主 build 输入。它表示 Blender 侧用户真实组件和相关对象采样，不表示 MC2 已编译拓扑。C++ 侧的 Blender transfer unit 负责把它转换为 MC2 PreBuild/Build 所需数据。

实现约定更新：

- Blender 侧组件语义必须直接对齐 MC2 Unity 组件，而不是发明一套 HoCloth 专用组件模型。
- BoneCloth/BoneSpring 在协议中统一表示为 `mc2_component_type = "MagicaCloth"`，并通过 `mc2_authoring_mode = "BoneCloth" | "BoneSpring"` 区分。
- MagicaCloth 参数主结构为 `serialize_data`，字段按 MC2 `ClothSerializeData` / constraint `SerializeData` 命名，例如 `rootBones`、`gravity`、`gravityDirection`、`inertiaConstraint`、`distanceConstraint`、`angleRestorationConstraint`、`springConstraint`、`colliderCollisionConstraint.colliderList`。
- Collider 组件表示为 `MagicaSphereCollider`、`MagicaCapsuleCollider`、`MagicaPlaneCollider` 等 MC2 collider component，使用 `center`、`size`、`direction`、`reverseDirection`、`alignedOnCenter` 等 MC2 语义；旧 `shape_type/radius/height` 字段仅作为过渡兼容别名。
- 当前 Blender `PropertyGroup` 类名可暂时保持旧名以兼容 .blend 存档，但 UI、导出、native transfer 的主语义必须以 MC2 component/serialize data 为准。
- Python 可以采样真实骨架 rest 数据和 object transform；不能再生成尾端骨、虚拟粒子、baseline、constraint 拓扑等 MC2 build 结果。

预期 payload:

```text
components
armatures
meshes
colliders
settings
```

关键约定：
- Blender 只传真实用户资产和采样数据，不自动补尾端骨/虚拟粒子/MC2 baseline。
- `components[]` 保存 BoneSpring/BoneCloth/MeshCloth/Collider/CacheOutput 的参数和引用 id。
- `armatures[]` 保存真实 bone 层级、rest transform、pose 采样和 root bone 引用。
- `meshes[]` 保存原始 mesh 拓扑、权重、选择属性、材质/对象引用等 PreBuild 输入。
- `colliders[]` 保存 collider object 的 shape 参数和 transform。
- C++ build 完成后返回 `build_output` 作为 Blender 绘制和调试数据。

`compiled_scene` 在过渡期继续存在，但长期降级为 legacy/debug payload。

## 3.1 Compiled Scene

`payload_type = "compiled_scene"`

`payload.scene` 当前包含：

```text
spring_bones
bone_chains                # spring_bones alias
colliders
collider_groups
collision_objects
collision_bindings
cache_descriptors          # optional
```

关键约定：

- `component_id` 是 Python authoring 组件到 native 数据的稳定关联键。
- `bone_chains` 只是兼容别名；长期主名保留 `spring_bones`。
- 向量统一为 3 个 float，四元数统一为 `(w, x, y, z)`。
- 编译结果必须脱离 Blender 对象生命周期，只能存 name、id、数值、扁平数组。
- 结构变化必须 rebuild runtime，不走逐帧输入。

导出入口：

```text
Advanced Tools -> Export Compiled Scene
_build/compiled_scene_preview.json
```

## 4. Frame Inputs

`payload_type = "frame_inputs"`

逐帧输入由 `runtime/inputs.py::build_runtime_inputs()` 产生，现在直接返回 envelope。

当前 payload：

```text
bone_chains
collision_objects
```

`bone_chains[]` 字段：

```text
component_id
armature_name
root_bone_name
center_object_name
center_bone_name
root_translation
root_rotation_quaternion
root_linear_velocity
root_scale
center_translation
center_rotation_quaternion
center_linear_velocity
center_scale
basic_head_positions       # flat vec3 array
basic_tail_positions       # flat vec3 array
basic_rotations            # flat quat array, wxyz
```

`collision_objects[]` 字段：

```text
collision_object_id
world_translation
world_rotation
linear_velocity
```

关键约定：

- 坐标空间是 Blender world。
- 每帧只传会随播放变化的 transform / velocity / override。
- 拓扑、约束参数、碰撞体结构、缓存目标等结构数据不在 frame inputs 里增删。
- 扁平数组长度必须与 compiled scene 对应 chain 的 joint 数一致。

## 4.1 Build Output

`payload_type = "build_output"`

`build_output` 是 native/backend 在 `build_scene()` 完成之后返回给 Blender 的构建结果视图。它的职责是给 Blender 绘制、检查、调试、后续写回映射使用；Blender authoring 层不再为了显示或模拟主动补 MC2 拓扑。

当前 payload:
```text
particles
lines
baselines
colliders
```

`particles[]`:
```text
component_id
bone_name
joint_index
parent_index
rest_head_local
rest_tail_local
radius
```

`lines[]`:
```text
component_id
start_index
end_index
```

`baselines[]`:
```text
joint_indices
```

`colliders[]`:
```text
collision_object_id
owner_component_id
shape_type
world_translation
world_rotation
radius
height
capsule_direction
capsule_aligned_on_center
capsule_reverse_direction
capsule_end_radius
source_object_name
```

关键约定:
- Blender 用户需要真实叶骨时自行在骨架中添加；HoCloth 不再自动追加尾端骨/尾端粒子。
- Blender authoring 层只采集原始骨架、参数、碰撞体引用和逐帧 transform；可视化数据优先从 `build_output` 读。
- 当前 `build_output` 先暴露 native 已解析的 joints/lines/baselines/colliders。后续 ParticleBuffer、VirtualMesh、proxy vertex、mapping、debug primitive 都继续扩展在这里。

## 5. Step Output

`payload_type = "step_output"`

当前 payload：

```text
runtime_state
transforms
```

`transforms[]` 当前用于骨骼回写：

```text
component_id
armature_name
bone_name
translation
rotation_quaternion
```

后续 MeshCloth / ClothBone 输出应继续扩展在 step output 内，例如：

```text
bone_transforms
mesh_vertices
particle_buffers
cache_frames
```

不要让 C++ 直接反向操作 Blender 对象。

## 6. Debug Dump

runtime 每次 step 后写：

```text
_build/runtime_debug_latest.json
```

`payload_type = "runtime_debug"`，内部包含：

```text
runtime_state
compiled_scene
build_output
runtime_inputs
step_output
```

Panel 的 Runtime 区域会显示当前 exchange schema 和 debug dump 路径。后续调试顺序建议固定为：

```text
compiled_scene_preview.json -> runtime_debug_latest.json -> native dump -> WinDbg
```

## 7. 后续迁移点

1. 新增 `authoring_snapshot` Python builder，直接收集 Blender 组件和对象采样。
2. C++ 新增 Blender transfer unit，把 `authoring_snapshot` 转为 MC2 PreBuild/Build 数据。
3. `compiled_scene` 保留到旧链路稳定迁移完成，但不得继续扩大 Python 侧 MC2 语义。
4. native build dump 使用同一套 schema metadata。
5. MeshCloth / ClothBone 加入后，只扩展 `payload`，不要新增另一套顶层协议。
6. 任何新增字段必须先更新本文档，再接入 Python 和 native。
