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
    Vec3 last_center_translation;
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

constexpr float kCollisionSlop = 1.0e-3f;
constexpr float kCollisionPushBias = 5.0e-4f;
constexpr float kCollisionResponse = 0.45f;
constexpr float kCollisionMaxPushFactor = 0.35f;
constexpr float kCollisionTangentialDamping = 0.82f;
constexpr float kCollisionRelaxationShallow = 0.55f;
constexpr float kCollisionRelaxationDeep = 0.14f;
constexpr float kCollisionDeepFreezeThreshold = 0.45f;

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
        state.previous_position = state.position;
        state.stretch_lambda = 0.0f;
        state.bend_lambda = 0.0f;
        state.shape_lambda = 0.0f;
    }

    chain_state.last_root_translation = root_translation;
    chain_state.last_root_rotation = root_rotation;
    chain_state.last_center_translation = center_translation;
    chain_state.initialized = true;
}

std::vector<const ColliderDescriptor*> colliders_for_chain(const SceneDescriptor& scene, const BoneChainDescriptor& chain) {
    std::vector<const ColliderDescriptor*> result;
    for (const std::string& group_id : chain.collider_group_ids) {
        for (const ColliderGroupDescriptor& group : scene.collider_groups) {
            if (group.component_id != group_id) {
                continue;
            }
            for (const std::string& collider_id : group.collider_ids) {
                for (const ColliderDescriptor& collider : scene.colliders) {
                    if (collider.component_id == collider_id) {
                        result.push_back(&collider);
                    }
                }
            }
        }
    }
    return result;
}

ColliderContact evaluate_collider_contact(
    Vec3 head_position,
    const RuntimeBoneState& state,
    float bone_length,
    float particle_radius,
    const ColliderDescriptor& collider
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
            fallback = sub(state.position, state.previous_position);
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
    contact.normal = delta;
    contact.distance = distance;
    contact.target_radius = target_radius;
    contact.overlapping = distance < (target_radius - kCollisionSlop);
    contact.bone_t = bone_t;
    return contact;
}

void project_contact(
    Vec3 head_position,
    RuntimeBoneState& state,
    const ColliderContact& contact,
    bool preserve_surface_velocity,
    float bone_length
) {
    if (!contact.overlapping) {
        return;
    }

    const float penetration = std::max(0.0f, contact.target_radius - contact.distance);
    const float penetration_ratio = penetration / std::max(contact.target_radius, 1.0e-6f);
    const float desired_push = (penetration + kCollisionPushBias) * kCollisionResponse;
    const float max_push = contact.target_radius * kCollisionMaxPushFactor + kCollisionPushBias;
    const float relaxation = std::clamp(
        kCollisionRelaxationShallow - penetration_ratio * (kCollisionRelaxationShallow - kCollisionRelaxationDeep),
        kCollisionRelaxationDeep,
        kCollisionRelaxationShallow
    );
    const float push_distance = std::min(desired_push, max_push) * relaxation;
    const Vec3 correction = mul(contact.normal, push_distance);
    const float tail_weight = std::clamp(contact.bone_t * 0.85f, 0.08f, 0.85f);
    const Vec3 tail_correction = mul(correction, tail_weight);
    state.position = add(state.position, tail_correction);
    state.previous_position = add(state.previous_position, tail_correction);

    const Vec3 direction = sub(state.position, head_position);
    const float direction_length = length(direction);
    if (bone_length > 1.0e-6f && direction_length > 1.0e-6f) {
        state.position = add(head_position, mul(direction, bone_length / direction_length));
    }

    if (preserve_surface_velocity) {
        Vec3 velocity = sub(state.position, state.previous_position);
        const float inward_speed = dot(velocity, contact.normal);
        if (inward_speed < 0.0f) {
            velocity = sub(velocity, mul(contact.normal, inward_speed));
        }
        const float tangential_damping = penetration_ratio >= kCollisionDeepFreezeThreshold
            ? 0.25f
            : kCollisionTangentialDamping;
        velocity = mul(velocity, tangential_damping);
        if (penetration_ratio >= kCollisionDeepFreezeThreshold) {
            state.previous_position = state.position;
            return;
        }
        state.previous_position = sub(state.position, velocity);
    }
}

