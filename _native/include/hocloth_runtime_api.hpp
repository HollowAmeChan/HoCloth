#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hocloth {

struct BoneDescriptor {
    std::string name;
    std::int32_t parent_index = -1;
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

struct RuntimeInputs {
    float dt = 1.0f / 60.0f;
    std::int32_t substeps = 1;
};

struct BoneTransform {
    std::string component_id;
    std::string bone_name;
    float translation[3] = {0.0f, 0.0f, 0.0f};
    float rotation_quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

using SceneHandle = std::uint64_t;

SceneHandle build_scene(const SceneDescriptor& scene);
void destroy_scene(SceneHandle handle);
void reset_scene(SceneHandle handle);
void step_scene(SceneHandle handle, const RuntimeInputs& inputs);
std::uint64_t get_step_count(SceneHandle handle);
std::vector<BoneTransform> get_bone_transforms(SceneHandle handle);

}  // namespace hocloth
