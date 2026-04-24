# MC2 BoneSpring 第一轮复现记录

## 1. 这轮的目标

这轮的核心目标不是继续扩展 HoCloth 自己的物理方案，而是把 `Magica Cloth 2` 的 `BoneSpring` 真正落进 Blender。

这里的“落进 Blender”指的是：

- Blender 侧继续负责 authoring、组件管理、编译、播放和姿态回写。
- native 侧开始承担真正的高频运行逻辑，而不是只保留空接口。
- 复现优先级以 `MC2` 行为对齐为先，不主动发明 `MC2` 没有的新机制。

---

## 2. 这轮实际做了什么

### 2.1 native 侧从空壳进入可运行状态

这一轮 `_native/` 不再只是几乎空目录，而是补出了第一批真正参与运行的文件：

- `include/hocloth_runtime_api.hpp`
- `src/hocloth_runtime_api.cpp`
- `src/hocloth_python_module.cpp`
- `src/hocloth_collision_world.cpp`

这些文件先把当前 HoCloth 需要的最小闭环接起来：

- `build_scene`
- `destroy_scene`
- `reset_scene`
- `set_runtime_inputs`
- `step_scene`
- `get_bone_transforms`

这意味着 Blender 侧现在不是单纯依赖 Python stub 才能跑，而是已经可以通过 native 模块跑 `BoneSpring` 主流程。

### 2.2 先搬了最影响 MC2 手感的主干

这一轮没有声称“已经完整逐文件翻完 MC2 BoneSpring”，而是先抓最影响当前效果的主干：

- `TimeManager` 风格的固定步频与 simulation power 规则
- `BoneSpring` 相关基础常量
- 中心骨驱动下的链式传播
- 基础碰撞输入、碰撞绑定和结果回写
- Blender 侧 `compile -> build -> step -> pose apply` 的稳定打通

其中已经明确参考并写入 native 的 MC2 来源包括：

- `SystemDefine.cs`
- `TimeManager.cs`
- `DistanceConstraint.cs`
- `TetherConstraint.cs`
- `ColliderCollisionConstraint.cs`

### 2.3 当前效果已经达到“非常接近 MC2”的阶段

从当前测试反馈看，这轮最重要的结果不是“代码结构好看”，而是效果已经明显靠近 `MC2`：

- 四个未专门调过的预设也能直接得到较好的结果
- `BoneSpring` 的整体节奏、拖拽感、中心骨影响和基础碰撞表现已经比较接近 Unity 里的 `MC2`

这说明当前方向是对的，native 重构路线也成立。

---

## 3. 这轮为什么先这样做

本轮采用的是“先抓最影响观感和手感的主干，再逐步往下补齐”的策略。

原因很直接：

- 之前代码过于复杂，很难继续在旧结构上把 `hocloth` 的 `BoneSpring` 对齐到 `MC2`
- 继续围绕旧实现修修补补，收益已经明显变低
- 先让 Blender 里跑出“味道对”的 `MC2 BoneSpring`，比先做漂亮抽象更重要

所以这轮故意做了这些取舍：

- 优先行为对齐，不优先抽象
- 优先移植关键逻辑，不优先扩展功能
- 优先保证 native 可运行，不优先构建完整的大而全 solver 框架

---

## 4. 这轮还没做完的地方

虽然当前效果已经很好，但不能把现在误判成“MC2 BoneSpring 已经完整复现完成”。

当前仍然属于：

- `MC2 BoneSpring` 第一轮 bootstrap 已成立
- 效果已经接近
- 关键主干已经接上
- 但还没有做到逐约束、逐细节的完整对齐

当前还没有完成的部分包括：

- 还没有把 `BoneSpring` 相关关键 C# 文件全部逐段完整翻成稳定的 C++ 结构
- `Distance / Tether / ColliderCollision / SpringConstraint` 还没有全部拆成更接近 MC2 原组织的独立 native 结构
- 一些边界行为、参数映射和求解细节仍然属于“为对味而做的近似实现”
- 当前还没有进入 `ClothBone` 和 `MeshCloth` 的正式 native 复现阶段

---

## 5. 当前结论

当前可以明确得出几个结论：

- 这条“直接把 MC2 做进 Blender”的路线是成立的
- native 侧直接翻译 MC2 关键逻辑，确实比继续围绕旧内核修补更有效
- 现在不应该太早转向 `MC2` 没有的 HoCloth 自有功能
- 当前最正确的优先级，是继续把 `MC2` 自己的三种模拟类型复现完整

---

## 6. 接下来的优先级

接下来只按下面的顺序推进：

1. 继续补齐 `MC2 BoneSpring`
2. 然后进入 `MC2 ClothBone`
3. 然后进入 `MC2 MeshCloth`

当前阶段明确不优先做：

- `MC2` 没有的扩展功能
- 偏离 `MC2` 的自定义物理路线
- 为了“更通用”而过早改写当前已经对味的实现

---

## 7. 对后续实现的约束

为了避免后面又慢慢偏回“自研味道”，这里补几条明确约束：

- 后续只要是直接移植或近似直译 `MC2` 的代码块，都要注释标明来源
- 当出现“更优雅的写法”和“更像 MC2 的写法”冲突时，当前阶段优先选更像 `MC2`
- 当前允许修 bug，但不鼓励顺手做逻辑改造
- 文档、工程大纲和代码实际方向必须保持一致，不再保留“XPBD 占位后再自研替换”的旧叙事

---

## 8. 本轮阶段判断

可以把当前阶段判断为：

`MC2 BoneSpring` 已经完成第一轮有效落地，效果已经进入“接近 MC2，可继续沿同一路线深化”的状态。

下一轮开始，重点不再是证明路线对不对，而是继续把 `MC2 BoneSpring` 的剩余关键部分补完整，并为后续 `ClothBone / MeshCloth` 做同样的方法复制。
