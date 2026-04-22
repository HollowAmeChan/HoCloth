# HoCloth XPBD 重构工程大纲与参考项目分析

本文档用于把这次重构方向一次性定清楚，后续回顾时可以同时回答三类问题：

- 我们为什么要把 HoCloth 改成现在这个架构？
- 每个参考工程到底借鉴哪一层，而不是“感觉像”？
- `SpringBone / ClothBone / MeshCloth / Blender UI` 分别应该落在哪些模块里？

---

## 1. 重构目标

### 1.1 功能目标

HoCloth 的目标不再是“能跑一条骨链的 MVP”，而是逐步还原三条主能力线：

- `SpringBone`
- `ClothBone`
- `MeshCloth`

这三条线的目标体验参考 `MagicaCloth2`，但底层不走 MC2 的 Unity/Job/Burst 技术路线，而是转成：

- Blender 负责 authoring、交互、预览控制、数据持久化
- C++ 负责 XPBD runtime、约束求解、碰撞与缓存导出

### 1.2 交互目标

Blender 端 UI 不能只停留在“参数面板能调”。这次交互逻辑必须更接近 `blender_bonex`：

- 面板分层清晰
- 当前选择驱动参数显示
- playback / bake / tool / preset 分区清楚
- handler、modal、shape editor 各司其职

### 1.3 架构目标

目标不是“先写一个很大的求解器再说”，而是先把三层边界定死：

- `Authoring`
- `Compiled`
- `Runtime`

只有这样后面做 `SpringBone -> ClothBone -> MeshCloth` 扩展时不会推倒重来。

---

## 2. HoCloth 当前状态评估

当前仓库已经搭出了一个很清晰但仍然偏 MVP 的骨架。

### 2.1 现有模块划分

- `authoring/`：提取、操作器、面板
- `components/`：组件属性组与注册表
- `compile/`：authoring -> compiled scene
- `runtime/`：bridge、session、live playback、pose apply

对应文档已经说明当前定位是“Blender 插件 + 原生占位 runtime + 后续自研 XPBD 接入”：

- [工程大纲.md](../工程大纲.md)
- [_docs/项目结构说明.md](./项目结构说明.md:1)
- [_docs/目录功能说明.md](./目录功能说明.md:1)

### 2.2 现有实现的优点

当前结构有几个方向是对的，应该保留：

1. `components -> compile -> runtime` 三层已经分开了。
2. 组件使用了主目录 + 类型容器双层结构，这个非常适合后续扩展。
3. runtime 已经走了 `handle + bridge + session` 思路，而不是 Python 直接持有复杂 C++ 对象。
4. live playback 已经单独抽成 handler 风格的运行逻辑，而不是塞进按钮回调里。

### 2.3 现有实现的限制

当前实现仍然只适合“骨链演示”，还不够支撑真正的 MC2 风格工作流。

#### 组件层限制

当前 registry 只有两种组件：

- `BONE_CHAIN`
- `COLLIDER`

见 [components/registry.py](../components/registry.py:11)。

而且 bone chain 的 authoring 数据只有：

- armature
- root bone
- stiffness / damping / drag / gravity

见 [components/properties.py](../components/properties.py:42)。

这离真正的 `Spring / Joint / ColliderGroup / Center / Per-joint override` 还差很多。

#### compiled 层限制

当前 compiled scene 只有：

- `CompiledBoneChain`
- `CompiledCollider`
- `SimulationCacheDescriptor`

见 [compile/compiled.py](../compile/compiled.py:15), [compile/compiled.py](../compile/compiled.py:33), [compile/compiled.py](../compile/compiled.py:44), [compile/compiled.py](../compile/compiled.py:62)。

这意味着：

- 还没有 `Spring` 级别的数据
- 还没有 `ColliderGroup`
- 还没有 `ClothBone` 的图结构
- 还没有 `MeshCloth` 的代理网格、映射和拓扑

#### runtime 层限制

当前 bridge 里真正跑的还是 placeholder 风格的 Python stub，主要为了保持 API 形状稳定：

- `NativeBridgeStub.build_scene(...)`
- `step_scene(...)`
- `get_bone_transforms(...)`

见 [runtime/bridge.py](../runtime/bridge.py:27), [runtime/bridge.py](../runtime/bridge.py:35), [runtime/bridge.py](../runtime/bridge.py:70), [runtime/bridge.py](../runtime/bridge.py:89)。

这对验证“流程是否闭环”很好，但还不是可扩展的 solver 结构。

#### UI 层限制

当前面板仍然是“组件列表 + 详情 + 基本运行按钮”模式：

- 顶层控制见 [authoring/panel.py](../authoring/panel.py:148)
- 运行按钮见 [authoring/panel.py](../authoring/panel.py:164)
- bone chain 详情见 [authoring/panel.py](../authoring/panel.py:14)
- collider 详情见 [authoring/panel.py](../authoring/panel.py:102)

它适合 MVP，但不适合后面做：

- 多面板工作流
- 选中对象驱动的参数编辑
- bake / preset / tools / logs 分区
- 复杂交互工具

---

## 3. 参考工程总览

本次参考项目可以分成五类：

### 3.1 MagicaCloth2

它不是 Blender 插件，但它给了我们最完整的“产品能力结构”：

