#include "hocloth_runtime_api.hpp"
#include "hocloth_collision_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
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
    Vec3 base_position;
    Vec3 position;
    Vec3 previous_position;
    Vec3 velocity_position;
    Vec3 velocity;
    Vec3 collision_normal;
    float collision_friction = 0.0f;
    float static_friction = 0.0f;
    float stretch_lambda = 0.0f;
};

struct RuntimeBoneChainState {
    std::vector<RuntimeBoneState> bones;
    std::vector<Vec3> step_basic_positions;
    std::vector<Quat> step_basic_rotations;
    Vec3 last_root_translation;
    Quat last_root_rotation;
    Vec3 last_center_translation;
    float velocity_weight = 0.0f;
    float blend_weight = 0.0f;
    bool initialized = false;
};

struct RuntimeSceneState {
    SceneDescriptor scene;
    RuntimeInputs inputs;
    RuntimeInputs previous_inputs;
    std::vector<RuntimeBoneChainState> chain_states;
    CollisionWorld collision_world;
    std::string backend = "xpbd";
    std::string build_message;
    bool physics_scene_ready = false;
    bool has_previous_inputs = false;
    float accumulated_time = 0.0f;
    std::uint64_t steps = 0;
    std::int32_t last_executed_steps = 0;
    std::int32_t last_skipped_steps = 0;
};

struct CollisionObjectStepStart {
    std::string collision_object_id;
    Vec3 translation;
    Quat rotation;
};

struct BaselineStepBuffers {
    std::vector<std::int32_t> start_indices;
    std::vector<std::int32_t> counts;
    std::vector<std::int32_t> joint_indices;
    std::vector<std::int32_t> joint_visit_counts;
    std::vector<Quat> rotation_buffer;
    std::vector<float> segment_lengths;
    std::vector<Vec3> local_positions;
    std::vector<Quat> local_rotations;
    std::vector<Vec3> limit_vectors;
    std::vector<Vec3> restoration_vectors;
};

struct CollisionAccumulation {
    Vec3 correction_sum;
    Vec3 normal_sum;
    std::int32_t correction_count = 0;
    Vec3 friction_normal_sum;
    float friction_max = 0.0f;
};

struct ColliderStepWorkData {
    std::string motion_type;
    std::string shape_type;
    Vec3 previous_translation;
    Quat previous_rotation;
    Vec3 previous_axis;
    Vec3 current_translation;
    Quat current_rotation;
    Vec3 current_axis;
    Vec3 previous_point_a;
    Vec3 previous_point_b;
    Vec3 current_point_a;
    Vec3 current_point_b;
    Vec3 swept_aabb_min;
    Vec3 swept_aabb_max;
    float radius = 0.0f;
    float height = 0.0f;
};

std::unordered_map<SceneHandle, RuntimeSceneState> g_scenes;
SceneHandle g_next_handle = 1;

constexpr float kCollisionSlop = 1.0e-3f;
constexpr float kCollisionDynamicFriction = 0.06f;
constexpr float kCollisionStaticFrictionSpeed = 0.035f;
constexpr float kCollisionStaticFrictionBuild = 0.04f;
constexpr float kCollisionStaticFrictionDecay = 0.05f;
constexpr float kCollisionFrictionDamping = 0.60f;
constexpr float kSpringCollisionLimitDistance = 0.05f;
constexpr float kDistanceVelocityAttenuation = 0.30f;
constexpr float kDirectionVelocityAttenuation = 0.80f;
constexpr float kAngleLimitVelocityAttenuation = 0.90f;
constexpr bool kImplicitSpringAngleLimit = false;
constexpr float kDefaultSimulationFrequency = 90.0f;
constexpr float kReferenceFrameDt = 1.0f / 90.0f;
constexpr float kResetStabilizationTime = 0.10f;
constexpr float kDepthMass = 5.0f;
constexpr std::int32_t kMaxSimulationStepsPerFrame = 5;
constexpr const char* kTailTipSuffix = "__hocloth_tail_tip__";

float clamp_unit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float scale_factor_for_dt(float per_ref_factor, float dt) {
    return std::pow(
        std::clamp(per_ref_factor, 0.0f, 1.0f),
        std::max(dt, 0.0f) / std::max(kReferenceFrameDt, 1.0e-8f)
    );
}

float fraction_for_dt(float per_frame_fraction, float dt) {
    const float keep = 1.0f - clamp_unit(per_frame_fraction);
    return 1.0f - std::pow(keep, std::max(dt, 0.0f) / kReferenceFrameDt);
}

float remap_bone_damping(float damping) {
    const float t = clamp_unit(damping);
    return std::pow(t, 2.2f) * 0.2f;
}

float calc_inverse_mass(float depth) {
    const float a = 1.0f - std::clamp(depth, 0.0f, 1.0f);
    const float mass = 1.0f + a * a * kDepthMass;
    return 1.0f / std::max(mass, 1.0e-6f);
}

float restoration_velocity_attenuation_for_bone(const BoneDescriptor& bone) {
    const float stiffness = clamp_unit(bone.stiffness);
    const float damping = clamp_unit(bone.damping);
    return std::clamp(0.90f - stiffness * 0.55f - damping * 0.15f, 0.40f, 0.80f);
}

struct SimulationPower {
    float x = 1.0f;
    float y = 1.0f;
    float z = 1.0f;
    float w = 1.0f;
};

