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

- `MotionConstraint` 的 max-distance / backstop 主路径已经落地并通过 native smoke；下一步优先移植 `TetherConstraint` 与 `TriangleBendingConstraint`。
- `Wind` 与完整 `VirtualMesh` 行为暂不进入 smoke 行为测试，只保留底层构建、数组所有权和编译安全检查，避免在 XPBD 主链稳定前扩大调试面。

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

1. 冻结旧 `_native/src/hocloth_*.cpp` 的继续扩展。
2. 创建新 native 目录骨架和 CMake 分组。
3. 建立 `MC2_PORTING_MAP.md`，列出 MC2 Core 每个 `.cs` 文件的移植状态。
4. 先移植 `ResultCode / Math / Time / NativeCollection` 等基础层。
5. 再搭建 Manager 空壳，而不是直接继续塞 Spring 逻辑。

这条路慢一点，但它能把“调试不知道是哪儿出问题”变成可追踪的问题树。
