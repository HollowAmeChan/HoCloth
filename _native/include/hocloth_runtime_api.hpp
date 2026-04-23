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
    float radius = 0.0f;
    float stiffness = 0.6f;
    float damping = 0.2f;
    float drag = 0.1f;
    float gravity_scale = 1.0f;
    float rest_head_local[3] = {0.0f, 0.0f, 0.0f};
    float rest_tail_local[3] = {0.0f, 0.0f, 0.0f};
    float rest_local_translation[3] = {0.0f, 0.0f, 0.0f};
    float rest_local_rotation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct BoneChainDescriptor {
    std::string component_id;
    std::string armature_name;
    std::string root_bone_name;
    std::string center_object_name;
    float joint_radius = 0.02f;
    float stiffness = 0.6f;
    float damping = 0.2f;
    float drag = 0.1f;
    float gravity_strength = 0.3f;
    float gravity_direction[3] = {0.0f, -1.0f, 0.0f};
    std::vector<std::string> collider_group_ids;
    std::vector<std::string> collision_binding_ids;
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

struct ColliderGroupDescriptor {
    std::string component_id;
    std::vector<std::string> collider_ids;
};

struct CollisionObjectDescriptor {
    std::string collision_object_id;
    std::string owner_component_id;
    std::string motion_type = "STATIC";
    std::string shape_type = "CAPSULE";
    float world_translation[3] = {0.0f, 0.0f, 0.0f};
    float world_rotation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float linear_velocity[3] = {0.0f, 0.0f, 0.0f};
    float angular_velocity[3] = {0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
    float height = 0.0f;
    std::string source_object_name;
    std::vector<std::string> source_group_ids;
};

struct CollisionBindingDescriptor {
    std::string binding_id;
    std::string owner_component_id;
    std::vector<std::string> source_group_ids;
    std::vector<std::string> collision_object_ids;
};

struct CacheDescriptor {
    std::string component_id;
    std::string source_object_name;
    std::string topology_hash;
    std::string cache_format = "pc2";
    std::string cache_path;
};

struct SceneDescriptor {
    std::vector<BoneChainDescriptor> bone_chains;
    std::vector<ColliderDescriptor> colliders;
    std::vector<ColliderGroupDescriptor> collider_groups;
    std::vector<CollisionObjectDescriptor> collision_objects;
    std::vector<CollisionBindingDescriptor> collision_bindings;
    std::vector<CacheDescriptor> cache_descriptors;
};

struct BoneChainRuntimeInput {
    std::string component_id;
    std::string armature_name;
    std::string root_bone_name;
    std::string center_object_name;
    float root_translation[3] = {0.0f, 0.0f, 0.0f};
    float root_rotation_quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float root_linear_velocity[3] = {0.0f, 0.0f, 0.0f};
    float root_scale[3] = {1.0f, 1.0f, 1.0f};
    float center_translation[3] = {0.0f, 0.0f, 0.0f};
    float center_rotation_quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float center_linear_velocity[3] = {0.0f, 0.0f, 0.0f};
    float center_scale[3] = {1.0f, 1.0f, 1.0f};
};

struct CollisionObjectRuntimeInput {
    std::string collision_object_id;
    float world_translation[3] = {0.0f, 0.0f, 0.0f};
    float world_rotation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float linear_velocity[3] = {0.0f, 0.0f, 0.0f};
    float angular_velocity[3] = {0.0f, 0.0f, 0.0f};
};

struct RuntimeInputs {
    float dt = 1.0f / 60.0f;
    std::int32_t simulation_frequency = 90;
    std::int32_t max_simulation_steps_per_frame = 5;
    std::vector<BoneChainRuntimeInput> bone_chains;
    std::vector<CollisionObjectRuntimeInput> collision_objects;
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
    std::uint64_t collider_group_count = 0;
    std::uint64_t collision_object_count = 0;
    std::uint64_t collision_binding_count = 0;
    std::uint64_t cache_descriptor_count = 0;
    bool physics_scene_ready = false;
};

struct RuntimeStepInfo {
    std::int32_t scheduled_steps = 0;
    std::int32_t executed_steps = 0;
    std::int32_t skipped_steps = 0;
};

SceneHandle build_scene(const SceneDescriptor& scene);
void destroy_scene(SceneHandle handle);
void reset_scene(SceneHandle handle);
void set_runtime_inputs(SceneHandle handle, const RuntimeInputs& inputs);
std::int32_t step_scene(SceneHandle handle, const RuntimeInputs& inputs);
std::uint64_t get_step_count(SceneHandle handle);
RuntimeStepInfo get_last_step_info(SceneHandle handle);
std::vector<BoneTransform> get_bone_transforms(SceneHandle handle);
RuntimeSceneInfo get_scene_info(SceneHandle handle);

}  // namespace hocloth
