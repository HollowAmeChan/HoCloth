# HoCloth 运动学碰撞体与 BoneSpring 接触改造记录

本文档记录的是碰撞改造的历史路径与踩坑结论。当前主线已经从“继续扩展 XPBD contact lambda”转向“对齐 Magica Cloth 2 的 BoneSpring 数据流”。

## 1. 旧问题回顾

Blender 中的碰撞体可以由动画强制驱动，它们在 runtime 中更接近 kinematic collider：会主动推链条，而不是被链条反推。

我们之前遇到过三类现象：

- 刚接触时整条链被瞬间抬直
- 初始重叠时被强推出去并获得很大速度
- 修完大问题后，接触建立瞬间仍会出现小范围抽搐

## 2. 走过的 XPBD Contact Lambda 路线

我们尝试过把碰撞当作 XPBD contact constraint 放进 solver，并叠加：

- speculative contact
- warm start
- contact lambda cache
- 多采样接触
- 接触滞回
- 表面速度阻尼

这些方案能缓解一部分问题，但很容易掉进调参地狱。根本原因是 HoCloth 的 spring bone 链条不是通用刚体系统：单点局部接触修正会被骨长约束和父子链传播放大，最后变成整段姿态异常或接触处反复抖动。

因此当前不再把“完整 XPBD contact lambda”作为主线。

## 3. MC2 给出的关键启发

MagicaCloth2 的 BoneSpring 稳定，不是因为某个阻尼参数神奇，而是因为它的数据流不同。

它至少维护了几类分离状态：

- `base_pos`
- `next_pos`
- `old_pos`
- `velocity_pos`

这里的关键不是命名，而是职责：

- 约束可以修改 `next_pos`
- 速度基准独立演化
- 碰撞只在局部阶段介入
- 速度修正不能和位移修正混在一起

## 4. 当前 HoCloth 的改造方向

当前方向是保留已经开始形成的 spring 预判，但把碰撞处理改成更接近 MC2：

1. 先做 spring / inertia / gravity 预测
2. 再做骨长和方向约束
3. 再做局部碰撞投影
4. 碰撞后补一次骨长和方向整理
5. 不再依赖 `warm_start_contact_constraints` / `solve_contact_constraints` / `contact lambda cache` 这条线

## 5. 已落地的状态拆分

runtime 状态已经开始从单一 `previous_position` 推导，拆成更接近 MC2 的三类位置：

- `position`：当前模拟位置，类比 MC2 `nextPos`
- `previous_position`：上一帧确定位置，类比 MC2 `oldPos`
- `velocity_position`：独立速度基准，类比 MC2 `velocityPos`

同时调度逻辑也已经在往固定步走：

- `simulation_frequency` 决定内部频率
- `dt` 只负责累计外部时间
- `substep` 不应继续作为主交互参数

## 6. 目前踩过的坑

- 把碰撞当成通用 XPBD contact lambda 主线，容易把问题变成参数堆叠
- 把修正写回速度基准，会制造隐式能量注入和接触抖动
- 只盯尾端点做静态碰撞，会漏掉骨段中部与大半径碰撞
- 让 `substep` 承担时间管理，会让交互语义变得不稳定

## 7. 当前开发规则

后续碰撞相关改动遵守下面几条：

1. 不再继续堆 `contact lambda` 参数
2. 不再把 speculative position solve 当主线
3. 碰撞投影后必须立刻做骨长整理
4. 接触修正是否进入速度，需要显式决定，不能隐式依赖 `previous_position`
5. 静态碰撞必须保留骨段最近点检测，直到 HoCloth 有更完整的粒子化骨链表示
6. 新增约束必须说明对 `velocity_position` 的影响比例，避免把 correction 重新变成隐式速度
7. 如果需要新行为，优先在 MC2 风格数据流里实现，不再保留 XPBD contact 兼容分支
8. 不再继续补手动 `substep` 语义，后续转向全局 fixed-step scheduler

## 8. 一句话总结

碰撞这条线已经从“用更多 contact 参数修补”转向“按 MC2 BoneSpring 数据流重构”，后续只保留能服务这条主线的内容。
