#include "hocloth_runtime_api.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hocloth {

namespace {

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

struct RuntimeBoneState {
    Vec3 position;
    Vec3 previous_position;
    float stretch_lambda = 0.0f;
    float bend_lambda = 0.0f;
    float shape_lambda = 0.0f;
};

struct RuntimeBoneChainState {
    std::vector<RuntimeBoneState> bones;
    Vec3 last_root_translation;
    Quat last_root_rotation;
    bool initialized = false;
};

struct RuntimeSceneState {
    SceneDescriptor scene;
    RuntimeInputs inputs;
    std::vector<RuntimeBoneChainState> chain_states;
    std::string backend = "xpbd";
    std::string build_message;
    bool physics_scene_ready = false;
    std::uint64_t steps = 0;
};

std::unordered_map<SceneHandle, RuntimeSceneState> g_scenes;
SceneHandle g_next_handle = 1;

float clamp_unit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

Vec3 make_vec3(const float value[3]) {
    return Vec3{value[0], value[1], value[2]};
}

Quat make_quat(const float value[4]) {
    return Quat{value[0], value[1], value[2], value[3]};
}

void copy_vec3(const Vec3& value, float out[3]) {
    out[0] = value.x;
    out[1] = value.y;
    out[2] = value.z;
}

void copy_quat(const Quat& value, float out[4]) {
    out[0] = value.w;
    out[1] = value.x;
    out[2] = value.y;
    out[3] = value.z;
}

Vec3 add(const Vec3& a, const Vec3& b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 sub(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 mul(const Vec3& value, float scalar) {
    return Vec3{value.x * scalar, value.y * scalar, value.z * scalar};
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float length_squared(const Vec3& value) {
    return dot(value, value);
}

float length(const Vec3& value) {
    return std::sqrt(length_squared(value));
}

Vec3 normalize_or_default(const Vec3& value, const Vec3& fallback) {
    const float len = length(value);
    if (len <= 1.0e-6f) {
        return fallback;
    }
    return mul(value, 1.0f / len);
}

Quat normalize_or_identity(const Quat& value) {
    const float len = std::sqrt(
        value.w * value.w +
        value.x * value.x +
        value.y * value.y +
        value.z * value.z
    );
    if (len <= 1.0e-6f) {
        return Quat{};
    }
    const float inv = 1.0f / len;
    return Quat{
        value.w * inv,
        value.x * inv,
        value.y * inv,
        value.z * inv,
    };
}

Quat conjugate(const Quat& value) {
    return Quat{value.w, -value.x, -value.y, -value.z};
}

Quat quat_multiply(const Quat& a, const Quat& b) {
    return Quat{
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

Vec3 rotate_vector(const Quat& rotation, const Vec3& value) {
    const Quat v{0.0f, value.x, value.y, value.z};
    const Quat rotated = quat_multiply(quat_multiply(rotation, v), conjugate(rotation));
    return Vec3{rotated.x, rotated.y, rotated.z};
}

Quat quat_between_vectors(const Vec3& from, const Vec3& to) {
    const Vec3 normalized_from = normalize_or_default(from, Vec3{0.0f, 1.0f, 0.0f});
    const Vec3 normalized_to = normalize_or_default(to, Vec3{0.0f, 1.0f, 0.0f});
    const float cosine = dot(normalized_from, normalized_to);

    if (cosine >= 1.0f - 1.0e-5f) {
        return Quat{};
    }

    if (cosine <= -1.0f + 1.0e-5f) {
        Vec3 axis = cross(normalized_from, Vec3{1.0f, 0.0f, 0.0f});
        if (length_squared(axis) <= 1.0e-6f) {
            axis = cross(normalized_from, Vec3{0.0f, 0.0f, 1.0f});
        }
        axis = normalize_or_default(axis, Vec3{0.0f, 0.0f, 1.0f});
        return Quat{0.0f, axis.x, axis.y, axis.z};
    }

    const Vec3 axis = cross(normalized_from, normalized_to);
    return normalize_or_identity(Quat{1.0f + cosine, axis.x, axis.y, axis.z});
}

Vec3 rest_head_world(const BoneDescriptor& bone, const Vec3& root_translation, const Quat& root_rotation, const Vec3& root_rest_head) {
    return add(root_translation, rotate_vector(root_rotation, sub(make_vec3(bone.rest_head_local), root_rest_head)));
}

Vec3 rest_tail_world(const BoneDescriptor& bone, const Vec3& root_translation, const Quat& root_rotation, const Vec3& root_rest_head) {
    return add(root_translation, rotate_vector(root_rotation, sub(make_vec3(bone.rest_tail_local), root_rest_head)));
}

const BoneChainRuntimeInput* find_chain_input(const RuntimeInputs& inputs, const BoneChainDescriptor& chain) {
    for (const BoneChainRuntimeInput& input : inputs.bone_chains) {
        if (input.component_id == chain.component_id) {
            return &input;
        }
    }
    return nullptr;
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

void initialize_chain_state(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const BoneChainRuntimeInput* chain_input
) {
    const Vec3 root_rest_head = chain.bones.empty()
        ? Vec3{}
        : make_vec3(chain.bones.front().rest_head_local);
    const Vec3 root_translation = chain_input != nullptr ? make_vec3(chain_input->root_translation) : root_rest_head;
    const Quat root_rotation = chain_input != nullptr
        ? normalize_or_identity(make_quat(chain_input->root_rotation_quaternion))
        : Quat{};

    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        const BoneDescriptor& bone = chain.bones[bone_index];
        RuntimeBoneState& state = chain_state.bones[bone_index];
        state.position = rest_tail_world(bone, root_translation, root_rotation, root_rest_head);
        state.previous_position = state.position;
        state.stretch_lambda = 0.0f;
        state.bend_lambda = 0.0f;
        state.shape_lambda = 0.0f;
    }

    chain_state.last_root_translation = root_translation;
    chain_state.last_root_rotation = root_rotation;
    chain_state.initialized = true;
}

void project_distance_constraint(
    Vec3 head_position,
    RuntimeBoneState& state,
    float rest_length,
    float compliance,
    float dt,
    float& lambda
) {
    const Vec3 delta = sub(state.position, head_position);
    const float current_length = length(delta);
    if (current_length <= 1.0e-6f || rest_length <= 1.0e-6f) {
        lambda = 0.0f;
        return;
    }

    const Vec3 gradient = mul(delta, 1.0f / current_length);
    const float constraint = current_length - rest_length;
    const float alpha = compliance / std::max(dt * dt, 1.0e-8f);
    const float delta_lambda = (-constraint - alpha * lambda) / (1.0f + alpha);
    lambda += delta_lambda;
    state.position = add(state.position, mul(gradient, delta_lambda));
}

void project_shape_constraint(
    Vec3 head_position,
    RuntimeBoneState& state,
    const Vec3& target_tail,
    float compliance,
    float dt
) {
    const Vec3 delta = sub(state.position, target_tail);
    const float alpha = compliance / std::max(dt * dt, 1.0e-8f);
    const float denominator = 1.0f + alpha;
    const Vec3 correction = mul(delta, -1.0f / denominator);
    state.position = add(state.position, correction);
    state.shape_lambda = 0.0f;
    const Vec3 direction = sub(state.position, head_position);
    const float current_length = length(direction);
    if (current_length > 1.0e-6f) {
        const float rest_length = length(sub(target_tail, head_position));
        if (rest_length > 1.0e-6f) {
            state.position = add(head_position, mul(direction, rest_length / current_length));
        }
    }
}

void step_bone_chain(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const BoneChainRuntimeInput* chain_input,
    float dt
) {
    if (chain.bones.empty() || dt <= 0.0f) {
        return;
    }

    if (!chain_state.initialized) {
        initialize_chain_state(chain, chain_state, chain_input);
    }

    const Vec3 root_rest_head = make_vec3(chain.bones.front().rest_head_local);
    const Vec3 root_translation = chain_input != nullptr
        ? make_vec3(chain_input->root_translation)
        : chain_state.last_root_translation;
    const Quat root_rotation = chain_input != nullptr
        ? normalize_or_identity(make_quat(chain_input->root_rotation_quaternion))
        : chain_state.last_root_rotation;
    const Vec3 root_linear_velocity = chain_input != nullptr
        ? make_vec3(chain_input->root_linear_velocity)
        : Vec3{};
    const Vec3 root_delta = sub(root_translation, chain_state.last_root_translation);
    const Vec3 gravity_direction = normalize_or_default(make_vec3(chain.gravity_direction), Vec3{0.0f, -1.0f, 0.0f});
    const float stiffness = std::clamp(chain.stiffness, 0.0f, 2.0f);
    const float stiffness_factor = std::clamp(stiffness * 0.5f, 0.0f, 1.0f);
    const float damping = clamp_unit(chain.damping);
    const float drag = clamp_unit(chain.drag);
    const float gravity_strength = std::clamp(chain.gravity_strength, 0.0f, 2.0f);
    const Vec3 gravity = mul(gravity_direction, 14.0f * gravity_strength);
    const float inertia_mix = std::clamp(0.96f - drag * 0.72f, 0.12f, 0.98f);
    const float root_inertia_mix = std::clamp(0.18f + stiffness_factor * 0.42f, 0.08f, 0.72f);
    const float chain_inertia_mix = std::clamp(0.45f + stiffness_factor * 0.35f - damping * 0.15f, 0.18f, 0.82f);
    const float stretch_compliance = 1.5e-3f + (1.0f - stiffness_factor) * 7.5e-3f;
    const float bend_compliance = 6.0e-3f + (1.0f - stiffness_factor) * 5.5e-2f;
    const float shape_compliance = 8.0e-2f + (1.0f - stiffness_factor) * 2.8e-1f;
    const float iterations = 8.0f;
    const std::size_t solver_iterations = 8;

    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        RuntimeBoneState& state = chain_state.bones[bone_index];
        const Vec3 velocity = mul(sub(state.position, state.previous_position), inertia_mix);
        Vec3 inherited_motion = mul(root_delta, root_inertia_mix);
        if (bone_index > 0) {
            const RuntimeBoneState& parent_state = chain_state.bones[bone_index - 1];
            const Vec3 parent_motion = sub(parent_state.position, parent_state.previous_position);
            const float depth_falloff = 1.0f / (1.0f + static_cast<float>(bone_index) * 0.35f);
            inherited_motion = add(
                inherited_motion,
                mul(parent_motion, chain_inertia_mix * depth_falloff)
            );
        }
        const Vec3 predicted = add(
            add(state.position, add(velocity, inherited_motion)),
            add(mul(gravity, dt * dt), mul(root_linear_velocity, 0.06f * dt))
        );
        state.previous_position = state.position;
        state.position = predicted;
        state.stretch_lambda = 0.0f;
        state.bend_lambda = 0.0f;
        state.shape_lambda = 0.0f;
    }

    for (std::size_t iteration = 0; iteration < solver_iterations; ++iteration) {
        for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
            const BoneDescriptor& bone = chain.bones[bone_index];
            RuntimeBoneState& state = chain_state.bones[bone_index];
            Vec3 head_position = root_translation;
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
                head_position = chain_state.bones[static_cast<std::size_t>(bone.parent_index)].position;
            }

            const Vec3 target_tail = rest_tail_world(bone, root_translation, root_rotation, root_rest_head);
            project_distance_constraint(
                head_position,
                state,
                std::max(1.0e-5f, bone.length),
                stretch_compliance,
                dt,
                state.stretch_lambda
            );
            if (bone_index > 0) {
                const BoneDescriptor& parent_bone = chain.bones[bone_index - 1];
                Vec3 bend_anchor = root_translation;
                if (bone_index > 1) {
                    bend_anchor = chain_state.bones[bone_index - 2].position;
                }
                const float bend_rest_length = std::max(1.0e-5f, parent_bone.length + bone.length);
                project_distance_constraint(
                    bend_anchor,
                    state,
                    bend_rest_length,
                    bend_compliance / iterations,
                    dt,
                    state.bend_lambda
                );
            }
            if (bone_index > 0) {
                project_shape_constraint(
                    head_position,
                    state,
                    target_tail,
                    shape_compliance / iterations,
                    dt
                );
            }
        }
    }

    const float velocity_damping = std::clamp(0.96f - damping * 0.72f - drag * 0.12f, 0.06f, 0.99f);
    for (RuntimeBoneState& state : chain_state.bones) {
        const Vec3 new_velocity = mul(sub(state.position, state.previous_position), velocity_damping);
        state.previous_position = sub(state.position, new_velocity);
    }

    chain_state.last_root_translation = root_translation;
    chain_state.last_root_rotation = root_rotation;
}

void advance_runtime(RuntimeSceneState& scene_state, const RuntimeInputs& inputs) {
    const std::int32_t substeps = std::max<std::int32_t>(1, inputs.substeps);
    const float dt = inputs.dt > 0.0f ? inputs.dt : (1.0f / 60.0f);
    const float substep_dt = dt / static_cast<float>(substeps);

    for (std::int32_t substep = 0; substep < substeps; ++substep) {
        for (std::size_t chain_index = 0; chain_index < scene_state.scene.bone_chains.size(); ++chain_index) {
            const BoneChainDescriptor& chain = scene_state.scene.bone_chains[chain_index];
            RuntimeBoneChainState& chain_state = scene_state.chain_states[chain_index];
            step_bone_chain(
                chain,
                chain_state,
                find_chain_input(scene_state.inputs, chain),
                substep_dt
            );
        }
    }
}

std::vector<BoneTransform> build_transforms(const RuntimeSceneState& scene_state) {
    std::vector<BoneTransform> transforms;

    for (std::size_t chain_index = 0; chain_index < scene_state.scene.bone_chains.size(); ++chain_index) {
        const BoneChainDescriptor& chain = scene_state.scene.bone_chains[chain_index];
        const RuntimeBoneChainState& chain_state = scene_state.chain_states[chain_index];
        const BoneChainRuntimeInput* chain_input = find_chain_input(scene_state.inputs, chain);
        const Vec3 root_rest_head = chain.bones.empty()
            ? Vec3{}
            : make_vec3(chain.bones.front().rest_head_local);
        const Vec3 root_translation = chain_input != nullptr
            ? make_vec3(chain_input->root_translation)
            : chain_state.last_root_translation;
        const Quat root_rotation = chain_input != nullptr
            ? normalize_or_identity(make_quat(chain_input->root_rotation_quaternion))
            : chain_state.last_root_rotation;

        std::vector<Quat> rest_global_rotations(chain.bones.size(), Quat{});
        std::vector<Quat> current_global_rotations(chain.bones.size(), Quat{});
        for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
            const BoneDescriptor& bone = chain.bones[bone_index];
            Vec3 current_head = root_translation;
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
                current_head = chain_state.bones[static_cast<std::size_t>(bone.parent_index)].position;
            }
            const Vec3 current_tail = chain_state.bones[bone_index].position;
            const Vec3 current_direction_world = sub(current_tail, current_head);
            const Vec3 current_direction = rotate_vector(conjugate(root_rotation), current_direction_world);
            const Vec3 rest_direction = sub(make_vec3(bone.rest_tail_local), make_vec3(bone.rest_head_local));
            const Quat swing = quat_between_vectors(rest_direction, current_direction);

            Quat rest_global_rotation = make_quat(bone.rest_local_rotation);
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < rest_global_rotations.size()) {
                rest_global_rotation = quat_multiply(
                    rest_global_rotations[static_cast<std::size_t>(bone.parent_index)],
                    make_quat(bone.rest_local_rotation)
                );
            }
            rest_global_rotations[bone_index] = rest_global_rotation;

            const Quat current_global_rotation = normalize_or_identity(quat_multiply(swing, rest_global_rotation));
            current_global_rotations[bone_index] = current_global_rotation;

            Quat current_local = current_global_rotation;
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < current_global_rotations.size()) {
                current_local = normalize_or_identity(
                    quat_multiply(
                        conjugate(current_global_rotations[static_cast<std::size_t>(bone.parent_index)]),
                        current_global_rotation
                    )
                );
            }

            const Quat delta = normalize_or_identity(quat_multiply(current_local, conjugate(make_quat(bone.rest_local_rotation))));
            BoneTransform transform;
            transform.component_id = chain.component_id;
            transform.armature_name = chain.armature_name;
            transform.bone_name = bone.name;
            copy_quat(delta, transform.rotation_quaternion);
            transforms.push_back(std::move(transform));
        }
    }

    return transforms;
}

}  // namespace

SceneHandle build_scene(const SceneDescriptor& scene) {
    const SceneHandle handle = g_next_handle++;
    RuntimeSceneState state;
    state.scene = scene;
    state.inputs = RuntimeInputs{};
    state.chain_states = make_chain_states(scene);
    state.backend = "xpbd";
    state.build_message = "XPBD runtime scene created";
    state.physics_scene_ready = false;
    state.steps = 0;
    g_scenes.emplace(handle, std::move(state));
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
    it->second.inputs = RuntimeInputs{};
    it->second.chain_states = make_chain_states(it->second.scene);
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

    advance_runtime(it->second, it->second.inputs);
    const std::uint64_t step_increment = it->second.inputs.substeps > 0
        ? static_cast<std::uint64_t>(it->second.inputs.substeps)
        : 1;
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
    return build_transforms(it->second);
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