- 一个总组件
- 一套可序列化 authoring 数据
- 一套 prebuild 数据
- 一套 process/build/runtime 分流
- 一套 mesh / bone / spring 的统一抽象

### 3.2 UniVRM

它给我们的核心价值不是“大布料”，而是 `SpringBone` 的数据组织非常干净：

- `Collider`
- `ColliderGroup`
- `Spring`
- `Joint`
- `Runtime interface`
- `BufferFactory`

这非常适合 HoCloth 的 `SpringBone` 与 `ClothBone` 组件设计。

### 3.3 PositionBasedDynamics

它给的是求解器骨架，而不是产品工作流：

- `SimulationModel`
- `Constraint`
- `TimeStepController`
- `CollisionDetection`

对我们最有价值的是：

- 约束对象拆分方式
- XPBD distance / bending / shape matching 的组织方式
- substep + projection + velocity update 的 step 结构

### 3.4 blender_bonex

它给的是 Blender 端“怎么把一个复杂物理工具做得能用”：

- 多面板组织
- modal operator
- handler 去重
- selection-driven 参数同步
- preset / shape editor / bake 流程

这部分几乎是 HoCloth Blender UI 的首要参考。

### 3.5 FleX

FleX 不适合直接作为 HoCloth 技术路线，但它仍然提供几个可以借的概念：

- moving frame / 局部空间模拟
- mesh -> cloth asset 的预处理思路
- asset / instance / container 的资源组织

它更像“概念参考”，不是直接照搬对象。

---

## 4. MagicaCloth2 架构分析

### 4.1 总管理器不是“一个大类”，而是多个 manager 的统一入口

`MagicaManager` 里把系统拆成多个 manager：

- `TimeManager`
- `TeamManager`
- `ClothManager`
- `RenderManager`
- `TransformManager`
- `VirtualMeshManager`
- `SimulationManager`
- `ColliderManager`
- `WindManager`
- `PreBuildManager`

对应代码见：

- [_ReferenceProject/MagicaCloth2/Scripts/Core/Manager/MagicaManager.cs:34](../_ReferenceProject/MagicaCloth2/Scripts/Core/Manager/MagicaManager.cs:34)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Manager/MagicaManager.cs:113](../_ReferenceProject/MagicaCloth2/Scripts/Core/Manager/MagicaManager.cs:113)

**对 HoCloth 的启发**：

- 我们的 native runtime 不应该是“一个 Solver 类吃掉所有东西”
- 应该按职责拆成 scene、transform、collider、solver、cache、mapping 这些子系统
- Python 侧也应避免把 session 写成万能类

### 4.2 MagicaCloth 组件本体很薄，真正逻辑放在 serialize data 和 process

`MagicaCloth` 本身只做几件事：

- 持有 `serializeData`
- 持有 `serializeData2`
- 持有 `process`
- 在生命周期里触发 `Init / StartUse / AutoBuild / Dispose`

见：

- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:21](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:21)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:29](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:29)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:44](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:44)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:77](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:77)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:91](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:91)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:101](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:101)

**对 HoCloth 的启发**：

- Blender 组件类型不要直接背 runtime 状态
- Python 组件只保存 authoring 数据
- 编译、运行、缓存应由外部流程控制

### 4.3 关键设计：authoring 数据和隐藏构建数据分成两份

`MagicaCloth` 有：

- `ClothSerializeData`：可导入导出、可运行时改的参数
- `ClothSerializeData2`：隐藏数据，不给运行时随便改

见：

- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:21](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:21)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:29](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/MagicaCloth.cs:29)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothSerializeData.cs:10](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothSerializeData.cs:10)

**对 HoCloth 的启发**：

- 我们也应该明确分成：
  - `authoring property group`
  - `compiled scene`
  - `runtime mutable state`
- 不能把未来的 selection、mapping、fixed mask、拓扑哈希直接塞进 Blender 属性里

### 4.4 MC2 明确把三种模式放进同一个 clothType 分流

`ClothSerializeData` 里直接定义：

- `MeshCloth`
- `BoneCloth`
- `BoneSpring`

当前 `ClothProcess` 在 build 流程里明确按 cloth type 分流：

- `MeshCloth` 分支
- `BoneCloth` 分支
- `BoneSpring` 分支

可定位到：

- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothSerializeData.cs:23](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothSerializeData.cs:23)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothProcess.cs:116](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothProcess.cs:116)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothProcess.cs:156](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothProcess.cs:156)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothProcess.cs:161](../_ReferenceProject/MagicaCloth2/Scripts/Core/Cloth/ClothProcess.cs:161)

**对 HoCloth 的启发**：

- HoCloth 不应该再把所有骨相关能力都塞在 `BONE_CHAIN`
- 更合理的做法是显式拆成：
  - `SPRING_BONE`
  - `CLOTH_BONE`
  - `MESH_CLOTH`

### 4.5 PreBuild 是 MC2 最关键的产品级架构

`PreBuildDataCreation.CreatePreBuildData(...)` 的流程很值得借鉴：

1. 确认 build id
2. 创建或绑定 prebuild asset
3. 构建 `SharePreBuildData` / `UniquePreBuildData`
4. 生成 setup data
5. 生成 proxy mesh / render mesh / selection / mapping
6. 落盘保存

