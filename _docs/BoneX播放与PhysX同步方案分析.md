# BoneX 播放与 PhysX 同步方案分析

这份文档用于总结 `blender_bonex` 在 Blender 动画播放期间，如何启动 PhysX、如何逐帧同步，以及它对 HoCloth 有哪些可直接借鉴的设计。

目标不是照搬 BoneX 的全部实现，而是把它的播放驱动思路拆成可以复用的工程要点。

bonex的本机参考路径在 `C:\Users\Hollow\AppData\Roaming\Blender Foundation\Blender\4.5\scripts\addons\blender_bonex`

## 1. 总体结论

BoneX 的播放同步方案有一个非常明确的分层：

- UI 层只负责暴露开关，比如 `Playback Simulate`
- 生命周期由 Blender handlers 驱动，而不是靠面板按钮自己维护一条长循环
- 物理解算进程的启动和停止，分别绑定到“动画开始播放”和“动画播放结束”
- 每一帧的数据交换绑定到 `frame_change_post`
- 物理输出不直接写回 pose bone，而是先写到 driver empty，再由骨骼约束跟随 driver

这套设计的核心优点是：

- 播放开始和停止的时机很稳定
- 物理同步跟 Blender 时间线天然对齐
- UI 不需要自己猜“现在是不是还在播放”
- 物理结果和骨骼本体解耦，回写风险更低

## 2. 入口结构

BoneX 在注册插件时就把播放相关 handlers 挂上去。

关键入口：

- `__init__.py`
  - 注册时调用 `handler.register_handlers()`
- `panel/handler.py`
  - `register_handlers()`
  - `unregister_handlers()`

对应代码位置：

- `blender_bonex/__init__.py:18-35`
- `blender_bonex/panel/handler.py:25-63`

BoneX 注册了四类 handler：

- `animation_playback_pre`
- `animation_playback_post`
- `frame_change_pre`
- `frame_change_post`

实际真正承担逻辑的是三段：

- `my_pre_animation_handler`
- `my_post_animation_handler`
- `my_post_frame_change_handler`

## 3. 播放开始时做什么

BoneX 在 `animation_playback_pre` 里检查用户是否开启了 `is_playback_simulate`，如果开启，就立即启动 PhysX 播放模式。

关键代码：

- `blender_bonex/panel/handler.py:66-73`

它的逻辑很简单：

1. 检查设置 `settings.is_playback_simulate`
2. 记录日志
3. 调用 `start_physx_playback()`

这意味着 BoneX 不是“按下某个播放按钮后自己循环 step”，而是等 Blender 真正进入播放状态后，再启动 PhysX。

这是它比很多 timer 方案更稳定的地方。

## 4. 播放模式如何初始化 PhysX

`start_physx_playback()` 是 BoneX 播放链路里最关键的初始化函数。

关键代码：

- `blender_bonex/panel/handler.py:206-275`

它做了这些事：

1. 检查 PhysX 可执行文件是否存在
2. 先调用 `stop_physx()`，确保旧进程一定被清掉
3. 把 `scene.sync_mode` 强制设为 `"NONE"`
4. 调用 `get_physx_init_data(is_playback_enable=True, is_bake=False)` 收集初始数据
5. 如果没有 rigidbody，直接终止
6. 启动外部 PhysX 进程
7. 把初始化 JSON 一次性写进进程 stdin
8. 调用 `utils.goto_frame(initial_variables["start_frame"])`

这里有两个非常值得 HoCloth 学的点。

### 4.1 它显式要求“播放每一帧”

BoneX 在播放开始时直接把：

- `bpy.context.scene.sync_mode = "NONE"`

写死。

这本质上是在要求 Blender 进入 “Play Every Frame” 的语义，避免实时播放时跳帧。

原因很直接：

- BoneX 的物理解算是按帧严格交换数据的
- 如果 Blender 播放时跳帧，PhysX 返回的帧号就会和 Blender 当前帧错位
- 一旦错位，播放链路就不再可信

