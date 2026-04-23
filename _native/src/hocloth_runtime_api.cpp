#include "hocloth_runtime_api.hpp"
#include "hocloth_collision_world.hpp"

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
    Vec3 last_root_translation;
    Quat last_root_rotation;
    Vec3 last_center_translation;
    bool initialized = false;
};

struct RuntimeSceneState {
    SceneDescriptor scene;
    RuntimeInputs inputs;
    std::vector<RuntimeBoneChainState> chain_states;
    CollisionWorld collision_world;
    std::string backend = "xpbd";
    std::string build_message;
    bool physics_scene_ready = false;
    float accumulated_time = 0.0f;
    std::uint64_t steps = 0;
    std::int32_t last_executed_steps = 0;
    std::int32_t last_scheduled_steps = 0;
    std::int32_t last_skipped_steps = 0;
};

struct CollisionObjectStepStart {
    std::string collision_object_id;
    Vec3 translation;
    Quat rotation;
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
constexpr float kDefaultSimulationFrequency = 90.0f;
constexpr float kReferenceFrameDt = 1.0f / 90.0f;

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
    const Vec3 center_translation = chain_input != nullptr
        ? make_vec3(chain_input->center_translation)
        : root_translation;

    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        const BoneDescriptor& bone = chain.bones[bone_index];
        RuntimeBoneState& state = chain_state.bones[bone_index];
        state.position = rest_tail_world(bone, root_translation, root_rotation, root_rest_head);
        state.base_position = state.position;
        state.previous_position = state.position;
        state.velocity_position = state.position;
        state.velocity = Vec3{};
        state.collision_normal = Vec3{};
        state.collision_friction = 0.0f;
        state.static_friction = 0.0f;
        state.stretch_lambda = 0.0f;
    }

    chain_state.last_root_translation = root_translation;
    chain_state.last_root_rotation = root_rotation;
    chain_state.last_center_translation = center_translation;
    chain_state.initialized = true;
}