见：

- [_ReferenceProject/MagicaCloth2/Scripts/Editor/PreBuild/PreBuildDataCreation.cs:24](../_ReferenceProject/MagicaCloth2/Scripts/Editor/PreBuild/PreBuildDataCreation.cs:24)
- [_ReferenceProject/MagicaCloth2/Scripts/Editor/PreBuild/PreBuildDataCreation.cs:64](../_ReferenceProject/MagicaCloth2/Scripts/Editor/PreBuild/PreBuildDataCreation.cs:64)
- [_ReferenceProject/MagicaCloth2/Scripts/Editor/PreBuild/PreBuildDataCreation.cs:99](../_ReferenceProject/MagicaCloth2/Scripts/Editor/PreBuild/PreBuildDataCreation.cs:99)
- [_ReferenceProject/MagicaCloth2/Scripts/Editor/PreBuild/PreBuildDataCreation.cs:199](../_ReferenceProject/MagicaCloth2/Scripts/Editor/PreBuild/PreBuildDataCreation.cs:199)

**对 HoCloth 的启发**：

- `compile/` 不应该只是“把骨头名字抄一遍”
- 它必须成为未来的正式 prebuild 阶段
- 尤其是 `MeshCloth`，一定要有稳定 compiled representation

### 4.6 VirtualMeshProxy 说明 MC2 的 BoneCloth 和 MeshCloth 本质共享一套 proxy 思想

`VirtualMeshProxy.cs` 里多处根据 `isBoneCloth` 分支：

- BoneCloth 需要回写 transform rotation
- BoneCloth 对 fixed 点处理和 transform flag 处理不同
- MeshCloth / BoneCloth 的 baseline 构建方式不同

可定位到：

- [_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:187](../_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:187)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:201](../_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:201)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:1539](../_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:1539)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:1618](../_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:1618)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:1647](../_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:1647)
- [_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:1963](../_ReferenceProject/MagicaCloth2/Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs:1963)

**对 HoCloth 的启发**：

- `ClothBone` 和 `MeshCloth` 不要完全割裂成两个世界
- 两者都应该落到“compiled proxy topology + writeback strategy”的共同框架里
- 差别主要在：
  - 输入拓扑来源
  - baseline 构建方式
  - 最终输出是 bone 还是 mesh

### 4.7 编辑器体验上，MC2 非常强调折叠区和状态信息

`ClothInspectorUtility.Foldout(...)` 不是复杂算法，但它体现了产品意识：

- 模块化折叠区
- enable toggle
- warning state

见：

- [_ReferenceProject/MagicaCloth2/Scripts/Editor/Cloth/ClothInspectorUtility.cs:20](../_ReferenceProject/MagicaCloth2/Scripts/Editor/Cloth/ClothInspectorUtility.cs:20)

`MagicaClothEditor` 还会直接显示 proxy mesh 的当前统计信息：

- vertex
- edge
- triangle
- skin bone count

见：

- [_ReferenceProject/MagicaCloth2/Scripts/Editor/Cloth/MagicaClothEditor.cs:224](../_ReferenceProject/MagicaCloth2/Scripts/Editor/Cloth/MagicaClothEditor.cs:224)

**对 HoCloth 的启发**：

- UI 不该只有属性控件
- 还要有：
  - compiled 统计
  - runtime 统计
  - rebuild 警告
  - 当前组件状态

### 4.8 MagicaCloth2 可借鉴项总结

#### 强借鉴

- `serialize data / hidden data / process` 三分法
- `prebuild` 思路
- `MeshCloth / BoneCloth / BoneSpring` 的功能分流
- proxy mesh 作为统一中介

#### 选择性借鉴

- manager 体系
- editor 折叠区与调试信息

#### 不照搬

- Unity 生命周期
- Job/Burst 依赖
- Unity asset 管理方式

---

## 5. UniVRM 架构分析

### 5.1 SpringBone 数据组织特别适合 HoCloth

`Vrm10InstanceSpringBone` 里把 SpringBone 信息组织成：

- 全局 `ColliderGroups`
- 多个 `Spring`
- 每个 `Spring` 有 `ColliderGroups`
- 每个 `Spring` 有 `Joints`

见：

- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Instance/Vrm10InstanceSpringBone.cs:17](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Instance/Vrm10InstanceSpringBone.cs:17)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Instance/Vrm10InstanceSpringBone.cs:20](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Instance/Vrm10InstanceSpringBone.cs:20)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Instance/Vrm10InstanceSpringBone.cs:23](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Instance/Vrm10InstanceSpringBone.cs:23)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Instance/Vrm10InstanceSpringBone.cs:77](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Instance/Vrm10InstanceSpringBone.cs:77)

**对 HoCloth 的启发**：

HoCloth 的 `SPRING_BONE` 组件建议不要再是“一个 root bone + 一堆参数”的单平面结构，而是：

- scene 级 collider group 容器
- spring 组件引用 collider group
- spring 内部维护 joint 列表或 root + 自动展开的链

### 5.2 Joint 级数据的粒度非常对

`VRM10SpringBoneJoint` 里单 joint 持有：

- stiffness
- gravity power
- gravity direction
- drag
- radius
- angle limit type
- pitch / yaw

