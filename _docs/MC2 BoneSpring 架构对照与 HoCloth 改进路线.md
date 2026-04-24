# MC2 BoneSpring 架构对照与 HoCloth 改进路线

> 目标：先把 Magica Cloth 2 的 BoneSpring 数据流、步进节奏、约束分层和碰撞处理方式梳理清楚，再对照 HoCloth 当前实现，明确哪些已经完成、哪些还在坑里、哪些方向应该停止继续投入。

## 1. 先说结论

HoCloth 当前最值得继续推进的方向，不是“通用 XPBD 接触 lambda”，而是向 MC2 的 BoneSpring 风格收敛：

- 明确分离 `base_pos`、`next_pos`、`old_pos`、`velocity_pos`
- 用固定频率的 step 驱动，而不是让碰撞和约束去承担时间管理
- 约束修改位移，速度基准单独演化，避免把修正误写回速度
- 碰撞先做局部投影，再做骨链整理，而不是把碰撞当成单独的全局求解器
- 静态/运动学碰撞体要保留“最近点 / 段 / 面”层级，而不是只看尾端点

## 2. MC2 BoneSpring 的核心结构

这里的“BoneSpring”可以理解成一个分阶段的数据流，而不是单个约束器。

### 2.1 关键状态

MC2 风格里，至少要把这些状态拆开：

- `base_pos`：本帧参考位置，来自动画、骨架或外部输入
- `next_pos`：约束与碰撞后的模拟位置
- `old_pos`：上一帧最终位置，用于稳定积分和回溯
- `velocity_pos`：独立的速度基准，用于下帧速度推进

这四个状态的意义不同：

- `next_pos` 可以被约束和碰撞修改
- `velocity_pos` 负责保留动量与衰减
- `old_pos` 用来维持时间连续性
- `base_pos` 只负责给本帧提供参考，不应该直接被当成修正目标

### 2.2 典型步进节奏

更接近 MC2 的顺序应当是：

1. 采样基准姿态 / 外部输入
2. 计算惯性、重力、阻尼后的预测位移
3. 执行骨长 / 方向约束
4. 执行碰撞投影
5. 再做一次骨长 / 方向整理
6. 统一更新速度基准和输出姿态

这和“一个 XPBD solver 里塞很多 contact lambda”的思路不一样。

### 2.3 为什么它更稳

稳定来自数据分工，不来自参数堆叠：

- 约束只改 `next_pos`
- 速度只从 `velocity_pos` 更新
- 碰撞只在局部阶段介入
- 步进频率固定，避免 dt 漂移把行为放大

## 3. HoCloth 当前实现对照

### 3.1 已经对齐的部分

目前项目里已经开始靠近 MC2 的地方：

- runtime 已经有独立的 `position` / `previous_position` / `velocity_position` 语义分层
- 调度逻辑开始转向固定步频，而不是完全依赖手动 substep
- 碰撞从纯 XPBD contact lambda 方向回退，开始更重视骨段最近点检测
- 旧的 `warm_start_contact_constraints`、`solve_contact_constraints`、`contact lambda cache` 方向已经不再作为主线

### 3.2 仍然不够的部分

目前还缺的关键点：

- `base_pos` 语义还不够清晰，容易和 `position` / `previous_position` 混用
- 碰撞体数据流还没有完整对齐 MC2 的“旧姿态 / 当前姿态 / swept”概念
- 骨长、方向、碰撞的执行顺序还不够固定
- 现有文档里还有一些“通用 XPBD contact”倾向，需要改成踩坑结论

## 4. 我们不再继续走的方向

这些方向不是“完全错”，但对 HoCloth 当前这条线来说，继续投入性价比不高：

- 继续扩展通用 XPBD contact lambda 主线
- 继续依赖 speculative contact 解决稳定性
- 继续把碰撞反馈写回速度，造成隐式能量注入
- 继续只盯尾端点做静态碰撞检测
- 继续把 substep 当作主要交互参数

这些都已经踩过坑：它们能修一部分问题，但会引入调参地狱和局部抖动。

## 5. HoCloth 接下来应该做什么

建议按这个顺序推进：

1. 把 runtime 的状态命名和职责再收紧一次，明确 `base_pos`、`next_pos`、`old_pos`、`velocity_pos`
2. 统一固定步 scheduler，让外部 `dt` 只负责累计，不再直接驱动 solver 语义
3. 把骨长 / 方向约束整理成固定顺序的链式流程
4. 把静态与运动学碰撞体统一成“点 vs 段 / 点 vs 胶囊”的局部投影
5. 再把碰撞后的整理步骤补回骨链

## 6. 对旧文档的修正原则

后续 `_docs` 里的旧文档要统一做三类处理：

- 已完成的内容：改成“当前状态”
- 过时的方向：删除主线表述，改成“踩坑经验”
- 仍在推进的内容：明确标注“待实现”或“下一步”

## 7. 一句话总结

HoCloth 现在不该继续围绕“通用 XPBD contact”打转，而应该沿着 MC2 BoneSpring 的数据分层、固定步调度和局部碰撞投影继续收敛。
