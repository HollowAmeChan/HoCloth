# HoCloth 交互架构重定义

## 1. 背景

当前 HoCloth 的 MC2 移植如果继续把 Blender 当成 Unity Inspector 的替身，会在三个方向持续变得别扭：

- UI：Blender `Panel`/RNA 不适合承载 MC2 风格的高频交互 authoring。
- 缓存：MC2 的 `PreBuild` 本质是构建缓存，不适合塞回 Blender 属性层充当主源。
- 持久化：Blender `.blend` 适合保存场景绑定关系，不适合作为 HoCloth 全量 authoring 资产的唯一真源。

因此从本阶段开始，HoCloth 重新定义宿主分工：Blender 是场景宿主与绑定入口，HoCloth C++ 侧是 authoring / prebuild / runtime 主体。

## 2. 核心结论

新的总原则：

```text
Blender 只做：
1. 参与模拟对象的绑定与标注
2. 构建触发
3. 构建后结果显示

C++ 侧负责：
1. 交互式参数编辑
2. 曲线编辑
3. 组件参数 authoring
4. PreBuild / Cache
5. Runtime
```

这意味着：

- Blender 侧不再承担 MC2 风格曲线编辑器。
- Blender 侧不再承担实时 authoring GPU 叠加绘制。
- Blender 侧允许显示构建后由 native 返回的可视化结果。
- C++ Inspector 才是后续完整交互 authoring 的目标形态。

## 3. 四层数据模型

HoCloth 后续按四层分工：

### 3.1 Scene Binding

Blender 私有场景绑定层，只描述“哪些 Blender 数据块参与模拟”。

典型内容：

- armature / mesh / collider object 引用
- root bone 列表
- cache output 目标
- 顶点组与 bone 自定义属性来源

这一层应当尽量稳定、薄、贴近 Blender 原始数据。

### 3.2 Authoring Asset

HoCloth 自己的 authoring 主源，不再要求完全寄存在 Blender `PropertyGroup` 里。

典型内容：

- component 列表
- cloth 参数
- 曲线参数
- collider binding
- selection / attribute authoring 数据
- preset / override

后续主要由 C++ Inspector 编辑。

### 3.3 Session State

交互会话层，只存在于运行中的 C++ Inspector / runtime 侧。

典型用途：

- 拖动曲线
- 调 stiffness / damping / radius
- 临时调试参数
- 预览尚未保存的修改

交互原则：

```text
UI -> Session -> Runtime Preview
```

而不是：

```text
UI -> Blender RNA -> Python draw/update -> Runtime
```

### 3.4 PreBuild Cache

纯派生缓存层，由 `Scene Binding + Authoring Asset` 编译得到。

典型内容：

- RenderSetupData
- proxy mesh / render mesh
- reduction 结果
- selection 映射结果
- distance / bending / inertia 等预计算约束数据
- share / unique prebuild 数据

这一层不作为主 authoring 源，只作为缓存与加速层。

## 4. Blender 侧职责

Blender 保留如下职责：

- 创建与绑定 MC2 组件
- 指定 armature / root bone / collider / cache output
- 提供构建前标注数据
- 触发 build / step / live runtime
- 显示 native `build_output` / `step_output`

Blender 不再承担如下职责：

- 曲线控制点拖动
- 参数曲线 HUD
- MC2 风格 inspector 交互
- 运行时 authoring 参数高频编辑
- 代替 native 预构建虚拟网格与拓扑

## 5. 构建前标注策略

粒子属性 authoring 以稳定语义为主，不以 index 作为长期真源。

### 5.1 Bone 属性

BoneCloth / BoneSpring 优先使用 bone 自定义属性。

建议保留一个 enum 语义：

- `DEFAULT`
- `FIXED`
- `MOVE`
- `DISABLE_COLLISION`

### 5.2 Mesh 属性

Mesh / VirtualMesh 相关 authoring 优先使用 Blender 顶点组。

当前阶段建议：

- 顶点组按 0/1 语义采样
- 构建前采集
- 构建后转成 MC2 `SelectionData` / `VertexAttribute` / index 化数据

不允许依赖 Blender 侧动态实时修改这些构建输入。

## 6. 曲线策略

曲线参数继续保留在后端与构建链路中：

- Blender authoring snapshot 可继续传递曲线参数
- native 侧继续消费 `CurveSerializeData` 语义
- runtime 生效链路保持不变

但 Blender 侧停止承担曲线交互式绘制与编辑：

- 不再绘制曲线 HUD
- 不再支持控制点 viewport 拖动
- `Panel` 里只保留标量值和 `use_curve` 开关的轻量显示
- 后续完整曲线编辑统一迁到 C++ Inspector

## 7. 构建后绘制策略

Blender 侧 viewport 只允许画构建后 / 运行时由 native 返回的数据。

允许继续保留的绘制类型：

- bones
- particle radius
- colliders
- 后续如需更多 debug primitive，也必须来自 native `build_output`

禁止继续扩展的绘制类型：

- Blender 侧 authoring 曲线预览
- 交互式 authoring HUD
- Python 本地推导出的 MC2 拓扑调试图

## 8. 推荐流水线

```text
Blender Scene Binding
  -> authoring snapshot / raw refs
  -> C++ authoring transfer
  -> Authoring Asset + Session
  -> PreBuild Cache
  -> Runtime
  -> build_output / step_output
  -> Blender draw / writeback
```

## 9. 当前阶段实施决议

本次调整立即生效的决议：

1. Blender 侧移除参数曲线实时绘制与拖拽。
2. Blender 侧主面板收缩为绑定、构建和构建后显示入口。
3. 曲线编辑职责迁移到未来的 C++ Inspector。
4. 粒子属性 authoring 优先走顶点组与 bone 自定义属性。
5. PreBuild 作为缓存层继续推进，不再误用为 Blender UI 主编辑层。

## 10. 后续工作

建议接下来按以下顺序推进：

1. 设计 `Scene Binding` 与 `Authoring Asset` 的外部存储协议。
2. 定义 bone 自定义属性与顶点组到 native `VertexAttribute` 的构建映射。
3. 为 C++ Inspector 预留 session / authoring / prebuild 边界。
4. 逐步把 Blender 面板中的复杂参数编辑迁出到 C++ 侧。