见：

- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneJoint.cs:15](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneJoint.cs:15)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneJoint.cs:41](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneJoint.cs:41)

**对 HoCloth 的启发**：

- 当前 `HoClothBoneChainComponent` 上把 stiffness/damping/drag 直接放在 chain 级别太粗了
- 至少要设计成：
  - model default 参数
  - per-joint 覆盖参数

### 5.3 Collider 与 ColliderGroup 是必须分层的

`VRM10SpringBoneCollider` 支持：

- sphere
- capsule
- plane
- sphere inside
- capsule inside

见：

- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneCollider.cs:6](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneCollider.cs:6)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneCollider.cs:18](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneCollider.cs:18)

`VRM10SpringBoneColliderGroup` 则单独维护 collider 列表，并支持把 group 从原节点分离出去、同时更新引用关系：

- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneColliderGroup.cs:17](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneColliderGroup.cs:17)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneColliderGroup.cs:29](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneColliderGroup.cs:29)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneColliderGroup.cs:53](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/SpringBone/VRM10SpringBoneColliderGroup.cs:53)

**对 HoCloth 的启发**：

- `COLLIDER` 和 `COLLIDER_GROUP` 必须是两个组件
- SpringBone / ClothBone / MeshCloth 不应直接绑定一堆 collider，而应绑定 collider group

### 5.4 Runtime 接口设计非常值得直接借

`IVrm10SpringBoneRuntime` 很值得 HoCloth 学：

- `InitializeAsync`
- `ReconstructSpringBone`
- `RestoreInitialTransform`
- `SetJointLevel`
- `SetModelLevel`
- `Process`
- `DrawGizmos`

见：

- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/IVrm10SpringBoneRuntime.cs:10](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/IVrm10SpringBoneRuntime.cs:10)

**对 HoCloth 的启发**：

HoCloth runtime 也应该分两类输入：

- build/reconstruct 级输入
- per-frame mutable 输入

而不是所有改动都混进 `build_scene`

### 5.5 BufferFactory 说明“构建 buffer”要单独成阶段

`FastSpringBoneBufferFactory.ConstructSpringBoneAsync(...)` 会做两件事：

1. 从 authoring/runtime object graph 收集 spring、collider、joint
2. 转换成可批量求解的 buffer 结构

见：

- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/FastSpringBoneBufferFactory.cs:20](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/FastSpringBoneBufferFactory.cs:20)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/FastSpringBoneBufferFactory.cs:43](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/FastSpringBoneBufferFactory.cs:43)

**对 HoCloth 的启发**：

- `compile/` 之后，native build 里仍然应该有一个 `buffer factory / scene builder`
- `compiled scene` 不等于最终 solver buffer

### 5.6 Runtime Provider + Service 模式能直接转成 HoCloth session + live runtime

`Vrm10FastSpringboneRuntime` 负责：

- 初始化
- 重建
- 注册/释放 buffer

见：

- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/Vrm10FastSpringboneRuntime.cs:16](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/Vrm10FastSpringboneRuntime.cs:16)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/Vrm10FastSpringboneRuntime.cs:43](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/Vrm10FastSpringboneRuntime.cs:43)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/Vrm10FastSpringboneRuntime.cs:69](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/Components/Vrm10Runtime/Springbone/Vrm10FastSpringboneRuntime.cs:69)

`FastSpringBoneService` 则专门负责：

- singleton service
- buffer combiner
- scheduler
- update order

见：

- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/FastSpringBone/FastSpringBoneService.cs:7](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/FastSpringBone/FastSpringBoneService.cs:7)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/FastSpringBone/FastSpringBoneService.cs:43](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/FastSpringBone/FastSpringBoneService.cs:43)
- [_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/FastSpringBone/FastSpringBoneService.cs:90](../_ReferenceProject/UniVRM-0.131.0/Packages/VRM10/Runtime/FastSpringBone/FastSpringBoneService.cs:90)

**对 HoCloth 的启发**：

- `runtime/session.py` 适合继续作为 session 管理层
- 但后续应该增加更明确的：
  - reconstruct
  - model-level override
  - joint-level override
  - draw/debug channel

### 5.7 UniVRM 可借鉴项总结

#### 强借鉴

- `Collider / ColliderGroup / Spring / Joint` 四层关系
- runtime interface
- buffer factory
- reconstruct 语义

#### 选择性借鉴

- runtime service / scheduler
- gizmo 组织

#### 不照搬

- Unity MonoBehaviour 生命周期
- UniGLTF job buffer 实现细节

---

## 6. PositionBasedDynamics 架构分析

### 6.1 SimulationModel 说明 solver 的核心是“持有全部模型与约束”

`SimulationModel` 直接持有：

- particles
- triangle models
- tet models
- line models
- constraints
- constraint groups
- rigid body / particle contacts

见：

- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:17](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:17)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:88](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:88)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:183](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:183)

**对 HoCloth 的启发**：

- native scene 应该是“统一持有资源”的地方
- 不能让每个功能模块自己偷偷持有一套粒子和碰撞缓存

### 6.2 约束对象拆得很清楚

`SimulationModel` 暴露了多种添加约束的入口：

- `addDistanceConstraint_XPBD`
- `addIsometricBendingConstraint_XPBD`
- `addShapeMatchingConstraint`
- `addVolumeConstraint_XPBD`

