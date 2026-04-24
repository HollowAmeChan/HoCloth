# HoCloth 参考项目分析与实现方向

本文档保留参考阅读用途，但当前项目主线已经切换为：
- 物理内核对齐 `Magica Cloth 2`
- UI 与交互对齐 `Bonex`
- 当前优先实现 `MC2` 的 `BoneSpring` 数据流

## 参考项目

### `referenceproject/`
目录内保存两套参考源码：
- `Magica Cloth 2`：用于对照物理行为、参数组织和运行时节奏
- `Bonex`：用于对照 Blender 端 UI、交互和工作流

## 当前结论

- 项目大框架已经搭好
- 现阶段不是再抽象一套通用 XPBD 路线，而是逐项对照 `MC2` 的具体实现
- `BoneSpring` 已经开始拆分出 `position`、`previous_position`、`velocity_position` 这类状态
- 下一步重点是补细节：固定步节奏、碰撞体层级、骨长/方向约束顺序和 UI 组织

## 实现原则

- 先保证行为贴近，再考虑抽象复用
- 先把 `BoneSpring` 做透，再扩到 `ClothBone` 和 `MeshCloth`
- 参考源码用于对照实现，不做机械复制式重构

## 已踩过的坑

- 把碰撞当成通用 XPBD contact lambda 主线，容易把问题变成参数堆叠
- 把修正写回速度基准，会制造隐式能量注入和接触抖动
- 只盯尾端点做静态碰撞，会漏掉骨段中部与大半径碰撞
- 让 substep 承担时间管理，会让交互语义变得不稳定