SimulationPower simulation_power_for_frequency(std::int32_t simulation_frequency) {
    const float frequency = std::max(1.0f, static_cast<float>(simulation_frequency));
    const float t = kDefaultSimulationFrequency / frequency;
    SimulationPower power;
    power.x = t;
    power.y = t > 1.0f ? std::pow(t, 0.5f) : t;
    power.z = t > 1.0f ? std::pow(t, 0.3f) : t;
    power.w = std::pow(t, 1.8f);
    return power;
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

Vec3 min_components(const Vec3& a, const Vec3& b) {
    return Vec3{
        std::min(a.x, b.x),
        std::min(a.y, b.y),
        std::min(a.z, b.z),
    };
}

Vec3 max_components(const Vec3& a, const Vec3& b) {
    return Vec3{
        std::max(a.x, b.x),
        std::max(a.y, b.y),
        std::max(a.z, b.z),
    };
}

Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return add(mul(a, 1.0f - t), mul(b, t));
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

Vec3 closest_point_on_segment(const Vec3& point, const Vec3& a, const Vec3& b) {
    const Vec3 ab = sub(b, a);
    const float ab_len_sq = length_squared(ab);
    if (ab_len_sq <= 1.0e-8f) {
        return a;
    }
    const float t = std::clamp(dot(sub(point, a), ab) / ab_len_sq, 0.0f, 1.0f);
    return add(a, mul(ab, t));
}

std::pair<Vec3, Vec3> closest_points_between_segments(
    const Vec3& p1,
    const Vec3& q1,
    const Vec3& p2,
    const Vec3& q2
) {
    const Vec3 d1 = sub(q1, p1);
    const Vec3 d2 = sub(q2, p2);
    const Vec3 r = sub(p1, p2);
    const float a = dot(d1, d1);
    const float e = dot(d2, d2);
    const float f = dot(d2, r);

    float s = 0.0f;
    float t = 0.0f;

    if (a <= 1.0e-8f && e <= 1.0e-8f) {
        return {p1, p2};
    }

    if (a <= 1.0e-8f) {
        t = std::clamp(f / std::max(e, 1.0e-8f), 0.0f, 1.0f);
    } else {
        const float c = dot(d1, r);
        if (e <= 1.0e-8f) {
            s = std::clamp(-c / std::max(a, 1.0e-8f), 0.0f, 1.0f);
        } else {
            const float b = dot(d1, d2);
            const float denom = a * e - b * b;
            if (std::abs(denom) > 1.0e-8f) {
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            }
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = std::clamp(-c / std::max(a, 1.0e-8f), 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = std::clamp((b - c) / std::max(a, 1.0e-8f), 0.0f, 1.0f);
            }
        }
    }

    return {
        add(p1, mul(d1, s)),
        add(p2, mul(d2, t)),
    };
}

struct ColliderContact {
    Vec3 closest;
    Vec3 bone_closest;
    Vec3 normal;
    float distance = 0.0f;
    float target_radius = 0.0f;
    bool overlapping = false;
    float bone_t = 1.0f;
};

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

Quat nlerp(const Quat& a, const Quat& b, float t) {
    Quat target = b;
    if ((a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z) < 0.0f) {
        target = Quat{-b.w, -b.x, -b.y, -b.z};
    }
    return normalize_or_identity(Quat{
        a.w * (1.0f - t) + target.w * t,
        a.x * (1.0f - t) + target.x * t,
        a.y * (1.0f - t) + target.y * t,
        a.z * (1.0f - t) + target.z * t,
    });
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

float angle_between_vectors(const Vec3& a, const Vec3& b) {
    const float a_len = length(a);
    const float b_len = length(b);
    if (a_len <= 1.0e-6f || b_len <= 1.0e-6f) {
        return 0.0f;
    }
    const float cosine = std::clamp(dot(a, b) / (a_len * b_len), -1.0f, 1.0f);
    return std::acos(cosine);
}

Vec3 slerp_direction(const Vec3& from, const Vec3& to, float t, float length_scale) {
    const Vec3 normalized_from = normalize_or_default(from, Vec3{0.0f, 1.0f, 0.0f});
    const Vec3 normalized_to = normalize_or_default(to, normalized_from);
    const Quat rotation = quat_between_vectors(normalized_from, normalized_to);
    const Quat partial = nlerp(Quat{}, rotation, std::clamp(t, 0.0f, 1.0f));
    return mul(rotate_vector(partial, normalized_from), length_scale);
}

Vec3 rest_head_world(const BoneDescriptor& bone, const Vec3& root_translation, const Quat& root_rotation, const Vec3& root_rest_head) {
    return add(root_translation, rotate_vector(root_rotation, sub(make_vec3(bone.rest_head_local), root_rest_head)));
}

Vec3 rest_tail_world(const BoneDescriptor& bone, const Vec3& root_translation, const Quat& root_rotation, const Vec3& root_rest_head) {
    return add(root_translation, rotate_vector(root_rotation, sub(make_vec3(bone.rest_tail_local), root_rest_head)));
}

void move_position_preserve_velocity(RuntimeBoneState& state, const Vec3& delta) {
    state.position = add(state.position, delta);
    state.velocity_position = add(state.velocity_position, delta);
}

void move_position_with_velocity_attenuation(RuntimeBoneState& state, const Vec3& delta, float attenuation) {
    state.position = add(state.position, delta);
    state.velocity_position = add(state.velocity_position, mul(delta, clamp_unit(attenuation)));
}

void record_collision_contact(RuntimeBoneState& state, const Vec3& normal, float friction) {
    state.collision_normal = add(state.collision_normal, normal);
    state.collision_friction = std::max(state.collision_friction, clamp_unit(friction));
}

Vec3 apply_end_step_contact_friction(
    RuntimeBoneState& state,
    const Vec3& raw_displacement,
    float dt
) {
    const float step_scale = std::max(dt, 0.0f) / std::max(kReferenceFrameDt, 1.0e-8f);
    const float normal_length_sq = length_squared(state.collision_normal);
    if (normal_length_sq <= 1.0e-8f || state.collision_friction <= 1.0e-6f || dt <= 0.0f) {
        state.static_friction = std::max(0.0f, state.static_friction - kCollisionStaticFrictionDecay * step_scale);
        state.collision_normal = Vec3{};
        state.collision_friction = 0.0f;
        return raw_displacement;
    }

    const Vec3 contact_normal = normalize_or_default(state.collision_normal, Vec3{0.0f, 1.0f, 0.0f});
    Vec3 displacement = raw_displacement;
    const Vec3 normal_displacement = mul(contact_normal, dot(displacement, contact_normal));
    Vec3 tangent_displacement = sub(displacement, normal_displacement);
    const float tangent_speed = length(tangent_displacement) / std::max(dt, 1.0e-6f);

    if (tangent_speed < kCollisionStaticFrictionSpeed) {
        state.static_friction = std::min(1.0f, state.static_friction + kCollisionStaticFrictionBuild * step_scale);
    } else {
        const float speed_excess = tangent_speed - kCollisionStaticFrictionSpeed;
        state.static_friction = std::max(
            0.0f,
            state.static_friction - std::max(speed_excess / 0.2f, kCollisionStaticFrictionDecay) * step_scale
        );
    }
    tangent_displacement = mul(tangent_displacement, 1.0f - state.static_friction);
    displacement = add(normal_displacement, tangent_displacement);

    const float displacement_len_sq = length_squared(displacement);
    if (displacement_len_sq > 1.0e-8f) {
        const Vec3 direction = mul(displacement, 1.0f / std::sqrt(displacement_len_sq));
        float normal_alignment = dot(contact_normal, direction);
        normal_alignment = 0.5f + 0.5f * normal_alignment;
        normal_alignment *= normal_alignment;
        const float dynamic_factor = (1.0f - normal_alignment) * std::min(1.0f, state.collision_friction * kCollisionDynamicFriction);
        displacement = mul(displacement, 1.0f - dynamic_factor);
    }

    state.collision_friction *= scale_factor_for_dt(kCollisionFrictionDamping, dt);
    if (state.collision_friction <= 1.0e-4f) {
        state.collision_friction = 0.0f;
    }
    state.collision_normal = Vec3{};
    return displacement;
}

void finalize_end_step_state(
    RuntimeBoneState& state,
    const Vec3& final_displacement,
    float velocity_weight,
    float dt
) {
    state.velocity = mul(
        final_displacement,
        (1.0f / std::max(dt, 1.0e-6f)) * velocity_weight
    );
    state.previous_position = state.position;
    state.velocity_position = state.position;
}

Vec3 clamp_distance_from(const Vec3& center, const Vec3& point, float max_distance) {
    if (max_distance <= 0.0f) {
        return point;
    }

    const Vec3 offset = sub(point, center);
    const float offset_length = length(offset);
    if (offset_length <= max_distance || offset_length <= 1.0e-8f) {
        return point;
    }
    return add(center, mul(offset, max_distance / offset_length));
}

void project_bone_tail_preserve_velocity(
    Vec3 head_position,
    RuntimeBoneState& state,
    float bone_length
);

void project_bone_tail_preserve_velocity(
    Vec3 head_position,
    RuntimeBoneState& state,
    float bone_length
) {
    if (bone_length <= 1.0e-6f) {
        return;
    }

    const Vec3 previous_position = state.position;
    const Vec3 direction = sub(state.position, head_position);
    const float direction_length = length(direction);
    if (direction_length <= 1.0e-6f) {
        return;
    }

    const Vec3 projected_position = add(head_position, mul(direction, bone_length / direction_length));
    move_position_preserve_velocity(state, sub(projected_position, previous_position));
}

const BoneChainRuntimeInput* find_chain_input(const RuntimeInputs& inputs, const BoneChainDescriptor& chain) {
    for (const BoneChainRuntimeInput& input : inputs.bone_chains) {
        if (input.component_id == chain.component_id) {
            return &input;
        }
    }
    return nullptr;
}

const BoneChainRuntimeInput* find_chain_input_by_component(
    const RuntimeInputs& inputs,
    const std::string& component_id
) {
    for (const BoneChainRuntimeInput& input : inputs.bone_chains) {
        if (input.component_id == component_id) {
            return &input;
        }
    }
    return nullptr;
}

const CollisionObjectRuntimeInput* find_collision_object_input(
    const RuntimeInputs& inputs,
    const std::string& collision_object_id
) {
    for (const CollisionObjectRuntimeInput& input : inputs.collision_objects) {
        if (input.collision_object_id == collision_object_id) {
            return &input;
        }
    }
    return nullptr;
}

bool runtime_input_has_basic_positions(const BoneChainRuntimeInput* chain_input, std::size_t bone_count) {
    return chain_input != nullptr &&
        chain_input->basic_head_positions.size() >= bone_count * 3 &&
        chain_input->basic_tail_positions.size() >= bone_count * 3;
}

bool runtime_input_has_basic_rotations(const BoneChainRuntimeInput* chain_input, std::size_t bone_count) {
    return chain_input != nullptr && chain_input->basic_rotations.size() >= bone_count * 4;
}

Vec3 runtime_basic_head(const BoneChainRuntimeInput& chain_input, std::size_t bone_index) {
    const std::size_t base = bone_index * 3;
    return Vec3{
        chain_input.basic_head_positions[base],
        chain_input.basic_head_positions[base + 1],
        chain_input.basic_head_positions[base + 2],
    };
}

Vec3 runtime_basic_tail(const BoneChainRuntimeInput& chain_input, std::size_t bone_index) {
    const std::size_t base = bone_index * 3;
    return Vec3{
        chain_input.basic_tail_positions[base],
        chain_input.basic_tail_positions[base + 1],
        chain_input.basic_tail_positions[base + 2],
    };
}

Quat runtime_basic_rotation(const BoneChainRuntimeInput& chain_input, std::size_t bone_index) {
    const std::size_t base = bone_index * 4;
    return normalize_or_identity(Quat{
        chain_input.basic_rotations[base],
        chain_input.basic_rotations[base + 1],
        chain_input.basic_rotations[base + 2],
        chain_input.basic_rotations[base + 3],
    });
}

void lerp_vec3_fields(const float start[3], const float target[3], float alpha, float out[3]) {
    const Vec3 blended = lerp(make_vec3(start), make_vec3(target), alpha);
    copy_vec3(blended, out);
}

void nlerp_quat_fields(const float start[4], const float target[4], float alpha, float out[4]) {
    const Quat blended = nlerp(
        normalize_or_identity(make_quat(start)),
        normalize_or_identity(make_quat(target)),
        alpha
    );
    copy_quat(blended, out);
}

std::vector<float> lerp_vec3_triplets(const std::vector<float>& start, const std::vector<float>& target, float alpha) {
    if (start.size() != target.size() || (target.size() % 3) != 0) {
        return target;
    }
    std::vector<float> result(target.size(), 0.0f);
    for (std::size_t index = 0; index < target.size(); index += 3) {
        const Vec3 blended = lerp(
            Vec3{start[index], start[index + 1], start[index + 2]},
            Vec3{target[index], target[index + 1], target[index + 2]},
            alpha
        );
        result[index] = blended.x;
        result[index + 1] = blended.y;
        result[index + 2] = blended.z;
    }
    return result;
}

std::vector<float> nlerp_quat_quads(const std::vector<float>& start, const std::vector<float>& target, float alpha) {
    if (start.size() != target.size() || (target.size() % 4) != 0) {
        return target;
    }
    std::vector<float> result(target.size(), 0.0f);
    for (std::size_t index = 0; index < target.size(); index += 4) {
        const Quat blended = nlerp(
            normalize_or_identity(Quat{start[index], start[index + 1], start[index + 2], start[index + 3]}),
            normalize_or_identity(Quat{target[index], target[index + 1], target[index + 2], target[index + 3]}),
            alpha
        );
        result[index] = blended.w;
        result[index + 1] = blended.x;
        result[index + 2] = blended.y;
        result[index + 3] = blended.z;
    }
    return result;
}

BoneChainRuntimeInput interpolate_chain_input(
    const BoneChainRuntimeInput* start,
    const BoneChainRuntimeInput& target,
    float alpha
) {
    if (start == nullptr) {
        return target;
    }
    BoneChainRuntimeInput result = target;
    lerp_vec3_fields(start->root_translation, target.root_translation, alpha, result.root_translation);
    nlerp_quat_fields(start->root_rotation_quaternion, target.root_rotation_quaternion, alpha, result.root_rotation_quaternion);
    lerp_vec3_fields(start->root_linear_velocity, target.root_linear_velocity, alpha, result.root_linear_velocity);
    lerp_vec3_fields(start->root_scale, target.root_scale, alpha, result.root_scale);
    lerp_vec3_fields(start->center_translation, target.center_translation, alpha, result.center_translation);
    nlerp_quat_fields(start->center_rotation_quaternion, target.center_rotation_quaternion, alpha, result.center_rotation_quaternion);
    lerp_vec3_fields(start->center_linear_velocity, target.center_linear_velocity, alpha, result.center_linear_velocity);
    lerp_vec3_fields(start->center_scale, target.center_scale, alpha, result.center_scale);
    result.basic_head_positions = lerp_vec3_triplets(start->basic_head_positions, target.basic_head_positions, alpha);
    result.basic_tail_positions = lerp_vec3_triplets(start->basic_tail_positions, target.basic_tail_positions, alpha);
    result.basic_rotations = nlerp_quat_quads(start->basic_rotations, target.basic_rotations, alpha);
    return result;
}

CollisionObjectRuntimeInput interpolate_collision_object_input(
    const CollisionObjectRuntimeInput* start,
    const CollisionObjectRuntimeInput& target,
    float alpha
) {
    if (start == nullptr) {
        return target;
    }
    CollisionObjectRuntimeInput result = target;
    lerp_vec3_fields(start->world_translation, target.world_translation, alpha, result.world_translation);
    nlerp_quat_fields(start->world_rotation, target.world_rotation, alpha, result.world_rotation);
    lerp_vec3_fields(start->linear_velocity, target.linear_velocity, alpha, result.linear_velocity);
    lerp_vec3_fields(start->angular_velocity, target.angular_velocity, alpha, result.angular_velocity);
    return result;
}

RuntimeInputs build_interpolated_runtime_inputs(
    const RuntimeSceneState& scene_state,
    float alpha,
    float dt,
    std::int32_t simulation_frequency
) {
    RuntimeInputs result;
    result.dt = dt;
    result.simulation_frequency = simulation_frequency;
    result.bone_chains.reserve(scene_state.inputs.bone_chains.size());
    result.collision_objects.reserve(scene_state.inputs.collision_objects.size());
    for (const BoneChainRuntimeInput& target : scene_state.inputs.bone_chains) {
        const BoneChainRuntimeInput* start = scene_state.has_previous_inputs
            ? find_chain_input_by_component(scene_state.previous_inputs, target.component_id)
            : nullptr;
        result.bone_chains.push_back(interpolate_chain_input(start, target, alpha));
    }
    for (const CollisionObjectRuntimeInput& target : scene_state.inputs.collision_objects) {
        const CollisionObjectRuntimeInput* start = scene_state.has_previous_inputs
            ? find_collision_object_input(scene_state.previous_inputs, target.collision_object_id)
            : nullptr;
        result.collision_objects.push_back(interpolate_collision_object_input(start, target, alpha));
    }
    return result;
}

std::vector<RuntimeBoneChainState> make_chain_states(const SceneDescriptor& scene) {
    std::vector<RuntimeBoneChainState> states;
    states.reserve(scene.bone_chains.size());
    for (const BoneChainDescriptor& chain : scene.bone_chains) {
        RuntimeBoneChainState chain_state;
        chain_state.bones.resize(chain.bones.size());
        chain_state.step_basic_positions.resize(chain.bones.size());
        chain_state.step_basic_rotations.resize(chain.bones.size());
        states.push_back(std::move(chain_state));
    }
    return states;
}

bool ends_with(const std::string& value, const char* suffix) {
    const std::size_t suffix_length = std::strlen(suffix);
    return value.size() >= suffix_length &&
        value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
}

std::vector<std::vector<std::size_t>> baseline_paths_for_chain(const BoneChainDescriptor& chain) {
    std::vector<std::vector<std::size_t>> paths;
    if (!chain.baseline_start_indices.empty() &&
        chain.baseline_start_indices.size() == chain.baseline_counts.size() &&
        !chain.baseline_data.empty()) {
        paths.reserve(chain.baseline_start_indices.size());
        for (std::size_t baseline_index = 0; baseline_index < chain.baseline_start_indices.size(); ++baseline_index) {
            const std::int32_t start = chain.baseline_start_indices[baseline_index];
            const std::int32_t count = chain.baseline_counts[baseline_index];
            if (start < 0 || count <= 0) {
                continue;
            }
            const std::size_t start_index = static_cast<std::size_t>(start);
            const std::size_t end_index = start_index + static_cast<std::size_t>(count);
            if (start_index >= chain.baseline_data.size() || end_index > chain.baseline_data.size()) {
                continue;
            }
            std::vector<std::size_t> path;
            path.reserve(static_cast<std::size_t>(count));
            for (std::size_t data_index = start_index; data_index < end_index; ++data_index) {
                const std::int32_t joint_index = chain.baseline_data[data_index];
                if (joint_index < 0 || static_cast<std::size_t>(joint_index) >= chain.bones.size()) {
                    continue;
                }
                path.push_back(static_cast<std::size_t>(joint_index));
            }
            if (!path.empty()) {
                paths.push_back(std::move(path));
            }
        }
        if (!paths.empty()) {
            return paths;
        }
    }

    if (!chain.baselines.empty()) {
        paths.reserve(chain.baselines.size());
        for (const BoneBaselineDescriptor& baseline : chain.baselines) {
            std::vector<std::size_t> path;
            path.reserve(baseline.joint_indices.size());
            for (const std::int32_t joint_index : baseline.joint_indices) {
                if (joint_index < 0 || static_cast<std::size_t>(joint_index) >= chain.bones.size()) {
                    continue;
                }
                path.push_back(static_cast<std::size_t>(joint_index));
            }
            if (!path.empty()) {
                paths.push_back(std::move(path));
            }
        }
        if (!paths.empty()) {
            return paths;
        }
    }

    std::vector<std::size_t> child_counts(chain.bones.size(), 0);
    for (const BoneDescriptor& bone : chain.bones) {
        if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < child_counts.size()) {
            child_counts[static_cast<std::size_t>(bone.parent_index)] += 1;
        }
    }
    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        if (child_counts[bone_index] > 0) {
            continue;
        }
        std::vector<std::size_t> path;
        std::int32_t current_index = static_cast<std::int32_t>(bone_index);
        while (current_index >= 0 && static_cast<std::size_t>(current_index) < chain.bones.size()) {
            path.push_back(static_cast<std::size_t>(current_index));
            current_index = chain.bones[static_cast<std::size_t>(current_index)].parent_index;
        }
        std::reverse(path.begin(), path.end());
        if (!path.empty()) {
            paths.push_back(std::move(path));
        }
    }
    return paths;
}