见：

- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:214](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:214)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:220](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:220)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:242](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.h:242)

在实现里也能看到 cloth constraints 与 bending constraints 被单独构建：

- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:565](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:565)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:577](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:577)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:615](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:615)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:728](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:728)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:1186](../_ReferenceProject/PositionBasedDynamics-master/Simulation/SimulationModel.cpp:1186)

**对 HoCloth 的启发**：

- 约束不要糊成一个“大一统 cloth solver”
- 先从可维护的独立约束类型做起：
  - distance
  - isometric bending XPBD
  - shape matching
  - collider collision
  - attachment / pin

### 6.3 XPBD 入口非常明确，适合 HoCloth 先做最小闭环

在 `Constraints.cpp` 里可以清楚看到：

- `DistanceConstraint_XPBD::initConstraint`
- `DistanceConstraint_XPBD::solvePositionConstraint`
- `IsometricBendingConstraint_XPBD::initConstraint`
- `IsometricBendingConstraint_XPBD::solvePositionConstraint`
- `ShapeMatchingConstraint::initConstraint`
- `ShapeMatchingConstraint::solvePositionConstraint`

对应定位：

- [_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1211](../_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1211)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1227](../_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1227)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1407](../_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1407)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1427](../_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1427)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1985](../_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:1985)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:2003](../_ReferenceProject/PositionBasedDynamics-master/Simulation/Constraints.cpp:2003)

**对 HoCloth 的启发**：

HoCloth 的最小 XPBD 路线可以定成：

1. SpringBone 先做 distance + angle/collision
2. ClothBone 再加 isometric bending XPBD
3. MeshCloth 需要时再引入 shape matching cluster

### 6.4 TimeStepController 提供了极清楚的 step 模板

`TimeStepController::step(...)` 的顺序很值得直接参考：

1. 清加速度
2. substep
3. 半隐式积分
4. 位置约束投影
5. 速度更新
6. 碰撞检测
7. 速度约束

见：

- [_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:75](../_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:75)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:91](../_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:91)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:132](../_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:132)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:189](../_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:189)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:196](../_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:196)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:251](../_ReferenceProject/PositionBasedDynamics-master/Simulation/TimeStepController.cpp:251)

**对 HoCloth 的启发**：

native solver 的 `step_scene(handle, dt, substeps)` 应明确包含：

- transform input update
- external force
- substep loop
- collision update
- position projection
- output extraction

### 6.5 CollisionDetection 说明碰撞系统应该是独立子模块

`CollisionDetection` 不直接绑在某个 cloth 类里，而是：

- 统一维护 collision objects
- 统一生成 contact callback
- 统一更新 AABB

见：

- [_ReferenceProject/PositionBasedDynamics-master/Simulation/CollisionDetection.h:11](../_ReferenceProject/PositionBasedDynamics-master/Simulation/CollisionDetection.h:11)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/CollisionDetection.h:32](../_ReferenceProject/PositionBasedDynamics-master/Simulation/CollisionDetection.h:32)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/CollisionDetection.h:75](../_ReferenceProject/PositionBasedDynamics-master/Simulation/CollisionDetection.h:75)
- [_ReferenceProject/PositionBasedDynamics-master/Simulation/CollisionDetection.h:95](../_ReferenceProject/PositionBasedDynamics-master/Simulation/CollisionDetection.h:95)

**对 HoCloth 的启发**：

- native 端要有独立的 `CollisionWorld`
- `SpringBone / ClothBone / MeshCloth` 都应该往它注册碰撞体

### 6.6 PositionBasedDynamics 可借鉴项总结

#### 强借鉴

- scene/model/constraint/collision 分层
- XPBD 约束最小闭环
- timestep/substep 结构

#### 选择性借鉴

- constraint group
- parameter object

#### 不照搬

- 整套通用物理框架的复杂度
- 过于宽泛的模型支持面

---

## 7. blender_bonex 架构分析

### 7.1 UI 不是一块面板，而是一组职责明确的面板

`ui.py` 里明显把功能拆成多个面板：

- `BoneXRigidbodyModifyPanel`
- `BoneXRigidbodySettingPanel`
- `BoneXJointSettingPanel`
- `BoneXSimulationSettingPanel`
- `BoneXLogPanel`
- `BoneXToolPanel`
- `BoneXSceneSettingPanel`
- `BoneXForceFieldSettingPanel`
- `BoneXSoftConnectionSettingPanel`

见：

- [_ReferenceProject/blender_bonex/panel/ui.py:22](../_ReferenceProject/blender_bonex/panel/ui.py:22)
- [_ReferenceProject/blender_bonex/panel/ui.py:109](../_ReferenceProject/blender_bonex/panel/ui.py:109)
- [_ReferenceProject/blender_bonex/panel/ui.py:204](../_ReferenceProject/blender_bonex/panel/ui.py:204)
- [_ReferenceProject/blender_bonex/panel/ui.py:246](../_ReferenceProject/blender_bonex/panel/ui.py:246)

**对 HoCloth 的启发**：

HoCloth UI 不应该继续增长在一个 `HOCLOTH_PT_main_panel` 里。

应该拆成：

