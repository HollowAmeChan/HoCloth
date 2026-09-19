# HoCloth交互架构重定义

## 1. 背景

HoCloth 在继续移植 MC2 时，最大的阻力已经不是 solver 本身，而是 authoring 交互边界。
如果继续把 Blender 强行当成 Unity Inspector 的等价替身，会同时遇到三类问题：

- Blender `Panel` / RNA 不适合承载完整的 MC2 inspector 交互体验。
- MC2 的 `PreBuild`、selection、virtual mesh、runtime cache 本质上属于构建派生层，不应该伪装成 Blender 前端真源。
- `.blend` 适合保存宿主绑定和参数数据，但不适合再人为造一套半独立的 bridge/txt 真源。

因此需要重新定义分层，明确哪些数据属于 Blender，哪些属于 imgui inspector，哪些只是 native build 的派生结果。

## 2. 总原则

```text
Blender 负责
1. Scene binding
2. Component 注册与持久化
3. 粒子/选择类 authoring 宿主
4. Build / Step / Live 触发
5. Build 结果显示与写回

Cpp/imgui 负责
1. MC2 风格 inspector 参数编辑
2. 曲线参数编辑体验
3. Runtime / build 操作入口
4. 运行时状态查看

Native 负责
1. 消费 authoring snapshot
2. PreBuild
3. Runtime
4. Build / Step 输出
```

关键收口：

- `Scene / PropertyGroup` 仍然是 component authoring 的唯一真源。
- `Text datablock` 和外部 bridge 文件只是 transport / debug mirror，不是长期 authoring 存储。
- 粒子属性、selection、骨骼属性这类数据回归 Blender 原生宿主，而不是迁进 imgui 成为新的真源。
- live runtime 每次启动前都必须先 rebuild / rebake 一次 Blender 当前 authoring 数据。

## 3. 数据分层

### 3.1 Scene Binding / Component Registry

Blender 侧必须保留一套 `Scene / CollectionProperty` 作为组件注册表。
这一层描述的是：

- 当前场景里有哪些 HoCloth / MC2 component
- 每个 component 绑定到哪个 object / armature / collider / cache target
- 哪些 component 会被导出到 imgui inspector 和 native build

这意味着 imgui 不负责“发现组件”，而是消费 Blender 已经声明好的 component 集合。

这层数据包括但不限于：

- `scene.hocloth_mc2_components`
- 各 typed component collection
- armature / collider / cache output 绑定
- root bone / collider reference 等宿主引用关系

这一层是整个 authoring 的注册表和入口，不迁出 Blender。

### 3.2 Component Parameters

MC2 component 参数仍然持久化在 Blender `PropertyGroup` 中。
imgui inspector 的职责是编辑这些参数，而不是拥有另一份长期真源。

典型内容：

- preset profile
- stiffness / damping / gravity / inertia
- collider component 参数
- cache output 参数
- curve parameter 的基础字段

原则：

- Blender datablock 保存参数真源
- imgui 只提供更像 MC2 / Unity inspector 的编辑体验
- imgui 对常规参数的任何修改，都应先回写 Blender datablock
- native build 始终从 Blender 当前参数重新导出

### 3.3 Curve Parameters

曲线参数是一个单独需要明确的层。

曲线不能只存在于 imgui 内存里，否则会丢失：

- 关闭 inspector 后状态会丢
- 保存 `.blend` 后无法持久化
- build / live runtime 无法稳定重建

因此曲线参数的真源必须保留在 Blender 侧，至少包括：

- `value`
- `use_curve`
- `control_points`
- `curve_samples`

推荐的职责划分：

- Blender 持久化 `control_points` 作为曲线真源
- Blender 同时维护 `curve_samples` 作为 build/runtime 友好的缓存
- imgui 负责曲线编辑体验与控制点调整
- imgui 修改后通过 bridge 回写 Blender datablock
- native build 永远从 Blender 当前曲线重新采样，不直接信任 imgui 内存态

也就是说，曲线的“编辑器”可以在 imgui，但曲线的“存档”必须在 Blender。

### 3.4 Particle / Selection Authoring

这类数据不再追求塞进 component property，也不交给 imgui 做权威持久化。

宿主方案固定为：

- mesh 侧：vertex groups
- bone 侧：bone custom properties

当前骨骼属性约定：

- 主 key：`hocloth_mc2_attribute`
- 兼容 alias：`mc2_attribute`

允许的枚举值：

- `DEFAULT`
- `MOVE`
- `FIXED`
- `DISABLE_COLLISION`
- `INVALID`

当前导出策略：

- native build 优先读取 bone custom property
- 旧 `joint_overrides.mc2_attribute` 只作为兼容回退

这样做的原因：

- 数据天然随 `.blend` 持久化
- 更贴近 Blender 用户习惯
- 不需要额外制造一层半吊子的 txt/component 存储
- native build 可以在构建阶段把它们转换为 `SelectionData` / `VertexAttribute` / runtime indices

### 3.5 Session / Bridge

这一层只负责外部 inspector 与 Blender Python 的通信。

当前内容：

