#include "hocloth_runtime_api.hpp"
#include "hocloth_physx_builder.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace hocloth {

namespace {

struct RuntimeBoneState {
    float pitch = 0.0f;
    float roll = 0.0f;
    float pitch_velocity = 0.0f;
    float roll_velocity = 0.0f;
};

struct RuntimeBoneChainState {
    std::vector<RuntimeBoneState> bones;
};

struct RuntimeSceneState {
    SceneDescriptor scene;
    RuntimeInputs inputs;
    std::vector<RuntimeBoneChainState> chain_states;
    std::string backend = "stub";
    std::string build_message;
    bool physics_scene_ready = false;
    std::uint64_t steps = 0;
};

std::unordered_map<SceneHandle, RuntimeSceneState> g_scenes;
SceneHandle g_next_handle = 1;


void quat_multiply(const float a[4], const float b[4], float out[4]) {
    out[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    out[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    out[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    out[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}

float clamp_unit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float clamp_symmetric(float value, float limit) {
    return std::clamp(value, -limit, limit);
}

float vector_length3(const float value[3]) {
    return std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
}

void normalize_vector3(const float input[3], float output[3]) {
    const float length = vector_length3(input);
    if (length <= 1.0e-6f) {
        output[0] = 0.0f;
        output[1] = -1.0f;
        output[2] = 0.0f;
        return;
    }

    output[0] = input[0] / length;
    output[1] = input[1] / length;
    output[2] = input[2] / length;
}

std::vector<RuntimeBoneChainState> make_chain_states(const SceneDescriptor& scene) {
    std::vector<RuntimeBoneChainState> states;
    states.reserve(scene.bone_chains.size());
    for (const BoneChainDescriptor& chain : scene.bone_chains) {
        RuntimeBoneChainState chain_state;
        chain_state.bones.resize(chain.bones.size());
        states.push_back(std::move(chain_state));
    }
    return states;
}

void step_bone_chain(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    float dt
) {
    if (chain_state.bones.empty() || dt <= 0.0f) {
        return;
    }

    float gravity_dir[3];
    normalize_vector3(chain.gravity_direction, gravity_dir);

    const float stiffness = clamp_unit(chain.stiffness);
    const float damping = clamp_unit(chain.damping);
    const float drag = clamp_unit(chain.drag);
    const float gravity = std::max(0.0f, chain.gravity_strength);
    const float stiffness_gain = 18.0f + stiffness * 54.0f;
    const float damping_gain = 3.0f + damping * 10.0f;
    const float drag_factor = std::max(0.0f, 1.0f - drag * dt * 10.0f);
    const float inherit_factor = 0.72f + stiffness * 0.18f;
    const float max_angle = 1.15f;
    const float max_velocity = 8.0f;
    const float chain_size = static_cast<float>(chain_state.bones.size());

    for (std::size_t bone_index = 0; bone_index < chain_state.bones.size(); ++bone_index) {
        RuntimeBoneState& bone_state = chain_state.bones[bone_index];
        const BoneDescriptor& bone = chain.bones[bone_index];
        const float depth = static_cast<float>(bone_index + 1) / std::max(1.0f, chain_size);
        const float length_scale = std::max(0.05f, bone.length);
        const float gravity_scale = gravity * (0.18f + depth * 0.48f) * std::sqrt(length_scale);

        float parent_pitch = 0.0f;
        float parent_roll = 0.0f;
        if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
            const RuntimeBoneState& parent_state = chain_state.bones[static_cast<std::size_t>(bone.parent_index)];
            parent_pitch = parent_state.pitch * inherit_factor;
            parent_roll = parent_state.roll * inherit_factor;
        }

        const float target_pitch = parent_pitch + (-gravity_dir[1]) * gravity_scale;
        const float target_roll = parent_roll + gravity_dir[0] * gravity_scale;
        const float pitch_accel = (target_pitch - bone_state.pitch) * stiffness_gain -
                                  bone_state.pitch_velocity * damping_gain;
        const float roll_accel = (target_roll - bone_state.roll) * stiffness_gain -
                                 bone_state.roll_velocity * damping_gain;

        bone_state.pitch_velocity = clamp_symmetric(
            (bone_state.pitch_velocity + pitch_accel * dt) * drag_factor,
            max_velocity
        );
        bone_state.roll_velocity = clamp_symmetric(
            (bone_state.roll_velocity + roll_accel * dt) * drag_factor,
            max_velocity
        );
        bone_state.pitch = clamp_symmetric(bone_state.pitch + bone_state.pitch_velocity * dt, max_angle);
        bone_state.roll = clamp_symmetric(bone_state.roll + bone_state.roll_velocity * dt, max_angle);
    }
}

void advance_runtime(RuntimeSceneState& scene_state, const RuntimeInputs& inputs) {
    const std::int32_t substeps = std::max<std::int32_t>(1, inputs.substeps);
    const float dt = inputs.dt > 0.0f ? inputs.dt : (1.0f / 60.0f);
    const float substep_dt = dt / static_cast<float>(substeps);

    for (std::int32_t substep = 0; substep < substeps; ++substep) {
        for (std::size_t chain_index = 0; chain_index < scene_state.scene.bone_chains.size(); ++chain_index) {
            step_bone_chain(
                scene_state.scene.bone_chains[chain_index],
                scene_state.chain_states[chain_index],
                substep_dt
            );
        }
    }
}


}  // namespace

SceneHandle build_scene(const SceneDescriptor& scene) {
    const PhysxBuildResult physx = probe_physx_backend();
    const SceneHandle handle = g_next_handle++;
    const PhysxBuildResult build_result = build_physx_scene(handle, scene);
    const std::string backend = build_result.scene_created ? build_result.backend : physx.backend;
    const std::string build_message = build_result.message.empty() ? physx.message : build_result.message;
    g_scenes.emplace(
        handle,
        RuntimeSceneState{
            scene,
            RuntimeInputs{},
            make_chain_states(scene),
            backend,
            build_message,
            build_result.scene_created,
            0
        }
    );
    return handle;
}

void destroy_scene(SceneHandle handle) {
    destroy_physx_scene(handle);
    g_scenes.erase(handle);
}

void reset_scene(SceneHandle handle) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return;
    }
    it->second.inputs = RuntimeInputs{};
    it->second.chain_states = make_chain_states(it->second.scene);
    it->second.steps = 0;
    if (it->second.physics_scene_ready) {
        reset_physx_scene(handle);
    }
}

void set_runtime_inputs(SceneHandle handle, const RuntimeInputs& inputs) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return;
    }
    it->second.inputs = inputs;
    if (it->second.physics_scene_ready) {
        set_physx_runtime_inputs(handle, inputs);
    }
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

    if (it->second.physics_scene_ready) {
        step_physx_scene(handle, it->second.inputs);
    } else {
        advance_runtime(it->second, inputs);
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

    if (it->second.physics_scene_ready) {
        return get_physx_bone_transforms(handle);
    }

    std::vector<BoneTransform> transforms;
    for (std::size_t chain_index = 0; chain_index < it->second.scene.bone_chains.size(); ++chain_index) {
        const BoneChainDescriptor& chain = it->second.scene.bone_chains[chain_index];
        const RuntimeBoneChainState& chain_state = it->second.chain_states[chain_index];
        for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
            const BoneDescriptor& bone = chain.bones[bone_index];
            const RuntimeBoneState& bone_state = chain_state.bones[bone_index];
            const float pitch = bone_state.pitch;
            const float roll = bone_state.roll;
            const float half_pitch = pitch * 0.5f;
            const float half_roll = roll * 0.5f;
            const float quat_x[4] = {
                std::cos(half_pitch),
                std::sin(half_pitch),
                0.0f,
                0.0f,
            };
            const float quat_z[4] = {
                std::cos(half_roll),
                0.0f,
                0.0f,
                std::sin(half_roll),
            };
            float delta_quat[4];
            quat_multiply(quat_z, quat_x, delta_quat);

            BoneTransform transform;
            transform.component_id = chain.component_id;
            transform.armature_name = chain.armature_name;
            transform.bone_name = bone.name;
            // Placeholder mode outputs pose-space delta rotation, not absolute
            // local orientation, so Blender can layer it on top of rest pose.
            transform.rotation_quaternion[0] = delta_quat[0];
            transform.rotation_quaternion[1] = delta_quat[1];
            transform.rotation_quaternion[2] = delta_quat[2];
            transform.rotation_quaternion[3] = delta_quat[3];
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
    info.build_message = it->second.build_message;
    info.step_count = it->second.steps;
    info.bone_chain_count = static_cast<std::uint64_t>(it->second.scene.bone_chains.size());
    info.collider_count = static_cast<std::uint64_t>(it->second.scene.colliders.size());
    info.physics_scene_ready = it->second.physics_scene_ready;
    for (const BoneChainDescriptor& chain : it->second.scene.bone_chains) {
        info.bone_count += static_cast<std::uint64_t>(chain.bones.size());
    }
    return info;
}

}  // namespace hocloth