- Main / Components
- SpringBone Settings
- ClothBone Settings
- MeshCloth Settings
- Simulation
- Bake / Cache
- Debug / Logs
- Tools

### 7.2 参数面板是 selection-driven，而不是列表驱动

`RigidbodyManager` 会先更新当前选中的 rigidbody bones，再把激活项同步到 settings 面板：

- `_selected_rigidbody_bones`
- `get_active_rigidbody_bone()`
- `update_rigidbody_setting_panel(...)`

见：

- [_ReferenceProject/blender_bonex/panel/rigidbody_manager.py:12](../_ReferenceProject/blender_bonex/panel/rigidbody_manager.py:12)
- [_ReferenceProject/blender_bonex/panel/rigidbody_manager.py:26](../_ReferenceProject/blender_bonex/panel/rigidbody_manager.py:26)
- [_ReferenceProject/blender_bonex/panel/rigidbody_manager.py:46](../_ReferenceProject/blender_bonex/panel/rigidbody_manager.py:46)
- [_ReferenceProject/blender_bonex/panel/rigidbody_manager.py:68](../_ReferenceProject/blender_bonex/panel/rigidbody_manager.py:68)

**对 HoCloth 的启发**：

- HoCloth 的骨系组件编辑也应支持“当前激活骨 / 当前选中组件”驱动面板
- 这会比单纯从组件列表点展开更高效

### 7.3 modal operator 是 BoneX 交互体验的关键

`BoneXMainModalOperator` 会长期驻留，统一处理：

- 切换状态
- 快捷键
- 形状编辑器
- 软连接
- 约束工具

可定位到：

- [_ReferenceProject/blender_bonex/panel/op.py:25](../_ReferenceProject/blender_bonex/panel/op.py:25)
- [_ReferenceProject/blender_bonex/panel/op.py:47](../_ReferenceProject/blender_bonex/panel/op.py:47)
- [_ReferenceProject/blender_bonex/panel/op.py:114](../_ReferenceProject/blender_bonex/panel/op.py:114)
- [_ReferenceProject/blender_bonex/panel/op.py:179](../_ReferenceProject/blender_bonex/panel/op.py:179)
- [_ReferenceProject/blender_bonex/panel/op.py:191](../_ReferenceProject/blender_bonex/panel/op.py:191)
- [_ReferenceProject/blender_bonex/panel/op.py:216](../_ReferenceProject/blender_bonex/panel/op.py:216)

**对 HoCloth 的启发**：

- 不是所有交互都应该在普通 operator 里结束
- 对于以下能力，modal 更合适：
  - collider shape 调整
  - spring chain preview tool
  - mesh proxy 选择/刷权重
  - cloth pin / attachment 编辑

### 7.4 handler 层做了非常重要的“去重和连续性保护”

`handler.py` 里做了几件事非常关键：

- 注册四类 handler
- playback start / stop 分离
- frame change 去重
- 用 `proc id + frame` 做 identity，防止多视图层重复处理
- 校验帧连续性

见：

- [_ReferenceProject/blender_bonex/panel/handler.py:25](../_ReferenceProject/blender_bonex/panel/handler.py:25)
- [_ReferenceProject/blender_bonex/panel/handler.py:66](../_ReferenceProject/blender_bonex/panel/handler.py:66)
- [_ReferenceProject/blender_bonex/panel/handler.py:76](../_ReferenceProject/blender_bonex/panel/handler.py:76)
- [_ReferenceProject/blender_bonex/panel/handler.py:94](../_ReferenceProject/blender_bonex/panel/handler.py:94)
- [_ReferenceProject/blender_bonex/panel/handler.py:108](../_ReferenceProject/blender_bonex/panel/handler.py:108)
- [_ReferenceProject/blender_bonex/panel/handler.py:177](../_ReferenceProject/blender_bonex/panel/handler.py:177)

**对 HoCloth 的启发**：

我们现在 `runtime/live.py` 已经有了初步的 frame discontinuity 检查：

- [runtime/live.py](../runtime/live.py:57)
- [runtime/live.py](../runtime/live.py:67)
- [runtime/live.py](../runtime/live.py:132)

但后面还应继续向 bonex 靠拢：

- 去重 identity 更明确
- 播放/烘焙分开
- 日志与错误状态更清楚

### 7.5 Shape editor 是一个独立协作者对象，而不是散在 operator 里

`RigidbodyShapeEditor` 单独负责：

- 开始缩放
- 停止缩放
- 开始 mesh 编辑
- 结束 mesh 编辑
- 切换 shape
- 从加权网格生成 shape

见：

- [_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:15](../_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:15)
- [_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:39](../_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:39)
- [_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:97](../_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:97)
- [_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:118](../_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:118)
- [_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:146](../_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:146)
- [_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:178](../_ReferenceProject/blender_bonex/panel/rigidbody_shape_editor.py:178)

**对 HoCloth 的启发**：

后续 HoCloth 很适合引入独立工具对象：

- `ColliderShapeEditor`
- `SpringAuthoringTool`
- `MeshProxyEditor`
- `WeightPaintAdapter`

### 7.6 preset 入口布局也值得借

在 `BoneXRigidbodyModifyPanel` 里，preset 的应用/新增/删除/保存用了一排明确的按钮：