std::vector<const CollisionWorldObject*> collision_objects_for_chain(
    const RuntimeSceneState& scene_state,
    const BoneChainDescriptor& chain
) {
    std::vector<const CollisionWorldObject*> result;
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
                result.push_back(&scene_state.collision_world.objects()[object_index]);
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

void apply_collision_inputs_for_substep(
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
    const CollisionWorldObject& collider
) {
    const Vec3 bone_tail = state.position;
    const bool use_bone_capsule = bone_length > 1.0e-6f && particle_radius > 0.0f;
    const Vec3 center = make_vec3(collider.world_translation);
    Vec3 closest = center;
    Vec3 bone_closest = bone_tail;
    float bone_t = 1.0f;
    if (collider.shape_type == "CAPSULE") {
        const Quat rotation = normalize_or_identity(make_quat(collider.world_rotation));
        const float half_height = std::max(0.0f, collider.height) * 0.5f;
        // Collider authoring uses Blender's local Z axis; after the
        // Blender->solver basis conversion that becomes solver +Y.
        const Vec3 axis = rotate_vector(rotation, Vec3{0.0f, 1.0f, 0.0f});
        const Vec3 a = add(center, mul(axis, -half_height));
        const Vec3 b = add(center, mul(axis, half_height));
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
    const CollisionWorldObject& collider,
    const Vec3& translation,
    const Quat& rotation,
    const Vec3& point
) {
    if (collider.shape_type == "CAPSULE") {
        const float half_height = std::max(0.0f, collider.height) * 0.5f;
        const Vec3 axis = rotate_vector(rotation, Vec3{0.0f, 1.0f, 0.0f});
        const Vec3 a = add(translation, mul(axis, -half_height));
        const Vec3 b = add(translation, mul(axis, half_height));
        return closest_point_on_segment(point, a, b);
    }
    return translation;  // SPHERE and fallback.
}

bool compute_contact_push(
    const ColliderContact& contact,
    const CollisionWorldObject& collider,
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
    const Vec3 prev_translation = make_vec3(collider.previous_world_translation);
    const Quat prev_rotation = normalize_or_identity(make_quat(collider.previous_world_rotation));
    const Vec3 curr_translation = make_vec3(collider.world_translation);
    const Quat curr_rotation = normalize_or_identity(make_quat(collider.world_rotation));

    const Vec3 old_axis = collider_axis_closest_point_at(collider, prev_translation, prev_rotation, contact.bone_closest);
    const Vec3 new_axis = collider_axis_closest_point_at(collider, curr_translation, curr_rotation, contact.bone_closest);
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

void apply_collider_constraints(
    Vec3 head_position,
    RuntimeBoneState& state,
    float bone_length,
    float particle_radius,
    const std::vector<const CollisionWorldObject*>& colliders,
    float dt
) {
    if (colliders.empty()) {
        return;
    }

    Vec3 add_pos{};
    Vec3 add_n{};
    std::int32_t add_cnt = 0;

    // MC2-style friction: based on surface distance within a small range around contact.
    const float friction_range = std::max(1.0e-4f, particle_radius);
    Vec3 friction_normal_sum{};
    float friction_max = 0.0f;

    for (const CollisionWorldObject* collider : colliders) {
        const ColliderContact contact = evaluate_collider_contact(head_position, state, bone_length, particle_radius, *collider);

        // Surface distance: 0 at contact/overlap, positive when separated.
        const float surface_dist = std::max(0.0f, contact.distance - contact.target_radius);
        const float local_friction = 1.0f - std::clamp(surface_dist / friction_range, 0.0f, 1.0f);

        Vec3 push_normal = contact.normal;
        float push_distance = 0.0f;
        float penetration_ratio = 0.0f;
        const bool should_push = compute_contact_push(contact, *collider, dt, push_normal, push_distance, penetration_ratio);

        // Accumulate friction info even if we don't push this step.
        const float effective_friction = std::max(local_friction, penetration_ratio);
        if (effective_friction > 1.0e-6f) {
            friction_max = std::max(friction_max, effective_friction);
            friction_normal_sum = add(friction_normal_sum, mul(push_normal, effective_friction));
        }

        if (!should_push) {
            continue;
        }

        const float tail_weight = contact.bone_t >= 0.999f
            ? 1.0f
            : std::clamp(contact.bone_t * 0.85f, 0.08f, 0.85f);
        const Vec3 correction = mul(push_normal, push_distance * tail_weight);
        add_pos = add(add_pos, correction);
        add_n = add(add_n, push_normal);
        add_cnt += 1;
    }

    // Apply aggregated correction once (MC2-style), then re-project bone length and clamp.
    if (add_cnt > 0) {
        const Vec3 avg_n = mul(add_n, 1.0f / static_cast<float>(add_cnt));
        const float len = length(avg_n);
        if (len > 1.0e-6f) {
            const float t = std::min(len, 1.0f);
            const Vec3 avg_pos = mul(add_pos, (1.0f / static_cast<float>(add_cnt)) * t);
            const Vec3 old_position = state.position;
            Vec3 solved_position = add(state.position, avg_pos);

            // MC2 BoneSpring soft collider: after geometric projection, limit how far the
            // particle may be pushed away from the current base pose, then blend back toward
            // the pre-collision position. This is the main difference from hard collider push.
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
    }

    if (friction_max > 1.0e-6f && length_squared(friction_normal_sum) > 1.0e-8f) {
        record_collision_contact(state, friction_normal_sum, friction_max);
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
    const Vec3& target_tail,
    float rest_length,
    float weight
) {
    const Vec3 target_direction = sub(target_tail, head_position);
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

void solve_bone_constraints_for_index(
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    std::size_t bone_index,
    const Vec3& root_translation,
    const Quat& root_rotation,
    const Vec3& root_rest_head,
    float dt,
    float iterations,
    bool apply_alignment,
    float simulation_power_distance,
    float simulation_power_alignment
) {
    const BoneDescriptor& bone = chain.bones[bone_index];
    RuntimeBoneState& state = chain_state.bones[bone_index];
    const float stiffness = std::clamp(bone.stiffness, 0.0f, 1.0f);
    const float stiffness_factor = stiffness;
    const float distance_stiffness = clamp_unit(0.50f * simulation_power_distance);
    Vec3 head_position = root_translation;
    if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_state.bones.size()) {
        head_position = chain_state.bones[static_cast<std::size_t>(bone.parent_index)].position;
    }

    const Vec3 target_tail = rest_tail_world(bone, root_translation, root_rotation, root_rest_head);
    project_distance_constraint(
        head_position,
        state,
        std::max(1.0e-5f, bone.length),
        distance_stiffness,
        dt
    );
    if (apply_alignment && bone_index > 0) {
        const float alignment_fraction = (0.008f + stiffness_factor * 0.055f) * simulation_power_alignment;
        const float alignment_weight = fraction_for_dt(alignment_fraction, dt) / iterations;
        project_directional_alignment(
            head_position,
            state,
            target_tail,
            std::max(1.0e-5f, bone.length),
            alignment_weight
        );
    }
}

void step_bone_chain(
    const RuntimeSceneState& scene_state,
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const BoneChainRuntimeInput* chain_input,
    float dt,
    const Vec3& start_root_translation,
    const Quat& start_root_rotation,
    const Vec3& start_center_translation,
    float substep_fraction,
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
    const Vec3 root_translation = lerp(start_root_translation, target_root_translation, substep_fraction);
    const Quat root_rotation = nlerp(start_root_rotation, target_root_rotation, substep_fraction);
    const Vec3 center_translation = lerp(start_center_translation, target_center_translation, substep_fraction);
    const Quat center_rotation = nlerp(start_root_rotation, target_center_rotation, substep_fraction);
    const Vec3 root_delta = sub(root_translation, chain_state.last_root_translation);
    const Vec3 center_delta = sub(center_translation, chain_state.last_center_translation);
    (void)center_rotation;
    const Vec3 gravity_direction = normalize_or_default(make_vec3(chain.gravity_direction), Vec3{0.0f, -1.0f, 0.0f});
    const float gravity_strength = std::clamp(chain.gravity_strength, 0.0f, 10.0f);
    const Vec3 gravity = mul(gravity_direction, (1.0f / std::max(kReferenceFrameDt, 1.0e-8f)) * gravity_strength);
    const float iterations = 8.0f;
    solver_iterations = std::max<std::size_t>(1, solver_iterations);
    const std::vector<const CollisionWorldObject*> colliders = collision_objects_for_chain(scene_state, chain);

    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        const BoneDescriptor& bone = chain.bones[bone_index];
        RuntimeBoneState& state = chain_state.bones[bone_index];
        const float stiffness = std::clamp(bone.stiffness, 0.0f, 1.0f);
        const float stiffness_factor = stiffness;
        const float damping = clamp_unit(bone.damping);
        const float drag = clamp_unit(bone.drag);
        const float depth = chain.bones.size() <= 1
            ? 0.0f
            : static_cast<float>(bone_index) / static_cast<float>(chain.bones.size() - 1);
        const Vec3 old_position = state.position;
        const Vec3 base_tail = rest_tail_world(bone, root_translation, root_rotation, root_rest_head);
        state.base_position = base_tail;

        // MC2-style StartSimulationStep: start from oldPos, shift by kinematic inertia,
        // then apply damped velocity and forces. Do not inject parent velocity here;
        // the chain relationship is handled by constraints below.
        const float inertia_depth = 1.0f - depth * depth;
        const float root_inertia_mix = std::clamp(0.12f + stiffness_factor * 0.24f - drag * 0.08f, 0.04f, 0.36f);
        Vec3 inertia_offset = mul(root_delta, root_inertia_mix * inertia_depth);
        inertia_offset = add(inertia_offset, mul(center_delta, 0.08f * inertia_depth));

        Vec3 velocity = state.velocity;
        velocity = mul(velocity, std::clamp(1.0f - damping * simulation_power.z, 0.0f, 1.0f));

        const float gravity_motion_scale = (0.45f + (1.0f - stiffness_factor) * 0.85f) * bone.gravity_scale;
        velocity = add(velocity, mul(gravity, gravity_motion_scale * dt));
        velocity = mul(velocity, std::clamp(1.0f - drag * 0.28f * simulation_power.z, 0.0f, 1.0f));

        const Vec3 shifted_position = add(old_position, inertia_offset);
        const Vec3 predicted = add(shifted_position, mul(velocity, dt));

        state.previous_position = old_position;
        state.velocity_position = shifted_position;
        state.position = predicted;
        state.stretch_lambda = 0.0f;
    }

    // MC2-style: keep a fixed sequence per simulation step instead of repeatedly cycling
    // forward/reverse/collision in a generic solver-iteration loop.
    const float power_distance = simulation_power.y;
    const float power_alignment = simulation_power.w;

    for (std::size_t iteration = 0; iteration < solver_iterations; ++iteration) {
        for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
            solve_bone_constraints_for_index(
                chain,
                chain_state,
                bone_index,
                root_translation,
                root_rotation,
                root_rest_head,
                dt,
                iterations,
                true,
                power_distance,
                power_alignment
            );
        }
        for (std::size_t reverse_index = chain.bones.size(); reverse_index-- > 0;) {
            solve_bone_constraints_for_index(
                chain,
                chain_state,
                reverse_index,
                root_translation,
                root_rotation,
                root_rest_head,
                dt,
                iterations,
                true,
                power_distance,
                power_alignment
            );
        }
    }

    for (std::size_t bone_index = 0; bone_index < chain_state.bones.size(); ++bone_index) {
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

    for (std::size_t iteration = 0; iteration < solver_iterations; ++iteration) {
        for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
            solve_bone_constraints_for_index(
                chain,
                chain_state,
                bone_index,
                root_translation,
                root_rotation,
                root_rest_head,
                dt,
                iterations,
                false,
                power_distance,
                power_alignment
            );
        }
        for (std::size_t reverse_index = chain.bones.size(); reverse_index-- > 0;) {
            solve_bone_constraints_for_index(
                chain,
                chain_state,
                reverse_index,
                root_translation,
                root_rotation,
                root_rest_head,
                dt,
                iterations,
                false,
                power_distance,
                power_alignment
            );
        }
    }

    for (std::size_t bone_index = 0; bone_index < chain_state.bones.size(); ++bone_index) {
        RuntimeBoneState& state = chain_state.bones[bone_index];
        const Vec3 raw_displacement = sub(state.position, state.velocity_position);
        const Vec3 friction_displacement = apply_end_step_contact_friction(state, raw_displacement, dt);
        state.velocity = mul(friction_displacement, 1.0f / std::max(dt, 1.0e-6f));

        state.previous_position = state.position;
        state.velocity_position = state.position;
    }

    chain_state.last_root_translation = root_translation;
    chain_state.last_root_rotation = root_rotation;
    chain_state.last_center_translation = center_translation;
}

std::int32_t advance_runtime(RuntimeSceneState& scene_state, const RuntimeInputs& inputs) {
    const float frame_dt = inputs.dt > 0.0f ? inputs.dt : (1.0f / 30.0f);
    const std::int32_t simulation_frequency = std::clamp(inputs.simulation_frequency, 30, 150);
    const std::int32_t max_steps = std::clamp(inputs.max_simulation_steps_per_frame, 1, 16);
    const float simulation_dt = 1.0f / static_cast<float>(simulation_frequency);
    const SimulationPower simulation_power = simulation_power_for_frequency(simulation_frequency);
    scene_state.accumulated_time += frame_dt;
    const std::int32_t scheduled_steps = static_cast<std::int32_t>(
        std::floor((scene_state.accumulated_time + 1.0e-6f) / simulation_dt)
    );
    scene_state.last_scheduled_steps = std::max(scheduled_steps, 0);
    if (scheduled_steps <= 0) {
        scene_state.last_skipped_steps = 0;
        return 0;
    }

    const std::int32_t fixed_steps = std::min(scheduled_steps, max_steps);
    scene_state.last_skipped_steps = std::max(0, scheduled_steps - fixed_steps);
    if (scheduled_steps > max_steps) {
        scene_state.accumulated_time -= static_cast<float>(scheduled_steps) * simulation_dt;
    } else {
        scene_state.accumulated_time -= static_cast<float>(fixed_steps) * simulation_dt;
    }
    scene_state.accumulated_time = std::clamp(scene_state.accumulated_time, 0.0f, simulation_dt);

    constexpr std::size_t kFixedStepSolverIterations = 1;
    const std::size_t solver_iterations_per_step = kFixedStepSolverIterations;
    std::vector<Vec3> start_root_translations(scene_state.chain_states.size(), Vec3{});
    std::vector<Quat> start_root_rotations(scene_state.chain_states.size(), Quat{});
    std::vector<Vec3> start_center_translations(scene_state.chain_states.size(), Vec3{});
    for (std::size_t chain_index = 0; chain_index < scene_state.chain_states.size(); ++chain_index) {
        start_root_translations[chain_index] = scene_state.chain_states[chain_index].last_root_translation;
        start_root_rotations[chain_index] = scene_state.chain_states[chain_index].last_root_rotation;
        start_center_translations[chain_index] = scene_state.chain_states[chain_index].last_center_translation;
    }
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

    for (std::int32_t fixed_step = 0; fixed_step < fixed_steps; ++fixed_step) {
        const float previous_step_alpha = std::min(
            1.0f,
            (static_cast<float>(fixed_step) * simulation_dt) / std::max(frame_dt, 1.0e-6f)
        );
        const float step_alpha = std::min(
            1.0f,
            (static_cast<float>(fixed_step + 1) * simulation_dt) / std::max(frame_dt, 1.0e-6f)
        );
        apply_collision_inputs_for_substep(scene_state, inputs, start_collision_objects, previous_step_alpha, step_alpha);
        for (std::size_t chain_index = 0; chain_index < scene_state.scene.bone_chains.size(); ++chain_index) {
            const BoneChainDescriptor& chain = scene_state.scene.bone_chains[chain_index];
            RuntimeBoneChainState& chain_state = scene_state.chain_states[chain_index];
            step_bone_chain(
                scene_state,
                chain,
                chain_state,
                find_chain_input(scene_state.inputs, chain),
                simulation_dt,
                start_root_translations[chain_index],
                start_root_rotations[chain_index],
                start_center_translations[chain_index],
                step_alpha,
                solver_iterations_per_step,
                simulation_power
            );
        }
    }

    return fixed_steps;
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
    it->second.accumulated_time = 0.0f;
    it->second.steps = 0;
    it->second.last_executed_steps = 0;
    it->second.last_scheduled_steps = 0;
    it->second.last_skipped_steps = 0;
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
        it->second.inputs.max_simulation_steps_per_frame = inputs.max_simulation_steps_per_frame;
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
    info.scheduled_steps = it->second.last_scheduled_steps;
    info.executed_steps = it->second.last_executed_steps;
    info.skipped_steps = it->second.last_skipped_steps;
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