这也是它后面会检查“帧是否连续”的原因。

### 4.2 初始化数据是完整 scene，而不是每帧重建

`get_physx_init_data()` 会把下面这些内容一次性打包发给 PhysX：

- rigidbodys
- joints
- scale
- sub_step_num
- gravity
- fps
- start_frame
- end_frame
- ground enable

关键代码：

- `blender_bonex/panel/handler.py:463-501`

这和 HoCloth 工程大纲里“结构数据只在 build 阶段进入 runtime，运行时只接受高频输入”的思路是一致的。

## 5. 每帧同步如何触发

BoneX 不是自己在 timer 里按固定时间步推进播放同步，而是把逐帧交换绑到：

- `frame_change_post`

关键代码：

- `blender_bonex/panel/handler.py:98-118`

逻辑是：

1. 先检查 PhysX 进程是否仍在运行
2. 读取当前 `frame_current`
3. 用 `last_processed_frame` 去重，避免多视图层或重复 handler 调用造成一帧执行多次
4. 调用 `get_data_from_physx_and_update_back()`

这里 BoneX 有一个很有价值的细节：

- 它没有假设 `frame_change_post` 每帧只调用一次
- 它用 `id(g.physx_proc) + 当前帧号` 作为 identity 去重

这对 Blender handler 来说很实用，因为多视图层、多次刷新时确实可能重复触发。

## 6. 每帧交换的数据内容

在 `get_data_from_physx_and_update_back()` 里，BoneX 每帧发给 PhysX 的不是整套 scene，而是高频输入：

- 当前帧号
- kinematic rigidbody 数据
- force field 数据

关键代码：

- `blender_bonex/panel/handler.py:121-139`
- `blender_bonex/panel/handler.py:1233-1266`

发送数据的形状大致是：

```json
{
  "frame": 当前帧,
  "kinematic_rigidbodys": {...},
  "force_field_data": [...]
}
```

这和 HoCloth 当前 `build_runtime_inputs()` 的思路很像：

- 每帧发送 root / target / force 这类高频输入
- 不在每帧重新构建骨链结构

## 7. 它如何等待并读取 PhysX 结果

BoneX 的每帧同步是同步阻塞式的。

关键代码：

- `blender_bonex/panel/handler.py:141-165`
- `blender_bonex/panel/handler.py:1303-1318`

做法是：

1. 向 PhysX 进程写入当前帧输入
2. 循环读取 stdout
3. 如果读到的是字符串消息，就记日志继续等
4. 如果读到的是合法 JSON 且是 frame output，就退出循环

这个设计说明 BoneX 的播放同步模型是：

- Blender 帧变化驱动一次 PhysX 交换
- 本帧必须拿到结果，才继续应用

优点是时序简单。

代价是：

- 如果 C++ 侧返回慢，Blender 播放会卡
- 所以它非常依赖“播放每一帧”和“结果及时返回”

## 8. 它如何检查帧是否连续

BoneX 有一段非常关键的保护逻辑：检查 Blender 当前帧和 PhysX 返回帧是否一致。

关键代码：

- `blender_bonex/panel/handler.py:167-183`

它的策略是：

- 正常情况下，`current_frame` 必须等于 `physx_output_frame`
- 如果场景有多个 `view_layer`，允许一个 1 帧误差
- 超出容忍度就直接报错，提示播放帧不连续

对应的提示文案里还直接提醒用户去检查是否启用了：

- “Play Every Frame”

这说明 BoneX 把“帧连续”视为播放物理同步成立的前提，而不是可选优化。

对 HoCloth 来说，这一点非常重要。

如果 HoCloth 后面也走 handler 驱动路线，建议明确加入：

- 帧连续性检查
- 对跳帧场景的硬性报错或自动停止

## 9. 它如何把结果回写到 Blender

BoneX 的回写设计不是直接写 pose bone，而是：

