#include "hocloth_runtime_api.hpp"
#include "hocloth/cloth/cloth_serialize_data.hpp"
#include "hocloth/cloth/constraints/collider_collision_constraint.hpp"
#include "hocloth/manager/transform/transform_record.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_container.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace hocloth {

namespace {

constexpr const char* kTailTipSuffix = "__hocloth_tail_tip__";
constexpr float kPi = 3.14159265358979323846f;

// From MC2: MagicaCloth2/Scripts/Core/Define/SystemDefine.cs
constexpr int kMc2DefaultSimulationFrequency = 90;
constexpr int kMc2SimulationFrequencyLow = 30;
constexpr int kMc2SimulationFrequencyHi = 150;
constexpr int kMc2DefaultMaxSimulationCountPerFrame = 3;
constexpr int kMc2MaxSimulationCountPerFrameLow = 1;
constexpr int kMc2MaxSimulationCountPerFrameHi = 5;
constexpr float kMc2BoneSpringDistanceStiffness = 0.5f;
constexpr float kMc2BoneSpringTetherCompressionLimit = 0.8f;
constexpr float kMc2BoneSpringCollisionFriction = 0.5f;

bool IsBoneCloth(const CompiledSpringBone& chain)
{
    return chain.cloth_type == "BoneCloth" || chain.component_type == "BONE_CLOTH";
}

bool IsBoneSpring(const CompiledSpringBone& chain)
{
    return !IsBoneCloth(chain);
}

float Clamp(float value, float low, float high)
{
    return std::clamp(value, low, high);
}

float Clamp01(float value)
{
    return Clamp(value, 0.0f, 1.0f);
}

float Sign(float value)
{
    if (value > 0.0f) {
        return 1.0f;
    }
    if (value < 0.0f) {
        return -1.0f;
    }
    return 0.0f;
}

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Scale(const Vec3& value, float scalar)
{
    return Vec3{value.x * scalar, value.y * scalar, value.z * scalar};
}

float Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length(const Vec3& value)
{
    return std::sqrt(Dot(value, value));
}

Vec3 ProjectOnPlane(const Vec3& value, const Vec3& normal)
{
    const float normal_length_sq = Dot(normal, normal);
    if (normal_length_sq <= 1.0e-8f) {
        return value;
    }
    return Sub(value, Scale(normal, Dot(value, normal) / normal_length_sq));
}

Vec3 ClampVectorLength(const Vec3& value, float max_length)
{
    const float length = Length(value);
    if (length <= max_length || length <= 1.0e-8f) {
        return value;
    }
    return Scale(value, max_length / length);
}

float AverageAbsScale(const Vec3& value)
{
    return (std::abs(value.x) + std::abs(value.y) + std::abs(value.z)) / 3.0f;
}

Vec3 Normalize(const Vec3& value)
{
    const float length = Length(value);
    if (length <= 1.0e-6f) {
        return Vec3{0.0f, -1.0f, 0.0f};
    }
    return Scale(value, 1.0f / length);
}

Quat Mul(const Quat& a, const Quat& b)
{
    return Quat{
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

Quat Conjugate(const Quat& value)
{
    return Quat{value.w, -value.x, -value.y, -value.z};
}

Quat NormalizeQuat(const Quat& value)
{
    const float length = std::sqrt(
        value.w * value.w
        + value.x * value.x
        + value.y * value.y
        + value.z * value.z
    );
    if (length <= 1.0e-8f) {
        return Quat{};
    }
    const float inv_length = 1.0f / length;
    return Quat{
        value.w * inv_length,
        value.x * inv_length,
        value.y * inv_length,
        value.z * inv_length,
    };
}

Vec3 RotateVector(const Quat& rotation, const Vec3& value)
{
    const Quat q = NormalizeQuat(rotation);
    const Quat p{0.0f, value.x, value.y, value.z};
    const Quat result = Mul(Mul(q, p), Conjugate(q));
    return Vec3{result.x, result.y, result.z};
}

Vec3 Mc2ToVec3(const mc2::float3& value)
{
    return Vec3{value.x, value.y, value.z};
}

Quat QuaternionFromPitchRoll(float pitch, float roll)
{
    // Blender pose bones use local +Y as the head-to-tail axis. The compact
    // BoneSpring plane stores positive pitch in the opposite visual direction,
    // so only flip the sign at the output adapter boundary.
    const float half_pitch = -pitch * 0.5f;
    const float half_roll = roll * 0.5f;
    const Quat quat_x{
        std::cos(half_pitch),
        std::sin(half_pitch),
        0.0f,
        0.0f,
    };
    const Quat quat_z{
        std::cos(half_roll),
        0.0f,
        0.0f,
        std::sin(half_roll),
    };
    return Mul(quat_z, quat_x);
}

Vec3 AxisToEuler(const Vec3& axis)
{
    const float angy = std::atan2(axis.x, axis.z);
    const Vec3 axis_y{0.0f, axis.y, 0.0f};
    const float angx = std::atan2(-axis.y, Length(Sub(axis, axis_y)));
    return Vec3{angx, angy, 0.0f};
}

float QuaternionDot(const Quat& a, const Quat& b)
{
    return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

Quat Slerp(const Quat& a, const Quat& b, float t)
{
    Quat qa = NormalizeQuat(a);
    Quat qb = NormalizeQuat(b);
    float dot = QuaternionDot(qa, qb);

    if (dot < 0.0f) {
        qb = Quat{-qb.w, -qb.x, -qb.y, -qb.z};
        dot = -dot;
    }

    if (dot > 0.9995f) {
        return NormalizeQuat(Quat{
            qa.w + (qb.w - qa.w) * t,
            qa.x + (qb.x - qa.x) * t,
            qa.y + (qb.y - qa.y) * t,
            qa.z + (qb.z - qa.z) * t,
        });
    }

    const float theta_0 = std::acos(Clamp(dot, -1.0f, 1.0f));
    const float theta = theta_0 * t;
    const float sin_theta = std::sin(theta);
    const float sin_theta_0 = std::sin(theta_0);

    const float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    const float s1 = sin_theta / sin_theta_0;
    return NormalizeQuat(Quat{
        qa.w * s0 + qb.w * s1,
        qa.x * s0 + qb.x * s1,
        qa.y * s0 + qb.y * s1,
        qa.z * s0 + qb.z * s1,
    });
}

float QuaternionAngle(const Quat& a, const Quat& b)
{
    const float dot = QuaternionDot(NormalizeQuat(a), NormalizeQuat(b));
    if (std::abs(dot) < 0.9999f) {
        const float angle = std::acos(Clamp(dot, -1.0f, 1.0f)) * 2.0f;
        return angle > kPi ? (kPi * 2.0f) - angle : angle;
    }
    return 0.0f;
}

void QuaternionToAngleAxis(const Quat& q, float& angle, Vec3& axis)
{
    const Quat normalized = NormalizeQuat(q);
    const float a = std::abs(normalized.w) < 0.9999f ? std::acos(normalized.w) : 0.0f;
    angle = 2.0f * a;
    const float s = std::sin(a);
    if (std::abs(s) < 1.0e-6f) {
        axis = Vec3{};
    } else {
        axis = Vec3{normalized.x / s, normalized.y / s, normalized.z / s};
    }
}

bool EndsWith(const std::string& value, const std::string& suffix)
{
    if (suffix.size() > value.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<CompiledSpringLine> BuildLineIndices(const CompiledSpringBone& chain)
{
    if (!chain.lines.empty()) {
        std::vector<CompiledSpringLine> result;
        result.reserve(chain.lines.size());
        const int joint_count = static_cast<int>(chain.joints.size());
        for (const CompiledSpringLine& line : chain.lines) {
            if (
                line.start_index >= 0
                && line.start_index < joint_count
                && line.end_index >= 0
                && line.end_index < joint_count
                && line.start_index != line.end_index
            ) {
                result.push_back(line);
            }
        }
        return result;
    }

    std::vector<CompiledSpringLine> result;
    result.reserve(chain.joints.size());
    for (std::size_t index = 0; index < chain.joints.size(); ++index) {
        const int parent_index = chain.joints[index].parent_index;
        if (
            parent_index >= 0
            && parent_index < static_cast<int>(chain.joints.size())
            && parent_index != static_cast<int>(index)
        ) {
            result.push_back(CompiledSpringLine{parent_index, static_cast<int>(index)});
        }
    }
    return result;
}

std::vector<std::vector<int>> BuildBaselinePaths(const CompiledSpringBone& chain)
{
    std::vector<std::vector<int>> result;
    if (!chain.baselines.empty()) {
        result.reserve(chain.baselines.size());
        for (const CompiledSpringBaseline& baseline : chain.baselines) {
            if (!baseline.joint_indices.empty()) {
                result.push_back(baseline.joint_indices);
            }
        }
        if (!result.empty()) {
            return result;
        }
    }

    std::unordered_map<int, std::vector<int>> parent_to_children;
    for (std::size_t index = 0; index < chain.joints.size(); ++index) {
        const int parent_index = chain.joints[index].parent_index;
        if (
            parent_index >= 0
            && parent_index < static_cast<int>(chain.joints.size())
            && parent_index != static_cast<int>(index)
        ) {
            parent_to_children[parent_index].push_back(static_cast<int>(index));
        }
    }

    for (std::size_t index = 0; index < chain.joints.size(); ++index) {
        if (parent_to_children.contains(static_cast<int>(index))) {
            continue;
        }

        std::vector<int> path;
        std::vector<bool> visited(chain.joints.size(), false);
        int current = static_cast<int>(index);
        while (current >= 0) {
            if (
                current >= static_cast<int>(chain.joints.size())
                || visited[static_cast<std::size_t>(current)]
            ) {
                path.clear();
                break;
            }
            visited[static_cast<std::size_t>(current)] = true;
            path.push_back(current);
            current = chain.joints[static_cast<std::size_t>(current)].parent_index;
        }
        if (path.empty()) {
            continue;
        }
        std::reverse(path.begin(), path.end());
        result.push_back(std::move(path));
    }

    return result;
}

struct JointState {
    float pitch = 0.0f;
    float roll = 0.0f;
    float pitch_velocity = 0.0f;
    float roll_velocity = 0.0f;
};

struct ChainCache {
    std::vector<CompiledSpringLine> line_indices;
    std::vector<std::vector<int>> baseline_paths;
    std::vector<JointState> joint_states;
    std::vector<Quat> last_basic_rotations;
    Quat last_root_rotation;
    Quat last_center_rotation;
    Vec3 last_center_translation;
    Vec3 step_vector;
    Quat step_rotation;
    Vec3 inertia_vector;
    Quat inertia_rotation;
    float velocity_weight = 1.0f;
    float angular_velocity = 0.0f;
    Vec3 rotation_axis;
    Vec3 rotation_axis_euler;
    bool has_runtime_rotation = false;
};

RuntimeChainInput* FindRuntimeChainInput(RuntimeInputs& inputs, const std::string& component_id)
{
    for (RuntimeChainInput& input : inputs.bone_chains) {
        if (input.component_id == component_id) {
            return &input;
        }
    }
    return nullptr;
}

const RuntimeChainInput* FindRuntimeChainInput(const RuntimeInputs& inputs, const std::string& component_id)
{
    for (const RuntimeChainInput& input : inputs.bone_chains) {
        if (input.component_id == component_id) {
            return &input;
        }
    }
    return nullptr;
}

Vec3 ToAnglePoint(const JointState& state)
{
    return Vec3{state.roll, state.pitch, 0.0f};
}

mc2::float3 ToMc2Float3(const Vec3& value)
{
    return mc2::float3{value.x, value.y, value.z};
}

mc2::quaternion ToMc2Quaternion(const Quat& value)
{
    return mc2::quaternion{value.w, value.x, value.y, value.z};
}

mc2::float3 ResolveParticleBasePosition(
    const CompiledSpringBone& chain,
    const RuntimeChainInput* runtime_input,
    std::size_t joint_index
)
{
    if (
        runtime_input != nullptr
        && joint_index < runtime_input->basic_head_positions.size()
    ) {
        return ToMc2Float3(runtime_input->basic_head_positions[joint_index]);
    }
    return ToMc2Float3(chain.joints[joint_index].rest_head_local);
}

mc2::ColliderManager::ColliderType CapsuleColliderType(
    const std::string& direction,
    bool aligned_on_center
)
{
    if (direction == "X") {
        return aligned_on_center
            ? mc2::ColliderManager::ColliderType::CapsuleXCenter
            : mc2::ColliderManager::ColliderType::CapsuleXStart;
    }
    if (direction == "Z") {
        return aligned_on_center
            ? mc2::ColliderManager::ColliderType::CapsuleZCenter
            : mc2::ColliderManager::ColliderType::CapsuleZStart;
    }
    return aligned_on_center
        ? mc2::ColliderManager::ColliderType::CapsuleYCenter
        : mc2::ColliderManager::ColliderType::CapsuleYStart;
}

mc2::ColliderManager::ColliderData ToMc2ColliderData(const CompiledCollisionObject& collision_object)
{
    mc2::ColliderManager::ColliderData data;
    data.enabled = true;
    data.reverse = collision_object.capsule_reverse_direction;
    data.center = mc2::float3{};
    data.frame_position = ToMc2Float3(collision_object.world_translation);
    data.frame_rotation = ToMc2Quaternion(collision_object.world_rotation);
    data.frame_scale = mc2::float3{1.0f, 1.0f, 1.0f};

    const float radius = std::max(0.001f, collision_object.radius);
    if (collision_object.shape_type == "CAPSULE") {
        data.type = CapsuleColliderType(
            collision_object.capsule_direction,
            collision_object.capsule_aligned_on_center
        );
        data.size = mc2::float3{
            radius,
            std::max(0.001f, collision_object.capsule_end_radius),
            std::max(radius * 2.0f, collision_object.height),
        };
    } else if (collision_object.shape_type == "PLANE") {
        data.type = mc2::ColliderManager::ColliderType::Plane;
        data.size = mc2::float3{};
    } else {
        data.type = mc2::ColliderManager::ColliderType::Sphere;
        data.size = mc2::float3{radius, 0.0f, 0.0f};
    }

    return data;
}

mc2::RenderSetupData BuildRuntimeBoneRenderSetup(const CompiledSpringBone& chain)
{
    mc2::RenderSetupData setup;
    setup.name = chain.component_id + "::runtime_render_setup";
    setup.setup_type = IsBoneCloth(chain)
        ? mc2::RenderSetupData::SetupType::BoneCloth
        : mc2::RenderSetupData::SetupType::BoneSpring;
    setup.bone_connection_mode = IsBoneCloth(chain)
        ? mc2::RenderSetupData::BoneConnectionMode::SequentialNonLoopMesh
        : mc2::RenderSetupData::BoneConnectionMode::Line;

    const int joint_count = static_cast<int>(chain.joints.size());
    setup.skin_bone_count = joint_count;
    setup.skin_root_bone_index = joint_count;
    setup.render_transform_index = joint_count;
    setup.vertex_count = joint_count;
    setup.transform_ids.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_parent_ids.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_names.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_positions.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_rotations.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_scales.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_local_positions.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_local_rotations.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_inverse_rotations.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_child_ids.resize(static_cast<std::size_t>(joint_count + 1));
    setup.collision_bone_indices = chain.collision_bone_indices;

    for (int joint_index = 0; joint_index < joint_count; ++joint_index) {
        const CompiledSpringJoint& joint = chain.joints[static_cast<std::size_t>(joint_index)];
        const bool is_root = joint.parent_index < 0;
        const int transform_id = joint_index + 1;
        const int parent_transform_id = joint.parent_index >= 0 ? joint.parent_index + 1 : 0;
        const mc2::float3 position = ToMc2Float3(joint.rest_head_local);
        const mc2::quaternion rotation = ToMc2Quaternion(joint.rest_local_rotation);

        setup.transform_ids.push_back(transform_id);
        setup.transform_parent_ids.push_back(parent_transform_id);
        setup.transform_names.push_back(joint.name);
        setup.transform_positions.push_back(position);
        setup.transform_rotations.push_back(rotation);
        setup.transform_scales.push_back(mc2::float3{1.0f, 1.0f, 1.0f});
        setup.transform_local_positions.push_back(ToMc2Float3(joint.rest_local_translation));
        setup.transform_local_rotations.push_back(rotation);
        setup.transform_inverse_rotations.push_back(mc2::Inverse(rotation));
        if (is_root) {
            setup.root_transform_ids.push_back(transform_id);
        } else if (joint.parent_index >= 0 && joint.parent_index < joint_count) {
            setup.transform_child_ids[static_cast<std::size_t>(joint.parent_index)].push_back(transform_id);
        }
    }

    const int render_transform_id = joint_count + 1;
    setup.transform_ids.push_back(render_transform_id);
    setup.transform_parent_ids.push_back(0);
    setup.transform_names.push_back(chain.armature_name.empty() ? "render" : chain.armature_name);
    setup.transform_positions.push_back(mc2::float3{});
    setup.transform_rotations.push_back(mc2::quaternion{});
    setup.transform_scales.push_back(mc2::float3{1.0f, 1.0f, 1.0f});
    setup.transform_local_positions.push_back(mc2::float3{});
    setup.transform_local_rotations.push_back(mc2::quaternion{});
    setup.transform_inverse_rotations.push_back(mc2::quaternion{});
    if (setup.root_transform_ids.empty() && joint_count > 0) {
        setup.root_transform_ids.push_back(1);
    }

    setup.init_render_local_to_world = mc2::float4x4{};
    setup.init_render_world_to_local = mc2::float4x4{};
    setup.init_render_rotation = mc2::quaternion{};
    setup.init_render_scale = mc2::float3{1.0f, 1.0f, 1.0f};
    setup.result.SetSuccess();
    return setup;
}

void FromAnglePoint(const Vec3& point, JointState& state)
{
    state.roll = Clamp(point.x, -1.2f, 1.2f);
    state.pitch = Clamp(point.y, -1.2f, 1.2f);
}

}  // namespace

struct RuntimeModule::SceneState {
    CompiledScene compiled_scene;
    RuntimeInputs runtime_inputs;
    std::vector<ChainCache> chain_caches;
    std::unordered_map<std::string, std::size_t> collision_object_lookup;
    std::unordered_map<std::string, std::vector<std::size_t>> collision_binding_lookup;
    std::vector<mc2::ColliderManager::ColliderData> mc2_collision_data;
    std::unordered_map<std::string, std::vector<std::size_t>> mc2_collision_binding_lookup;
    mc2::TeamManager mc2_team_manager;
    mc2::TransformManager mc2_transform_manager;
    mc2::SimulationManager mc2_simulation_manager;
    mc2::VirtualMeshManager mc2_virtual_mesh_manager;
    mc2::ColliderManager mc2_collider_manager;
    mc2::ColliderCollisionConstraint mc2_collider_collision_constraint;
    std::vector<int> mc2_chain_team_ids;
    std::vector<std::vector<std::size_t>> mc2_chain_collider_indices;
    std::vector<mc2::SimulationManager::ParticleChunkSet> mc2_chain_particle_chunks;
    std::vector<std::shared_ptr<mc2::VirtualMesh>> mc2_chain_proxy_meshes;
    SimulationTimeState time_state;
    std::uint64_t steps = 0;
};

std::string CompiledScene::Summary() const
{
    std::size_t total_bones = 0;
    std::size_t bone_cloths = 0;
    for (const CompiledSpringBone& chain : spring_bones) {
        total_bones += chain.joints.size();
        if (chain.cloth_type == "BoneCloth" || chain.component_type == "BONE_CLOTH") {
            ++bone_cloths;
        }
    }
    const std::size_t bone_springs = spring_bones.size() - bone_cloths;

    std::ostringstream stream;
    stream << "spring_bones=" << spring_bones.size()
           << ", bone_cloths=" << bone_cloths
           << ", bone_springs=" << bone_springs
           << ", bones=" << total_bones
           << ", colliders=0"
           << ", collider_groups=0"
           << ", collision_objects=" << collision_objects.size()
           << ", collision_bindings=" << collision_bindings.size()
           << ", cache_outputs=0"
           << ", mesh_writeback_targets=" << mesh_writeback_targets.size();
    return stream.str();
}

std::string RuntimeBackendStatus(const RuntimeModule::SceneState& scene)
{
    std::ostringstream stream;
    stream << "hocloth_mc2_core:runtime_build managers=deferred"
           << " mc2_collider_data=" << scene.mc2_collision_data.size()
           << " mc2_collider_bindings=" << scene.mc2_collision_binding_lookup.size()
           << " mc2_collider_teams=" << scene.mc2_chain_team_ids.size()
           << " mc2_registered_colliders=" << scene.mc2_collider_manager.DataCount()
           << " mc2_collider_work=" << scene.mc2_collider_manager.WorkDataArray().Count()
           << " mc2_particles=" << scene.mc2_simulation_manager.ParticleCount()
           << " mc2_proxy_vertices=" << scene.mc2_virtual_mesh_manager.ProxyVertexCount()
           << " mc2_step_particles=" << scene.mc2_simulation_manager.ProcessingStepParticles().Count()
           << " bone_cloths=" << std::count_if(
               scene.compiled_scene.spring_bones.begin(),
               scene.compiled_scene.spring_bones.end(),
               IsBoneCloth
           )
           << " mesh_writeback_targets=" << scene.compiled_scene.mesh_writeback_targets.size();
    return stream.str();
}

std::vector<std::size_t> ResolveChainColliderIndices(
    const RuntimeModule::SceneState& scene,
    const CompiledSpringBone& chain
)
{
    std::vector<std::size_t> result;
    for (const std::string& binding_id : chain.collision_binding_ids) {
        auto binding_it = scene.mc2_collision_binding_lookup.find(binding_id);
        if (binding_it == scene.mc2_collision_binding_lookup.end()) {
            continue;
        }

        for (std::size_t collider_index : binding_it->second) {
            if (collider_index < scene.mc2_collision_data.size()
                && std::find(result.begin(), result.end(), collider_index) == result.end()) {
                result.push_back(collider_index);
            }
        }
    }
    return result;
}

std::shared_ptr<mc2::VirtualMesh> BuildRuntimeProxyMesh(const CompiledSpringBone& chain)
{
    const mc2::RenderSetupData render_setup = BuildRuntimeBoneRenderSetup(chain);
    auto mesh = std::make_shared<mc2::VirtualMesh>();
    mesh->name = chain.component_id + "::runtime_proxy";
    mesh->ImportFromRenderSetup(render_setup);
    if (mesh->IsValid()) {
        mc2::ClothSerializeData serialize_data;
        serialize_data.cloth_type = IsBoneCloth(chain)
            ? mc2::ClothType::BoneCloth
            : mc2::ClothType::BoneSpring;
        serialize_data.root_bone_ids = render_setup.root_transform_ids;
        serialize_data.connection_mode = render_setup.bone_connection_mode;
        serialize_data.gravity = chain.gravity_strength;
        serialize_data.gravity_direction = ToMc2Float3(Normalize(chain.gravity_direction));
        serialize_data.custom_skinning_setting.enable = false;
        serialize_data.normal_alignment_setting.alignment_mode =
            mc2::NormalAlignmentSettings::AlignmentMode::None;

        mc2::TransformRecord cloth_transform_record =
            render_setup.GetTransformRecordFromIndex(render_setup.render_transform_index);
        if (!cloth_transform_record.IsValid()) {
            cloth_transform_record.id = render_setup.render_transform_index + 1;
            cloth_transform_record.scale = render_setup.init_render_scale;
            cloth_transform_record.local_to_world_matrix = render_setup.init_render_local_to_world;
            cloth_transform_record.world_to_local_matrix = render_setup.init_render_world_to_local;
        }
        mesh->ConvertProxyMesh(
            serialize_data,
            cloth_transform_record,
            {},
            mc2::TransformRecord{}
        );
    }
    mesh->is_managed = true;
    mesh->mesh_type = mc2::VirtualMesh::MeshType::ProxyBoneMesh;

    return mesh;
}

void RebuildMc2ColliderManagers(RuntimeModule::SceneState& scene)
{
    scene.mc2_collider_collision_constraint.Dispose();
    scene.mc2_collider_manager.Dispose();
    scene.mc2_virtual_mesh_manager.Dispose();
    scene.mc2_simulation_manager.Dispose();
    scene.mc2_transform_manager.Dispose();
    scene.mc2_team_manager.Dispose();
    scene.mc2_collider_manager.Initialize();
    scene.mc2_virtual_mesh_manager.Initialize();
    scene.mc2_simulation_manager.Initialize();
    scene.mc2_transform_manager.Initialize();
    scene.mc2_team_manager.Initialize();

    scene.mc2_chain_team_ids.clear();
    scene.mc2_chain_collider_indices.clear();
    scene.mc2_chain_particle_chunks.clear();
    scene.mc2_chain_proxy_meshes.clear();
    scene.mc2_chain_team_ids.reserve(scene.compiled_scene.spring_bones.size());
    scene.mc2_chain_collider_indices.reserve(scene.compiled_scene.spring_bones.size());
    scene.mc2_chain_particle_chunks.reserve(scene.compiled_scene.spring_bones.size());
    scene.mc2_chain_proxy_meshes.reserve(scene.compiled_scene.spring_bones.size());

    for (const CompiledSpringBone& chain : scene.compiled_scene.spring_bones) {
        mc2::ClothParameters parameters;
        parameters.gravity = IsBoneCloth(chain) ? std::max(0.0f, chain.gravity_strength) : 0.0f;
        parameters.world_gravity_direction = ToMc2Float3(Normalize(chain.gravity_direction));
        parameters.radius_curve_data = mc2::ConstantCurve(std::max(0.0001f, chain.joint_radius));
        parameters.damping_curve_data = mc2::ConstantCurve(Clamp01(chain.damping));
        parameters.collider_collision_constraint.mode = mc2::ColliderCollisionMode::Point;
        parameters.collider_collision_constraint.dynamic_friction = Clamp(chain.collider_friction, 0.0f, 0.5f);
        parameters.collider_collision_constraint.static_friction = Clamp(chain.collider_friction, 0.0f, 0.5f);
        parameters.collider_collision_constraint.limit_distance =
            mc2::ConstantCurve(std::max(0.0001f, chain.collider_limit_distance));

        const int team_id = scene.mc2_team_manager.CreateTeam(parameters, true, IsBoneSpring(chain));
        auto proxy_mesh = BuildRuntimeProxyMesh(chain);
        mc2::VirtualMeshContainer proxy_container(proxy_mesh);
        scene.mc2_virtual_mesh_manager.RegisterProxyMesh(
            team_id,
            proxy_container,
            scene.mc2_team_manager,
            scene.mc2_transform_manager
        );

        mc2::SimulationManager::ParticleChunkSet particle_chunks =
            scene.mc2_simulation_manager.RegisterParticleRange(
                team_id,
                static_cast<int>(chain.joints.size())
            );
        mc2::TeamManager::TeamData& team_data = scene.mc2_team_manager.GetTeamData(team_id);
        team_data.particle_chunk = particle_chunks.next_pos_chunk;
        team_data.proxy_mesh_type = proxy_mesh->mesh_type;

        for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
            const int particle_index = particle_chunks.next_pos_chunk.start_index + static_cast<int>(joint_index);
            if (particle_index < 0 || particle_index >= scene.mc2_simulation_manager.NextPositions().Length()) {
                continue;
            }

            const CompiledSpringJoint& joint = chain.joints[joint_index];
            const mc2::float3 position = ToMc2Float3(joint.rest_head_local);
            const mc2::quaternion rotation = ToMc2Quaternion(joint.rest_local_rotation);
            scene.mc2_simulation_manager.NextPositions()[particle_index] = position;
            scene.mc2_simulation_manager.OldPositions()[particle_index] = position;
            scene.mc2_simulation_manager.BasePositions()[particle_index] = position;
            scene.mc2_simulation_manager.VelocityPositions()[particle_index] = position;
            scene.mc2_simulation_manager.StepBasicPositions()[particle_index] = position;
            scene.mc2_simulation_manager.BaseRotations()[particle_index] = rotation;
            scene.mc2_simulation_manager.StepBasicRotations()[particle_index] = rotation;
        }

        std::vector<std::size_t> collider_indices = ResolveChainColliderIndices(scene, chain);
        std::vector<mc2::ColliderManager::ColliderData> colliders;
        colliders.reserve(collider_indices.size());
        for (std::size_t collider_index : collider_indices) {
            colliders.push_back(scene.mc2_collision_data[collider_index]);
        }

        if (!colliders.empty()) {
            scene.mc2_collider_manager.RegisterColliderDataRange(
                team_id,
                scene.mc2_team_manager,
                scene.mc2_transform_manager,
                colliders
            );
        }

        scene.mc2_chain_team_ids.push_back(team_id);
        scene.mc2_chain_collider_indices.push_back(std::move(collider_indices));
        scene.mc2_chain_particle_chunks.push_back(particle_chunks);
        scene.mc2_chain_proxy_meshes.push_back(std::move(proxy_mesh));
    }
}

void UpdateMc2ColliderWorkData(RuntimeModule::SceneState& scene)
{
    scene.mc2_simulation_manager.BeginSimulationStep();
    const int update_count = scene.mc2_team_manager.AlwaysTeamUpdate(
        scene.time_state.simulation_delta_time,
        scene.time_state.simulation_delta_time,
        scene.time_state.simulation_delta_time,
        scene.time_state.global_time_scale,
        scene.time_state.simulation_delta_time,
        1
    );
    if (update_count <= 0) {
        scene.mc2_simulation_manager.EndSimulationStep();
        return;
    }

    for (const mc2::SimulationManager::ParticleChunkSet& chunks : scene.mc2_chain_particle_chunks) {
        for (int offset = 0; offset < chunks.ParticleCount(); ++offset) {
            scene.mc2_simulation_manager.MarkStepParticle(chunks.next_pos_chunk.start_index + offset);
        }
    }

    scene.mc2_virtual_mesh_manager.PreProxyMeshUpdate(
        scene.mc2_team_manager,
        scene.mc2_transform_manager
    );
    scene.mc2_collider_manager.PreSimulationUpdate(scene.mc2_team_manager, scene.mc2_transform_manager);
    scene.mc2_collider_manager.CreateUpdateColliderList(
        0,
        scene.mc2_team_manager,
        scene.mc2_simulation_manager
    );
    scene.mc2_collider_manager.StartSimulationStep(scene.mc2_team_manager, scene.mc2_simulation_manager);
    scene.mc2_collider_collision_constraint.WorkBufferUpdate(
        scene.mc2_simulation_manager.ParticleCount(),
        0
    );
    scene.mc2_collider_collision_constraint.Solve(
        scene.mc2_team_manager,
        scene.mc2_virtual_mesh_manager,
        scene.mc2_collider_manager,
        scene.mc2_simulation_manager
    );
    scene.mc2_collider_manager.EndSimulationStep(scene.mc2_simulation_manager);
    scene.mc2_simulation_manager.EndSimulationStep();
}

void SyncJointStatesToMc2Particles(
    RuntimeModule::SceneState& scene,
    std::size_t chain_index
)
{
    if (chain_index >= scene.compiled_scene.spring_bones.size()
        || chain_index >= scene.chain_caches.size()
        || chain_index >= scene.mc2_chain_particle_chunks.size()) {
        return;
    }

    const CompiledSpringBone& chain = scene.compiled_scene.spring_bones[chain_index];
    const ChainCache& cache = scene.chain_caches[chain_index];
    const mc2::SimulationManager::ParticleChunkSet& chunks =
        scene.mc2_chain_particle_chunks[chain_index];
    const RuntimeChainInput* runtime_input = FindRuntimeChainInput(scene.runtime_inputs, chain.component_id);

    for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
        const int particle_index = chunks.next_pos_chunk.start_index + static_cast<int>(joint_index);
        if (particle_index < 0 || particle_index >= scene.mc2_simulation_manager.NextPositions().Length()) {
            continue;
        }

        const CompiledSpringJoint& joint = chain.joints[joint_index];
        const int driven_joint_index = joint.parent_index >= 0
            ? joint.parent_index
            : static_cast<int>(joint_index);
        const CompiledSpringJoint& driven_joint =
            chain.joints[static_cast<std::size_t>(driven_joint_index)];
        const Vec3 point = ToAnglePoint(cache.joint_states[static_cast<std::size_t>(driven_joint_index)]);
        const float point_scale = std::max(0.05f, driven_joint.length);
        const mc2::float3 base_position = ResolveParticleBasePosition(chain, runtime_input, joint_index);
        Quat basis_rotation = driven_joint.rest_local_rotation;
        if (
            runtime_input != nullptr
            && static_cast<std::size_t>(driven_joint_index) < runtime_input->basic_rotations.size()
        ) {
            basis_rotation = runtime_input->basic_rotations[static_cast<std::size_t>(driven_joint_index)];
        }
        const Vec3 world_offset = RotateVector(
            basis_rotation,
            Vec3{-point.x * point_scale, 0.0f, -point.y * point_scale}
        );
        const mc2::float3 offset = ToMc2Float3(world_offset);
        const mc2::float3 next_position{
            base_position.x + offset.x,
            base_position.y + offset.y,
            base_position.z + offset.z,
        };

        scene.mc2_simulation_manager.BasePositions()[particle_index] = base_position;
        scene.mc2_simulation_manager.NextPositions()[particle_index] = next_position;
        scene.mc2_simulation_manager.VelocityPositions()[particle_index] = next_position;
        scene.mc2_simulation_manager.StepBasicPositions()[particle_index] = base_position;
    }
}

void ApplyMc2ParticlesToJointStates(
    RuntimeModule::SceneState& scene,
    std::size_t chain_index
)
{
    if (chain_index >= scene.compiled_scene.spring_bones.size()
        || chain_index >= scene.chain_caches.size()
        || chain_index >= scene.mc2_chain_particle_chunks.size()) {
        return;
    }

    const CompiledSpringBone& chain = scene.compiled_scene.spring_bones[chain_index];
    if (IsBoneCloth(chain)) {
        return;
    }

    ChainCache& cache = scene.chain_caches[chain_index];
    const mc2::SimulationManager::ParticleChunkSet& chunks =
        scene.mc2_chain_particle_chunks[chain_index];
    const RuntimeChainInput* runtime_input = FindRuntimeChainInput(scene.runtime_inputs, chain.component_id);

    std::vector<Vec3> accumulated_points(chain.joints.size(), Vec3{});
    std::vector<int> accumulated_counts(chain.joints.size(), 0);

    for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
        const CompiledSpringJoint& joint = chain.joints[joint_index];
        if (joint.parent_index < 0) {
            continue;
        }
        const std::size_t driven_joint_index = static_cast<std::size_t>(joint.parent_index);
        if (driven_joint_index >= chain.joints.size()) {
            continue;
        }
        const CompiledSpringJoint& driven_joint = chain.joints[driven_joint_index];

        const int particle_index = chunks.next_pos_chunk.start_index + static_cast<int>(joint_index);
        if (particle_index < 0 || particle_index >= scene.mc2_simulation_manager.NextPositions().Length()) {
            continue;
        }

        const mc2::float3 next_position = scene.mc2_simulation_manager.NextPositions()[particle_index];
        const mc2::float3 base_position = scene.mc2_simulation_manager.BasePositions()[particle_index];
        const float point_scale = std::max(0.05f, driven_joint.length);
        Vec3 world_offset = Sub(Mc2ToVec3(next_position), Mc2ToVec3(base_position));
        Quat basis_rotation = driven_joint.rest_local_rotation;
        if (
            runtime_input != nullptr
            && driven_joint_index < runtime_input->basic_rotations.size()
        ) {
            basis_rotation = runtime_input->basic_rotations[driven_joint_index];
        }
        const Vec3 local_offset = RotateVector(Conjugate(basis_rotation), world_offset);
        Vec3 point{
            -local_offset.x / point_scale,
            -local_offset.z / point_scale,
            0.0f,
        };
        if (IsBoneCloth(chain)) {
            point = ClampVectorLength(point, 0.35f);
        }
        accumulated_points[driven_joint_index] = Add(accumulated_points[driven_joint_index], point);
        accumulated_counts[driven_joint_index] += 1;
    }

    for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
        if (accumulated_counts[joint_index] <= 0) {
            continue;
        }
        const Vec3 average_point = Scale(
            accumulated_points[joint_index],
            1.0f / static_cast<float>(accumulated_counts[joint_index])
        );
        FromAnglePoint(average_point, cache.joint_states[joint_index]);
    }
}

void SimulationTimeState::FrameUpdate(int requested_frequency)
{
    // From MC2: MagicaCloth2/Scripts/Core/Manager/Simulation/TimeManager.cs
    simulation_frequency = std::clamp(
        requested_frequency,
        kMc2SimulationFrequencyLow,
        kMc2SimulationFrequencyHi
    );
    max_simulation_count_per_frame = std::clamp(
        max_simulation_count_per_frame,
        kMc2MaxSimulationCountPerFrameLow,
        kMc2MaxSimulationCountPerFrameHi
    );
    global_time_scale = Clamp01(global_time_scale);

    simulation_delta_time = 1.0f / static_cast<float>(simulation_frequency);
    max_delta_time = simulation_delta_time * static_cast<float>(max_simulation_count_per_frame);

    const float t = static_cast<float>(kMc2DefaultSimulationFrequency) / static_cast<float>(simulation_frequency);
    simulation_power[0] = t;
    simulation_power[1] = t > 1.0f ? std::pow(t, 0.5f) : t;
    simulation_power[2] = t > 1.0f ? std::pow(t, 0.3f) : t;
    simulation_power[3] = std::pow(t, 1.8f);
}

namespace {

void BuildSceneCaches(RuntimeModule::SceneState& scene)
{
    scene.chain_caches.clear();
    scene.chain_caches.reserve(scene.compiled_scene.spring_bones.size());
    for (const CompiledSpringBone& chain : scene.compiled_scene.spring_bones) {
        ChainCache cache;
        cache.line_indices = BuildLineIndices(chain);
        cache.baseline_paths = BuildBaselinePaths(chain);
        cache.joint_states.resize(chain.joints.size());
        cache.last_basic_rotations.resize(chain.joints.size());
        scene.chain_caches.push_back(std::move(cache));
    }

    scene.collision_object_lookup.clear();
    for (std::size_t index = 0; index < scene.compiled_scene.collision_objects.size(); ++index) {
        const std::string& collision_object_id = scene.compiled_scene.collision_objects[index].collision_object_id;
        scene.collision_object_lookup[collision_object_id] = index;
    }

    scene.mc2_collision_data.clear();
    scene.mc2_collision_data.reserve(scene.compiled_scene.collision_objects.size());
    for (const CompiledCollisionObject& collision_object : scene.compiled_scene.collision_objects) {
        scene.mc2_collision_data.push_back(ToMc2ColliderData(collision_object));
    }

    scene.collision_binding_lookup.clear();
    scene.mc2_collision_binding_lookup.clear();
    for (const CompiledCollisionBinding& binding : scene.compiled_scene.collision_bindings) {
        std::vector<std::size_t> indices;
        indices.reserve(binding.collision_object_ids.size());
        for (const std::string& collision_object_id : binding.collision_object_ids) {
            auto it = scene.collision_object_lookup.find(collision_object_id);
            if (it != scene.collision_object_lookup.end()) {
                indices.push_back(it->second);
            }
        }
        scene.collision_binding_lookup[binding.binding_id] = std::move(indices);
        scene.mc2_collision_binding_lookup[binding.binding_id] = scene.collision_binding_lookup[binding.binding_id];
    }
}

void ApplyCollisionObjectInputs(RuntimeModule::SceneState& scene)
{
    for (const RuntimeCollisionObjectInput& collision_input : scene.runtime_inputs.collision_objects) {
        auto it = scene.collision_object_lookup.find(collision_input.collision_object_id);
        if (it == scene.collision_object_lookup.end()) {
            continue;
        }

        CompiledCollisionObject& collision_object = scene.compiled_scene.collision_objects[it->second];
        collision_object.world_translation = collision_input.world_translation;
        collision_object.world_rotation = collision_input.world_rotation;
        collision_object.linear_velocity = collision_input.linear_velocity;
        if (it->second < scene.mc2_collision_data.size()) {
            scene.mc2_collision_data[it->second] = ToMc2ColliderData(collision_object);
        }
    }

    for (std::size_t chain_index = 0; chain_index < scene.mc2_chain_team_ids.size(); ++chain_index) {
        const int team_id = scene.mc2_chain_team_ids[chain_index];
        if (!scene.mc2_team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const std::vector<std::size_t>& collider_indices = scene.mc2_chain_collider_indices[chain_index];
        for (std::size_t local_index = 0; local_index < collider_indices.size(); ++local_index) {
            const std::size_t collider_index = collider_indices[local_index];
            if (collider_index >= scene.mc2_collision_data.size()) {
                continue;
            }
            scene.mc2_collider_manager.UpdateColliderData(
                team_id,
                static_cast<int>(local_index),
                scene.mc2_team_manager,
                scene.mc2_transform_manager,
                scene.mc2_collision_data[collider_index]
            );
        }
    }
}

void ApplyMc2SpringConstraint(
    const RuntimeModule::SceneState& scene,
    const CompiledSpringBone& chain,
    const RuntimeChainInput* runtime_input,
    const CompiledSpringJoint& joint,
    std::size_t joint_index,
    JointState& joint_state
)
{
    if (!IsBoneSpring(chain)) {
        return;
    }
    if (!chain.use_spring || chain.spring_power <= 1.0e-6f) {
        return;
    }

    Vec3 point = ToAnglePoint(joint_state);
    Vec3 v = point;

    const Vec3 scale_source = runtime_input ? runtime_input->root_scale : chain.armature_scale;
    const float scale_ratio = std::max(0.0001f, AverageAbsScale(scale_source));

    // From MC2: MagicaCloth2/Scripts/Core/Manager/Simulation/SimulationManager.cs Spring(...)
    const float limit_distance = std::max(0.0f, chain.limit_distance * scale_ratio) / std::max(0.05f, joint.length);
    if (limit_distance > 1.0e-8f) {
        const float len = Length(v);
        if (len > limit_distance) {
            v = Scale(v, limit_distance / len);
        }

        if (chain.normal_limit_ratio < 1.0f) {
            const float ylen = v.y;
            Vec3 vx = v;
            vx.y = 0.0f;
            const float xlen = Length(vx);
            const float t = Clamp01(limit_distance > 1.0e-8f ? xlen / limit_distance : 0.0f);
            float y = std::cos(std::asin(t));
            y *= limit_distance * Clamp01(chain.normal_limit_ratio);

            if (std::abs(ylen) > y) {
                v.y -= (std::abs(ylen) - y) * Sign(ylen);
            }
        }
    } else {
        v = Vec3{};
    }

    float power = Clamp(chain.spring_power, 0.0f, 1.0f);
    if (chain.spring_noise > 0.0f) {
        const float noise_time =
            static_cast<float>((scene.steps + 1) * 24512ULL) * 0.0001f
            + static_cast<float>(joint_index) * 49.6198f
            + point.x
            + point.y;
        float noise = std::sin(noise_time);
        noise *= Clamp01(chain.spring_noise) * 0.6f;
        power = std::max(power + power * noise, 0.0f);
    }

    v = Sub(v, Scale(v, power));
    FromAnglePoint(v, joint_state);
}

void StepChain(
    RuntimeModule::SceneState& scene,
    const CompiledSpringBone& chain,
    ChainCache& cache
)
{
    if (cache.joint_states.empty()) {
        return;
    }

    const RuntimeChainInput* runtime_input = FindRuntimeChainInput(scene.runtime_inputs, chain.component_id);

    for (const std::vector<int>& path : cache.baseline_paths) {
        if (path.empty()) {
            continue;
        }

        JointState& root_state = cache.joint_states[static_cast<std::size_t>(path.front())];
        root_state = JointState{};
    }

    const Vec3 center_offset = runtime_input
        ? Sub(runtime_input->center_translation, runtime_input->root_translation)
        : Vec3{};
    const Vec3 center_velocity = runtime_input
        ? Sub(runtime_input->center_linear_velocity, runtime_input->root_linear_velocity)
        : Vec3{};
    const bool is_bone_cloth = IsBoneCloth(chain);
    const Vec3 gravity_direction = Normalize(chain.gravity_direction);
    const float gravity_strength = is_bone_cloth ? std::max(0.0f, chain.gravity_strength) : 0.0f;

    if (
        runtime_input != nullptr
        && cache.has_runtime_rotation
        && runtime_input->basic_rotations.size() == cache.last_basic_rotations.size()
    ) {
        // From MC2: SimulationManager keeps per-particle baseRot and applies inertia rotation
        // before the spring solve. Here we mirror that with per-joint basic rotation deltas.
        for (std::size_t joint_index = 0; joint_index < cache.joint_states.size(); ++joint_index) {
            const Quat joint_delta = NormalizeQuat(
                Mul(runtime_input->basic_rotations[joint_index], Conjugate(cache.last_basic_rotations[joint_index]))
            );

            JointState& state = cache.joint_states[joint_index];
            const Vec3 rotated_point = RotateVector(joint_delta, ToAnglePoint(state));
            const Vec3 rotated_velocity = RotateVector(
                joint_delta,
                Vec3{state.roll_velocity, state.pitch_velocity, 0.0f}
            );
            FromAnglePoint(rotated_point, state);
            state.roll_velocity = Clamp(rotated_velocity.x, -8.0f, 8.0f);
            state.pitch_velocity = Clamp(rotated_velocity.y, -8.0f, 8.0f);
        }
    }

    if (runtime_input != nullptr) {
        // From MC2: TeamManager computes centerData.stepVector / stepRotation / inertiaVector / inertiaRotation.
        const Vec3 raw_step_vector = cache.has_runtime_rotation
            ? Sub(runtime_input->center_translation, cache.last_center_translation)
            : Vec3{};
        const Quat raw_step_rotation = cache.has_runtime_rotation
            ? NormalizeQuat(Mul(runtime_input->center_rotation_quaternion, Conjugate(cache.last_center_rotation)))
            : Quat{};

        cache.step_vector = raw_step_vector;
        cache.step_rotation = raw_step_rotation;

        // From MC2: MagicaCloth2/Res/Preset/*.json + InertiaConstraint.cs
        const float local_movement_inertia = Clamp01(chain.inertia_local_inertia);
        const float local_rotation_inertia = Clamp01(chain.inertia_local_inertia);
        cache.inertia_vector = Scale(raw_step_vector, local_movement_inertia);
        cache.inertia_rotation = Slerp(Quat{}, raw_step_rotation, local_rotation_inertia);
        cache.velocity_weight = cache.has_runtime_rotation ? 1.0f : 0.0f;
        float step_angle = cache.has_runtime_rotation
            ? QuaternionAngle(cache.last_center_rotation, runtime_input->center_rotation_quaternion)
            : 0.0f;
        cache.angular_velocity = scene.time_state.simulation_delta_time > 1.0e-8f
            ? step_angle / scene.time_state.simulation_delta_time
            : 0.0f;
        QuaternionToAngleAxis(cache.step_rotation, step_angle, cache.rotation_axis);
        cache.rotation_axis = Normalize(cache.rotation_axis);
        cache.rotation_axis_euler = AxisToEuler(cache.rotation_axis);
    }

    for (const std::vector<int>& path : cache.baseline_paths) {
        if (path.size() < 2) {
            continue;
        }

        const float path_depth = static_cast<float>(std::max<std::size_t>(1, path.size() - 1));
        for (std::size_t path_index = 1; path_index < path.size(); ++path_index) {
            const int joint_index = path[path_index];
            const CompiledSpringJoint& joint = chain.joints[static_cast<std::size_t>(joint_index)];
            JointState& state = cache.joint_states[static_cast<std::size_t>(joint_index)];
            const JointState& parent_state = cache.joint_states[static_cast<std::size_t>(joint.parent_index)];

            const float stiffness = Clamp01(joint.stiffness > 0.0f ? joint.stiffness : chain.stiffness);
            const float damping = Clamp01(joint.damping > 0.0f ? joint.damping : chain.damping);
            const float drag = Clamp01(joint.drag > 0.0f ? joint.drag : chain.drag);
            const float depth_ratio = static_cast<float>(path_index) / path_depth;
            const float inertia_depth = Clamp01(chain.inertia_depth_inertia) * (1.0f - depth_ratio * depth_ratio);

            Vec3 inertia_vector = cache.inertia_vector;
            Quat inertia_rotation = cache.inertia_rotation;
            inertia_vector = Add(
                Scale(inertia_vector, 1.0f - inertia_depth),
                Scale(cache.step_vector, inertia_depth)
            );
            inertia_rotation = Slerp(inertia_rotation, cache.step_rotation, inertia_depth);

            const Vec3 inertia_point = RotateVector(inertia_rotation, ToAnglePoint(state));
            const Vec3 inertia_velocity = RotateVector(
                inertia_rotation,
                Vec3{state.roll_velocity, state.pitch_velocity, 0.0f}
            );
            const float world_inertia = Clamp01(chain.inertia_world_inertia);
            const float movement_inertia_smoothing = Clamp01(chain.inertia_movement_inertia_smoothing);
            Vec3 shifted_point = Add(
                inertia_point,
                Vec3{
                    inertia_vector.x * world_inertia * (0.12f * (1.0f - movement_inertia_smoothing * 0.4f)),
                    inertia_vector.y * world_inertia * (0.12f * (1.0f - movement_inertia_smoothing * 0.4f)),
                    0.0f,
                }
            );
            if (gravity_strength > 1.0e-6f) {
                const float gravity_gain =
                    gravity_strength
                    * std::max(0.0f, joint.gravity_scale)
                    * (0.0009f + depth_ratio * 0.0012f)
                    * std::sqrt(std::max(0.05f, joint.length));
                shifted_point = Add(
                    shifted_point,
                    Vec3{
                        gravity_direction.x * gravity_gain,
                        -gravity_direction.y * gravity_gain,
                        0.0f,
                    }
                );
            }
            FromAnglePoint(shifted_point, state);
            const float particle_speed_limit = chain.inertia_particle_speed_limit_enabled
                ? std::max(0.0f, chain.inertia_particle_speed_limit)
                : 8.0f;
            state.roll_velocity = Clamp(
                inertia_velocity.x * cache.velocity_weight,
                -particle_speed_limit,
                particle_speed_limit
            );
            state.pitch_velocity = Clamp(
                inertia_velocity.y * cache.velocity_weight,
                -particle_speed_limit,
                particle_speed_limit
            );

            if (cache.angular_velocity > 1.0e-5f) {
                // From MC2: centrifugal acceleration reacts to angular velocity and rotation axis.
                const Vec3 lpos = ToAnglePoint(state);
                const Vec3 plane_pos = ProjectOnPlane(lpos, cache.rotation_axis_euler);
                const float radius = Length(plane_pos);
                if (radius > 1.0e-6f) {
                    const Vec3 radial = Scale(plane_pos, 1.0f / radius);
                    const Vec3 tangent = Normalize(Vec3{
                        cache.rotation_axis_euler.y * radial.z - cache.rotation_axis_euler.z * radial.y,
                        cache.rotation_axis_euler.z * radial.x - cache.rotation_axis_euler.x * radial.z,
                        cache.rotation_axis_euler.x * radial.y - cache.rotation_axis_euler.y * radial.x,
                    });
                    const Vec3 velocity_dir = Normalize(Vec3{state.roll_velocity, state.pitch_velocity, 0.0f});
                    const float tangent_follow = Clamp01(Dot(velocity_dir, tangent));
                    const float accel = (1.0f + (1.0f - depth_ratio))
                        * cache.angular_velocity
                        * cache.angular_velocity
                        * radius
                        * (0.02f + Clamp01(chain.inertia_centrifugal_acceleration) * 0.02f)
                        * tangent_follow;
                    state.roll_velocity = Clamp(
                        state.roll_velocity + radial.x * accel,
                        -particle_speed_limit,
                        particle_speed_limit
                    );
                    state.pitch_velocity = Clamp(
                        state.pitch_velocity + radial.y * accel,
                        -particle_speed_limit,
                        particle_speed_limit
                    );
                }
            }

            const float inherit = Clamp01(chain.tether_distance_compression) * (0.55f + stiffness * 0.25f);
            const float center_offset_scale = 0.16f * (1.0f - depth_ratio * 0.35f);
            const float inertia_scale = 0.014f * (1.0f - depth_ratio * 0.45f);

            const float target_pitch =
                parent_state.pitch * inherit
                + center_offset.y * center_offset_scale
                + center_velocity.y * inertia_scale;
            const float target_roll =
                parent_state.roll * inherit
                + center_offset.x * center_offset_scale
                + center_velocity.x * inertia_scale;

            const float spring_gain =
                (0.45f + stiffness * 0.55f)
                * std::max(0.0f, chain.distance_stiffness)
                * (0.85f + scene.time_state.simulation_power[1] * 0.35f);
            const float damping_gain = chain.angle_restoration_enabled
                ? (0.25f + Clamp01(chain.angle_restoration_stiffness) * 0.65f)
                : 0.0f;
            const float drag_factor = std::max(
                0.0f,
                1.0f
                - (
                    (drag * 0.5f)
                    + (Clamp01(chain.angle_restoration_velocity_attenuation) * 0.25f)
                ) * scene.time_state.simulation_delta_time * 18.0f
            );

            state.pitch_velocity = Clamp(
                (state.pitch_velocity + (target_pitch - state.pitch) * spring_gain) * drag_factor,
                -particle_speed_limit,
                particle_speed_limit
            );
            state.roll_velocity = Clamp(
                (state.roll_velocity + (target_roll - state.roll) * spring_gain) * drag_factor,
                -particle_speed_limit,
                particle_speed_limit
            );

            state.pitch = Clamp(
                state.pitch + state.pitch_velocity * scene.time_state.simulation_delta_time - state.pitch * damping_gain * 0.02f,
                -1.2f,
                1.2f
            );
            state.roll = Clamp(
                state.roll + state.roll_velocity * scene.time_state.simulation_delta_time - state.roll * damping_gain * 0.02f,
                -1.2f,
                1.2f
            );

            ApplyMc2SpringConstraint(scene, chain, runtime_input, joint, static_cast<std::size_t>(joint_index), state);
        }
    }

    for (const CompiledSpringLine& line : cache.line_indices) {
        if (line.start_index < 0 || line.end_index < 0) {
            continue;
        }

        const JointState& parent = cache.joint_states[static_cast<std::size_t>(line.start_index)];
        JointState& child = cache.joint_states[static_cast<std::size_t>(line.end_index)];
        const float relax = 0.08f + kMc2BoneSpringDistanceStiffness * 0.2f;
        child.pitch = Clamp(child.pitch * (1.0f - relax) + parent.pitch * relax, -1.2f, 1.2f);
        child.roll = Clamp(child.roll * (1.0f - relax) + parent.roll * relax, -1.2f, 1.2f);
    }

    if (runtime_input != nullptr) {
        if (runtime_input->basic_rotations.size() == cache.last_basic_rotations.size()) {
            cache.last_basic_rotations = runtime_input->basic_rotations;
        }
        cache.last_root_rotation = runtime_input->root_rotation_quaternion;
        cache.last_center_rotation = runtime_input->center_rotation_quaternion;
        cache.last_center_translation = runtime_input->center_translation;
        cache.has_runtime_rotation = true;
    }
}

}  // namespace

BuildSceneResult RuntimeModule::BuildScene(CompiledScene compiled_scene)
{
    const SceneHandle handle = next_handle_++;

    auto scene = std::make_unique<SceneState>();
    scene->compiled_scene = std::move(compiled_scene);
    scene->time_state.simulation_frequency = kMc2DefaultSimulationFrequency;
    scene->time_state.max_simulation_count_per_frame = kMc2DefaultMaxSimulationCountPerFrame;
    scene->time_state.FrameUpdate(scene->time_state.simulation_frequency);
    BuildSceneCaches(*scene);
    RebuildMc2ColliderManagers(*scene);
    UpdateMc2ColliderWorkData(*scene);

    BuildSceneResult result;
    result.handle = handle;
    result.summary = scene->compiled_scene.Summary();
    result.backend = "native_mc2_particle_bridge";
    result.build_message = "HoCloth native MC2 particle/collider bridge active; BoneCloth chains use cloth gravity and non-spring teams.";
    result.backend_status = RuntimeBackendStatus(*scene);

    scenes_[handle] = std::move(scene);
    return result;
}

SceneHandle RuntimeModule::DestroyScene(SceneHandle handle)
{
    scenes_.erase(handle);
    return handle;
}

bool RuntimeModule::ResetScene(SceneHandle handle)
{
    SceneState& scene = RequireScene(handle);
    scene.runtime_inputs = RuntimeInputs{};
    scene.steps = 0;
    scene.time_state.simulation_frequency = kMc2DefaultSimulationFrequency;
    scene.time_state.max_simulation_count_per_frame = kMc2DefaultMaxSimulationCountPerFrame;
    scene.time_state.FrameUpdate(scene.time_state.simulation_frequency);
    BuildSceneCaches(scene);
    RebuildMc2ColliderManagers(scene);
    UpdateMc2ColliderWorkData(scene);
    return true;
}

bool RuntimeModule::SetRuntimeInputs(SceneHandle handle, RuntimeInputs runtime_inputs)
{
    SceneState& scene = RequireScene(handle);
    scene.runtime_inputs = std::move(runtime_inputs);
    ApplyCollisionObjectInputs(scene);
    UpdateMc2ColliderWorkData(scene);
    return true;
}

StepSceneResult RuntimeModule::StepScene(SceneHandle handle, float dt, int simulation_frequency)
{
    SceneState& scene = RequireScene(handle);
    scene.time_state.FrameUpdate(simulation_frequency);

    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        StepChain(scene, scene.compiled_scene.spring_bones[chain_index], scene.chain_caches[chain_index]);
        SyncJointStatesToMc2Particles(scene, chain_index);
    }
    UpdateMc2ColliderWorkData(scene);
    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        ApplyMc2ParticlesToJointStates(scene, chain_index);
    }

