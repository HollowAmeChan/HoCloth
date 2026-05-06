# AI 须知

HoCloth 当前已经转向“完整移植 Magica Cloth 2 Core 到 C++ 后端，再接入 Blender”的路线。不要再把任务理解为继续修补一个简化 XPBD / BoneSpring MVP。

## 当前最高优先级

1. 参考源码在 `_ReferenceProject/MagicaCloth2/Scripts/Core`。
2. 先读 `_docs/MC2_Core完整移植重构方案.md` 和根目录 `工程大纲.md`。
3. C++ 侧要按 MC2 Core 的模块和管理器体系重建，不要继续把逻辑堆进少数 cpp 文件。
4. 旧 `_native/src/hocloth_*.cpp` 只作为历史桥接参考，不作为新架构基础。
5. Blender 侧已有 authoring、compile、runtime 桥接成果，优先保留并适配新 native backend。

## 移植规则

1. 直接移植或近似直译 MC2 代码时，必须在代码注释中标出来源文件。
2. 默认保持 MC2 行为，不主动做算法再设计。
3. 可以做 C# -> C++ 必需的语言适配、内存管理、数据布局和 Blender 接入边界调整。
4. Python / Blender 不应直接操作 solver 内部对象，只传 compiled data 和逐帧输入。
5. 每个 native 模块都应有 dump 或测试入口，不能只靠视口效果判断对错。

## 推荐推进顺序

1. 建立 MC2 Core 文件移植状态表。
2. 移植 `Define / Interface / Utility / ResultCode / Time / NativeCollection`。
3. 搭建 `MagicaManager / TeamManager / TransformManager / SimulationManager / ColliderManager / WindManager / ClothManager / VirtualMeshManager` 空壳。
4. 让 BoneSpring 作为第一条完整穿透链接入新 manager 和 constraint pipeline。
5. 再推进 ClothBone 和 MeshCloth。

## 不要做

- 不要继续扩展自研物理内核作为主线。
- 不要把 Blender 自定义属性直接当 native runtime 状态。
- 不要为了短期视口效果跳过 MC2 的 manager、virtual mesh、prebuild、constraint 数据流。
- 不要在没有 MC2 对照的情况下改写核心 XPBD 行为。