void apply_collider_constraints(
    Vec3 head_position,
    RuntimeBoneState& state,
    float bone_length,
    float particle_radius,
    const std::vector<const ColliderDescriptor*>& colliders
) {
    if (colliders.empty()) {
        return;
    }

    for (const ColliderDescriptor* collider : colliders) {
        const ColliderContact contact = evaluate_collider_contact(head_position, state, bone_length, particle_radius, *collider);
        project_contact(head_position, state, contact, true, bone_length);
    }
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
    state.position = add(
        mul(state.position, 1.0f - weight),
        mul(desired_tail, weight)
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
    const std::vector<const ColliderDescriptor*>& colliders,
    bool apply_collision
) {
    const BoneDescriptor& bone = chain.bones[bone_index];
    RuntimeBoneState& state = chain_state.bones[bone_index];
    const float stiffness = std::clamp(bone.stiffness, 0.0f, 1.0f);
    const float stiffness_factor = stiffness;
    const float stretch_compliance = 1.5e-3f + (1.0f - stiffness_factor) * 7.5e-3f;
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
        const float alignment_weight = (0.008f + stiffness_factor * 0.055f) / iterations;
        project_directional_alignment(
            head_position,
            state,
            target_tail,
            std::max(1.0e-5f, bone.length),
            alignment_weight
        );
    }
    if (apply_collision) {
        apply_collider_constraints(
            head_position,
            state,
            std::max(1.0e-5f, bone.length),
            std::max(chain.joint_radius, bone.radius),
            colliders
        );
    }
}

void step_bone_chain(
    const SceneDescriptor& scene,
    const BoneChainDescriptor& chain,
    RuntimeBoneChainState& chain_state,
    const BoneChainRuntimeInput* chain_input,
    float dt,
    const Vec3& start_root_translation,
    const Quat& start_root_rotation,
    const Vec3& start_center_translation,
    float substep_fraction
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
    const Vec3 center_linear_velocity = chain_input != nullptr
        ? make_vec3(chain_input->center_linear_velocity)
        : Vec3{};
    const Vec3 root_linear_velocity = chain_input != nullptr
        ? make_vec3(chain_input->root_linear_velocity)
        : Vec3{};
    const Vec3 root_delta = sub(root_translation, chain_state.last_root_translation);
    const Vec3 center_delta = sub(center_translation, chain_state.last_center_translation);
    const Vec3 center_offset = sub(center_translation, root_translation);
    (void)center_rotation;
    const Vec3 gravity_direction = normalize_or_default(make_vec3(chain.gravity_direction), Vec3{0.0f, -1.0f, 0.0f});
    const float gravity_strength = std::clamp(chain.gravity_strength, 0.0f, 10.0f);
    const Vec3 gravity = mul(gravity_direction, 60.0f * gravity_strength);
    const float iterations = 8.0f;
    const std::size_t solver_iterations = 8;
    const std::vector<const ColliderDescriptor*> colliders = colliders_for_chain(scene, chain);

    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
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
            colliders
        );
        const float stiffness = std::clamp(bone.stiffness, 0.0f, 1.0f);
        const float stiffness_factor = stiffness;
        const float damping = clamp_unit(bone.damping);
        const float drag = clamp_unit(bone.drag);
        const float inertia_mix = std::clamp(0.985f - drag * 0.42f, 0.40f, 0.995f);
        const float root_inertia_mix = std::clamp(0.10f + stiffness_factor * 0.28f, 0.06f, 0.42f);
        const float chain_inertia_mix = std::clamp(0.36f + stiffness_factor * 0.22f - damping * 0.10f, 0.18f, 0.66f);
        const Vec3 velocity = mul(sub(state.position, state.previous_position), inertia_mix);
        Vec3 inherited_motion = mul(root_delta, root_inertia_mix);
        inherited_motion = add(inherited_motion, mul(center_delta, 0.12f));
        if (bone_index > 0) {
            const RuntimeBoneState& parent_state = chain_state.bones[bone_index - 1];
            const Vec3 parent_motion = sub(parent_state.position, parent_state.previous_position);
            const float depth_falloff = 1.0f / (1.0f + static_cast<float>(bone_index) * 0.35f);
            inherited_motion = add(
                inherited_motion,
                mul(parent_motion, chain_inertia_mix * depth_falloff)
            );
        }
        const float center_influence = 0.020f * (1.0f / (1.0f + static_cast<float>(bone_index) * 0.25f));
        const Vec3 center_force = add(mul(center_offset, center_influence), mul(center_linear_velocity, 0.015f * dt));
        const float gravity_motion_scale = (0.45f + (1.0f - stiffness_factor) * 0.85f) * bone.gravity_scale;
        const Vec3 predicted = add(
            add(state.position, add(velocity, inherited_motion)),
            add(add(mul(gravity, gravity_motion_scale * dt * dt), mul(root_linear_velocity, 0.06f * dt)), center_force)
        );
        state.previous_position = state.position;
        state.position = predicted;
        state.stretch_lambda = 0.0f;
        state.bend_lambda = 0.0f;
        state.shape_lambda = 0.0f;
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
                colliders,
                true
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
                colliders,
                false
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
        const float velocity_damping = std::clamp(
            0.992f - clamp_unit(bone.damping) * 0.35f - clamp_unit(bone.drag) * 0.18f,
            0.40f,
            0.995f
        );
        const Vec3 new_velocity = mul(sub(state.position, state.previous_position), velocity_damping);
        state.previous_position = sub(state.position, new_velocity);
        apply_collider_constraints(
            head_position,
            state,
            std::max(1.0e-5f, bone.length),
            std::max(chain.joint_radius, bone.radius),
            colliders
        );
    }

    chain_state.last_root_translation = root_translation;
    chain_state.last_root_rotation = root_rotation;
    chain_state.last_center_translation = center_translation;
}