std::vector<std::pair<std::size_t, std::size_t>> line_pairs_for_chain(const BoneChainDescriptor& chain) {
    std::vector<std::pair<std::size_t, std::size_t>> lines;
    if (!chain.line_start_indices.empty() &&
        chain.line_start_indices.size() == chain.line_counts.size() &&
        !chain.line_data.empty()) {
        lines.reserve(chain.line_data.size());
        for (std::size_t start_joint = 0; start_joint < chain.line_start_indices.size(); ++start_joint) {
            const std::int32_t start = chain.line_start_indices[start_joint];
            const std::int32_t count = chain.line_counts[start_joint];
            if (start < 0 || count <= 0 || start_joint >= chain.bones.size()) {
                continue;
            }
            const std::size_t start_index = static_cast<std::size_t>(start);
            const std::size_t end_index = start_index + static_cast<std::size_t>(count);
            if (start_index >= chain.line_data.size() || end_index > chain.line_data.size()) {
                continue;
            }
            for (std::size_t data_index = start_index; data_index < end_index; ++data_index) {
                const std::int32_t child_index = chain.line_data[data_index];
                if (child_index < 0 || static_cast<std::size_t>(child_index) >= chain.bones.size()) {
                    continue;
                }
                lines.emplace_back(start_joint, static_cast<std::size_t>(child_index));
            }
        }
        if (!lines.empty()) {
            return lines;
        }
    }

    if (!chain.lines.empty()) {
        lines.reserve(chain.lines.size());
        for (const BoneLineDescriptor& line : chain.lines) {
            if (line.start_index < 0 || line.end_index < 0) {
                continue;
            }
            const std::size_t start_index = static_cast<std::size_t>(line.start_index);
            const std::size_t end_index = static_cast<std::size_t>(line.end_index);
            if (start_index >= chain.bones.size() || end_index >= chain.bones.size()) {
                continue;
            }
            lines.emplace_back(start_index, end_index);
        }
        if (!lines.empty()) {
            return lines;
        }
    }

    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        const BoneDescriptor& bone = chain.bones[bone_index];
        if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain.bones.size()) {
            lines.emplace_back(static_cast<std::size_t>(bone.parent_index), bone_index);
        }
    }
    return lines;
}

BaselineStepBuffers build_baseline_step_buffers(
    const BoneChainDescriptor& chain,
    const RuntimeBoneChainState& chain_state,
    const BoneChainRuntimeInput* chain_input,
    const Vec3& root_translation,
    const std::vector<std::vector<std::size_t>>& baseline_paths
) {
    BaselineStepBuffers buffers;

    if (!chain.baseline_start_indices.empty() &&
        chain.baseline_start_indices.size() == chain.baseline_counts.size() &&
        !chain.baseline_data.empty()) {
        buffers.start_indices.assign(chain.baseline_start_indices.begin(), chain.baseline_start_indices.end());
        buffers.counts.assign(chain.baseline_counts.begin(), chain.baseline_counts.end());
        buffers.joint_indices.assign(chain.baseline_data.begin(), chain.baseline_data.end());
    } else {
        for (const std::vector<std::size_t>& path : baseline_paths) {
            buffers.start_indices.push_back(static_cast<std::int32_t>(buffers.joint_indices.size()));
            buffers.counts.push_back(static_cast<std::int32_t>(path.size()));
            for (const std::size_t joint_index : path) {
                buffers.joint_indices.push_back(static_cast<std::int32_t>(joint_index));
            }
        }
    }

    buffers.joint_visit_counts.resize(chain.bones.size(), 0);
    for (const std::int32_t joint_index : buffers.joint_indices) {
        if (joint_index >= 0 && static_cast<std::size_t>(joint_index) < buffers.joint_visit_counts.size()) {
            buffers.joint_visit_counts[static_cast<std::size_t>(joint_index)] += 1;
        }
    }

    buffers.rotation_buffer.resize(buffers.joint_indices.size(), Quat{});
    buffers.segment_lengths.resize(buffers.joint_indices.size(), 0.0f);
    buffers.local_positions.resize(buffers.joint_indices.size(), Vec3{});
    buffers.local_rotations.resize(buffers.joint_indices.size(), Quat{});
    buffers.limit_vectors.resize(buffers.joint_indices.size(), Vec3{});
    buffers.restoration_vectors.resize(buffers.joint_indices.size(), Vec3{});
    const bool has_runtime_basic_positions = runtime_input_has_basic_positions(chain_input, chain.bones.size());

    for (std::size_t baseline_index = 0; baseline_index < buffers.start_indices.size(); ++baseline_index) {
        const std::int32_t start = buffers.start_indices[baseline_index];
        const std::int32_t count = baseline_index < buffers.counts.size() ? buffers.counts[baseline_index] : 0;
        if (start < 0 || count <= 0) {
            continue;
        }
        const std::size_t start_index = static_cast<std::size_t>(start);
        const std::size_t end_index = start_index + static_cast<std::size_t>(count);
        if (start_index >= buffers.joint_indices.size() || end_index > buffers.joint_indices.size()) {
            continue;
        }

        for (std::size_t data_index = start_index; data_index < end_index; ++data_index) {
            const std::int32_t joint_index = buffers.joint_indices[data_index];
            if (joint_index < 0 || static_cast<std::size_t>(joint_index) >= chain.bones.size()) {
                continue;
            }

            const BoneDescriptor& bone = chain.bones[static_cast<std::size_t>(joint_index)];
            Quat baseline_rotation = Quat{};
            if (static_cast<std::size_t>(joint_index) < chain_state.step_basic_rotations.size()) {
                baseline_rotation = chain_state.step_basic_rotations[static_cast<std::size_t>(joint_index)];
            } else {
                Quat parent_rotation = Quat{};
                if (data_index > start_index) {
                    parent_rotation = buffers.rotation_buffer[data_index - 1];
                } else if (bone.parent_index >= 0 &&
                    static_cast<std::size_t>(bone.parent_index) < chain_state.step_basic_rotations.size()) {
                    parent_rotation = chain_state.step_basic_rotations[static_cast<std::size_t>(bone.parent_index)];
                }
                baseline_rotation = normalize_or_identity(
                    quat_multiply(parent_rotation, make_quat(bone.rest_local_rotation))
                );
            }
            buffers.rotation_buffer[data_index] = baseline_rotation;

            Vec3 limit_vector = Vec3{};
            Vec3 restoration_vector = Vec3{};
            if (data_index > start_index) {
                const std::int32_t parent_joint_index = buffers.joint_indices[data_index - 1];
                Vec3 basic_head = root_translation;
                Vec3 current_head = root_translation;
                if (has_runtime_basic_positions && chain_input != nullptr) {
                    basic_head = runtime_basic_head(*chain_input, static_cast<std::size_t>(joint_index));
                }
                if (parent_joint_index >= 0 &&
                    !has_runtime_basic_positions &&
                    static_cast<std::size_t>(parent_joint_index) < chain_state.step_basic_positions.size()) {
                    basic_head = chain_state.step_basic_positions[static_cast<std::size_t>(parent_joint_index)];
                }
                if (parent_joint_index >= 0 &&
                    static_cast<std::size_t>(parent_joint_index) < chain_state.bones.size()) {
                    current_head = chain_state.bones[static_cast<std::size_t>(parent_joint_index)].position;
                }
                Vec3 basic_tail = basic_head;
                Vec3 current_tail = current_head;
                if (static_cast<std::size_t>(joint_index) < chain_state.step_basic_positions.size()) {
                    basic_tail = chain_state.step_basic_positions[static_cast<std::size_t>(joint_index)];
                }
                if (static_cast<std::size_t>(joint_index) < chain_state.bones.size()) {
                    current_tail = chain_state.bones[static_cast<std::size_t>(joint_index)].position;
                }
                const Vec3 position_direction = sub(basic_tail, basic_head);
                const float position_length = length(position_direction);
                const float current_length = length(sub(current_tail, current_head));
                limit_vector = position_direction;
                restoration_vector = position_direction;

                Quat parent_rotation = Quat{};
                if (parent_joint_index >= 0 &&
                    static_cast<std::size_t>(parent_joint_index) < chain_state.step_basic_rotations.size()) {
                    parent_rotation = chain_state.step_basic_rotations[static_cast<std::size_t>(parent_joint_index)];
                }
                const Quat inv_parent_rotation = conjugate(normalize_or_identity(parent_rotation));
                const Vec3 normalized_position = position_length > 1.0e-6f
                    ? mul(position_direction, 1.0f / position_length)
                    : Vec3{0.0f, 1.0f, 0.0f};
                buffers.segment_lengths[data_index] = std::max(1.0e-5f, current_length > 1.0e-6f ? current_length : bone.length);
                buffers.local_positions[data_index] = rotate_vector(inv_parent_rotation, normalized_position);
                buffers.local_rotations[data_index] = normalize_or_identity(
                    quat_multiply(inv_parent_rotation, baseline_rotation)
                );
            }
            buffers.limit_vectors[data_index] = limit_vector;
            buffers.restoration_vectors[data_index] = restoration_vector;
        }
    }

    return buffers;
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
    const Vec3 center_translation = chain_input != nullptr
        ? make_vec3(chain_input->center_translation)
        : root_translation;

    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        const BoneDescriptor& bone = chain.bones[bone_index];
        RuntimeBoneState& state = chain_state.bones[bone_index];
        Vec3 initial_tail = rest_tail_world(bone, root_translation, root_rotation, root_rest_head);
        if (runtime_input_has_basic_positions(chain_input, chain.bones.size())) {
            initial_tail = runtime_basic_tail(*chain_input, bone_index);
        }
        state.position = initial_tail;
        state.base_position = state.position;
        state.previous_position = state.position;
        state.velocity_position = state.position;
        state.velocity = Vec3{};
        state.collision_normal = Vec3{};
        state.collision_friction = 0.0f;
        state.static_friction = 0.0f;
        state.stretch_lambda = 0.0f;
        chain_state.step_basic_positions[bone_index] = state.position;
        if (runtime_input_has_basic_rotations(chain_input, chain.bones.size())) {
            chain_state.step_basic_rotations[bone_index] = runtime_basic_rotation(*chain_input, bone_index);
        } else {
            chain_state.step_basic_rotations[bone_index] = Quat{};
        }
    }

    chain_state.last_root_translation = root_translation;
    chain_state.last_root_rotation = root_rotation;
    chain_state.last_center_translation = center_translation;
    chain_state.velocity_weight = kResetStabilizationTime > 1.0e-6f ? 0.0f : 1.0f;
    chain_state.blend_weight = chain_state.velocity_weight;
    chain_state.initialized = true;
}

