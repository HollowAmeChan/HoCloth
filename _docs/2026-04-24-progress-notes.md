# 2026-04-24 进度记录（修订版）

## 当前结论

- 项目主线已经从通用 XPBD contact 转向 MC2 BoneSpring 数据流
- runtime 已开始按固定步节奏组织，而不是依赖碰撞侧承担时间逻辑
- 碰撞改造重点已经改成骨段最近点、局部投影和碰撞后骨长整理

## 已完成

- 运行时接触路径从“contact lambda 主线”退回到数据流主线
- `position`、`previous_position`、`velocity_position` 的语义拆分已经开始
- 旧的 `warm_start_contact_constraints`、`solve_contact_constraints`、`contact lambda cache` 不再是主线

## 踩坑记录

- 把碰撞当成通用 XPBD 约束会迅速变成参数堆叠问题
- 把 correction 回写成速度会放大接触抖动
- 只检测尾端点会漏掉骨段中部碰撞
- 把 `substep` 当成主要交互参数会让行为不稳定

## 后续方向

- 继续推进 MC2 风格 BoneSpring 数据流
- 继续收紧固定步 scheduler
- 继续统一碰撞体的局部投影处理
- 继续减少对过时 XPBD contact 分支的依赖
