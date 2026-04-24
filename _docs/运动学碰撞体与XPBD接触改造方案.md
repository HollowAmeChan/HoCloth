# HoCloth 运动学碰撞体与 SpringBone 接触改造方案

这份文档记录当前碰撞改造方向。结论已经从“继续完善 XPBD contact lambda”调整为“优先对齐 MagicaCloth2 的 BoneSpring 数据流”。

## 1. 当前问题

Blender 中的碰撞体可以由动画强制驱动。它们在 runtime 中相当于 kinematic collider，会主动推链条，而不会被链条反推。

之前我们遇到过三类现象：

- 刚接触时整条链被瞬间抬直。
- 初始重叠时被强推出去并获得很大速度。
- 修掉大问题后，仍然在接触建立瞬间出现小范围抽搐。

目前已经解决的部分：

- 不再出现“整根立刻竖直”的大错误。
- 无碰撞下垂可以保持正常。
- 静态碰撞体重新使用骨段最近点检测，避免只检测 tail 时碰撞无效。
- runtime 已开始引入 MC2 风格的独立速度基准 `velocity_position`。

仍在处理的部分：

- 接触建立瞬间的小范围抽搐。
- 静态碰撞和运动学碰撞都需要稳定、直观、可调。

## 2. XPBD Contact Lambda 路线的问题

我们尝试过把碰撞作为 XPBD contact constraint 放进 solver，并加入：

- speculative contact
- warm start
- contact lambda cache
- 多采样接触
- 接触滞回
- 表面速度阻尼

这些方案能缓解一部分问题，但很容易陷入调参地狱。根本原因是 HoCloth 的 spring bone 链条不是通用刚体系统。单个局部接触修正会被骨长约束和父子链传播放大，最后表现成整条链姿态异常或接触处反复抖动。

因此当前不再把“完善 XPBD contact lambda”作为主路线。

## 3. MagicaCloth2 给出的关键启发

MagicaCloth2 的 BoneSpring 稳定，不是因为某个阻尼参数神奇，而是因为它的数据流不同。

MC2 维护了几条分离状态：

- `basePosArray`: 当前帧参考位形。
- `nextPosArray`: 约束和碰撞后的模拟位置。
- `oldPosArray`: 上一帧确定位置。
- `velocityPosArray`: 用来计算下一帧速度的独立基准位置。

关键点是：约束可以修改 `nextPos`，但这个修改是否进入下一帧速度，不是天然发生的，而是通过 `velocityPosArray += correction * attenuation` 单独控制。

这和我们之前用 `previous_position` 同时承担 old position 和 velocity basis 的做法不同。之前一旦碰撞修正位置，就很容易把修正变成速度，造成接触抽搐或能量注入。

## 4. 当前 HoCloth 新方向

当前方向是保留 HoCloth 已经能正常下垂的 spring 主预测，但把碰撞处理改成更接近 MC2：

1. 先做正常 spring/inertia/gravity 预测。
2. 做骨长和方向约束。
3. 做局部碰撞投影。
4. 碰撞后再做一次骨长和方向约束整理。
5. 旧的 `warm_start_contact_constraints` / `solve_contact_constraints` / `contact lambda cache` 已从 runtime 主体清理掉。

碰撞检测层当前使用骨段最近点，而不是只看 tail 点。原因是 HoCloth 当前每根骨仍然是一个段；如果只看 tail 点，静态碰撞体碰到骨段中间时会完全没有效果。

## 5. 已落地的 MC2 状态拆分

runtime 状态已经开始从单一 `previous_position` 速度推导，拆成更接近 MC2 的三类位置：

- `position`: 当前模拟位置，类似 MC2 `nextPos`。
- `previous_position`: 上一帧确定位置，类似 MC2 `oldPos`。
- `velocity_position`: 独立速度基准，类似 MC2 `velocityPos`。

并且 native 已开始引入 MC2 的时间调度与频率修正：

- scheduler 使用全局 `simulation_frequency` + `accumulated_time` 计算每帧 fixed-step 数量，不再依赖手动 substep。
- 当 `scheduled_steps` 超过 `max_simulation_steps_per_frame` 时，超出的步数会被记录为 `skipped_steps`（对应 MC2 的 `skipCount` 思路），用于避免掉帧时无限追赶。
- `SimulationPower`（以 90Hz 为基准）用于修正约束强度随模拟频率变化的手感，目前已接入 distance、alignment/restoration 权重，以及 step 结算阶段的 damping 修正（对应 `simulationPower.z`）。

同时增加了独立的 `velocity`，主预测阶段不再直接用 `position - previous_position` 推导惯性，而是使用上一帧结算出的 `velocity`。

当前语义：