void update_step_basic_pose_for_chain(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const BoneChainRuntimeInput* chain_input,
    const Vec3& root_translation,
    const Quat& root_rotation,
    const Vec3& root_rest_head,
    const std::vector<std::vector<std::size_t>>& baseline_paths
) {
    if (chain_state.step_basic_positions.size() != chain.bones.size()) {
        chain_state.step_basic_positions.resize(chain.bones.size());
    }
    if (chain_state.step_basic_rotations.size() != chain.bones.size()) {
        chain_state.step_basic_rotations.resize(chain.bones.size());
    }

    const bool has_basic_positions = runtime_input_has_basic_positions(chain_input, chain.bones.size());
    const bool has_basic_rotations = runtime_input_has_basic_rotations(chain_input, chain.bones.size());
    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        const BoneDescriptor& bone = chain.bones[bone_index];
        chain_state.step_basic_positions[bone_index] = has_basic_positions
            ? runtime_basic_tail(*chain_input, bone_index)
            : rest_tail_world(bone, root_translation, root_rotation, root_rest_head);
        chain_state.step_basic_rotations[bone_index] = has_basic_rotations
            ? runtime_basic_rotation(*chain_input, bone_index)
            : Quat{};
    }

    if (has_basic_positions && has_basic_rotations) {
        return;
    }

    for (const std::vector<std::size_t>& path : baseline_paths) {
        for (std::size_t path_index = 0; path_index < path.size(); ++path_index) {
            const std::size_t bone_index = path[path_index];
            const BoneDescriptor& bone = chain.bones[bone_index];
            if (has_basic_rotations) {
                continue;
            }
            Quat basic_rotation = normalize_or_identity(quat_multiply(root_rotation, make_quat(bone.rest_local_rotation)));
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.step_basic_rotations.size()) {
                basic_rotation = normalize_or_identity(
                    quat_multiply(
                        chain_state.step_basic_rotations[static_cast<std::size_t>(bone.parent_index)],
                        make_quat(bone.rest_local_rotation)
                    )
                );
            }
            chain_state.step_basic_rotations[bone_index] = basic_rotation;
        }
    }
}

ColliderStepWorkData build_collider_step_work_data(const CollisionWorldObject& collider) {
    ColliderStepWorkData work;
    work.motion_type = collider.motion_type;
    work.shape_type = collider.shape_type;
    work.previous_translation = make_vec3(collider.previous_world_translation);
    work.previous_rotation = normalize_or_identity(make_quat(collider.previous_world_rotation));
    work.current_translation = make_vec3(collider.world_translation);
    work.current_rotation = normalize_or_identity(make_quat(collider.world_rotation));
    work.radius = collider.radius;
    work.height = collider.height;
    if (work.shape_type == "CAPSULE") {
        const float half_height = std::max(0.0f, work.height) * 0.5f;
        work.previous_axis = rotate_vector(work.previous_rotation, Vec3{0.0f, 1.0f, 0.0f});
        work.previous_point_a = add(work.previous_translation, mul(work.previous_axis, -half_height));
        work.previous_point_b = add(work.previous_translation, mul(work.previous_axis, half_height));
        work.current_axis = rotate_vector(work.current_rotation, Vec3{0.0f, 1.0f, 0.0f});
        work.current_point_a = add(work.current_translation, mul(work.current_axis, -half_height));
        work.current_point_b = add(work.current_translation, mul(work.current_axis, half_height));
    } else {
        work.previous_axis = Vec3{};
        work.current_axis = Vec3{};
        work.previous_point_a = work.previous_translation;
        work.previous_point_b = work.previous_translation;
        work.current_point_a = work.current_translation;
        work.current_point_b = work.current_translation;
    }
    const Vec3 radius_extent{work.radius, work.radius, work.radius};
    Vec3 point_min = min_components(
        min_components(work.previous_point_a, work.previous_point_b),
        min_components(work.current_point_a, work.current_point_b)
    );
    Vec3 point_max = max_components(
        max_components(work.previous_point_a, work.previous_point_b),
        max_components(work.current_point_a, work.current_point_b)
    );
    work.swept_aabb_min = sub(point_min, radius_extent);
    work.swept_aabb_max = add(point_max, radius_extent);
    return work;
}

std::vector<ColliderStepWorkData> build_collider_work_data_for_chain(
    const RuntimeSceneState& scene_state,
    const BoneChainDescriptor& chain
) {
    std::vector<ColliderStepWorkData> result;
    if (chain.collision_binding_ids.empty() || scene_state.collision_world.binding_count() == 0) {
        return result;
    }

    for (const std::string& binding_id : chain.collision_binding_ids) {
        for (const CollisionWorldBinding& binding : scene_state.collision_world.bindings()) {
            if (binding.binding_id != binding_id) {
                continue;
            }
            for (const std::size_t object_index : binding.object_indices) {
                if (object_index >= scene_state.collision_world.objects().size()) {
                    continue;
                }
                result.push_back(build_collider_step_work_data(scene_state.collision_world.objects()[object_index]));
            }
        }
    }
    return result;
}

const CollisionObjectStepStart* find_collision_start(
    const std::vector<CollisionObjectStepStart>& starts,
    const std::string& collision_object_id
) {
    for (const CollisionObjectStepStart& start : starts) {
        if (start.collision_object_id == collision_object_id) {
            return &start;
        }
    }
    return nullptr;
}

void apply_collision_inputs_for_step(
    RuntimeSceneState& scene_state,
    const RuntimeInputs& inputs,
    const std::vector<CollisionObjectStepStart>& starts,
    float previous_alpha,
    float alpha
) {
    for (const CollisionObjectRuntimeInput& collision_input : inputs.collision_objects) {
        CollisionWorldObject* collision_object = scene_state.collision_world.find_object(collision_input.collision_object_id);
        if (collision_object == nullptr) {
            continue;
        }
        const CollisionObjectStepStart* start = find_collision_start(starts, collision_input.collision_object_id);
        const Vec3 target_translation = make_vec3(collision_input.world_translation);
        const Quat target_rotation = normalize_or_identity(make_quat(collision_input.world_rotation));

        // Pose at the start of this fixed step.
        const Vec3 prev_translation = start != nullptr
            ? lerp(start->translation, target_translation, previous_alpha)
            : target_translation;
        const Quat prev_rotation = start != nullptr
            ? nlerp(start->rotation, target_rotation, previous_alpha)
            : target_rotation;
        copy_vec3(prev_translation, collision_object->previous_world_translation);
        copy_quat(prev_rotation, collision_object->previous_world_rotation);

        // Pose at the end of this fixed step.
        const Vec3 translation = start != nullptr
            ? lerp(start->translation, target_translation, alpha)
            : target_translation;
        const Quat rotation = start != nullptr
            ? nlerp(start->rotation, target_rotation, alpha)
            : target_rotation;
        copy_vec3(translation, collision_object->world_translation);
        copy_quat(rotation, collision_object->world_rotation);

        collision_object->linear_velocity[0] = collision_input.linear_velocity[0];
        collision_object->linear_velocity[1] = collision_input.linear_velocity[1];
        collision_object->linear_velocity[2] = collision_input.linear_velocity[2];
        collision_object->angular_velocity[0] = collision_input.angular_velocity[0];
        collision_object->angular_velocity[1] = collision_input.angular_velocity[1];
        collision_object->angular_velocity[2] = collision_input.angular_velocity[2];
    }
}

