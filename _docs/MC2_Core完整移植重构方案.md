# MC2 Core 完整移植重构方案

本文档用于把 HoCloth 的推进方式从“在现有几个 C++ 文件上继续修补 BoneSpring”正式切换为“以 Magica Cloth 2 Core 为蓝本，先完整移植 native 后端架构，再接入 Blender”。

参考源码位置：

```text
_ReferenceProject/MagicaCloth2/Scripts/Core
```

## 1. 重构结论

当前 C++ 侧不再继续沿用“小型 MVP 求解器”的组织方式。现有 `_native/include/hocloth_runtime_api.hpp` 与 `_native/src/*.cpp` 只能作为历史桥接参考，不能作为新物理内核的架构基础。

新的主线是：

1. 先在 C++ 侧建立接近 MC2 Core 的完整后端系统。
2. 按 MC2 的模块关系移植数据结构、管理器、约束、虚拟网格、碰撞、时间步进与调试诊断。
3. 先让 native 后端能独立构建、独立测试、独立 dump 中间状态。
4. Blender 侧只作为 authoring、compiled scene 输入、逐帧输入输出和结果回写层。

这意味着短期内可以放慢功能演示，但不能再用“效果差一点先凑合”的方式推进。移植不完整就很难定位问题；完整系统骨架先起来，后面的调试才有坐标系。

## 2. MC2 Core 模块拆解

MC2 Core 的主要目录如下：

```text
Cloth/
  Collider/
  Constraints/
  Wind/
Define/
Interface/
Manager/
  Cloth/
  Render/
  Simulation/
  Team/
  TransformManager/
  VirtualMesh/
PreBuild/
Reduction/
Settings/
Utility/
  Data/
  Grid/
  Jobs/
  Math/
  Mesh/
  Misc/
  NativeCollection/
  ResultCode/
  Time/
VirtualMesh/
  Function/
```

对 HoCloth 来说，优先级不是“先单独抄一个 SpringConstraint”，而是按系统依赖关系分层移植：

1. `Define / Interface / Utility`：基础类型、结果码、数学、容器、时间与调试工具。
2. `Manager`：生命周期、team、transform、simulation、cloth、virtual mesh 的组织方式。
3. `Cloth / Constraints / Collider / Wind`：参数、约束、碰撞与风场。
4. `VirtualMesh / Reduction / PreBuild`：MeshCloth 和 ClothBone 真正需要的拓扑、代理网格与预构建数据。
5. `Render`：在 Blender 里不照搬 Unity 渲染管线，但要保留 MC2 的映射、回写和渲染数据边界。

## 3. C++ 侧目标目录

新的 `_native` 推荐结构：

```text
_native/
  CMakeLists.txt
  include/
    hocloth/
      api/
      core/
      cloth/
      manager/
      simulation/
      virtual_mesh/
      utility/
  src/
    api/
    core/
    cloth/
      collider/
      constraints/
      wind/
    manager/
      cloth/
      render/
      simulation/
      team/
      transform/
      virtual_mesh/
    prebuild/
    reduction/
    utility/
      data/
      grid/
      math/
      mesh/
      native_collection/
      result_code/
      time/
    virtual_mesh/
      function/
    binding/
  tests/
    native/
  tools/
    dump_scene/
```

命名可以 C++ 化，但文件归属必须能一眼对应 MC2 Core。比如 `SimulationManager.cs` 可以对应 `manager/simulation/simulation_manager.hpp/.cpp`，`SpringConstraint.cs` 可以对应 `cloth/constraints/spring_constraint.hpp/.cpp`。

## 4. 移植规则

1. 直接移植或近似直译的代码块必须注释来源，例如：

```cpp
// Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/SpringConstraint.cs
```

2. 默认不做算法再设计。可以改语言层实现、内存管理和 Blender 接入边界，但不能因为“看起来更优雅”改变物理行为。
3. MC2 依赖 Unity、Burst、Job、NativeArray 的部分，先建立等价抽象层，再翻译上层逻辑。
4. 不允许把 Blender 对象、Python dict、UI 属性直接塞进 solver 内部。Python 只给 compiled data 和逐帧输入。
5. 每个模块移植后必须能被 native 测试或 dump 工具单独验证，不能只靠 Blender 视口主观观察。
6. 旧 BoneSpring 成果只作为测试样例和行为对照，不再作为新架构的中心。

## 5. Native 后端分层

### 5.1 Core 基础层

负责与 MC2 等价的基础设施：