- 预测阶段：`position` 从上一帧位置、惯性速度、root/center 继承运动、重力计算得到。
- 预测阶段：`velocity` 采用 MC2 一样的 m/s 语义，预测时使用 `velocity * simulationDeltaTime`，不再把上一 substep 位移当作下一 substep 速度。
- 预测阶段：`velocity_position` 记录本帧速度基准，继承 root/center 位移，但不包含后续碰撞推出。
- 约束/碰撞阶段：位置投影通过 helper 同步修正 `position` 与 `velocity_position`，避免投影被下一帧误认为新增速度。
- 约束阶段：骨长约束参考 MC2 `DistanceVelocityAttenuation = 0.3`，方向整理参考 restoration/angle 类约束使用较高 attenuation，而不是所有 correction 都完全 preserve velocity。
- 接触阶段：碰撞投影不再直接把 collider carry 写入 `velocity_position`；它只记录接触法线和 friction 强度，接近 MC2 的 `collisionNormalArray` / `frictionArray`。
- 结算阶段：`velocity = position - velocity_position`，再应用 damping/drag。
- 结算阶段：参考 MC2 `EndSimulationStepJob`，统一用接触法线和 friction 做静止摩擦、动摩擦，再写回 `velocity`。
- 时间步阶段：碰撞体 transform 在 native substep 内从上一状态插值到目标状态，避免 Blender 输入先把碰撞体瞬移到目标帧。
- 结算阶段：`previous_position = position`，只保留 old position 语义。

这一步是从“碰撞参数调节”切到“数据流对齐 MC2”的关键变化。后续如果仍有尾端状态异常，需要继续检查哪些约束应该部分影响 `velocity_position`，哪些应该完全不影响，而不是继续调碰撞推力。

## 6. MC2 时间调度研究结论

这轮重点查看了 MC2 的几个位置：

- `TimeManager.FrameUpdate`: 维护全局 `simulationFrequency`，默认 `90`，每个 simulation step 的时间为 `1 / simulationFrequency`。
- `TimeManager.FrameUpdate`: 根据 simulation frequency 计算 `SimulationPower`，用于约束强度随模拟频率变化而修正。
- `TeamManager`: 每帧根据累计时间和 `SimulationDeltaTime` 算出 `updateCount`，并用 `maxSimulationCountPerFrame` 限制一帧最多补多少步。
- `SimulationManager`: 每个 simulation step 使用固定 `SimulationDeltaTime`，执行固定顺序的 start step、约束、collision、end step。

重要结论：

1. MC2 没有把 `substep` 暴露成局部调参项。
2. MC2 的核心是“全局固定频率模拟”，不是“这一帧随手切几份”。
3. 一帧 30fps、simulation frequency 90Hz 时，通常就是这一帧需要跑约 3 个固定 simulation step。
4. 如果掉帧，需要通过 `maxSimulationCountPerFrame` 限制最多补几步，而不是无限追赶。
5. 约束强度不是简单和 substep 线性绑定，而是通过 `SimulationPower` 这类频率修正系数保持手感。

因此 HoCloth 当前 `dt + substep` 的设计需要改方向：

- `dt` 保留，表示 Blender 外部帧跨度，例如 30fps 时为 `1/30`。
- UI 不应继续暴露手动 `substep` 作为主要调参项。
- runtime 应新增全局 `simulation_frequency`，默认建议 `90`，和 MC2 默认一致。
- native scheduler 根据累计时间决定本帧跑几个固定 step，每个 step 使用 `simulation_dt = 1 / simulation_frequency`。
- 需要新增 `max_simulation_steps_per_frame`，防止掉帧时一次补太多导致爆炸。
- Blender 端 UI 可以显示 “Simulation Frequency / Quality”，而不是 “Substeps”。

## 7. MC2 单步约束顺序

MC2 的 `SimulationManager` 每个 simulation step 大致是：

1. 更新 collider list。
2. 更新 collider work data。
3. `StartSimulationStepJob`: 计算 base pose、inertia、velocity、gravity/wind、`nextPos`、`velocityPos`。
4. `UpdateStepBasicPotureJob`: 计算本 step 的约束参考姿态。
5. `tetherConstraint`。
6. `distanceConstraint`。
7. `angleConstraint`。
8. `bendingConstraint`。
9. `colliderCollisionConstraint`。
10. 碰撞后再次 `distanceConstraint`，用于整理碰撞扰乱后的距离。
11. `motionConstraint`。
12. `selfCollisionConstraint`。
13. `EndSimulationStepJob`: 用 `velocityPosArray`、`frictionArray`、`collisionNormalArray` 统一结算速度。

这对 HoCloth 的启发：

- 不应该在每个 substep 里重复很多轮“forward/reverse/collision/forward/reverse”。
- 更接近 MC2 的方向是每个固定 simulation step 运行一套确定的约束序列。
- HoCloth 当前只有骨链，可以先收敛成：predict -> distance -> direction/restoration -> collider -> distance -> optional motion limit -> end velocity。
- 后续需要把当前 `solver_iterations` 逐步降级为固定序列，而不是继续让它承担稳定性。