ColliderContact evaluate_collider_contact(
    Vec3 head_position,
    const RuntimeBoneState& state,
    float bone_length,
    float particle_radius,
    const ColliderStepWorkData& collider
) {
    const Vec3 bone_tail = state.position;
    const bool use_bone_capsule = bone_length > 1.0e-6f && particle_radius > 0.0f;
    const Vec3 center = collider.current_translation;
    Vec3 closest = center;
    Vec3 bone_closest = bone_tail;
    float bone_t = 1.0f;
    if (collider.shape_type == "CAPSULE") {
        const Vec3 a = collider.current_point_a;
        const Vec3 b = collider.current_point_b;
        if (use_bone_capsule) {
            const auto segment_pair = closest_points_between_segments(head_position, bone_tail, a, b);
            bone_closest = segment_pair.first;
            closest = segment_pair.second;
            const float bone_segment_length_sq = length_squared(sub(bone_tail, head_position));
            if (bone_segment_length_sq > 1.0e-8f) {
                bone_t = std::clamp(
                    dot(sub(bone_closest, head_position), sub(bone_tail, head_position)) / bone_segment_length_sq,
                    0.0f,
                    1.0f
                );
            }
        } else {
            closest = closest_point_on_segment(state.position, a, b);
        }
    } else if (use_bone_capsule) {
        bone_closest = closest_point_on_segment(center, head_position, bone_tail);
        const float bone_segment_length_sq = length_squared(sub(bone_tail, head_position));
        if (bone_segment_length_sq > 1.0e-8f) {
            bone_t = std::clamp(
                dot(sub(bone_closest, head_position), sub(bone_tail, head_position)) / bone_segment_length_sq,
                0.0f,
                1.0f
            );
        }
    }

    Vec3 delta = sub(bone_closest, closest);
    float distance = length(delta);
    const float target_radius = std::max(0.0f, collider.radius) + std::max(0.0f, particle_radius);
    if (distance <= 1.0e-6f) {
        Vec3 fallback = sub(bone_tail, head_position);
        if (length_squared(fallback) <= 1.0e-8f) {
            fallback = state.velocity;
        }
        if (length_squared(fallback) <= 1.0e-8f) {
            fallback = Vec3{0.0f, 1.0f, 0.0f};
        }
        delta = normalize_or_default(fallback, Vec3{0.0f, 1.0f, 0.0f});
        distance = 0.0f;
    } else {
        delta = mul(delta, 1.0f / distance);
    }

    ColliderContact contact;
    contact.closest = closest;
    contact.bone_closest = bone_closest;
    contact.normal = delta;
    contact.distance = distance;
    contact.target_radius = target_radius;
    contact.overlapping = distance < (target_radius - kCollisionSlop);
    contact.bone_t = bone_t;
    return contact;
}

Vec3 collider_axis_closest_point_at(
    const Vec3& a,
    const Vec3& b,
    const Vec3& fallback_translation,
    const std::string& shape_type,
    const Vec3& point
) {
    if (shape_type == "CAPSULE") {
        return closest_point_on_segment(point, a, b);
    }
    return fallback_translation;  // SPHERE and fallback.
}

bool compute_contact_push(
    const ColliderContact& contact,
    const ColliderStepWorkData& collider,
    float dt,
    Vec3& out_normal,
    float& out_push_distance,
    float& out_penetration_ratio
) {
    out_normal = contact.normal;
    out_push_distance = 0.0f;
    out_penetration_ratio = 0.0f;

    // If not overlapping in the usual sense, we still may have a kinematic "moving plane"
    // overlap. We'll check that below when we have motion.
    const Vec3 prev_translation = collider.previous_translation;
    const Vec3 curr_translation = collider.current_translation;

    const Vec3 old_axis = collider_axis_closest_point_at(
        collider.previous_point_a,
        collider.previous_point_b,
        prev_translation,
        collider.shape_type,
        contact.bone_closest
    );
    const Vec3 new_axis = collider_axis_closest_point_at(
        collider.current_point_a,
        collider.current_point_b,
        curr_translation,
        collider.shape_type,
        contact.bone_closest
    );
    const Vec3 axis_motion = sub(new_axis, old_axis);
    const float axis_motion_len = length(axis_motion);

    // Only treat as "moving kinematic" if it meaningfully moved this step.
    const bool moving_kinematic = collider.motion_type != "STATIC" && axis_motion_len > 1.0e-6f;

    if (moving_kinematic) {
        // MC2-style: compute a push plane from the particle's position relative to the collider's
        // previous pose, then apply it at the collider's current pose. This tends to avoid the
        // "hard radial push" look for animated colliders.
        const Vec3 plane_normal = normalize_or_default(sub(contact.bone_closest, old_axis), contact.normal);
        const Vec3 plane_point = add(new_axis, mul(plane_normal, contact.target_radius));
        const float plane_dist = dot(plane_normal, sub(contact.bone_closest, plane_point));
        if (plane_dist >= -kCollisionSlop) {
            return false;
        }

        const float penetration = -plane_dist;
        out_penetration_ratio = penetration / std::max(contact.target_radius, 1.0e-6f);
        out_normal = plane_normal;

        // MC2 pushes to the moving collider plane first; SpringBone softness is applied later
        // against base_position/limitDistance, not by weakening this geometric projection.
        out_push_distance = std::max(0.0f, penetration);
        return out_push_distance > 0.0f;
    }

    if (!contact.overlapping) {
        return false;
    }

    // Static / non-moving: radial correction.
    const float penetration = std::max(0.0f, contact.target_radius - contact.distance);
    out_penetration_ratio = penetration / std::max(contact.target_radius, 1.0e-6f);
    out_push_distance = penetration;
    (void)dt;
    return out_push_distance > 0.0f;
}

bool bone_segment_intersects_swept_aabb(
    Vec3 head_position,
    const RuntimeBoneState& state,
    float particle_radius,
    const ColliderStepWorkData& collider
) {
    const Vec3 radius_extent{particle_radius, particle_radius, particle_radius};
    const Vec3 segment_min = sub(min_components(head_position, state.position), radius_extent);
    const Vec3 segment_max = add(max_components(head_position, state.position), radius_extent);
    return !(segment_max.x < collider.swept_aabb_min.x || segment_min.x > collider.swept_aabb_max.x ||
        segment_max.y < collider.swept_aabb_min.y || segment_min.y > collider.swept_aabb_max.y ||
        segment_max.z < collider.swept_aabb_min.z || segment_min.z > collider.swept_aabb_max.z);
}

void accumulate_collider_contacts(
    Vec3 head_position,
    const RuntimeBoneState& state,
    float bone_length,
    float particle_radius,
    const std::vector<ColliderStepWorkData>& colliders,
    float dt,
    CollisionAccumulation& accumulation
) {
    const float friction_range = std::max(1.0e-4f, particle_radius);
    for (const ColliderStepWorkData& collider : colliders) {
        if (!bone_segment_intersects_swept_aabb(head_position, state, particle_radius, collider)) {
            continue;
        }
        const ColliderContact contact = evaluate_collider_contact(head_position, state, bone_length, particle_radius, collider);

        const float surface_dist = std::max(0.0f, contact.distance - contact.target_radius);
        const float local_friction = 1.0f - std::clamp(surface_dist / friction_range, 0.0f, 1.0f);

        Vec3 push_normal = contact.normal;
        float push_distance = 0.0f;
        float penetration_ratio = 0.0f;
        const bool should_push = compute_contact_push(contact, collider, dt, push_normal, push_distance, penetration_ratio);

        const float effective_friction = std::max(local_friction, penetration_ratio);
        if (effective_friction > 1.0e-6f) {
            accumulation.friction_max = std::max(accumulation.friction_max, effective_friction);
            accumulation.friction_normal_sum = add(
                accumulation.friction_normal_sum,
                mul(push_normal, effective_friction)
            );
        }

        if (!should_push) {
            continue;
        }

        const float tail_weight = contact.bone_t >= 0.999f
            ? 1.0f
            : std::clamp(contact.bone_t * 0.85f, 0.08f, 0.85f);
        const Vec3 correction = mul(push_normal, push_distance * tail_weight);
        accumulation.correction_sum = add(accumulation.correction_sum, correction);
        accumulation.normal_sum = add(accumulation.normal_sum, push_normal);
        accumulation.correction_count += 1;
    }
}

void apply_soft_collider_correction(
    Vec3 head_position,
    RuntimeBoneState& state,
    float bone_length,
    float particle_radius,
    const CollisionAccumulation& accumulation
) {
    if (accumulation.correction_count <= 0) {
        return;
    }

    const Vec3 average_normal = mul(accumulation.normal_sum, 1.0f / static_cast<float>(accumulation.correction_count));
    const float average_normal_length = length(average_normal);
    if (average_normal_length <= 1.0e-6f) {
        return;
    }

    const float normal_scale = std::min(average_normal_length, 1.0f);
    const Vec3 average_correction = mul(
        accumulation.correction_sum,
        (1.0f / static_cast<float>(accumulation.correction_count)) * normal_scale
    );
    const Vec3 old_position = state.position;
    Vec3 solved_position = add(state.position, average_correction);

    const float max_length = std::max(
        1.0e-4f,
        std::max(kSpringCollisionLimitDistance, std::max(particle_radius * 2.0f, bone_length * 0.12f))
    );
    solved_position = clamp_distance_from(state.base_position, solved_position, max_length);
    const float base_distance = length(sub(solved_position, state.base_position));
    float softness = std::clamp(base_distance / std::max(particle_radius, 1.0e-5f), 0.0f, 1.0f);
    softness = 0.85f * softness;
    solved_position = lerp(solved_position, old_position, softness);

    move_position_preserve_velocity(state, sub(solved_position, state.position));
    project_bone_tail_preserve_velocity(head_position, state, bone_length);
}

void apply_collider_constraints(
    Vec3 head_position,
    RuntimeBoneState& state,
    float bone_length,
    float particle_radius,
    const std::vector<ColliderStepWorkData>& colliders,
    float dt
) {
    if (colliders.empty()) {
        return;
    }

    CollisionAccumulation accumulation;
    accumulate_collider_contacts(
        head_position,
        state,
        bone_length,
        particle_radius,
        colliders,
        dt,
        accumulation
    );
    apply_soft_collider_correction(
        head_position,
        state,
        bone_length,
        particle_radius,
        accumulation
    );

    if (accumulation.friction_max > 1.0e-6f && length_squared(accumulation.friction_normal_sum) > 1.0e-8f) {
        record_collision_contact(state, accumulation.friction_normal_sum, accumulation.friction_max);
    }
}

void project_distance_constraint(
    Vec3 head_position,
    RuntimeBoneState& state,
    float rest_length,
    float stiffness,
    float dt
) {
    const Vec3 delta = sub(state.position, head_position);
    const float current_length = length(delta);
    if (current_length <= 1.0e-6f || rest_length <= 1.0e-6f) {
        return;
    }

    const Vec3 gradient = mul(delta, 1.0f / current_length);
    const float correction_distance = (rest_length - current_length) * fraction_for_dt(stiffness, dt);
    move_position_with_velocity_attenuation(
        state,
        mul(gradient, correction_distance),
        kDistanceVelocityAttenuation
    );
}

