#include "hocloth_runtime_api.hpp"
#include "hocloth_physx_builder.hpp"

#include <cmath>
#include <unordered_map>

namespace hocloth {

namespace {

struct RuntimeSceneState {
    SceneDescriptor scene;
    RuntimeInputs inputs;
    std::string backend = "stub";
    std::uint64_t steps = 0;
};

std::unordered_map<SceneHandle, RuntimeSceneState> g_scenes;
SceneHandle g_next_handle = 1;


}  // namespace

SceneHandle build_scene(const SceneDescriptor& scene) {
    const PhysxBuildResult physx = probe_physx_backend();
    const SceneHandle handle = g_next_handle++;
    g_scenes.emplace(handle, RuntimeSceneState{scene, RuntimeInputs{}, physx.backend, 0});
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

void set_runtime_inputs(SceneHandle handle, const RuntimeInputs& inputs) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return;
    }
    it->second.inputs = inputs;
}

void step_scene(SceneHandle handle, const RuntimeInputs& inputs) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return;
    }

    if (!inputs.bone_chains.empty()) {
        it->second.inputs = inputs;
    } else {
        it->second.inputs.dt = inputs.dt;
        it->second.inputs.substeps = inputs.substeps;
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
        for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
            const BoneDescriptor& bone = chain.bones[bone_index];
            (void) bone;
            const float angle = 0.015f * std::sin(static_cast<float>(it->second.steps) * 0.08f + static_cast<float>(bone_index) * 0.3f);
            const float half_angle = angle * 0.5f;
            BoneTransform transform;
            transform.component_id = chain.component_id;
            transform.armature_name = chain.armature_name;
            transform.bone_name = bone.name;
            // Placeholder mode outputs pose-space delta rotation, not absolute
            // local orientation, so Blender can layer it on top of rest pose.
            transform.rotation_quaternion[0] = std::cos(half_angle);
            transform.rotation_quaternion[1] = std::sin(half_angle);
            transform.rotation_quaternion[2] = 0.0f;
            transform.rotation_quaternion[3] = 0.0f;
            transforms.push_back(transform);
        }
    }
    return transforms;
}

RuntimeSceneInfo get_scene_info(SceneHandle handle) {
    RuntimeSceneInfo info;
    info.handle = handle;

    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return info;
    }

    info.backend = it->second.backend;
    info.step_count = it->second.steps;
    return info;
}

}  // namespace hocloth