- `ResultCode`
- `AABB / IntAABB / MathUtility`
- bit flag、chunk、processing list、simple array 等容器
- fixed time step 与 profiling 计数
- handle、id、range、chunk 的统一表达

目标是让后面的 manager 和 constraint 可以用接近 MC2 的数据流写出来，而不是每个文件各自发明容器。

### 5.2 Manager 层

Manager 层是完整移植的关键，不应跳过。建议至少保留这些概念：

- `MagicaManager`：全局生命周期入口
- `TimeManager`：固定步进与帧时间
- `TeamManager`：一个 cloth/bone/mesh 模拟单元的运行状态、参数和范围
- `TransformManager`：骨骼与外部 transform 输入缓存
- `SimulationManager`：XPBD step、约束调度、碰撞调度
- `ColliderManager`：碰撞体注册、更新、查询数据
- `WindManager`：风场数据
- `ClothManager`：cloth process 生命周期
- `VirtualMeshManager`：虚拟网格资源与引用

Blender runtime session 可以包住这些 manager，但不能替代这些 manager。

### 5.3 Cloth 与约束层

按照 MC2 的约束文件逐个落地：

- `DistanceConstraint`
- `TriangleBendingConstraint`
- `TetherConstraint`
- `AngleConstraint`
- `MotionConstraint`
- `InertiaConstraint`
- `ColliderCollisionConstraint`
- `SelfCollisionConstraint`
- `SpringConstraint`

BoneSpring 只是其中一个使用场景。后续 ClothBone 和 MeshCloth 需要的是同一套 constraint pipeline，而不是三套互不相干的求解器。

当前约束层闭合标准分两层判断：

- `solver/data-owner complete`：约束参数、运行时 buffer、注册/释放、step list 消费、XPBD 投影路径已经落到 C++。
- `builder complete`：MC2 的 `CreateData(...)` 或等价数据生成入口已经能从 native `VirtualMesh` / manager 数据生成约束数据。

截至当前，`MotionConstraint`、`TetherConstraint`、`SpringConstraint`、`DistanceConstraint`、`InertiaConstraint`、`TriangleBendingConstraint` 已进入 file/API level complete。`AngleConstraint` 的 runtime solver 已完成，底层 baseline 数组、parent-driven builder、Mesh edge parent feed、Bone transform parent feed 已迁入 native；剩余工作是把完整 `VirtualMeshProxy / PreBuild` 转换链闭合。`ColliderCollisionConstraint` 的 point/edge runtime 已完成，但 Blender 当前可见 BoneSpring 路径仍在 bootstrap angular-space solver 中使用 compiled collision 数据，完整 `ColliderManager + ColliderCollisionConstraint` 还没有接到 active runtime；因此碰撞问题要分清“临时运行时修正”和“完整 MC2 manager 接入”。`SelfCollisionConstraint` 已有 primitive、broad phase、XPBD contact、intersect/tangle 路径，但完整 builder parity 还要继续核对。

### 5.4 VirtualMesh / PreBuild 层

这是从 BoneSpring 走向 ClothBone / MeshCloth 的核心：

- selection data
- virtual mesh primitive
- vertex attribute
- bone weight
- transform mapping
- mesh reduction
- proxy mesh
- input/output mapping
- serialization / debug dump

Blender 的网格、骨骼、权重最终要编译成这里的稳定数据，而不是在每帧临时推导。

当前 `VirtualMesh` 底层已不再只是数组壳：固定点/AABB、顶点 bind pose、BoneCloth transform restore rotation、MeshCloth edge baseline parent 生成、BoneCloth transform baseline 生成、baseline local pose/root/depth 已进入 native。后续应继续按 `VirtualMeshProxy.cs` 的函数顺序补齐 proxy conversion、normal/tangent、edge flag、mapping、reduction、custom skinning，而不是在约束里临时拼 topology。

### 5.5 API / Binding 层

Python 可见 API 只负责：

- 创建 / 销毁 session
- build compiled scene
- reset
- set frame inputs
- fixed-step simulation
- 读取骨骼输出或粒子输出
- debug dump

API 层不应暴露内部 manager 指针，不应让 Python 逐项操作 constraint buffer。

## 6. 推进阶段

### 阶段 A：架构落地

目标：建立新 `_native` 目录结构、CMake 目标和基础类型。

验收：

- native 能独立编译一个空 runtime
- Python module 仍可 import
- 有最小 native test 或 dump 工具
- 文档中每个 MC2 Core 目录都有 HoCloth 对应位置

### 阶段 B：基础设施移植

目标：移植 `Define / Interface / Utility / Time / ResultCode / NativeCollection` 的 C++ 等价层。