void project_directional_alignment(
    Vec3 head_position,
    RuntimeBoneState& state,
    const Vec3& target_direction,
    float rest_length,
    float weight
) {
    const float target_length = length(target_direction);
    if (target_length <= 1.0e-6f || rest_length <= 1.0e-6f) {
        return;
    }

    const Vec3 desired_tail = add(head_position, mul(target_direction, rest_length / target_length));
    const Vec3 aligned_position = add(
        mul(state.position, 1.0f - weight),
        mul(desired_tail, weight)
    );
    move_position_with_velocity_attenuation(
        state,
        sub(aligned_position, state.position),
        kDirectionVelocityAttenuation
    );
}

void apply_pair_directional_constraint(
    RuntimeBoneState* parent_state,
    RuntimeBoneState& child_state,
    const Vec3& head_position,
    const Vec3& target_direction,
    float rest_length,
    float weight,
    float velocity_attenuation,
    float rotation_center_ratio,
    float parent_inverse_mass,
    float child_inverse_mass
) {
    const float target_length = length(target_direction);
    if (target_length <= 1.0e-6f || rest_length <= 1.0e-6f || weight <= 1.0e-6f) {
        return;
    }

    const Vec3 desired_vector = mul(target_direction, rest_length / target_length);
    const Vec3 current_vector = sub(child_state.position, head_position);
    const Vec3 blended_vector = add(
        mul(current_vector, 1.0f - weight),
        mul(desired_vector, weight)
    );

    const Vec3 rotation_center = add(head_position, mul(current_vector, std::clamp(rotation_center_ratio, 0.0f, 0.5f)));
    const Vec3 target_parent = sub(rotation_center, mul(blended_vector, std::clamp(rotation_center_ratio, 0.0f, 0.5f)));
    const Vec3 target_child = add(rotation_center, mul(blended_vector, 1.0f - std::clamp(rotation_center_ratio, 0.0f, 0.5f)));

    const float total_inverse_mass = std::max(parent_inverse_mass + child_inverse_mass, 1.0e-6f);
    const float parent_directional_correction_scale = parent_inverse_mass / total_inverse_mass;
    const float child_directional_correction_scale = child_inverse_mass / total_inverse_mass;

    if (parent_state != nullptr) {
        move_position_with_velocity_attenuation(
            *parent_state,
            mul(sub(target_parent, parent_state->position), parent_directional_correction_scale),
            velocity_attenuation
        );
    }
    move_position_with_velocity_attenuation(
        child_state,
        mul(sub(target_child, child_state.position), child_directional_correction_scale),
        velocity_attenuation
    );
}

struct DirectionConstraintContext {
    std::size_t bone_index = 0;
    Vec3 head_position;
    RuntimeBoneState* parent_state = nullptr;
    Vec3 basic_direction;
    Vec3 limit_direction;
    Quat parent_rotation;
    Quat local_rotation;
    float stiffness_factor = 0.0f;
    float current_length = 0.0f;
    float parent_inverse_mass = 0.0f;
    float child_inverse_mass = 0.0f;
    float visit_scale = 1.0f;
    float max_angle_rad = 0.0f;
};

bool build_direction_constraint_context(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    BaselineStepBuffers& baseline_buffers,
    std::size_t buffer_index,
    const Vec3& root_translation,
    DirectionConstraintContext& context
) {
    if (buffer_index >= baseline_buffers.joint_indices.size()) {
        return false;
    }
    const std::int32_t raw_joint_index = baseline_buffers.joint_indices[buffer_index];
    if (raw_joint_index < 0 || static_cast<std::size_t>(raw_joint_index) >= chain.bones.size()) {
        return false;
    }
    const std::size_t bone_index = static_cast<std::size_t>(raw_joint_index);
    const BoneDescriptor& bone = chain.bones[bone_index];
    const float stiffness = std::clamp(bone.stiffness, 0.0f, 1.0f);
    const float stiffness_factor = stiffness;
    Vec3 head_position = root_translation;
    RuntimeBoneState* parent_state = nullptr;
    if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
        head_position = chain_state.bones[static_cast<std::size_t>(bone.parent_index)].position;
        if (chain.bones[static_cast<std::size_t>(bone.parent_index)].parent_index >= 0) {
            parent_state = &chain_state.bones[static_cast<std::size_t>(bone.parent_index)];
        }
    }

    if (bone_index == 0) {
        return false;
    }
    const Vec3 basic_direction = baseline_buffers.restoration_vectors[buffer_index];
    RuntimeBoneState& state = chain_state.bones[bone_index];
    const Vec3 current_direction = sub(state.position, head_position);
    const float current_length = std::max(1.0e-5f, length(current_direction));
    Quat parent_rotation = Quat{};
    if (buffer_index > 0) {
        parent_rotation = baseline_buffers.rotation_buffer[buffer_index - 1];
    } else if (bone.parent_index >= 0 &&
        static_cast<std::size_t>(bone.parent_index) < chain_state.step_basic_rotations.size()) {
        parent_rotation = chain_state.step_basic_rotations[static_cast<std::size_t>(bone.parent_index)];
    }
    const Vec3 local_position = baseline_buffers.local_positions[buffer_index];
    const Quat local_rotation = baseline_buffers.local_rotations[buffer_index];
    const float buffered_length = baseline_buffers.segment_lengths[buffer_index] > 1.0e-6f
        ? baseline_buffers.segment_lengths[buffer_index]
        : std::max(1.0e-5f, bone.length);
    const Vec3 raw_limit_direction = rotate_vector(parent_rotation, local_position);
    const float raw_limit_length = length(raw_limit_direction);
    const float blended_limit_length = current_length * 0.5f + buffered_length * 0.5f;
    const Vec3 limit_direction = raw_limit_length > 1.0e-6f
        ? mul(raw_limit_direction, blended_limit_length / raw_limit_length)
        : basic_direction;
    std::int32_t max_bone_depth = 0;
    for (const BoneDescriptor& depth_bone : chain.bones) {
        max_bone_depth = std::max(max_bone_depth, depth_bone.depth);
    }
    const float depth_ratio = chain.bones.empty()
        ? 0.0f
        : std::clamp(
            static_cast<float>(bone.depth + 1) /
                static_cast<float>(std::max<std::int32_t>(1, max_bone_depth + 1)),
            0.0f,
            1.0f
        );
    const float child_inverse_mass = calc_inverse_mass(depth_ratio);
    float parent_inverse_mass = 0.0f;
    if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain.bones.size()) {
        const BoneDescriptor& parent_bone = chain.bones[static_cast<std::size_t>(bone.parent_index)];
        const float parent_depth_ratio = chain.bones.empty()
            ? 0.0f
            : std::clamp(
                static_cast<float>(parent_bone.depth + 1) /
                    static_cast<float>(std::max<std::int32_t>(1, max_bone_depth + 1)),
                0.0f,
                1.0f
            );
        parent_inverse_mass = calc_inverse_mass(parent_depth_ratio);
    }
    float visit_scale = 1.0f;
    if (bone_index < baseline_buffers.joint_visit_counts.size()) {
        const float visit_count = static_cast<float>(std::max(1, baseline_buffers.joint_visit_counts[bone_index]));
        const float normalized_visit_scale = 1.0f / std::sqrt(visit_count);
        visit_scale = (1.0f - depth_ratio) + depth_ratio * normalized_visit_scale;
    }
    context.bone_index = bone_index;
    context.head_position = head_position;
    context.parent_state = parent_state;
    context.basic_direction = basic_direction;
    context.limit_direction = limit_direction;
    context.parent_rotation = parent_rotation;
    context.local_rotation = local_rotation;
    context.stiffness_factor = stiffness_factor;
    context.current_length = current_length;
    context.parent_inverse_mass = parent_inverse_mass;
    context.child_inverse_mass = child_inverse_mass;
    context.visit_scale = visit_scale;
    context.max_angle_rad = std::clamp(1.25f - stiffness_factor * 0.35f, 0.45f, 1.25f);
    return true;
}

void solve_limit_constraint_for_buffer_entry(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    BaselineStepBuffers& baseline_buffers,
    std::size_t buffer_index,
    const Vec3& root_translation,
    float dt,
    float iterations,
    float iteration_ratio
) {
    DirectionConstraintContext context;
    if (!build_direction_constraint_context(chain, chain_state, baseline_buffers, buffer_index, root_translation, context)) {
        return;
    }

    RuntimeBoneState& state = chain_state.bones[context.bone_index];
    const BoneDescriptor& bone = chain.bones[context.bone_index];
    const Vec3 current_direction = sub(state.position, context.head_position);
    const float current_angle = angle_between_vectors(current_direction, context.limit_direction);
    if (length(context.limit_direction) <= 1.0e-6f || current_angle <= context.max_angle_rad) {
        baseline_buffers.rotation_buffer[buffer_index] = normalize_or_identity(
            quat_multiply(context.parent_rotation, context.local_rotation)
        );
        return;
    }

    const float target_fraction = std::clamp(
        1.0f - (context.max_angle_rad / std::max(current_angle, 1.0e-6f)),
        0.0f,
        1.0f
    );
    const Vec3 clamped_direction = slerp_direction(
        current_direction,
        context.limit_direction,
        target_fraction,
        context.current_length
    );
    const float limit_weight =
        (fraction_for_dt(0.12f + context.stiffness_factor * 0.16f, dt) / iterations) * context.visit_scale;
    const float limit_rotation_center_ratio =
        std::min(0.5f, 0.18f + 0.32f * std::clamp(iteration_ratio, 0.0f, 1.0f));
    apply_pair_directional_constraint(
        context.parent_state,
        state,
        context.head_position,
        clamped_direction,
        std::max(1.0e-5f, bone.length),
        limit_weight,
        kAngleLimitVelocityAttenuation,
        limit_rotation_center_ratio,
        context.parent_inverse_mass,
        context.child_inverse_mass
    );

    Vec3 updated_head_position = context.head_position;
    if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
        updated_head_position = chain_state.bones[static_cast<std::size_t>(bone.parent_index)].position;
    }
    const Vec3 updated_direction = sub(state.position, updated_head_position);
    const Quat base_rotation = normalize_or_identity(quat_multiply(context.parent_rotation, context.local_rotation));
    const Quat swing = quat_between_vectors(context.limit_direction, updated_direction);
    baseline_buffers.rotation_buffer[buffer_index] = normalize_or_identity(quat_multiply(swing, base_rotation));
}