1. PhysX 输出刚体位置和旋转
2. 把结果写到对应的 driver empty
3. pose bone 通过 `COPY_TRANSFORMS` 约束跟随这个 driver

关键代码：

- `blender_bonex/utils/utils.py:1175-1225`
- `blender_bonex/panel/handler.py:655-737`

在 `set_transform_from_data()` 里：

- 它根据 rigidbody identity 找到 armature 和 bone
- 找到 driver object
- 更新 `driver_obj.location`
- 更新 `driver_obj.rotation_quaternion`

这个模式的优点很明确：

- 不直接把求解结果硬写回 pose channel
- 允许用户通过约束链再做限制、混合或开关
- 骨骼动画和物理层之间有一个中间缓冲层

这也是 BoneX 稳定性的一个关键来源。

HoCloth 当前是直接写 `pose_bone.location` 和 `pose_bone.rotation_quaternion`，这比 BoneX 更直接，但也更容易出现：

- 约束打架
- connected bone 偏移问题
- pose baseline 累积误差

## 10. 播放结束时如何收尾

BoneX 在 `animation_playback_post` 中统一停止 PhysX。

关键代码：

- `blender_bonex/panel/handler.py:76-91`

播放结束时它会：

1. 检查 PhysX 是否还在运行
2. 记录“播放结束，停止 PhysX”
3. 调用 `stop_physx()`
4. 刷新 UI
5. 如果播放预览期间积累了临时 keyframe 结果，就统一写回

这说明 BoneX 把“播放结束”视为一个明确的生命周期边界，而不是仅仅停止 step。

## 11. `stop_physx()` 的设计特点

关键代码：

- `blender_bonex/panel/handler.py:277-315`

它的停止流程是：

1. 如果有进程，先 `kill()`
2. `wait()` 确保进程真正退出
3. 清空全局状态
4. 重置 `last_processed_frame`
5. 弹出停止提示
6. 如果 Blender 还在播放，则调用 `bpy.ops.screen.animation_cancel(restore_frame=False)`

这说明 BoneX 的停止是“双向”的：

- Blender 播放结束会停 PhysX
- PhysX 停止时也会反过来停 Blender 播放

这一点比 HoCloth 当前的 live timer 更完整，因为它能保证两边生命周期严格对齐。

## 12. BoneX 的 Bake 路线和 Playback 路线是分开的

BoneX 没有强行用一套逻辑同时处理 playback 和 bake，而是分成两条路：

- Playback：handler 驱动，逐帧实时交换
- Bake：modal operator 驱动，定时读取结果，最终一次性回填

关键代码：

- `blender_bonex/panel/op.py:447-521`
- `blender_bonex/panel/handler.py:1269-1300`

Bake 模式的特征是：

- 用 modal timer 保持 UI 可响应
- 从 PhysX 取回整段 `frame_datas`
- 再统一设置关键帧

而 Playback 模式强调的是：

- 直接跟 Blender 播放绑定
- 一帧一交换
- 一帧一预览

这对 HoCloth 很有启发：

- 连续预览和离线烘焙最好不要混成一个循环
- 两条路线共享数据格式和 runtime API，但不共享完全相同的调度策略

## 13. 对 HoCloth 最值得借鉴的点

如果只看“动画播放与物理同步”的机制，BoneX 最值得 HoCloth 学的不是外部进程，而是下面这些调度原则。

### 13.1 用 Blender playback handlers 驱动生命周期

建议优先考虑：

- `animation_playback_pre` 负责启动 runtime
- `animation_playback_post` 负责停止 runtime
- `frame_change_post` 负责每帧输入/step/回写

而不是把“播放状态检测”和“逐帧 step”全部堆在 `bpy.app.timers` 上。

### 13.2 把“播放每一帧”当作前提条件

BoneX 明确要求帧连续，否则同步失真。

HoCloth 如果要做可靠的实时预览，建议至少：