验收：

- 基础容器和数学工具有单元测试
- 坐标、矩阵、四元数、AABB、flag 行为可 dump
- 不接入 Blender 也能跑基础测试

### 阶段 C：Manager 骨架

目标：建立 Team、Transform、Simulation、Collider、Wind、VirtualMesh、Cloth manager 的生命周期。

验收：

- 可以创建 team
- 可以注册 transform / collider / virtual mesh
- 可以 step 空 simulation
- 可以 dump manager 状态和 buffer range

### 阶段 D：BoneSpring 作为第一条穿透链

目标：用新 manager 和 constraint pipeline 重做 BoneSpring。

验收：

- BoneSpring 的数据不绕开 TeamManager / SimulationManager
- `SpringConstraint / InertiaConstraint / ColliderCollisionConstraint / AngleConstraint` 来源和行为有 MC2 对照
- 当前 native smoke 已串起 `SimulationStepTeamUpdate -> StartSimulationStep -> MotionConstraint -> DistanceConstraint -> EndSimulationStepSolve -> EndSimulationStep -> CalcDisplayPosition`，并覆盖 Motion max-distance / backstop、固定点 Spring、Distance、old/display/proxy 写回的有限值断言。
- Blender 侧只传 compiled data + 每帧 transform
- 旧效果对照场景能稳定复现或明确指出差异来源

当前推进边界：

- Constraints 进入大模块 closure pass：`DistanceConstraint::CreateData(...)`、`TriangleBendingConstraint::CreateData(...)`、`InertiaConstraint::CreateData(...)` 已迁入 native；`AngleConstraint` 的底层 baseline feed 已开始闭合，后续优先补完整 `VirtualMeshProxy / PreBuild` 生成、Collider component 注册、SelfCollision builder parity。
- `Wind` 与 `VirtualMesh` 已开始补底层行为，但 smoke 仍保持克制；避免在完整 proxy/mapping/reduction/skinning 没闭合前做深行为断言。
- `PreBuild` 已开始从 planned 进入 partial：`SharePreBuildData`、`UniquePreBuildData`、`PreBuildSerializeData`、原 `PreBuildScriptableObject` 的 build-id 数据库行为、`RenderSetupDataSerialization` 和 `VirtualMeshSerialization` 的保存数据壳已迁入 native。当前只闭合数据容器/校验/索引/transform-id 替换，Unity asset、renderer/mesh 采集、raw-byte 反序列化和 manager warmup 仍留在后续编译数据链。
- 底层 Utility / NativeCollection 继续向 MC2 对齐：`FixedList*BytesExtensions.cs` 已统一映射到 `utility/native_collection/fixed_list.hpp`，TeamManager 的 sync-parent list 与 mapping-index list 已改用同一套 fixed-list 语义；`StaticStringBuilder.cs`、`NativeReferenceExtensions.cs`、`NativeMultiHashMapExtensions.cs`、`InterlockUtility.cs`、`JobUtility.cs` 的 native 同步兼容层也已补入，后续 VirtualMesh/Reduction 的 packed dictionary、AABB/UV/transform job helper、aggregate buffer 与 dump 工具可直接复用。

### 阶段 E：ClothBone

目标：基于同一套 pipeline 接入骨网络布料。

验收：

- 多 root 骨集合
- 骨拓扑连接
- 距离 / 弯曲 / 形状保持类约束
- 骨姿态回写

### 阶段 F：MeshCloth

目标：接入 virtual mesh、proxy、mapping、cache 输出。

验收：

- 代理网格可构建
- simulation mesh 到 render mesh 可映射
- 粒子输出可缓存
- 高模输出不依赖实时视口观察

## 7. Blender 接入原则

Blender 侧已经有不少可用工作，不需要推翻。但它必须重新定位：

- `authoring/`：用户编辑、选择、工具、面板。
- `components/`：Blender 属性与组件容器。
- `compile/`：把 Blender 对象编译为 native 可稳定消费的数据。
- `runtime/`：session 生命周期、逐帧输入、结果回写。

当前 BL 侧已经承担并应继续承担的 MC2 边界：

- `ClothBehaviour.cs` / `MagicaCloth.cs`：对应 `components/` 和 `authoring/` 的属性、预设、面板、构建入口。
- `MagicaClothAPI.cs`：对应 `runtime/exchange.py`、`runtime/session.py`、`runtime/bridge.py` 和 native Python module 的 envelope API。
- `ITransform.cs` / `ExTransformAccessArray.cs`：对应 Blender object id、compiled transform record、逐帧 transform input。
- `MeshUtility.cs` / `Manager/Render/*`：对应 `compile/` 的 mesh/renderer 数据采集和 `runtime/pose_apply.py` 的结果写回，不作为 native solver 内部模块。