void solve_restoration_constraint_for_buffer_entry(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    BaselineStepBuffers& baseline_buffers,
    std::size_t buffer_index,
    const Vec3& root_translation,
    float dt,
    float iterations,
    float iteration_ratio,
    float simulation_power_alignment
) {
    DirectionConstraintContext context;
    if (!build_direction_constraint_context(chain, chain_state, baseline_buffers, buffer_index, root_translation, context)) {
        return;
    }

    RuntimeBoneState& state = chain_state.bones[context.bone_index];
    const BoneDescriptor& bone = chain.bones[context.bone_index];
    const float alignment_fraction = (0.010f + context.stiffness_factor * 0.040f) * simulation_power_alignment;
    const float alignment_weight = fraction_for_dt(alignment_fraction, dt) / iterations;
    const float restoration_velocity_attenuation = restoration_velocity_attenuation_for_bone(bone);
    const float restoration_rotation_center_ratio = 0.10f + 0.40f * std::clamp(iteration_ratio, 0.0f, 1.0f);
    apply_pair_directional_constraint(
        context.parent_state,
        state,
        context.head_position,
        context.basic_direction,
        std::max(1.0e-5f, bone.length),
        alignment_weight * context.visit_scale,
        restoration_velocity_attenuation,
        restoration_rotation_center_ratio,
        context.parent_inverse_mass,
        context.child_inverse_mass
    );
}

void solve_bone_constraints_for_buffer_entry(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    BaselineStepBuffers& baseline_buffers,
    std::size_t buffer_index,
    const Vec3& root_translation,
    float dt,
    float iterations,
    float iteration_ratio,
    float simulation_power_alignment
) {
    if (kImplicitSpringAngleLimit) {
        solve_limit_constraint_for_buffer_entry(
            chain,
            chain_state,
            baseline_buffers,
            buffer_index,
            root_translation,
            dt,
            iterations,
            iteration_ratio
        );
    }
    solve_restoration_constraint_for_buffer_entry(
        chain,
        chain_state,
        baseline_buffers,
        buffer_index,
        root_translation,
        dt,
        iterations,
        iteration_ratio,
        simulation_power_alignment
    );
}

void solve_line_distance_constraint(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    std::size_t child_index,
    const Vec3& root_translation,
    float dt,
    float simulation_power_distance
) {
    const BoneDescriptor& bone = chain.bones[child_index];
    RuntimeBoneState& state = chain_state.bones[child_index];
    Vec3 head_position = root_translation;
    if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
        head_position = chain_state.bones[static_cast<std::size_t>(bone.parent_index)].position;
    }
    const float distance_stiffness = clamp_unit(0.50f * simulation_power_distance);
    project_distance_constraint(
        head_position,
        state,
        std::max(1.0e-5f, bone.length),
        distance_stiffness,
        dt
    );
}

void solve_baseline_buffer_phase(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    BaselineStepBuffers& baseline_buffers,
    const Vec3& root_translation,
    float dt,
    float iterations,
    float iteration_ratio,
    float simulation_power_alignment
) {
    for (std::size_t baseline_index = 0; baseline_index < baseline_buffers.start_indices.size(); ++baseline_index) {
        const std::int32_t start = baseline_buffers.start_indices[baseline_index];
        const std::int32_t count = baseline_index < baseline_buffers.counts.size()
            ? baseline_buffers.counts[baseline_index]
            : 0;
        if (start < 0 || count <= 1) {
            continue;
        }
        const std::size_t start_index = static_cast<std::size_t>(start);
        const std::size_t end_index = start_index + static_cast<std::size_t>(count);
        if (start_index >= baseline_buffers.joint_indices.size() || end_index > baseline_buffers.joint_indices.size()) {
            continue;
        }
        for (std::size_t buffer_index = start_index + 1; buffer_index < end_index; ++buffer_index) {
            solve_bone_constraints_for_buffer_entry(
                chain,
                chain_state,
                baseline_buffers,
                buffer_index,
                root_translation,
                dt,
                iterations,
                iteration_ratio,
                simulation_power_alignment
            );
        }
        for (std::size_t buffer_index = end_index; buffer_index-- > start_index + 1;) {
            solve_bone_constraints_for_buffer_entry(
                chain,
                chain_state,
                baseline_buffers,
                buffer_index,
                root_translation,
                dt,
                iterations,
                iteration_ratio,
                simulation_power_alignment
            );
        }
    }
}

void solve_direction_phase(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    BaselineStepBuffers& baseline_buffers,
    const Vec3& root_translation,
    float dt,
    std::size_t solver_iterations,
    float iterations,
    float simulation_power_alignment
) {
    for (std::size_t iteration = 0; iteration < solver_iterations; ++iteration) {
        const float iteration_ratio = solver_iterations <= 1
            ? 1.0f
            : static_cast<float>(iteration + 1) / static_cast<float>(solver_iterations);
        solve_baseline_buffer_phase(
            chain,
            chain_state,
            baseline_buffers,
            root_translation,
            dt,
            iterations,
            iteration_ratio,
            simulation_power_alignment
        );
    }
}

void solve_distance_phase(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const std::vector<std::pair<std::size_t, std::size_t>>& line_pairs,
    const Vec3& root_translation,
    float dt,
    float simulation_power_distance,
    std::size_t solver_iterations
) {
    const std::size_t iterations = std::max<std::size_t>(1, solver_iterations);
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        for (const auto& line : line_pairs) {
            solve_line_distance_constraint(
                chain,
                chain_state,
                line.second,
                root_translation,
                dt,
                simulation_power_distance
            );
        }
        for (auto line_it = line_pairs.rbegin(); line_it != line_pairs.rend(); ++line_it) {
            solve_line_distance_constraint(
                chain,
                chain_state,
                line_it->second,
                root_translation,
                dt,
                simulation_power_distance
            );
        }
    }
}

void solve_collision_phase(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const std::vector<ColliderStepWorkData>& colliders,
    BaselineStepBuffers& baseline_buffers,
    const Vec3& root_translation,
    float dt
) {
    for (std::size_t baseline_index = 0; baseline_index < baseline_buffers.start_indices.size(); ++baseline_index) {
        const std::int32_t start = baseline_buffers.start_indices[baseline_index];
        const std::int32_t count = baseline_index < baseline_buffers.counts.size()
            ? baseline_buffers.counts[baseline_index]
            : 0;
        if (start < 0 || count <= 1) {
            continue;
        }
        const std::size_t start_index = static_cast<std::size_t>(start);
        const std::size_t end_index = start_index + static_cast<std::size_t>(count);
        if (start_index >= baseline_buffers.joint_indices.size() || end_index > baseline_buffers.joint_indices.size()) {
            continue;
        }
        for (std::size_t buffer_index = end_index; buffer_index-- > start_index + 1;) {
            const std::int32_t raw_bone_index = baseline_buffers.joint_indices[buffer_index];
            if (raw_bone_index < 0 || static_cast<std::size_t>(raw_bone_index) >= chain.bones.size()) {
                continue;
            }
            const std::size_t bone_index = static_cast<std::size_t>(raw_bone_index);
            const BoneDescriptor& bone = chain.bones[bone_index];
            RuntimeBoneState& state = chain_state.bones[bone_index];
            Vec3 head_position = root_translation;
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
                head_position = chain_state.bones[static_cast<std::size_t>(bone.parent_index)].position;
            }
            apply_collider_constraints(
                head_position,
                state,
                std::max(1.0e-5f, bone.length),
                std::max(chain.joint_radius, bone.radius),
                colliders,
                dt
            );
        }
    }
}

void start_simulation_step_for_chain(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const BoneChainRuntimeInput* chain_input,
    const Vec3& root_translation,
    const Quat& root_rotation,
    const Vec3& root_rest_head,
    const Vec3& root_delta,
    const Vec3& center_delta,
    const Vec3& gravity,
    float dt,
    const SimulationPower& simulation_power
) {
    (void)root_delta;
    (void)center_delta;
    const bool has_basic_positions = runtime_input_has_basic_positions(chain_input, chain.bones.size());
    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        const BoneDescriptor& bone = chain.bones[bone_index];
        RuntimeBoneState& state = chain_state.bones[bone_index];
        const float damping = remap_bone_damping(bone.damping);
        const float drag = clamp_unit(bone.drag);
        const Vec3 old_position = state.position;
        Vec3 base_tail = rest_tail_world(bone, root_translation, root_rotation, root_rest_head);
        if (has_basic_positions) {
            base_tail = runtime_basic_tail(*chain_input, bone_index);
        }
        state.base_position = base_tail;

        if (bone.parent_index < 0) {
            state.previous_position = base_tail;
            state.velocity_position = base_tail;
            state.position = base_tail;
            state.velocity = Vec3{};
            state.stretch_lambda = 0.0f;
            continue;
        }

        Vec3 velocity = mul(state.velocity, chain_state.velocity_weight);
        velocity = mul(velocity, std::clamp(1.0f - damping * simulation_power.z, 0.0f, 1.0f));

        const float gravity_motion_scale = bone.gravity_scale;
        velocity = add(velocity, mul(gravity, gravity_motion_scale * dt));
        velocity = mul(velocity, std::clamp(1.0f - drag * 0.28f * simulation_power.z, 0.0f, 1.0f));

        const Vec3 shifted_position = old_position;
        const Vec3 predicted = add(shifted_position, mul(velocity, dt));
        const Vec3 stabilized_shifted = lerp(base_tail, shifted_position, chain_state.blend_weight);
        const Vec3 stabilized_predicted = lerp(base_tail, predicted, chain_state.blend_weight);

        state.previous_position = chain_state.blend_weight < 0.999f ? stabilized_predicted : old_position;
        state.velocity_position = stabilized_shifted;
        state.position = stabilized_predicted;
        state.stretch_lambda = 0.0f;
    }
}

void finalize_end_step_for_chain(
    RuntimeBoneChainState& chain_state,
    float dt
) {
    for (RuntimeBoneState& state : chain_state.bones) {
        const Vec3 raw_displacement = sub(state.position, state.velocity_position);
        const Vec3 friction_displacement = apply_end_step_contact_friction(state, raw_displacement, dt);
        finalize_end_step_state(state, friction_displacement, chain_state.velocity_weight, dt);
    }
}