## 8. HoCloth 下一阶段改造计划

下一阶段建议大刀阔斧改 runtime 调度层：

1. 废弃或隐藏 UI 上的手动 `substep`。
2. 新增全局 `simulation_frequency`，默认 `90`。
3. 新增全局 `max_simulation_steps_per_frame`，默认可先设为 `4` 或 `5`。
4. native 保存 `accumulated_time` 或等价状态，用输入 `dt` 累计出需要执行的 fixed step 数。
5. 每个 fixed step 都使用固定 `simulation_dt = 1 / simulation_frequency`。
6. armature/collider transform 在 fixed step 内按 frame interpolation 取样。
7. 约束强度引入 MC2 风格 `simulation_power`，先至少实现 distance/damping 的频率修正。
8. 把当前多轮 solver 改成固定序列：distance -> direction -> collider -> distance -> end velocity。
9. 只有当固定序列仍不稳定时，再增加内部质量档位，而不是恢复手动 substep。

这样用户最终只需要理解两个时间参数：

- Blender `dt`: 外部时间推进了多少。
- HoCloth `simulation_frequency`: 内部每秒模拟多少次。

`substep` 作为局部求解参数应该从交互层消失。

## 9. 当前开发规则

后续碰撞相关改动遵守下面几条规则：

1. 不再继续堆 contact lambda 参数。
2. 不再用 speculative position solve 提前抬链条。
3. 碰撞投影后必须立刻做骨长整理。
4. 接触修正是否进入速度，需要显式决定，不能隐式依赖 `previous_position`。
5. 静态碰撞必须保留骨段最近点检测，直到 HoCloth 有更完整的粒子化骨链表示。
6. 新增约束时必须说明它对 `velocity_position` 的影响比例，避免重新把 correction 变成隐式速度。
7. 旧 XPBD contact 代码不再保留兼容分支；如果需要新行为，直接在 MC2 风格数据流里实现。
8. 不再继续修补手动 `substep` 语义；后续应转向全局 fixed-step scheduler。

## 10. 一句话总结

HoCloth 的 SpringBone 碰撞不再以通用 XPBD contact lambda 为主线，而是转向 MagicaCloth2 风格的数据流和时间调度：固定 simulation frequency、参考位形、模拟位置、速度基准分离、碰撞局部投影、EndStep 统一速度结算。

## 11. 进一步对齐 MC2 的物理/碰撞数据流

当前最新 native 改动把范围从“只修碰撞投影”推进到“StartStep -> Constraint -> Collider -> Distance -> EndStep”的整体数据流：

- `RuntimeBoneState` 新增 `base_position`，对应 MC2 `basePosArray`，作为本 step 的动画参考位置。
- StartStep 不再用父骨速度、center force、root linear velocity 额外推预测位置；改为 old position + kinematic inertia offset + damping + gravity 的 MC2 风格预测。
- 阻尼从 EndStep 前移到 StartStep，EndStep 只负责根据 `position - velocity_position` 结算速度和接触摩擦，避免二次阻尼掩盖碰撞问题。
- 距离约束刚性改为 BoneSpring 固定值 `0.5 * SimulationPower.y`，并保持 `velocity_position += correction * 0.3`，对齐 MC2 `BoneSpringDistanceStiffness` 与 `DistanceVelocityAttenuation`。
- collider 保存 fixed step 的 previous/current pose，碰撞检测使用 MC2 moving-plane 思路：由上一姿态求法线，在当前姿态上构造推出平面。
- SpringBone 碰撞不再靠调小 push，而是在几何投影后使用 `base_position + limitDistance` 做 soft collider 限制，并按 MC2 的 `lerp(projected, old, softness)` 把推出压软。
- 摩擦衰减改为 MC2 的 `FrictionDampingRate = 0.6`。
- HoCloth 当前需要重新收束到 MC2 的 BoneSpring 碰撞模型：BoneSpring 以点粒子为核心，外部 capsule collider 只作为“点 vs 胶囊”的碰撞体参与检测，不应把骨段自身扩展成连续胶囊或虚拟采样链。下一步候选方案是补齐 MC2 `ColliderManager.WorkData` 对应的数据流：每个 fixed step 先生成 collider 的 old/next 端点、半径、swept AABB、old inverse rotation 与 current rotation，再由 `PointSphere/PointCapsule` 风格的旧姿态法线和当前姿态推出平面解决交叉。但这部分必须以更小补丁逐步接入，避免破坏当前“不抽搐”的主循环。

后续测试时优先观察：运动学碰撞体接触建立瞬间是否还会“整段硬推”、静态重叠是否能逐步脱出、贴着 collider 滑动时是否仍有明显能量注入。
