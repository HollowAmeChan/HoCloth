# Blender / Native 数据交换契约

本文档定义 Blender Python 侧与 C++ native backend 的数据边界。当前方向是：Blender 只提交 MC2 风格的组件快照和逐帧变换输入，C++ 负责把这些数据转换为 MC2 PreBuild/Build/Runtime 所需结构。

## 1. 统一 Envelope

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

顶层结构：

```python
{
    "schema": "hocloth.exchange",
    "schema_version": 1,
    "payload_type": "authoring_snapshot | build_output | frame_inputs | step_output",
    "coordinate_space": "blender_world",
    "length_unit": "meter",
    "quaternion_order": "wxyz",
    "payload": {...}
}
```

Python 不再维护 `runtime_debug` payload、预览导出 operator、Python fallback solver 或 Blender-side compiled scene。

## 2. Authoring Snapshot

`payload_type = "authoring_snapshot"`

这是 native build 的主输入。它表示 Blender 用户真实创建的 MC2 风格组件和相关对象采样，不表示 C++ 已编译拓扑。

约定：

- Blender 组件属性直接对齐 MC2 Unity 组件，而不是再发明 HoCloth 专用编译模型。
- BoneCloth/BoneSpring 统一表示为 `mc2_component_type = "MagicaCloth"`，通过 `mc2_authoring_mode = "BoneCloth" | "BoneSpring"` 区分。
- MagicaCloth 参数主结构为 `serialize_data`，字段按 MC2 `ClothSerializeData` 和各 constraint `SerializeData` 命名。
- Collider 组件表示为 `MagicaSphereCollider`、`MagicaCapsuleCollider`、`MagicaPlaneCollider` 风格数据。
- Blender 可以采样真实 armature、bone、mesh、object transform；不再生成尾端骨、虚拟粒子、baseline、constraint 拓扑或 MC2 PreBuild 结果。
- 用户需要叶骨时，应在 Blender 骨架里创建真实叶骨。

## 3. Frame Inputs

`payload_type = "frame_inputs"`

逐帧输入由 `runtime/inputs.py::build_runtime_inputs()` 从当前 authoring snapshot 和 Blender 场景采样生成。

当前 payload：

```text
bone_chains
collision_objects
```

职责：

- 只传播放时变化的数据，例如 root/center/bone/collider transform、velocity、dt 相关输入。
- 不在 frame inputs 中增删拓扑、约束参数、碰撞体结构或缓存目标。
- 扁平数组长度必须与 native build 后对应 chain 的 bone/particle 数一致。

## 4. Build Output

`payload_type = "build_output"`

`build_output` 是 native/backend 在 `build_scene()` 完成后返回给 Blender 的构建结果视图。Blender 视图绘制、检查和后续写回映射都应优先读取它，而不是在 Python authoring 层重新推导 MC2 拓扑。

当前 payload：

```text
particles
lines
baselines
colliders
```

后续 ParticleBuffer、VirtualMesh proxy vertices、mapping buffers、RenderData/writeback metadata 继续扩展在这里。

## 5. Step Output

`payload_type = "step_output"`

当前 payload：

```text
runtime_state
transforms
mesh_outputs
```

`transforms[]` 用于骨骼回写，`mesh_outputs[]` 用于 mesh/cache 写回。C++ 不直接操作 Blender 对象，只返回稳定的数据结构。

## 6. Native 入口策略

C++ nanobind 入口应优先接收 envelope：

```text
build_authoring_snapshot(authoring_snapshot envelope)
set_runtime_inputs(frame_inputs envelope)
step_scene(handle, dt, simulation_frequency)
get_bone_transforms(handle)
get_mesh_outputs(handle)
```

旧 `compiled_scene` 仅作为 C++ legacy parser/runtime struct 过渡保留。Python 侧不得恢复 `runtime/compiled_scene.py`、backend scene view 或 stub solver。

## 7. 后续迁移规则

1. 新增跨端字段先更新本文档，再接入 Python 和 native。
2. Python authoring 只提交真实组件、对象引用和采样值。
3. MC2 PreBuild/Build、constraint buffer、VirtualMesh、ParticleBuffer、RenderData 映射都归 C++ transfer/build 层。
4. 视图绘制和写回消费 native `build_output` / `step_output`。
5. 调试输出不再作为 Python 面板或 operator 的一等功能；需要排错时用临时 native 日志、断点或 WinDbg。
