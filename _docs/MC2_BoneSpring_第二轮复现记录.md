# MC2 BoneSpring 第二轮复现记录

## 1. 这一轮做了什么

这一轮没有去扩展 `MC2` 之外的新功能，而是继续补 `BoneSpring` 里上一轮还没打通的 `SpringConstraint` 主链路。

本轮完成的内容：

- 在 Blender 侧补上 `MC2 SpringConstraint` 对应参数：
  - `useSpring`
  - `springPower`
  - `limitDistance`
  - `normalLimitRatio`
  - `springNoise`
- 把这组参数接入：
  - `components/properties.py`
  - `authoring/panel.py`
  - `authoring/operators.py`
  - `compile/compiled.py`
  - `compile/compiler.py`
- 把这组参数继续传到 native：
  - `_native/include/hocloth_runtime_api.hpp`
  - `_native/src/hocloth_python_module.cpp`
- 在 `_native/src/hocloth_runtime_api.cpp` 中补入一段新的 `ApplyMc2SpringConstraint(...)`

---

## 2. 这一轮直接参考了哪些 MC2 代码

本轮直接对照的主要来源：

- `MagicaCloth2/Scripts/Core/Cloth/Constraints/SpringConstraint.cs`
- `MagicaCloth2/Scripts/Core/Manager/Simulation/SimulationManager.cs`

其中真正影响行为的关键不是只有 `SpringConstraint.cs` 的参数定义，而是 `SimulationManager.Spring(...)` 里的实际求解逻辑。

这一轮 native 里新增的弹簧夹紧和回弹处理，就是按这段逻辑翻进来的，并在代码里标了 `From MC2` 注释。

---

## 3. 这轮实际落地的行为

### 3.1 Spring 开关

现在 `BoneSpring` 已经可以显式打开或关闭 `MC2` 风格 spring 段。

- `useSpring = false` 时，native 不执行这段额外的 spring 收缩逻辑
- `useSpring = true` 且 `springPower > 0` 时，native 会执行 spring 约束

### 3.2 距离夹紧

参考 `SimulationManager.Spring(...)`，当前已经补入：

- 从原点出发的最大移动距离 `limitDistance`
- 超限时的球形夹紧

### 3.3 法线方向限制

参考 `normalLimitRatio` 的处理，当前已经补入一个与现有角度空间相匹配的近似版本：

- 先分离“主平面分量”和“法线方向分量”
- 再按 `normalLimitRatio` 收窄法线方向的可动范围

当前实现仍是“映射到 HoCloth 当前角度表达后的近似复现”，还不是 MC2 粒子空间里的逐项原样实现。

### 3.4 Spring Noise

参考 `SimulationManager.Spring(...)`，当前已经补入：

- `sin(noiseTime)` 形式的 spring power 扰动
- `springNoise * 0.6` 的缩放思路
- 对每个 joint 做时间错相，避免完全同步

### 3.5 BoneSpring 仍按 MC2 思路不以 gravity 为主驱动

上一轮已经确认 `ClothSerializeDataFunction.cs` 里 `BoneSpring` 的 `gravity = 0`。

这一轮没有把 `gravity_strength` 重新塞回 native 主求解，而是继续保持：

- UI 上保留参数，便于后面继续对照或做兼容
- MC2 风格 native 主线仍不依赖它作为核心驱动力

---

## 4. 为什么这轮先补这块

因为上一轮虽然效果已经很接近 `MC2`，但还主要是：

- `TimeManager`
- `Distance`
- `Tether`
- `ColliderCollision`

这几段组合出来的“味道接近”版本。

而 `SpringConstraint` 正是 `BoneSpring` 名字里“Spring”的直接来源之一。

如果这一段不补，后续会出现两个问题：

- 参数层面已经不像 `MC2 BoneSpring`
- 行为层面会继续停留在“接近，但不是同一路实现”

所以这一轮优先把这段补进去，比继续扩新功能更重要。

---

## 5. 当前 BoneSpring 还没做完的部分

虽然这轮已经把 `SpringConstraint` 主链打通，但现在仍然不能说 `MC2 BoneSpring` 已经完全搬完。

还剩下的重点包括：

- `ColliderCollisionConstraint.cs` 里更细的 spring 特化行为
- 当前 HoCloth 角度空间与 MC2 粒子空间之间的表达差异
- 更细的中心骨 / 惯性 / 速度影响映射
- 参数默认值、预设值与实际 Unity 手感的继续对表

也就是说，当前状态更适合定义为：

`MC2 BoneSpring` 第二轮继续收敛中，已经从“像 MC2”进一步推进到“开始直接使用 MC2 SpringConstraint 思路”。

---

## 6. 下一轮建议

在继续只做 `MC2` 内容的前提下，下一轮建议优先顺序如下：

1. 继续补 `BoneSpring` 的 `ColliderCollisionConstraint` 细节
2. 继续检查 `BoneSpring` 是否还有直接决定手感的 C# 段未搬
3. 确认 `BoneSpring` 基本完成后，再进入 `ClothBone`

当前仍然不建议开始做：

- `MC2` 没有的 HoCloth 自定义物理功能
- 为了“结构更漂亮”而提前大改当前 native 骨架
