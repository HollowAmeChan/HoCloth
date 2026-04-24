#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hocloth {

using SceneHandle = std::uint64_t;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quat {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct CompiledSpringJoint {
    std::string name;
    int parent_index = -1;
    int depth = 0;
    float length = 0.0f;
    float radius = 0.0f;
    float stiffness = 0.0f;
    float damping = 0.0f;
    float drag = 0.0f;
    float gravity_scale = 1.0f;
    Vec3 rest_head_local;
    Vec3 rest_tail_local;
    Vec3 rest_local_translation;
    Quat rest_local_rotation;
};

struct CompiledSpringLine {
    int start_index = -1;
    int end_index = -1;
};

struct CompiledSpringBaseline {
    std::vector<int> joint_indices;
};

struct CompiledSpringBone {
    std::string component_id;
    std::string armature_name;
    std::string root_bone_name;
    std::string center_object_name;
    std::string center_bone_name;
    float joint_radius = 0.0f;
    float stiffness = 0.0f;
    float damping = 0.0f;
    float drag = 0.0f;
    bool use_spring = true;
    float spring_power = 0.04f;
    float limit_distance = 0.1f;
    float normal_limit_ratio = 1.0f;
    float spring_noise = 0.0f;
    float gravity_strength = 0.0f;
    Vec3 gravity_direction{0.0f, -1.0f, 0.0f};
    std::vector<std::string> collider_group_ids;
    std::vector<std::string> collision_binding_ids;
    Vec3 armature_scale{1.0f, 1.0f, 1.0f};
    std::vector<CompiledSpringJoint> joints;
    std::vector<CompiledSpringLine> lines;
    std::vector<CompiledSpringBaseline> baselines;
};

struct CompiledCollisionObject {
    std::string collision_object_id;
    std::string owner_component_id;
    std::string motion_type;
    std::string shape_type;
    Vec3 world_translation;
    Quat world_rotation;
    Vec3 linear_velocity;
    Vec3 angular_velocity;
    float radius = 0.0f;
    float height = 0.0f;
    std::string source_object_name;
    std::vector<std::string> source_group_ids;
};

struct CompiledCollisionBinding {
    std::string binding_id;
    std::string owner_component_id;
    std::vector<std::string> source_group_ids;
    std::vector<std::string> collision_object_ids;
};

struct CompiledScene {
    std::vector<CompiledSpringBone> spring_bones;
    std::vector<CompiledCollisionObject> collision_objects;
    std::vector<CompiledCollisionBinding> collision_bindings;

    [[nodiscard]] std::string Summary() const;
};

struct RuntimeChainInput {
    std::string component_id;
    std::string armature_name;
    std::string root_bone_name;
    std::string center_object_name;
    std::string center_bone_name;
    Vec3 root_translation;
    Quat root_rotation_quaternion;
    Vec3 root_linear_velocity;
    Vec3 root_scale{1.0f, 1.0f, 1.0f};
    Vec3 center_translation;
    Quat center_rotation_quaternion;
    Vec3 center_linear_velocity;
    Vec3 center_scale{1.0f, 1.0f, 1.0f};
    std::vector<Vec3> basic_head_positions;
    std::vector<Vec3> basic_tail_positions;
    std::vector<Quat> basic_rotations;
};

struct RuntimeCollisionObjectInput {
    std::string collision_object_id;
    Vec3 world_translation;
    Quat world_rotation;
    Vec3 linear_velocity;
};

struct RuntimeInputs {
    std::vector<RuntimeChainInput> bone_chains;
    std::vector<RuntimeCollisionObjectInput> collision_objects;
};

struct BoneTransform {
    std::string component_id;
    std::string armature_name;
    std::string bone_name;
    Vec3 translation;
    Quat rotation_quaternion;
};

struct BuildSceneResult {
    SceneHandle handle = 0;
    std::string summary;
    std::string backend;
    std::string build_message;
};

struct StepSceneResult {
    SceneHandle handle = 0;
    float dt = 0.0f;
    int simulation_frequency = 90;
    int executed_steps = 0;
    std::uint64_t steps = 0;
    std::string summary;
};

struct SimulationTimeState {
    int simulation_frequency = 90;
    int max_simulation_count_per_frame = 3;
    float global_time_scale = 1.0f;
    float simulation_delta_time = 1.0f / 90.0f;
    float max_delta_time = (1.0f / 90.0f) * 3.0f;
    float simulation_power[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    void FrameUpdate(int requested_frequency);
};

class RuntimeModule {
public:
    struct SceneState;

    BuildSceneResult BuildScene(CompiledScene compiled_scene);
    SceneHandle DestroyScene(SceneHandle handle);
    bool ResetScene(SceneHandle handle);
    bool SetRuntimeInputs(SceneHandle handle, RuntimeInputs runtime_inputs);
    StepSceneResult StepScene(SceneHandle handle, float dt, int simulation_frequency);
    std::vector<BoneTransform> GetBoneTransforms(SceneHandle handle) const;

private:
    SceneState& RequireScene(SceneHandle handle);
    const SceneState& RequireScene(SceneHandle handle) const;

    SceneHandle next_handle_ = 1;
    std::unordered_map<SceneHandle, std::unique_ptr<SceneState>> scenes_;
};

RuntimeModule& GetRuntimeModule();

Vec3 ClosestPointOnSegment(const Vec3& point, const Vec3& a, const Vec3& b);

}  // namespace hocloth
