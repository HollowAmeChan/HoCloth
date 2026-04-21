#include "hocloth_runtime_api.hpp"

#include <unordered_map>

namespace hocloth {

namespace {

struct RuntimeSceneState {
    SceneDescriptor scene;
    std::uint64_t steps = 0;
};

std::unordered_map<SceneHandle, RuntimeSceneState> g_scenes;
SceneHandle g_next_handle = 1;

}  // namespace

SceneHandle build_scene(const SceneDescriptor& scene) {
    const SceneHandle handle = g_next_handle++;
    g_scenes.emplace(handle, RuntimeSceneState{scene, 0});
    return handle;
}

void destroy_scene(SceneHandle handle) {
    g_scenes.erase(handle);
}

void reset_scene(SceneHandle handle) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return;
    }
    it->second.steps = 0;
}

void step_scene(SceneHandle handle, const RuntimeInputs& inputs) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return;
    }

    const std::uint64_t step_increment = inputs.substeps > 0 ? static_cast<std::uint64_t>(inputs.substeps) : 1;
    it->second.steps += step_increment;
}

std::uint64_t get_step_count(SceneHandle handle) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return 0;
    }
    return it->second.steps;
}

std::vector<BoneTransform> get_bone_transforms(SceneHandle handle) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return {};
    }

    std::vector<BoneTransform> transforms;
    for (const BoneChainDescriptor& chain : it->second.scene.bone_chains) {
        for (const BoneDescriptor& bone : chain.bones) {
            BoneTransform transform;
            transform.component_id = chain.component_id;
            transform.bone_name = bone.name;
            transforms.push_back(transform);
        }
    }
    return transforms;
}

}  // namespace hocloth