- 在启动 live 模式时切换到 `scene.sync_mode = "NONE"`
- 检查帧是否连续
- 发现跳帧就自动停止，并给出明确提示

### 13.3 明确区分 build 数据和 per-frame 输入

BoneX 的 `get_physx_init_data()` 和每帧 `next_frame_data` 是分开的。

HoCloth 也应该继续坚持：

- build 阶段：骨链结构、rest pose、关节配置、碰撞体
- runtime 阶段：root transform、kinematic target、外力、dt

### 13.4 回写层最好与骨骼本体解耦

BoneX 用 driver empty 做缓冲层，这是它很成熟的一点。

HoCloth 后面如果直接写 pose 继续不稳定，可以评估两种路线：

- 继续保留直接写 pose，但加强 baseline / local delta / connected bone 规则
- 引入 HoCloth 自己的 driver proxy 层，让骨骼通过约束跟随 runtime 输出

### 13.5 停止逻辑必须双向一致

BoneX 的经验是：

- 播放停了，runtime 必须停
- runtime 崩了或结束了，播放也要停

HoCloth 如果只是一边停止，另一边继续跑，很容易留下：

- timer 残留
- scene 状态脏掉
- 多次启动后 step 叠加

## 14. HoCloth 当前实现和 BoneX 的主要差异

截至目前，HoCloth 的连续运行更像是：

- 一个 timer 驱动的 live loop
- 检查 `screen.is_animation_playing`
- 然后 `step_runtime + apply_runtime_pose`

而 BoneX 更像是：

- 播放生命周期由 handlers 驱动
- frame change 是唯一的逐帧入口
- runtime 是播放系统的一部分，而不是附属轮询器

两者相比，BoneX 的方案在“和 Blender 时间线的耦合”上更深，因此：

- 触发更准
- 停止更准
- 帧连续性更容易校验

## 15. 对 HoCloth 的落地建议

基于 BoneX 的实现，HoCloth 可以考虑按这个顺序演进。

### 第一阶段：先把 live loop 改成 handler 驱动

建议目标：

- `animation_playback_pre` 自动 start runtime live session
- `animation_playback_post` 自动 stop runtime live session
- `frame_change_post` 里做一次 `build_runtime_inputs -> step_runtime -> apply`

### 第二阶段：补上帧连续性保护

建议增加：

- 记录上次处理帧
- 如果当前帧不是上一帧的相邻帧，就停止 live runtime
- UI 中明确提示用户启用“逐帧播放”

### 第三阶段：评估是否引入 driver proxy 层

如果 HoCloth 继续出现：

- 骨骼扭转
- pose baseline 抖动
- 约束和直接写回冲突

那么可以考虑把 runtime 输出先写到 proxy object，再由骨骼约束跟随。

## 16. 参考代码位置

BoneX 中与本文最相关的代码位置如下：

- `blender_bonex/__init__.py:18-35`
- `blender_bonex/panel/handler.py:25-63`
- `blender_bonex/panel/handler.py:66-91`
- `blender_bonex/panel/handler.py:98-118`
- `blender_bonex/panel/handler.py:121-196`
- `blender_bonex/panel/handler.py:206-275`
- `blender_bonex/panel/handler.py:277-330`
- `blender_bonex/panel/handler.py:463-586`
- `blender_bonex/panel/handler.py:605-737`
- `blender_bonex/panel/op.py:447-521`
- `blender_bonex/panel/ui.py:11-20`
- `blender_bonex/panel/ui.py:255-304`
- `blender_bonex/utils/utils.py:512-539`
- `blender_bonex/utils/utils.py:1175-1225`

## 17. 一句话总结

BoneX 的关键不是“播放时一直 step”，而是：

- 用 Blender 的播放事件作为 runtime 生命周期边界
- 用 `frame_change_post` 作为唯一逐帧同步入口
- 用 driver object 作为物理解算结果和骨骼姿态之间的缓冲层

这三点，是 HoCloth 后面最值得系统性吸收的部分。