Blender 侧不负责：

- 约束求解
- 粒子状态管理
- 碰撞投影
- XPBD 迭代调度
- 临时补齐 MC2 中缺失的 manager 概念

逐帧交换的核心边界应保持简单：

```text
Blender frame data -> native managers -> simulation step -> output buffers -> Blender apply
```

## 8. 调试策略

必须从一开始保留可诊断性：

- compiled scene dump
- native build dump
- manager buffer dump
- per-step debug counters
- particle / transform / collider range dump
- constraint enable mask dump
- MC2 源文件对应表

每次效果异常时，优先回答：

1. 是输入编译错了，还是 native 构建错了？
2. 是 manager 范围错了，还是 constraint 数据错了？
3. 是 fixed step 调度错了，还是单个约束投影错了？
4. 是 Blender 坐标转换错了，还是 MC2 行为尚未移植？

## 9. 当前下一步

推荐立即执行：

1. 继续按 `MC2_PORTING_MAP.md` 从 `partial/planned` 文件往 `complete` 推，不再扩展旧 `_native/src/hocloth_*.cpp` MVP 逻辑。
2. 继续补 `PreBuildManager.cs`、`ClothSerializeData*.cs`、`ClothProcess*.cs` 的数据链，让 compiled cloth 能自然产生 proxy mesh、constraint data 和 team/manager 注册请求。
3. `VirtualMeshProxy.cs` 只补会卡住上述数据链的底层生成入口：proxy conversion、normal/tangent、edge flag、baseline、mapping/reduction/custom skinning 的输入输出边界，避免提前深挖完整行为测试。
4. 用 `VirtualMesh / PreBuild` 生成的数据反喂 `AngleConstraint`、`SelfCollisionConstraint`、`ColliderCollisionConstraint`，不要在约束内临时推导拓扑。
5. 继续补 `TeamManager / SimulationManager` 的 MC2 生命周期、culling、参数同步、update-list population。
6. 继续在 `MC2_PORTING_MAP.md` 中把 `bl-boundary` 项标清楚，避免把 Unity/Blender 对象访问误判为 native solver 缺口。
7. 当前阶段可以暂缓逐步编译；做大块原样移植后，再统一进入编译修复与 WinDbg/cdb 定位。

这条路的关键不再是“能不能跑一个小 smoke”，而是把 MC2 Core 的数据流完整搬过来，让后续调试有稳定坐标系。

## 10. 当前碰撞接入决策

- Blender 侧碰撞主流程改为 MC2 风格：BoneSpring/BoneCloth 组件直接持有链级 `collider_ids`，对应 MC2 `ColliderCollisionConstraint.colliderList`。
- 旧 ColliderGroup/Binding 仅作为兼容/组织层保留，不再作为新交互主路径；新建链会默认接入已有 collider，新建 collider 会默认挂到已有链，但构建时不会强制全局自动绑定。
- native runtime 已把骨骼粒子和 collider 统一到 `blender_world`：每帧 `basic_head_positions` 写入 MC2 particle base/step-basic position，collider 使用同一帧 world transform。
- `joint_radius` 已接到 MC2 `radius_curve_data`，`collider_limit_distance` 已接到 `ColliderCollisionConstraint.limit_distance`，后续碰撞异常应优先从 collider list、世界坐标、radius/limit 参数和 manager lifecycle 四处定位。
## 当前边界决策：Blender 不再补尾端骨

从本阶段开始，Blender Python 侧只采集用户真实资产输入，不再主动追加 `append_tail_tip` / 虚拟尾端骨 / 虚拟尾端粒子。需要叶端粒子的链条由用户在 armature 中添加真实叶骨，native/backend 按真实骨架构建 MC2 数据。

新的交换方向：
- Blender 侧输入：真实骨架、root bone、参数、碰撞体引用、逐帧 transform。
- Native/backend 构建：解析 joints/lines/baselines/colliders，并逐步接管 ParticleBuffer、VirtualMesh、mapping、debug primitive。
- Blender 侧绘制/调试：优先读取 `build_output`，不再从 authoring 层反推 MC2 拓扑。

`runtime_debug_latest.json` 已包含 `build_output` envelope，当前字段为 `particles / lines / baselines / colliders`。后续补全 VirtualMesh 与 ParticleBuffer 时继续扩展这个输出，而不是恢复 Blender 侧的自动拓扑加工。

组件对齐原则：