- [_ReferenceProject/blender_bonex/panel/ui.py:65](../_ReferenceProject/blender_bonex/panel/ui.py:65)

**对 HoCloth 的启发**：

- SpringBone、ClothBone、MeshCloth 都适合有 preset，但应放在当前组件可见的位置
- 这比把 preset 藏进菜单更好

### 7.7 blender_bonex 可借鉴项总结

#### 强借鉴

- 面板分层
- selection-driven 设置同步
- modal operator + 专用 editor helper
- handler 去重与播放逻辑
- preset 交互布局

#### 选择性借鉴

- bake 流程
- log panel
- shape editing 工具细节

#### 不照搬

- BoneX 的刚体/PhysX 数据模型
- 与其旧有自定义属性格式强绑定的部分

---

## 8. FleX 架构分析

### 8.1 FleX 最值得借的是“asset/container”思维，不是 GPU 依赖

`NvFlexExtAsset` 明确把 cloth/soft/rigid 的预处理结果收束成一类 asset：

- 粒子
- springs
- triangles

见：

- [_ReferenceProject/FleX-master/include/NvFlexExt.h:311](../_ReferenceProject/FleX-master/include/NvFlexExt.h:311)
- [_ReferenceProject/FleX-master/include/NvFlexExt.h:319](../_ReferenceProject/FleX-master/include/NvFlexExt.h:319)
- [_ReferenceProject/FleX-master/include/NvFlexExt.h:337](../_ReferenceProject/FleX-master/include/NvFlexExt.h:337)

**对 HoCloth 的启发**：

- 我们的 `CompiledMeshCloth` 也应是“资产化”的稳定结构

### 8.2 mesh -> cloth asset 的预处理函数很有参考意义

FleX 提供：

- `NvFlexExtCreateWeldedMeshIndices`
- `NvFlexExtCreateClothFromMesh`

见：

- [_ReferenceProject/FleX-master/include/NvFlexExt.h:402](../_ReferenceProject/FleX-master/include/NvFlexExt.h:402)
- [_ReferenceProject/FleX-master/include/NvFlexExt.h:419](../_ReferenceProject/FleX-master/include/NvFlexExt.h:419)

**对 HoCloth 的启发**：

- `MeshCloth` 编译阶段一定要有：
  - weld
  - topology cleanup
  - constraint generation

### 8.3 moving frame 非常适合角色局部空间模拟思路

`NvFlexExtMovingFrame` 的用途是：

- 在移动角色坐标系中模拟
- 控制惯性注入

见：

- [_ReferenceProject/FleX-master/include/NvFlexExt.h:229](../_ReferenceProject/FleX-master/include/NvFlexExt.h:229)
- [_ReferenceProject/FleX-master/include/NvFlexExt.h:250](../_ReferenceProject/FleX-master/include/NvFlexExt.h:250)

**对 HoCloth 的启发**：

- 未来 `SpringBone` / `ClothBone` 可以考虑支持中心骨或 model root 惯性参考
- 这比简单世界空间重力更接近角色二级动画需求

### 8.4 container / map particle data 是很好的 runtime 资源组织启发

见：

- [_ReferenceProject/FleX-master/include/NvFlexExt.h:550](../_ReferenceProject/FleX-master/include/NvFlexExt.h:550)
- [_ReferenceProject/FleX-master/include/NvFlexExt.h:606](../_ReferenceProject/FleX-master/include/NvFlexExt.h:606)

**对 HoCloth 的启发**：

- runtime scene 应该把“实例”和“底层 buffer”关系理清
- 即使不用 GPU，也应该像容器一样管理 active data

### 8.5 FleX 可借鉴项总结

#### 强借鉴

- moving frame 概念
- mesh preprocessing asset 化思路

#### 选择性借鉴

- container / instance 模型

#### 不照搬

- GPU solver
- 大量 API 级宿主绑定方式

---

## 9. HoCloth 新工程大纲

下面是结合以上参考后，给 HoCloth 定的新蓝图。

### 9.1 顶层产品结构

HoCloth 的能力树正式定为三条主线：

1. `SpringBone`
2. `ClothBone`
3. `MeshCloth`

它们共享：

- 碰撞体系统
- 风场系统
- compiled scene
- XPBD runtime
- Blender playback/bake 工作流

### 9.2 组件层设计

#### 一级组件

- `SPRING_BONE`
- `CLOTH_BONE`
- `MESH_CLOTH`
- `COLLIDER`
- `COLLIDER_GROUP`
- `WIND`
- `PIN`
- `CACHE_OUTPUT`
- `PRESET`

#### 组件关系

- `COLLIDER_GROUP` 引用多个 `COLLIDER`
- `SPRING_BONE / CLOTH_BONE / MESH_CLOTH` 引用多个 `COLLIDER_GROUP`
- `MESH_CLOTH` 可引用 `CACHE_OUTPUT`
- `PRESET` 提供参数模板，不直接参与 runtime

### 9.3 数据层设计

#### Authoring

Blender 内保存：

- 组件引用关系
- root/joint/bone/mesh 选择
- 参数
- 预设
- UI 展开状态

#### Compiled

编译后输出：

- spring 列表
- joint 扁平数组
- cloth bone 图结构
- mesh cloth 粒子/边/三角面
- collider/collider group
- mapping/cache descriptor