- `HoCloth_InspectorState`
- `HoCloth_InspectorCommands`
- `_bin/inspector_bridge/state.txt`
- `_bin/inspector_bridge/commands/`

这一层负责：

- inspector transport
- debug mirror
- 状态观察

这一层不负责：

- component 真源
- 曲线真源
- 粒子 authoring 真源
- 长期持久化

### 3.6 PreBuild / Runtime Cache

这一层是纯派生层。

典型内容：

- virtual mesh
- reduction 结果
- topology 映射
- selection 编译结果
- 约束预计算数据
- runtime build output

原则：

- 可以外部缓存
- 可以 native 内部持有
- 但不作为用户 authoring 真源

## 4. Blender 职责

Blender 保留的职责：

- 创建和绑定 MC2 component
- 用 `Scene / CollectionProperty` 管理要参与导出的 component 集合
- 指定 armature / root bones / colliders / cache outputs
- 持久化 component 参数
- 持久化曲线控制点与曲线 samples
- 编辑粒子属性宿主数据
- 触发 build / step / live runtime
- 显示 native `build_output` / `step_output`

Blender 不再承担的职责：

- 曲线 HUD
- Blender GPU 实时 authoring overlay
- 完整 MC2 风格 inspector 布局
- imgui 平行真源
- 另一套桥接 txt component 存储

## 5. imgui Inspector 职责

imgui inspector 的定位是：

- component 参数编辑入口
- 曲线编辑入口
- runtime/build 操作入口
- 运行状态查看

它不负责：

- 替代 Blender 做 scene binding
- 替代 Blender 做 component 注册表
- 替代 Blender 持久化曲线真源
- 替代 Blender 持久化粒子/selection authoring

换句话说：

- “组件存在、绑定谁、是否参与导出” 由 Blender 决定
- “组件参数怎么调、曲线怎么改” 可以在 imgui 中完成
- “最终持久化存档” 仍然回到 Blender datablock

## 6. Live Runtime 规则

这一条必须固定下来：

```text
Start Live Runtime
  -> flush inspector parameter edits back into Blender first
  -> collect Blender-side particle authoring hosts
  -> rebuild / rebake from Blender authoring
  -> then arm live runtime
```

原因：

- 粒子属性宿主仍在 Blender 原生数据里
- 曲线真源仍在 Blender datablock 里
- imgui 不直接拥有最终 authoring 状态

因此开始模拟前必须重新把 Blender 当前状态 bake 进 native build 输入。

这条规则不只适用于 imgui 内点击运行，也适用于 Blender 侧点击运行。
无论入口来自哪里，运行前的标准顺序都应该一致：

```text
Run / Build Barrier
1. 把 imgui 中修改过的常规 component 参数回写到 Blender datablock
2. 以 Blender 当前状态作为唯一真源
3. 从 Blender 读取 bone custom properties
4. 从 Blender 读取 vertex groups
5. 组合成 authoring snapshot / selection-style build inputs
6. 执行 native build
7. 再进入 step / live runtime
```

这意味着：

- imgui 不允许绕过 Blender 直接拿自己的内存态去构建
- Blender 侧运行按钮也不能假设 inspector 参数已经天然同步完成
- “参数同步到 Blender” 是 build 之前的必经屏障，而不是可选优化

这也是当前架构下最不别扭、最稳定的路径：

- 常规参数统一收口到 Blender
- 粒子属性统一从 Blender 宿主读取
- native 永远只吃 Blender 当前导出的构建输入
- 不会出现 imgui 参数态、Blender 参数态、particle 宿主态三者彼此错位的问题

## 7. 当前阶段结论

从当前版本开始，以下结论立即生效：

1. `Scene / PropertyGroup` 继续作为 component authoring 真源。
2. `scene.hocloth_mc2_components` 等 collection 继续作为 component 注册表和导出入口。
3. 曲线参数真源继续保存在 Blender datablock 中，控制点必须可持久化。
4. imgui inspector 只负责曲线和 component 参数的编辑体验，修改后回写 Blender。
5. 粒子/selection authoring 优先走 vertex groups 与 bone custom properties。
6. `joint_overrides` 在骨骼属性导出上降级为兼容回退层，而不是长期主路径。
7. bridge text/file 只保留 transport 与 debug mirror 角色。
8. 任何 build / step / live runtime 入口在构建前，都必须先把 inspector 参数刷新回 Blender，再从 Blender 读取 bone custom properties 与 vertex groups，然后执行 build。
9. live runtime 启动前必须强制 rebuild 一次。
10. Blender 面板继续收缩为绑定入口、inspector 开关、runtime 控制与结果显示。

## 8. 接下来的推进顺序

建议按下面顺序继续：

1. 完成 imgui 曲线控制点编辑与 Blender 回写闭环。
2. 继续把 Blender 侧 component 参数 UI 迁到 imgui inspector。
3. 明确 vertex group -> `SelectionData` / `VertexAttribute` 的 build-time 映射。
4. 逐步把旧 `joint_overrides` 从主工作流里降级为兼容层。
5. 再推进 virtual mesh / selection / prebuild 的正式构建通道。