    scene.steps += 1;

    StepSceneResult result;
    result.handle = handle;
    result.dt = dt;
    result.simulation_frequency = scene.time_state.simulation_frequency;
    result.executed_steps = 1;
    result.steps = scene.steps;
    result.summary = scene.compiled_scene.Summary();
    return result;
}

std::vector<BoneTransform> RuntimeModule::GetBoneTransforms(SceneHandle handle) const
{
    const SceneState& scene = RequireScene(handle);
    std::vector<BoneTransform> transforms;

    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        const CompiledSpringBone& chain = scene.compiled_scene.spring_bones[chain_index];
        const ChainCache& cache = scene.chain_caches[chain_index];
        for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
            const CompiledSpringJoint& joint = chain.joints[joint_index];
            if (joint.parent_index < 0 || EndsWith(joint.name, kTailTipSuffix)) {
                continue;
            }

            const JointState& state = cache.joint_states[joint_index];
            transforms.push_back(BoneTransform{
                chain.component_id,
                chain.armature_name,
                joint.name,
                Vec3{0.0f, 0.0f, 0.0f},
                QuaternionFromPitchRoll(state.pitch, state.roll),
            });
        }
    }

    return transforms;
}

std::vector<MeshOutput> RuntimeModule::GetMeshOutputs(SceneHandle handle) const
{
    const SceneState& scene = RequireScene(handle);
    std::vector<MeshOutput> outputs;
    outputs.reserve(scene.compiled_scene.mesh_writeback_targets.size());

    for (const CompiledMeshWritebackTarget& target : scene.compiled_scene.mesh_writeback_targets) {
        if (target.source_object_name.empty() || target.vertex_count <= 0) {
            continue;
        }

        MeshOutput output;
        output.component_id = target.component_id;
        output.object_name = target.source_object_name;
        output.source_object_name = target.source_object_name;
        output.space = target.space.empty() ? "object_local" : target.space;
        if (static_cast<int>(output.positions.size()) != target.vertex_count) {
            continue;
        }
        outputs.push_back(std::move(output));
    }

    return outputs;
}

RuntimeModule::SceneState& RuntimeModule::RequireScene(SceneHandle handle)
{
    auto it = scenes_.find(handle);
    if (it == scenes_.end()) {
        throw std::runtime_error("Unknown runtime handle.");
    }
    return *it->second;
}

const RuntimeModule::SceneState& RuntimeModule::RequireScene(SceneHandle handle) const
{
    auto it = scenes_.find(handle);
    if (it == scenes_.end()) {
        throw std::runtime_error("Unknown runtime handle.");
    }
    return *it->second;
}

RuntimeModule& GetRuntimeModule()
{
    static RuntimeModule module;
    return module;
}

}  // namespace hocloth
