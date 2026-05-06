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
    "payload_type": "compiled_scene | frame_inputs | step_output | runtime_debug",
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

## 3. Compiled Scene

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
runtime_inputs
step_output
```

Panel 的 Runtime 区域会显示当前 exchange schema 和 debug dump 路径。后续调试顺序建议固定为：

```text
compiled_scene_preview.json -> runtime_debug_latest.json -> native dump -> WinDbg
```

## 7. 后续迁移点

1. C++ binding 新增 envelope parser，但保留 legacy parser 到 BoneSpring 全链路稳定。
2. native build dump 使用同一套 schema metadata。
3. MeshCloth / ClothBone 加入后，只扩展 `payload`，不要新增另一套顶层协议。
4. 任何新增字段必须先更新本文档，再接入 Python 和 native。
