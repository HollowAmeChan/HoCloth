# MC2 BoneSpring 第三轮复现记录

## 1. 本轮目标

这一轮继续只做 `MC2 BoneSpring`，没有扩到 `MC2` 之外的新功能。

重点做了两件事：

1. 把 BoneSpring UI 里开始出现的“MC2 裸参数散落在根面板”的问题收回来，改成更接近 `MC2 SerializeData / Constraint` 的分组结构。
2. 继续补 `ColliderCollisionConstraint` 里 `BoneSpring` 专有的一段逻辑：碰撞推离后，粒子离原点的距离不能无限增大。

---

## 2. 这一轮搬了哪些 MC2 思路

### 2.1 参数封装思路

参考了：

- `CheckSliderSerializeData.cs`
- `CurveSerializeData.cs`
- `SpringConstraint.cs`
- `ColliderCollisionConstraint.cs`

虽然 Blender 侧还没有完整照搬 `MC2` 的所有曲线编辑器和 drawer，但这一轮已经开始按 `MC2` 的“约束参数分组”来组织数据，而不是继续把参数都平铺在 `HoClothSpringBoneComponent` 根层。

### 2.2 碰撞推离上限

参考了：

- `ColliderCollisionConstraint.cs`

尤其是这段：

- `BoneSpring` 被 collider 推开时，存在 `limitDistance`
- 这个距离是“从原点出发的最大推离距离”
- `BoneSpring` 下碰撞摩擦使用固定常量 / 特化路径

本轮 native 已把“推离后再做最大距离夹紧”的主逻辑补进去。

---

## 3. 这一轮实际改了什么

### 3.1 BoneSpring UI 开始从平铺参数改成约束分组

当前 `HoClothSpringBoneComponent` 内已经新增：

- `spring_constraint`
- `collider_collision_constraint`

其中：

- `spring_constraint` 管理：
  - `useSpring`
  - `springPower`
  - `limitDistance`
  - `normalLimitRatio`
  - `springNoise`
- `collider_collision_constraint` 管理：
  - `friction`
  - `limitDistance`

这让 UI 结构开始更像 `MC2`，而不是把所有参数都直接堆在同一层。

### 3.2 预设和编译链已改用新分组

这轮不是只改了 UI 外观。

以下链路已经切到新分组：

- preset 写入
- compiled scene 输出
- native binding 解析
- native runtime 使用

所以这次不是“换个面板摆放”，而是数据结构真的往 `MC2` 的组织方式靠了一步。

### 3.3 native 补上 BoneSpring 碰撞推离上限

在 `_native/src/hocloth_runtime_api.cpp` 中：

- 继续保留当前 HoCloth 的 angle-space 近似碰撞响应
- 在每次碰撞推离后，新增一次“离原点最大距离”限制

这段的目标是对齐 `MC2 ColliderCollisionConstraint` 里 `BoneSpring` 的：

- `maxLength`
- `PointSphereColliderDetection(..., isSpring, maxLength, ...)`

当前仍然是映射到 HoCloth 现有角度空间之后的近似实现，不是 MC2 粒子世界坐标里的完全同构版本，但逻辑方向已经一致。

---

## 4. 为什么这一轮先做 UI 整理

因为继续往下搬 `MC2` 代码时，如果 UI 和 authoring 数据还保持“旧 HoCloth 平铺参数 + 新 MC2 裸参数混写”的状态，会越来越难维护：

- 不利于判断哪些参数已经是 `MC2`，哪些还是旧口径
- 不利于后续继续搬 `MotionConstraint / ColliderCollisionConstraint / ClothBone`
- 不利于预设和文档逐轮对齐

所以这一轮先把最先落地的两个约束分组立起来，是为了让后续继续照搬 `MC2` 时不至于越搬越乱。

---

## 5. 当前还没搬完的重点

即使做完这一轮，`MC2 BoneSpring` 也还没有结束。

当前还值得继续补的重点包括：

- `ColliderCollisionConstraint` 更完整的软碰撞行为
- `MotionConstraint` 虽然在 `MC2` 中对 `BoneSpring` 大多关闭，但相关禁用逻辑和 UI 约束关系还可以继续对齐
- `CurveSerializeData` 目前还只是轻量近似包装，没有完整曲线编辑能力
- Python stub fallback 仍然落后于 native 主线

---

## 6. 下一轮建议

下一轮仍建议只做 `MC2 BoneSpring`，优先顺序如下：

1. 继续补 `ColliderCollisionConstraint` 的 BoneSpring 细节
2. 让 Python stub 至少不明显落后于当前 native 行为
3. 继续把 UI/authoring 结构向 `MC2` 的约束分组推进

仍然不建议开始做：

- `MC2` 没有的 HoCloth 自定义特性
- 为了抽象整洁而提前重写现在这条已验证有效的复现路线

---

## 7. 后续补记

在第三轮之后又继续做了两处重要修正，和当前 BoneSpring 手感直接相关：

### 7.1 `center = NONE` 时不再退回世界原点

之前 runtime 输入里如果没有显式 center，会把 center 当成 `(0, 0, 0)`。

这会导致：

- `center - root` 从第一帧开始就出现错误偏移
- BoneSpring 在“开始模拟时静止不动”这件事上偏离 `MC2`

现在已经改成：

- 没有显式 center 时，直接退回当前 root transform

这更符合当前 `MC2 BoneSpring` 需要的静止起始状态。

### 7.2 native 开始接入 root / center 旋转增量

之前当前 native 主要只利用了：

- center 平移
- center 速度

但 `MC2` 的 `Spring(...)` 明确依赖 `baseRot` 来定义 spring 限制方向。

现在 native 已经开始记录并使用：

- root rotation delta
- center rotation delta

把它们折算成 BoneSpring 的目标偏移，改善“旋转时效果远不如 MC2”的问题。

这还不是 `MC2 InertiaConstraint` 的全量移植，但已经补上了一条之前明显缺失的主干。

### 7.3 CurveSerializeData 暂时只保留接口

根据当前阶段目标，`CurveSerializeData` 暂时只保留数据接口：

- 保留 `use_curve`
- 保留 `value`
- 不继续扩 Blender 侧曲线编辑 UI

等后面专门设计曲线数据结构时再统一实现。