void step_bone_chain(
    const RuntimeSceneState& scene_state,
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const BoneChainRuntimeInput* chain_input,
    float dt,
    std::size_t solver_iterations,
    const SimulationPower& simulation_power
) {
    if (chain.bones.empty() || dt <= 0.0f) {
        return;
    }

    if (!chain_state.initialized) {
        initialize_chain_state(chain, chain_state, chain_input);
    }

    const Vec3 root_rest_head = make_vec3(chain.bones.front().rest_head_local);
    const Vec3 target_root_translation = chain_input != nullptr
        ? make_vec3(chain_input->root_translation)
        : chain_state.last_root_translation;
    const Quat target_root_rotation = chain_input != nullptr
        ? normalize_or_identity(make_quat(chain_input->root_rotation_quaternion))
        : chain_state.last_root_rotation;
    const Vec3 target_center_translation = chain_input != nullptr
        ? make_vec3(chain_input->center_translation)
        : target_root_translation;
    const Quat target_center_rotation = chain_input != nullptr
        ? normalize_or_identity(make_quat(chain_input->center_rotation_quaternion))
        : target_root_rotation;
    const Vec3 root_translation = target_root_translation;
    const Quat root_rotation = target_root_rotation;
    const Vec3 center_translation = target_center_translation;
    const Quat center_rotation = target_center_rotation;
    const Vec3 root_delta = sub(root_translation, chain_state.last_root_translation);
    const Vec3 center_delta = sub(center_translation, chain_state.last_center_translation);
    (void)center_rotation;
    const Vec3 gravity_direction = normalize_or_default(make_vec3(chain.gravity_direction), Vec3{0.0f, -1.0f, 0.0f});
    const float gravity_strength = 0.0f;
    const Vec3 gravity = mul(gravity_direction, (1.0f / std::max(kReferenceFrameDt, 1.0e-8f)) * gravity_strength);
    const float iterations = 3.0f;
    if (chain_state.velocity_weight < 1.0f) {
        const float add_weight = kResetStabilizationTime > 1.0e-6f ? dt / kResetStabilizationTime : 1.0f;
        chain_state.velocity_weight = std::clamp(chain_state.velocity_weight + add_weight, 0.0f, 1.0f);
    }
    chain_state.blend_weight = chain_state.velocity_weight;
    solver_iterations = std::max<std::size_t>(1, solver_iterations);
    const std::vector<ColliderStepWorkData> colliders = build_collider_work_data_for_chain(scene_state, chain);
    start_simulation_step_for_chain(
        chain,
        chain_state,
        chain_input,
        root_translation,
        root_rotation,
        root_rest_head,
        root_delta,
        center_delta,
        gravity,
        dt,
        simulation_power
    );

    const float power_distance = simulation_power.y;
    const float power_alignment = simulation_power.w;
    const std::vector<std::vector<std::size_t>> baseline_paths = baseline_paths_for_chain(chain);
    const std::vector<std::pair<std::size_t, std::size_t>> line_pairs = line_pairs_for_chain(chain);
    update_step_basic_pose_for_chain(
        chain,
        chain_state,
        chain_input,
        root_translation,
        root_rotation,
        root_rest_head,
        baseline_paths
    );
    BaselineStepBuffers baseline_buffers = build_baseline_step_buffers(
        chain,
        chain_state,
        chain_input,
        root_translation,
        baseline_paths
    );
    solve_distance_phase(
        chain,
        chain_state,
        line_pairs,
        root_translation,
        dt,
        power_distance,
        solver_iterations
    );
    solve_direction_phase(
        chain,
        chain_state,
        baseline_buffers,
        root_translation,
        dt,
        solver_iterations,
        iterations,
        power_alignment
    );
    if (!colliders.empty()) {
        solve_collision_phase(
            chain,
            chain_state,
            colliders,
            baseline_buffers,
            root_translation,
            dt
        );
    }
    solve_distance_phase(
        chain,
        chain_state,
        line_pairs,
        root_translation,
        dt,
        power_distance,
        solver_iterations
    );
    finalize_end_step_for_chain(chain_state, dt);

    chain_state.last_root_translation = root_translation;
    chain_state.last_root_rotation = root_rotation;
    chain_state.last_center_translation = center_translation;
}

std::int32_t advance_runtime(RuntimeSceneState& scene_state, const RuntimeInputs& inputs) {
    const std::int32_t simulation_frequency = std::clamp(inputs.simulation_frequency, 30, 150);
    const float frame_dt = std::max(inputs.dt, 0.0f);
    const float simulation_dt = 1.0f / std::max(1.0f, static_cast<float>(simulation_frequency));
    const SimulationPower simulation_power = simulation_power_for_frequency(simulation_frequency);
    constexpr std::size_t kFixedStepSolverIterations = 3;
    const std::size_t solver_iterations_per_step = kFixedStepSolverIterations;
    if (frame_dt <= 0.0f) {
        scene_state.last_skipped_steps = 0;
        return 0;
    }

    const float accumulated_before_frame = std::max(0.0f, scene_state.accumulated_time);
    scene_state.accumulated_time = accumulated_before_frame + frame_dt;
    const std::int32_t scheduled_steps = static_cast<std::int32_t>(
        std::floor(scene_state.accumulated_time / std::max(simulation_dt, 1.0e-6f))
    );
    if (scheduled_steps <= 0) {
        scene_state.last_skipped_steps = 0;
        return 0;
    }
    const std::int32_t executed_steps = std::min(scheduled_steps, kMaxSimulationStepsPerFrame);
    const std::int32_t skipped_steps = std::max(0, scheduled_steps - executed_steps);
    scene_state.last_skipped_steps = skipped_steps;
    scene_state.accumulated_time = std::max(
        0.0f,
        scene_state.accumulated_time - static_cast<float>(scheduled_steps) * simulation_dt
    );

    std::vector<CollisionObjectStepStart> start_collision_objects;
    start_collision_objects.reserve(inputs.collision_objects.size());
    for (const CollisionObjectRuntimeInput& collision_input : inputs.collision_objects) {
        const CollisionWorldObject* collision_object = scene_state.collision_world.find_object(collision_input.collision_object_id);
        if (collision_object == nullptr) {
            continue;
        }
        CollisionObjectStepStart start;
        start.collision_object_id = collision_input.collision_object_id;
        start.translation = make_vec3(collision_object->world_translation);
        start.rotation = normalize_or_identity(make_quat(collision_object->world_rotation));
        start_collision_objects.push_back(std::move(start));
    }

    const float safe_frame_dt = std::max(frame_dt, 1.0e-6f);
    const float first_step_time = std::max(0.0f, simulation_dt - accumulated_before_frame);
    const float skipped_start_time = skipped_steps > 0
        ? std::max(0.0f, first_step_time + static_cast<float>(skipped_steps - 1) * simulation_dt)
        : 0.0f;
    float previous_alpha = std::clamp(skipped_start_time / safe_frame_dt, 0.0f, 1.0f);
    for (std::int32_t step_index = skipped_steps; step_index < scheduled_steps; ++step_index) {
        const float step_time = first_step_time + static_cast<float>(step_index) * simulation_dt;
        const float alpha = std::clamp(step_time / safe_frame_dt, previous_alpha, 1.0f);
        const RuntimeInputs step_inputs = build_interpolated_runtime_inputs(
            scene_state,
            alpha,
            simulation_dt,
            simulation_frequency
        );
        apply_collision_inputs_for_step(scene_state, inputs, start_collision_objects, previous_alpha, alpha);
        for (std::size_t chain_index = 0; chain_index < scene_state.scene.bone_chains.size(); ++chain_index) {
            const BoneChainDescriptor& chain = scene_state.scene.bone_chains[chain_index];
            RuntimeBoneChainState& chain_state = scene_state.chain_states[chain_index];
            step_bone_chain(
                scene_state,
                chain,
                chain_state,
                find_chain_input(step_inputs, chain),
                simulation_dt,
                solver_iterations_per_step,
                simulation_power
            );
        }
        previous_alpha = alpha;
    }

    return executed_steps;
}

std::vector<BoneTransform> build_transforms(const RuntimeSceneState& scene_state) {
    std::vector<BoneTransform> transforms;

    for (std::size_t chain_index = 0; chain_index < scene_state.scene.bone_chains.size(); ++chain_index) {
        const BoneChainDescriptor& chain = scene_state.scene.bone_chains[chain_index];
        const RuntimeBoneChainState& chain_state = scene_state.chain_states[chain_index];
        const BoneChainRuntimeInput* chain_input = find_chain_input(scene_state.inputs, chain);
        const bool has_runtime_basic_positions = runtime_input_has_basic_positions(chain_input, chain.bones.size());
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
            const bool emit_transform = bone.parent_index >= 0 && !ends_with(bone.name, kTailTipSuffix);
            Vec3 current_head = root_translation;
            Vec3 basic_head = root_translation;
            if (has_runtime_basic_positions && chain_input != nullptr) {
                basic_head = runtime_basic_head(*chain_input, bone_index);
            }
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
                current_head = chain_state.bones[static_cast<std::size_t>(bone.parent_index)].position;
                if (!has_runtime_basic_positions &&
                    static_cast<std::size_t>(bone.parent_index) < chain_state.step_basic_positions.size()) {
                    basic_head = chain_state.step_basic_positions[static_cast<std::size_t>(bone.parent_index)];
                }
            }
            const Vec3 current_tail = chain_state.bones[bone_index].position;
            const Vec3 basic_tail = bone_index < chain_state.step_basic_positions.size()
                ? chain_state.step_basic_positions[bone_index]
                : current_tail;
            const Vec3 current_direction_world = sub(current_tail, current_head);
            const Vec3 basic_direction_world = sub(basic_tail, basic_head);

            Quat rest_global_rotation = make_quat(bone.rest_local_rotation);
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < rest_global_rotations.size()) {
                rest_global_rotation = quat_multiply(
                    rest_global_rotations[static_cast<std::size_t>(bone.parent_index)],
                    make_quat(bone.rest_local_rotation)
                );
            }
            rest_global_rotations[bone_index] = rest_global_rotation;

            Quat basic_global_rotation = rest_global_rotation;
            if (bone_index < chain_state.step_basic_rotations.size()) {
                basic_global_rotation = chain_state.step_basic_rotations[bone_index];
            }
            const Quat swing = quat_between_vectors(basic_direction_world, current_direction_world);
            const Quat current_global_rotation = normalize_or_identity(quat_multiply(swing, basic_global_rotation));
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
            if (!emit_transform) {
                continue;
            }
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
    state.collision_world.build_from_scene(scene);
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
    it->second.last_executed_steps = 0;
}

void set_runtime_inputs(SceneHandle handle, const RuntimeInputs& inputs) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return;
    }
    it->second.inputs = inputs;
}

std::int32_t step_scene(SceneHandle handle, const RuntimeInputs& inputs) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return 0;
    }

    if (!inputs.bone_chains.empty()) {
        it->second.inputs = inputs;
    } else {
        it->second.inputs.dt = inputs.dt;
        it->second.inputs.simulation_frequency = inputs.simulation_frequency;
    }

    const std::int32_t executed_steps = advance_runtime(it->second, it->second.inputs);
    it->second.last_executed_steps = std::max(executed_steps, 0);
    it->second.steps += static_cast<std::uint64_t>(it->second.last_executed_steps);
    return it->second.last_executed_steps;
}

std::uint64_t get_step_count(SceneHandle handle) {
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return 0;
    }
    return it->second.steps;
}

RuntimeStepInfo get_last_step_info(SceneHandle handle) {
    RuntimeStepInfo info;
    auto it = g_scenes.find(handle);
    if (it == g_scenes.end()) {
        return info;
    }
    info.executed_steps = it->second.last_executed_steps;
    return info;
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
    info.collider_group_count = static_cast<std::uint64_t>(it->second.scene.collider_groups.size());
    info.collision_object_count = static_cast<std::uint64_t>(it->second.scene.collision_objects.size());
    info.collision_binding_count = static_cast<std::uint64_t>(it->second.scene.collision_bindings.size());
    info.cache_descriptor_count = static_cast<std::uint64_t>(it->second.scene.cache_descriptors.size());
    info.physics_scene_ready = it->second.physics_scene_ready;
    for (const BoneChainDescriptor& chain : it->second.scene.bone_chains) {
        info.bone_count += static_cast<std::uint64_t>(chain.bones.size());
    }
    return info;
}

}  // namespace hocloth
