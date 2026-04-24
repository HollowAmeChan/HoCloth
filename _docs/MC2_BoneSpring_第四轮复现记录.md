# MC2 BoneSpring 第四轮复现记录
## 1. 本轮目标

本轮目标继续围绕 `MC2 BoneSpring`，并且把观察基准进一步收紧到：

- `MC2_Preset_MiddleSpring.json`

这一轮不再接受“看起来还行”的旧 HoCloth 预设，而是直接把：

1. MC2 真实预设资源
2. Blender 侧参数封装
3. compiled scene
4. native 解析
5. native 求解使用

尽量串成一条线。

---

## 2. 本轮关键发现

### 2.1 `MiddleSpring` 有独立预设文件

这一轮确认 `MiddleSpring` 的真实来源在：

- `_ReferenceProject/MagicaCloth2/Res/Preset/MC2_Preset_MiddleSpring.json`

因此后续判断效果时，应直接以这份 JSON 为基准，而不是继续使用旧的 HoCloth 自定义预设。

### 2.2 `MiddleSpring` 依赖的是一组参数组合

`MiddleSpring` 不是只有下面几个参数：

- `springPower`
- `limitDistance`
- `normalLimitRatio`

它同时依赖：

- `damping`
- `inertiaConstraint`
- `distanceConstraint`
- `tetherConstraint`
- `angleRestorationConstraint`
- `colliderCollisionConstraint`

这也是为什么之前只调 spring 相关参数，很难真正逼近 `MC2`。

### 2.3 BoneSpring 的默认重力依然应视为 0

从 `MC2_Preset_MiddleSpring.json` 再次确认：

- `gravity = 0.0`

所以当前若出现“开始模拟就自己动”，依然优先怀疑：

- inertia 路径
- center/root 增量处理
- 基姿态跟随

而不是把它当成 BoneSpring 的正常重力行为。

---

## 3. 本轮实际工作

### 3.1 BoneSpring 预设入口改成 `EnumProperty`

本轮先把预设入口从一排旧按钮改成了组件上的枚举属性：

- `preset_profile`

当前枚举项直接对齐 `MC2` 预设命名：

- `MiddleSpring`
- `SoftSpring`
- `HardSpring`
- `Tail`

这样 Blender 面板里显示的目标就和参考工程资源名一致，不再混用旧 HoCloth 命名。

### 3.2 新建 Spring Bone 组件默认落到 `MiddleSpring`

本轮继续把新建组件的默认参数也切到了 `MiddleSpring` 口径。

也就是说现在新增 Spring Bone 时，默认初始化会优先按：

- `MC2_Preset_MiddleSpring.json`

设置一组基础参数，而不是先落到旧 HoCloth 风格默认值。

### 3.3 Blender 组件开始按 MC2 参数组封装

这一轮不是只改了预设入口，也继续把 `MiddleSpring` 直接依赖的参数组接进了 Blender 组件结构：

- `damping_curve`
- `inertia_constraint`
- `distance_constraint`
- `tether_constraint`
- `angle_restoration_constraint`
- `spring_constraint`
- `collider_collision_constraint`

同时保留了旧 root-level 字段作为兼容层，用来维持现有 joint override 和摘要显示。

### 3.4 预设应用逻辑开始直接写 MC2 参数组

`authoring/operators.py` 中的 preset apply 逻辑，已经不再只是改：

- `stiffness`
- `damping`
- `drag`

而是开始直接写入 MC2 预设里的关键参数，例如：

- damping
- world/local inertia
- movement / rotation / particle speed limit
- distance stiffness
- tether distance compression
- angle restoration stiffness
- angle restoration velocity attenuation
- spring constraint
- collider friction / limit distance

当前预设表已经直接来自以下 MC2 资源：

- `MC2_Preset_MiddleSpring.json`
- `MC2_Preset_SoftSpring.json`
- `MC2_Preset_HardSpring.json`
- `MC2_Preset_Tail.json`

### 3.5 面板开始改成 MC2 参数分组视图

BoneSpring 面板这一轮继续从旧 HoCloth 的平铺参数，改成更接近 MC2 的分组显示。

当前已经单独显示：

- `MC2 Damping`
- `MC2 Inertia Constraint`
- `MC2 Distance / Tether`
- `MC2 Angle Restoration Constraint`
- `MC2 Spring Constraint`
- `MC2 Collider Collision Constraint`

`CurveSerializeData` 相关字段目前依然只保留：

- `use_curve`
- `value`

曲线编辑 UI 仍然按之前约定暂缓。

### 3.6 compiled scene 已开始携带 MC2 关键参数

Python 编译链这一轮已经开始把以下参数送进 `CompiledSpringBone`：

- damping curve value
- inertia constraint 系列字段
- tether distance compression
- distance stiffness
- angle restoration enable / stiffness / velocity attenuation

这一步让新参数不再只停留在 Blender UI。

### 3.7 native binding 已开始解析这些参数

`_native/src/hocloth_python_module.cpp` 已增加对应字段解析。

这样 compiled scene 导出的新字段，已经可以进到 native 侧 `CompiledSpringBone`。

### 3.8 native 求解开始使用 MC2 预设驱动的数据

`_native/src/hocloth_runtime_api.cpp` 本轮继续把一部分硬编码近似值替换为来自参数组的数据，主要包括：

- local/world inertia
- movement inertia smoothing
- depth inertia
- particle speed limit
- tether distance compression
- distance stiffness
- angle restoration velocity attenuation
- centrifugal acceleration

当前这还不是 `MC2` 求解器的完整逐段同构版本，但已经从“预设名称像 MC2”推进到“预设参数开始真实驱动 native 行为”。

---

## 4. 这一轮仍未完成的关键点

虽然这一轮已经把 `MiddleSpring` 的关键参数组开始接进运行链，但 `MC2 BoneSpring` 仍然没有搬完。

当前仍需继续完成的重点是：

- 把 `MiddleSpring` 剩余参数继续完整映射进组件和运行链
- 继续补 `TeamManager / SimulationManager` 的 inertia 主链
- 继续修正“旋转依然只有一个方向摆动、像是没角速度”的问题
- 继续检查当前 native 里哪些位置仍然是近似实现，而不是 MC2 原逻辑

---

## 5. 下一步建议

下一步仍然建议继续只做 `MiddleSpring` 对齐，不回到泛调参。

优先顺序建议：

1. 继续补完整 `MiddleSpring` 剩余参数和约束路径
2. 对照 `MC2 TeamManager / SimulationManager` 继续搬 inertia / angular velocity / rotation axis 逻辑
3. 编译验证后，再继续修当前旋转摆动方向明显不对的缺口

当前这轮已经把“目标锚点、参数入口、数据流入口”都立住了，后续可以更稳定地围绕 `MiddleSpring` 继续推进。