#### Runtime

native 内保存：

- solver scene
- buffers
- mutable override
- debug/profiler

### 9.4 Native XPBD 分层

#### scene

- build
- destroy
- reset
- reconstruct

#### model

- particles
- transforms
- collider shapes
- mapping buffers

#### constraints

- spring distance
- bend
- angle limit
- shape matching
- pin / attachment
- collider collision

#### solver

- substep
- projection
- velocity update

#### io

- Python binding
- cache export
- debug dump

### 9.5 Blender 侧目录建议

```text
authoring/
  extract/
  operators/
  handlers/
  gizmos/
  tools/

components/
  common.py
  registry.py
  spring_bone.py
  cloth_bone.py
  mesh_cloth.py
  collider.py
  collider_group.py
  wind.py
  cache_output.py
  preset.py

ui/
  panels/
  lists/
  draw_utils.py

compile/
  compiled_types.py
  compile_spring.py
  compile_cloth_bone.py
  compile_mesh_cloth.py
  compile_collider.py
  compile_scene.py
  debug_export.py

runtime/
  bridge.py
  session.py
  inputs.py
  live.py
  pose_apply.py
  mesh_apply.py
  cache_export.py
```

### 9.6 三个功能的落地方式

#### SpringBone

- 数据组织参考 UniVRM
- UI 工作流参考 BoneX
- 求解最小闭环参考 PBD XPBD distance

#### ClothBone

- 功能目标参考 MC2 BoneCloth
- baseline / transform writeback 思路参考 `VirtualMeshProxy`
- 约束层参考 XPBD distance + isometric bending + optional shape matching

#### MeshCloth

- proxy mesh / selection / mapping 参考 MC2
- mesh preprocessing 参考 FleX
- 求解层参考 PBD cloth constraints

---

## 10. UI 与工作流蓝图

### 10.1 面板规划

- `HoCloth Main`
  - 运行状态
  - Build / Rebuild / Reset / Destroy
- `HoCloth Components`
  - 组件列表
  - 组件创建入口
- `HoCloth Spring`
- `HoCloth Cloth Bone`
- `HoCloth Mesh Cloth`
- `HoCloth Simulation`
- `HoCloth Bake`
- `HoCloth Tools`
- `HoCloth Debug`

### 10.2 交互原则

- 组件列表负责组织关系
- 具体设置面板由“当前激活组件 / 当前选中骨 / 当前选中对象”驱动
- shape 编辑、proxy 编辑、刷权重采用 modal/tool helper
- playback handler 只做：
  - 帧连续性校验
  - runtime input 更新
  - result 回写

### 10.3 当前代码如何演进

#### 可以保留的

- 组件双层容器设计
  - [components/properties.py:33](../components/properties.py:33)
  - [components/properties.py:91](../components/properties.py:91)
- compile 与 runtime 分层
  - [compile/compiled.py:62](../compile/compiled.py:62)
  - [runtime/session.py:46](../runtime/session.py:46)
- live handler 闭环
  - [runtime/live.py](../runtime/live.py:80)
  - [runtime/live.py](../runtime/live.py:123)

#### 需要拆分重构的

- `BONE_CHAIN` 升级为 `SPRING_BONE`
- `authoring/panel.py` 拆成多面板
- `compile/compiler.py` 按功能拆分编译器
- `runtime/inputs.py` 支持 model/joint 级输入
- `runtime/bridge.py` 从 stub 逻辑过渡到真正 native XPBD

---

## 11. 实施顺序建议

### 阶段 1：SpringBone 正式架构化

- 引入 `COLLIDER_GROUP`
- 引入 `SPRING_BONE`
- joint 级参数结构成型
- native 端最小 XPBD distance + collision 跑通
- 面板拆分为 Main / Components / Spring / Simulation

### 阶段 2：ClothBone

- 多 root 骨集合
- compiled bone graph
- bend / shape preserving
- 骨回写批处理

### 阶段 3：MeshCloth

- proxy mesh 构建
- selection / reduction / mapping
- cache 输出

### 阶段 4：工作流补完

- preset
- gizmo / debug
- bake / cache
- profiling

---

## 12. 最终结论

这次重构最重要的结论不是“做一个 XPBD 求解器”，而是下面四点：

1. HoCloth 的产品结构正式定成 `SpringBone / ClothBone / MeshCloth` 三主线。
2. HoCloth 的技术结构正式定成 `Authoring / Compiled / Runtime` 三态分离。
3. `UniVRM` 负责教我们怎么组织 springbone 数据，`PBD` 负责教我们怎么组织 XPBD 求解器，`MagicaCloth2` 负责教我们怎么把功能与预构建做成产品，`blender_bonex` 负责教我们怎么把 Blender 端交互做顺手。
4. HoCloth 未来的关键不是继续堆一个“更大的骨链组件”，而是把组件系统、compiled scene、native solver 和 Blender 工作流一次性按可扩展结构搭正。

如果后续只允许优先做一件事，那应该是：

- 先把当前 `BONE_CHAIN` 体系升级成 `SPRING_BONE + COLLIDER_GROUP + joint-level compiled data`

因为这是三条主线里最小、最稳、最能验证整套架构的一步。
