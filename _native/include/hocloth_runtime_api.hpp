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
    float stiffness = 0.6f;
    float damping = 0.2f;
    float drag = 0.1f;
    float gravity_strength = 0.3f;
    float gravity_direction[3] = {0.0f, -1.0f, 0.0f};
    std::vector<BoneDescriptor> bones;
};

struct ColliderDescriptor {
    std::string component_id;
    std::string object_name;
    std::string shape_type = "CAPSULE";
    float radius = 0.05f;
    float height = 0.1f;
    float world_translation[3] = {0.0f, 0.0f, 0.0f};
    float world_rotation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct SceneDescriptor {
    std::vector<BoneChainDescriptor> bone_chains;
    std::vector<ColliderDescriptor> colliders;
};

struct BoneChainRuntimeInput {
    std::string component_id;
    std::string armature_name;
    std::string root_bone_name;
    float root_translation[3] = {0.0f, 0.0f, 0.0f};
    float root_rotation_quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float root_linear_velocity[3] = {0.0f, 0.0f, 0.0f};
    float root_scale[3] = {1.0f, 1.0f, 1.0f};
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
    std::string build_message;
    std::uint64_t step_count = 0;
    std::uint64_t bone_chain_count = 0;
    std::uint64_t bone_count = 0;
    std::uint64_t collider_count = 0;
    bool physics_scene_ready = false;
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
