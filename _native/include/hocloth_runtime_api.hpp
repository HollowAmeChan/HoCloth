#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hocloth {

using SceneHandle = std::uint64_t;

struct BoneDescriptor {
    std::string name;
    std::int32_t parent_index = -1;
    float length = 0.0f;
    float rest_head_local[3] = {0.0f, 0.0f, 0.0f};
    float rest_tail_local[3] = {0.0f, 0.0f, 0.0f};
    float rest_local_translation[3] = {0.0f, 0.0f, 0.0f};
    float rest_local_rotation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct BoneChainDescriptor {
    std::string component_id;
    std::string armature_name;
    std::string root_bone_name;
    std::vector<BoneDescriptor> bones;
};

struct SceneDescriptor {
    std::vector<BoneChainDescriptor> bone_chains;
};

struct BoneChainRuntimeInput {
    std::string component_id;
    std::string armature_name;
    std::string root_bone_name;
    float root_translation[3] = {0.0f, 0.0f, 0.0f};
    float root_rotation_quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct RuntimeInputs {
    float dt = 1.0f / 60.0f;
    std::int32_t substeps = 1;
    std::vector<BoneChainRuntimeInput> bone_chains;
};

struct BoneTransform {
    std::string component_id;
    std::string armature_name;
    std::string bone_name;
    float translation[3] = {0.0f, 0.0f, 0.0f};
    float rotation_quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct RuntimeSceneInfo {
    SceneHandle handle = 0;
    std::string backend = "stub";
    std::uint64_t step_count = 0;
};

SceneHandle build_scene(const SceneDescriptor& scene);
void destroy_scene(SceneHandle handle);
void reset_scene(SceneHandle handle);
void set_runtime_inputs(SceneHandle handle, const RuntimeInputs& inputs);
void step_scene(SceneHandle handle, const RuntimeInputs& inputs);
std::uint64_t get_step_count(SceneHandle handle);
std::vector<BoneTransform> get_bone_transforms(SceneHandle handle);
RuntimeSceneInfo get_scene_info(SceneHandle handle);

}  // namespace hocloth