void advance_runtime(RuntimeSceneState& scene_state, const RuntimeInputs& inputs) {
    const std::int32_t substeps = std::max<std::int32_t>(1, inputs.substeps);
    const float dt = inputs.dt > 0.0f ? inputs.dt : (1.0f / 60.0f);
    const float substep_dt = dt / static_cast<float>(substeps);
    std::vector<Vec3> start_root_translations(scene_state.chain_states.size(), Vec3{});
    std::vector<Quat> start_root_rotations(scene_state.chain_states.size(), Quat{});
    std::vector<Vec3> start_center_translations(scene_state.chain_states.size(), Vec3{});
    for (std::size_t chain_index = 0; chain_index < scene_state.chain_states.size(); ++chain_index) {
        start_root_translations[chain_index] = scene_state.chain_states[chain_index].last_root_translation;
        start_root_rotations[chain_index] = scene_state.chain_states[chain_index].last_root_rotation;
        start_center_translations[chain_index] = scene_state.chain_states[chain_index].last_center_translation;
    }

    for (std::int32_t substep = 0; substep < substeps; ++substep) {
        const float step_alpha = static_cast<float>(substep + 1) / static_cast<float>(substeps);
        for (std::size_t chain_index = 0; chain_index < scene_state.scene.bone_chains.size(); ++chain_index) {
            const BoneChainDescriptor& chain = scene_state.scene.bone_chains[chain_index];
            RuntimeBoneChainState& chain_state = scene_state.chain_states[chain_index];
            step_bone_chain(
                scene_state.scene,
                chain,
                chain_state,
                find_chain_input(scene_state.inputs, chain),
                substep_dt,
                start_root_translations[chain_index],
                start_root_rotations[chain_index],
                start_center_translations[chain_index],
                step_alpha
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
    info.collider_group_count = static_cast<std::uint64_t>(it->second.scene.collider_groups.size());
    info.cache_descriptor_count = static_cast<std::uint64_t>(it->second.scene.cache_descriptors.size());
    info.physics_scene_ready = it->second.physics_scene_ready;
    for (const BoneChainDescriptor& chain : it->second.scene.bone_chains) {
        info.bone_count += static_cast<std::uint64_t>(chain.bones.size());
    }
    return info;
}

}  // namespace hocloth
