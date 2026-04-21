# HoCloth Runtime MVP 状态说明

本文档说明当前 `Build Runtime -> Step Runtime -> Apply Pose` 这条最小闭环已经做到什么程度，以及各目录/文件在这条链路中的职责。

## 当前已经打通的链路

1. `authoring/`
   Blender 面板、操作器、骨链提取入口。

2. `components/`
   保存作者态数据。
   当前骨链组件只保存：
   - armature 对象引用
   - root bone 名称

3. `compile/`
   把作者态组件编译成稳定的中间数据。
   当前会输出：
   - bone chain 列表
   - 每条 chain 的骨骼层级
   - root bone
   - armature 名称

4. `runtime/`
   负责 Python 侧 runtime 会话管理、native bridge、以及把 transform 回写到 Blender pose。
   当前新增了：
   - `session.py`：保存当前 runtime handle、compiled scene、最近一次 transform
   - `pose_apply.py`：把 runtime transform 应用到 pose bones

5. `_native/`
   负责 C++ runtime。
   当前 native/stub 都会返回骨骼 transform，并且 transform 里已经带上 `armature_name`，方便 Python 侧精确定位回写目标。

## 当前按钮的实际含义

- `Build Runtime`
  重新从 scene 中提取组件，编译出 compiled scene，并创建 runtime handle。

- `Step Runtime`
  让 runtime 往前推进一步。
  如果 `Apply Pose On Step` 开启，会自动把本次得到的 transform 回写到 Blender 骨骼。

- `Apply Runtime Pose`
  不推进 simulation，只把最近一次 runtime 返回的 transform 手动再应用一次。

- `Reset Runtime`
  重置 runtime 内部步数和临时状态，但不重建结构。

- `Destroy Runtime`
  销毁当前 runtime handle。

## 当前 placeholder 的意义

目前还没有把真正的 PhysX articulation 绑定进去，所以 native/stub 会输出一个轻微的占位摆动。

这样做不是为了假装已经有物理，而是为了先验证这条主线是否成立：

`骨链提取 -> compiled scene -> native bridge -> transform 返回 -> pose 回写`

只要这条链路稳定，后面替换成真实 PhysX 输出时，Blender 侧大部分代码都不用重写。

## 现在 physics 绑定应该接在哪里

下一步真正的 physics 绑定，主要进入 `_native/`：

- 把 `CompiledBoneChain` 构造成 articulation/rigid body 运行时对象
- 用 root/animated target 驱动 runtime 输入
- 在 `step_scene()` 里执行真实物理解算
- 在 `get_bone_transforms()` 里返回真实骨骼结果

Python 侧这时主要只需要继续做三件事：

- 更稳地把 authoring 数据编译成 native 需要的输入
- 把 runtime 输入参数喂给 native
- 把 native 输出回写到 Blender

## 当前目录在 MVP 中的功能分工

- `authoring/`
  用户交互层。

- `components/`
  作者态持久化层。

- `compile/`
  作者态到运行态输入格式的转换层。

- `runtime/`
  Python 侧运行时桥接层。

- `_native/`
  C++ 运行时实现层。

- `_bin/`
  Blender 实际加载的 native 二进制目录。

- `_build/`
  预览导出和本地打包过程中的临时目录。

- `_dist/`
  最终 zip 发布包输出目录。

## 你接下来最值得验证的事

1. 重新编译 `_native`，确保新的 `armature_name` 和占位摆动已经进入 `_bin/`
2. 在 Blender 里重载插件
3. `Build Runtime`
4. 勾选 `Apply Pose On Step`
5. 连续点击 `Step Runtime`

如果骨链开始出现轻微摆动，说明当前 MVP 主线已经不是“只返回 transform 数量”，而是真正完成了回写闭环。