- Blender authoring 侧的组件属性需要直接对齐 MagicaCloth2 的 Unity 组件，而不是维护一套 HoCloth 自己的中间组件概念。
- BoneCloth / BoneSpring 都应视为 `MagicaCloth` 组件的不同 authoring mode，主参数结构按 MC2 `ClothSerializeData` 和各 constraint `SerializeData` 命名。
- Collider 侧按 `ColliderComponent`、`MagicaSphereCollider`、`MagicaCapsuleCollider`、`MagicaPlaneCollider` 组织，链级碰撞引用对齐 `ColliderCollisionConstraint.colliderList`。
- Blender 旧 `PropertyGroup` 类名和 `compile/` 数据结构只作为迁移兼容壳存在；UI 文案、导出协议、native transfer 都应优先使用 MC2 component / serialize data 名称。
- 新前端主线已经转到 `components/mc2.py`：主面板只创建 MC2-native component，保留 Build / Step / Live Run / Reset-to-first-frame 这些有价值的 runtime 控制。旧 Blender component/compile 面板不再作为主入口，只在过渡期给旧文件 fallback。

## 架构转向：移除 Blender 侧重编译层

当前 HoCloth 同时存在四层逻辑：
- MC2 native 后端。
- Blender authoring/UI 组件。
- Blender Python 侧 `compile/` 预编译层。
- C++ 侧 nanobind/API 接收与 runtime build。

新的判断是：Blender Python 侧 `compile/` 不应该继续作为一套重型抽象层存在。它最初主要服务实时视图刷新、JSON 预览和早期 BoneSpring MVP，但现在目标已经变成完整 MC2 后端移植；继续让 Python 提前生成 joints、lines、baselines、collision bindings、尾端粒子或绘制数据，会形成一套“半个 MC2”，最终和 C++ 后端的真实 MC2 Build/PreBuild 分叉。

目标架构改为：

```text
Blender authoring components
  -> authoring snapshot / raw scene refs
  -> C++ Blender transfer unit
  -> MC2-style PreBuild / Build / manager registration
  -> build_output / step_output
  -> Blender viewport drawing / pose or mesh writeback
```

### Blender 侧保留职责

Blender Python 侧只负责：
- UI、属性、预设、组件创建和用户交互。
- 采集稳定引用：armature、root bone、mesh object、collider object、参数、曲线采样值、对象名/id。
- 逐帧采集 transform、velocity、scale、基础 pose。
- 接收 `build_output` 做绘制/检查，接收 `step_output` 做 pose/mesh/cache 写回。

Blender Python 侧不再负责：
- 自动补尾端骨、补虚拟粒子。
- 推导 MC2 topology 作为权威数据。
- 生成最终 constraint/baseline/VirtualMesh/ParticleBuffer。
- 为实时视图刷新维护一套独立编译模型。
- 把 ColliderGroup/Binding 作为新主路径。碰撞选择以链上 collider list 为主，旧组仅作为兼容导入数据。

### C++ 侧新增边界：Blender Transfer Unit

C++ 侧已经开始新增一个明确的 Blender 输入转换单元。当前落点是：

```text
_native/include/hocloth/blender/authoring_snapshot_transfer.hpp
_native/src/blender/authoring_snapshot_transfer.cpp
```

它负责把 Blender authoring snapshot 转为 MC2 自己的数据：
- 骨架层级、rest head/tail、parent index、transform record。
- BoneSpring/BoneCloth/MeshCloth 参数。
- collider refs/list 与 collider serialize data。
- mesh 顶点、边、面、权重、选择属性。
- PreBuild/VirtualMesh/ParticleBuffer 需要的中间数据。

这个转换单元是边界适配层，不是 solver。进入 MC2 manager/constraint 之后，只使用 native MC2 风格数据结构。

### 迁移路线

1. `compile/` 已从 active code 中移除；后续不再恢复 Python 侧重型预编译层。
2. `authoring_snapshot` exchange payload 作为主构建输入，用来传 Blender 原始组件和对象采样数据。
3. nanobind `build_authoring_snapshot()` 直接调用 C++ Blender transfer unit，再走 native runtime build；后续逐步替换为 MC2 PreBuild/Build。
4. `compiled_scene` payload 降级为 legacy/debug 路径，直到 native Build 能覆盖 BoneSpring/BoneCloth/MeshCloth。
5. 视口绘制切到 `build_output`，不再依赖 Python 编译层实时推导。
6. Python 侧只保留 `authoring_snapshot` 采样、运行时帧输入、导出/调试和 build/step 调用胶水。
