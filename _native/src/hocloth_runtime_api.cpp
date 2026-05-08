#include "hocloth_runtime_api.hpp"
#include "hocloth/cloth/cloth_serialize_data.hpp"
#include "hocloth/cloth/constraints/distance_constraint.hpp"
#include "hocloth/cloth/constraints/inertia_constraint.hpp"
#include "hocloth/cloth/constraints/triangle_bending_constraint.hpp"
#include "hocloth/manager/cloth/cloth_manager.hpp"
#include "hocloth/manager/transform/transform_record.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/simulation/wind_manager.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
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

mc2::RenderSetupData::BoneConnectionMode ToBoneConnectionMode(const std::string& mode)
{
    if (mode == "AutomaticMesh") {
        return mc2::RenderSetupData::BoneConnectionMode::AutomaticMesh;
    }
    if (mode == "SequentialLoopMesh") {
        return mc2::RenderSetupData::BoneConnectionMode::SequentialLoopMesh;
    }
    if (mode == "SequentialNonLoopMesh") {
        return mc2::RenderSetupData::BoneConnectionMode::SequentialNonLoopMesh;
    }
    return mc2::RenderSetupData::BoneConnectionMode::Line;
}

mc2::VertexAttribute ToVertexAttribute(const std::string& attribute)
{
    if (attribute == "MOVE") {
        return mc2::VertexAttribute::Move();
    }
    if (attribute == "FIXED") {
        return mc2::VertexAttribute::Fixed();
    }
    if (attribute == "INVALID") {
        return mc2::VertexAttribute::Invalid();
    }
    return mc2::VertexAttribute::Invalid();
}

float Clamp(float value, float low, float high)
{
    return std::clamp(value, low, high);
}

float Clamp01(float value)
{
    return Clamp(value, 0.0f, 1.0f);
}

float EvaluateCurve(const CompiledCurve& curve, float fallback_value, float depth_ratio, bool clamp01 = false)
{
    const float value = curve.has_value ? curve.value : fallback_value;
    float sample = value;
    if (curve.use_curve && curve.samples.size() == 16) {
        const float scaled_time = Clamp01(depth_ratio) * 15.0f;
        const int index0 = static_cast<int>(std::floor(scaled_time));
        const int index1 = std::min(index0 + 1, 15);
        const float t = scaled_time - static_cast<float>(index0);
        const float v0 = curve.samples[static_cast<std::size_t>(index0)];
        const float v1 = curve.samples[static_cast<std::size_t>(index1)];
        sample = value * (v0 + (v1 - v0) * t);
    }
    return clamp01 ? Clamp01(sample) : sample;
}

mc2::float4x4 ToCurveMatrix(const CompiledCurve& curve, float fallback_value, bool clamp01 = false)
{
    const float value = curve.has_value ? curve.value : fallback_value;
    if (!curve.use_curve || curve.samples.size() != 16) {
        return mc2::ConstantCurve(value);
    }

    mc2::float4x4 result = mc2::ConstantCurve(0.0f);
    for (int index = 0; index < 16; ++index) {
        float sample = curve.samples[static_cast<std::size_t>(index)] * value;
        if (clamp01) {
            sample = Clamp01(sample);
        }
        mc2::MC2SetValue(result, index, sample);
    }
    return result;
}

mc2::float4x4 ScaleCurveMatrix(mc2::float4x4 curve, float scale, bool clamp01 = false)
{
    for (int index = 0; index < 16; ++index) {
        float value = mc2::MC2GetValue(curve, index) * scale;
        if (clamp01) {
            value = Clamp01(value);
        }
        mc2::MC2SetValue(curve, index, value);
    }
    return curve;
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

Quat Mc2ToQuat(const mc2::quaternion& value)
{
    return NormalizeQuat(Quat{value.w, value.x, value.y, value.z});
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

mc2::float3 AverageScaleToMc2(const Vec3& scale)
{
    const float uniform_scale = std::max(0.0001f, AverageAbsScale(scale));
    return mc2::float3{uniform_scale, uniform_scale, uniform_scale};
}

mc2::float3 ScaleToMc2(const Vec3& scale)
{
    return mc2::float3{
        std::abs(scale.x) > 0.0001f ? scale.x : 0.0001f,
        std::abs(scale.y) > 0.0001f ? scale.y : 0.0001f,
        std::abs(scale.z) > 0.0001f ? scale.z : 0.0001f,
    };
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
    const float dot = std::abs(QuaternionDot(NormalizeQuat(a), NormalizeQuat(b)));
    if (std::abs(dot) < 0.9999f) {
        const float angle = std::acos(Clamp(dot, -1.0f, 1.0f)) * 2.0f;
        return angle > kPi ? (kPi * 2.0f) - angle : angle;
    }
    return 0.0f;
}

float Distance(const Vec3& a, const Vec3& b)
{
    return Length(Sub(a, b));
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

mc2::float4x4 ToMc2Float4x4(const Mat4& value)
{
    // Blender/Python Matrix is flattened row-major; MC2/math stores float4x4
    // as columns matching Unity.Mathematics float4x4 layout.
    return mc2::float4x4{
        mc2::float4{value.m00, value.m10, value.m20, value.m30},
        mc2::float4{value.m01, value.m11, value.m21, value.m31},
        mc2::float4{value.m02, value.m12, value.m22, value.m32},
        mc2::float4{value.m03, value.m13, value.m23, value.m33},
    };
}

mc2::float3 LossyScaleFromLocalToWorld(
    const mc2::float4x4& local_to_world,
    const mc2::quaternion& rotation
)
{
    const mc2::float4x4 inverse_rotation_matrix =
        mc2::TRS(mc2::float3{}, mc2::Inverse(rotation), mc2::float3{1.0f, 1.0f, 1.0f});
    const mc2::float4x4 matrix = mc2::Multiply(inverse_rotation_matrix, local_to_world);
    return mc2::float3{matrix.c0.x, matrix.c1.y, matrix.c2.z};
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

Quat ResolveRuntimeLocalRotation(
    const CompiledSpringBone& chain,
    const RuntimeChainInput* runtime_input,
    std::size_t joint_index
)
{
    if (joint_index >= chain.joints.size()) {
        return Quat{};
    }

    const CompiledSpringJoint& joint = chain.joints[joint_index];
    if (runtime_input == nullptr) {
        return joint.rest_local_rotation;
    }

    if (joint_index < runtime_input->basic_local_rotations.size()) {
        return NormalizeQuat(runtime_input->basic_local_rotations[joint_index]);
    }
    if (joint_index >= runtime_input->basic_rotations.size()) {
        return joint.rest_local_rotation;
    }

    const Quat world_rotation = runtime_input->basic_rotations[joint_index];
    if (joint.parent_index < 0
        || static_cast<std::size_t>(joint.parent_index) >= runtime_input->basic_rotations.size()) {
        return NormalizeQuat(world_rotation);
    }

    const Quat parent_world_rotation =
        runtime_input->basic_rotations[static_cast<std::size_t>(joint.parent_index)];
    return NormalizeQuat(Mul(Conjugate(parent_world_rotation), world_rotation));
}

Vec3 ResolveRuntimeLocalPosition(
    const CompiledSpringBone& chain,
    const RuntimeChainInput* runtime_input,
    std::size_t joint_index
)
{
    if (joint_index >= chain.joints.size()) {
        return Vec3{};
    }

    const CompiledSpringJoint& joint = chain.joints[joint_index];
    if (runtime_input != nullptr
        && joint_index < runtime_input->basic_local_positions.size()) {
        return runtime_input->basic_local_positions[joint_index];
    }

    if (joint.parent_index < 0) {
        return joint.rest_local_translation;
    }

    if (runtime_input == nullptr
        || joint_index >= runtime_input->basic_head_positions.size()
        || static_cast<std::size_t>(joint.parent_index) >= runtime_input->basic_head_positions.size()
        || static_cast<std::size_t>(joint.parent_index) >= runtime_input->basic_rotations.size()) {
        return joint.rest_local_translation;
    }

    const Vec3 world_delta = Sub(
        runtime_input->basic_head_positions[joint_index],
        runtime_input->basic_head_positions[static_cast<std::size_t>(joint.parent_index)]
    );
    const Quat parent_world_rotation =
        runtime_input->basic_rotations[static_cast<std::size_t>(joint.parent_index)];
    return RotateVector(Conjugate(parent_world_rotation), world_delta);
}

Vec3 ResolveRuntimeWorldPosition(
    const CompiledSpringBone& chain,
    const RuntimeChainInput* runtime_input,
    std::size_t joint_index
)
{
    if (runtime_input != nullptr
        && joint_index < runtime_input->basic_head_positions.size()) {
        return runtime_input->basic_head_positions[joint_index];
    }
    if (joint_index < chain.joints.size()) {
        return chain.joints[joint_index].rest_head_local;
    }
    return Vec3{};
}

Quat ResolveRuntimeWorldRotation(
    const CompiledSpringBone& chain,
    const RuntimeChainInput* runtime_input,
    std::size_t joint_index
)
{
    if (runtime_input != nullptr
        && joint_index < runtime_input->basic_rotations.size()) {
        return NormalizeQuat(runtime_input->basic_rotations[joint_index]);
    }
    if (joint_index < chain.joints.size()) {
        const CompiledSpringJoint& joint = chain.joints[joint_index];
        return NormalizeQuat(
            joint.has_rest_world_rotation
                ? joint.rest_world_rotation
                : joint.rest_local_rotation
        );
    }
    return Quat{};
}

BoneTransform MakeBoneTransformDiagnostic(
    const CompiledSpringBone& chain,
    const RuntimeChainInput* runtime_input,
    std::size_t joint_index,
    const Vec3& translation,
    const Quat& rotation,
    const Quat& world_rotation,
    const std::string& write_mode,
    int transform_flags,
    int vertex_attribute,
    const Vec3& output_world_position,
    const Vec3& output_local_position,
    const Quat& proxy_vertex_rotation = Quat{},
    const Quat& vertex_to_transform_rotation = Quat{},
    const Vec3& proxy_local_position = Vec3{},
    const Vec3& proxy_local_normal = Vec3{},
    const Vec3& proxy_local_tangent = Vec3{},
    const Vec3& proxy_posed_position = Vec3{},
    const Vec3& proxy_posed_normal = Vec3{},
    const Vec3& proxy_posed_tangent = Vec3{},
    const Vec3& proxy_world_normal = Vec3{},
    const Vec3& proxy_world_tangent = Vec3{}
)
{
    BoneTransform transform;
    if (joint_index >= chain.joints.size()) {
        return transform;
    }

    const CompiledSpringJoint& joint = chain.joints[joint_index];
    transform.component_id = chain.component_id;
    transform.armature_name = chain.armature_name;
    transform.bone_name = joint.name;
    transform.joint_index = static_cast<int>(joint_index);
    transform.parent_index = joint.parent_index;
    if (joint.parent_index >= 0
        && static_cast<std::size_t>(joint.parent_index) < chain.joints.size()) {
        transform.parent_bone_name =
            chain.joints[static_cast<std::size_t>(joint.parent_index)].name;
        transform.parent_world_rotation_quaternion = ResolveRuntimeWorldRotation(
            chain,
            runtime_input,
            static_cast<std::size_t>(joint.parent_index)
        );
    }
    transform.translation = translation;
    transform.rotation_quaternion = NormalizeQuat(rotation);
    transform.world_rotation_quaternion = NormalizeQuat(world_rotation);
    transform.input_world_position = ResolveRuntimeWorldPosition(chain, runtime_input, joint_index);
    transform.output_world_position = output_world_position;
    transform.input_local_position = ResolveRuntimeLocalPosition(chain, runtime_input, joint_index);
    transform.output_local_position = output_local_position;
    transform.write_mode = write_mode;
    transform.transform_flags = transform_flags;
    transform.vertex_attribute = vertex_attribute;
    transform.input_local_rotation = ResolveRuntimeLocalRotation(chain, runtime_input, joint_index);
    transform.input_world_rotation = ResolveRuntimeWorldRotation(chain, runtime_input, joint_index);
    transform.output_local_rotation = NormalizeQuat(rotation);
    transform.proxy_vertex_rotation = NormalizeQuat(proxy_vertex_rotation);
    transform.vertex_to_transform_rotation = NormalizeQuat(vertex_to_transform_rotation);
    transform.proxy_local_position = proxy_local_position;
    transform.proxy_local_normal = proxy_local_normal;
    transform.proxy_local_tangent = proxy_local_tangent;
    transform.proxy_posed_position = proxy_posed_position;
    transform.proxy_posed_normal = proxy_posed_normal;
    transform.proxy_posed_tangent = proxy_posed_tangent;
    transform.proxy_world_normal = proxy_world_normal;
    transform.proxy_world_tangent = proxy_world_tangent;
    transform.has_rest_local_to_world_matrix = joint.has_rest_local_to_world_matrix;
    transform.proxy_to_input_world_delta_degrees =
        QuaternionAngle(transform.input_world_rotation, transform.proxy_vertex_rotation)
        * 57.29577951308232f;
    transform.local_rotation_delta_degrees =
        QuaternionAngle(transform.input_local_rotation, transform.output_local_rotation)
        * 57.29577951308232f;
    transform.world_rotation_delta_degrees =
        QuaternionAngle(transform.input_world_rotation, transform.world_rotation_quaternion)
        * 57.29577951308232f;
    transform.local_position_delta =
        Distance(transform.input_local_position, transform.output_local_position);
    transform.world_position_delta =
        Distance(transform.input_world_position, transform.output_world_position);
    return transform;
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
        ? ToBoneConnectionMode(chain.bone_connection_mode)
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
    setup.transform_local_to_world_matrices.reserve(static_cast<std::size_t>(joint_count + 1));
    setup.transform_child_ids.resize(static_cast<std::size_t>(joint_count + 1));
    setup.collision_bone_indices = chain.collision_bone_indices;

    std::vector<Quat> world_rotations(static_cast<std::size_t>(joint_count), Quat{});
    for (int joint_index = 0; joint_index < joint_count; ++joint_index) {
        const CompiledSpringJoint& joint = chain.joints[static_cast<std::size_t>(joint_index)];
        Quat world_rotation = joint.has_rest_world_rotation
            ? joint.rest_world_rotation
            : joint.rest_local_rotation;
        if (!joint.has_rest_world_rotation
            && joint.parent_index >= 0
            && joint.parent_index < joint_count) {
            world_rotation = Mul(
                world_rotations[static_cast<std::size_t>(joint.parent_index)],
                joint.rest_local_rotation
            );
        }
        world_rotations[static_cast<std::size_t>(joint_index)] = NormalizeQuat(world_rotation);
    }

    for (int joint_index = 0; joint_index < joint_count; ++joint_index) {
        const CompiledSpringJoint& joint = chain.joints[static_cast<std::size_t>(joint_index)];
        const bool is_root = joint.parent_index < 0;
        const int transform_id = joint_index + 1;
        const int parent_transform_id = joint.parent_index >= 0 ? joint.parent_index + 1 : 0;
        const mc2::float3 position = ToMc2Float3(joint.rest_head_local);
        const mc2::quaternion local_rotation = ToMc2Quaternion(joint.rest_local_rotation);
        const mc2::quaternion world_rotation =
            ToMc2Quaternion(world_rotations[static_cast<std::size_t>(joint_index)]);

        setup.transform_ids.push_back(transform_id);
        setup.transform_parent_ids.push_back(parent_transform_id);
        setup.transform_names.push_back(joint.name);
        setup.transform_positions.push_back(position);
        setup.transform_rotations.push_back(world_rotation);
        setup.transform_scales.push_back(ScaleToMc2(joint.rest_world_scale));
        setup.transform_local_positions.push_back(ToMc2Float3(joint.rest_local_translation));
        setup.transform_local_rotations.push_back(local_rotation);
        setup.transform_inverse_rotations.push_back(mc2::Inverse(world_rotation));
        setup.transform_local_to_world_matrices.push_back(
            joint.has_rest_local_to_world_matrix
                ? ToMc2Float4x4(joint.rest_local_to_world_matrix)
                : mc2::TRS(position, world_rotation, ScaleToMc2(joint.rest_world_scale))
        );
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
    const mc2::float3 render_position = ToMc2Float3(chain.armature_position);
    const mc2::quaternion render_rotation = ToMc2Quaternion(chain.armature_rotation);
    const mc2::float3 render_scale = ScaleToMc2(chain.armature_scale);
    setup.transform_positions.push_back(render_position);
    setup.transform_rotations.push_back(render_rotation);
    setup.transform_scales.push_back(render_scale);
    setup.transform_local_positions.push_back(render_position);
    setup.transform_local_rotations.push_back(render_rotation);
    setup.transform_inverse_rotations.push_back(mc2::Inverse(render_rotation));
    setup.transform_local_to_world_matrices.push_back(
        mc2::TRS(render_position, render_rotation, render_scale)
    );
    if (!chain.root_bone_names.empty()) {
        std::vector<int> explicit_root_transform_ids;
        explicit_root_transform_ids.reserve(chain.root_bone_names.size());
        for (const std::string& root_bone_name : chain.root_bone_names) {
            auto found = std::find_if(
                chain.joints.begin(),
                chain.joints.end(),
                [&root_bone_name](const CompiledSpringJoint& joint) {
                    return joint.name == root_bone_name;
                }
            );
            if (found == chain.joints.end()) {
                continue;
            }
            const int joint_index = static_cast<int>(std::distance(chain.joints.begin(), found));
            const int transform_id = joint_index + 1;
            if (std::find(
                    explicit_root_transform_ids.begin(),
                    explicit_root_transform_ids.end(),
                    transform_id
                ) == explicit_root_transform_ids.end()) {
                explicit_root_transform_ids.push_back(transform_id);
            }
        }
        if (!explicit_root_transform_ids.empty()) {
            setup.root_transform_ids = std::move(explicit_root_transform_ids);
        }
    }
    if (setup.root_transform_ids.empty() && joint_count > 0) {
        setup.root_transform_ids.push_back(1);
    }

    setup.init_render_local_to_world = mc2::TRS(render_position, render_rotation, render_scale);
    setup.init_render_world_to_local = mc2::InverseAffine(setup.init_render_local_to_world);
    setup.init_render_rotation = render_rotation;
    setup.init_render_scale = render_scale;
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
    mc2::ClothManager mc2_cloth_manager;
    mc2::WindManager mc2_wind_manager;
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

BuildSceneOutput MakeBuildSceneOutput(const RuntimeModule::SceneState& scene)
{
    BuildSceneOutput output;

    for (const CompiledSpringBone& chain : scene.compiled_scene.spring_bones) {
        for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
            const CompiledSpringJoint& joint = chain.joints[joint_index];
            output.particles.push_back(BuildDrawParticle{
                chain.component_id,
                joint.name,
                static_cast<int>(joint_index),
                joint.parent_index,
                joint.rest_head_local,
                joint.rest_tail_local,
                joint.radius,
            });
        }

        const std::vector<CompiledSpringLine> lines = BuildLineIndices(chain);
        for (const CompiledSpringLine& line : lines) {
            output.lines.push_back(BuildDrawLine{
                chain.component_id,
                line.start_index,
                line.end_index,
            });
        }

        for (const CompiledSpringBaseline& baseline : chain.baselines) {
            output.baselines.push_back(baseline);
        }
    }

    for (const CompiledCollisionObject& collider : scene.compiled_scene.collision_objects) {
        output.colliders.push_back(BuildDrawCollider{
            collider.collision_object_id,
            collider.owner_component_id,
            collider.shape_type,
            collider.world_translation,
            collider.world_rotation,
            collider.radius,
            collider.height,
            collider.capsule_direction,
            collider.capsule_aligned_on_center,
            collider.capsule_reverse_direction,
            collider.capsule_end_radius,
            collider.source_object_name,
        });
    }

    return output;
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
        if (IsBoneCloth(chain) && !chain.bone_attribute_overrides.empty()) {
            mc2::SelectionData selection = mc2::SelectionData::CreateBoneClothDefault(render_setup);
            for (const CompiledBoneAttributeOverride& override : chain.bone_attribute_overrides) {
                auto found = std::find_if(
                    chain.joints.begin(),
                    chain.joints.end(),
                    [&override](const CompiledSpringJoint& joint) {
                        return joint.name == override.bone_name;
                    }
                );
                if (found == chain.joints.end()) {
                    continue;
                }
                const int joint_index = static_cast<int>(std::distance(chain.joints.begin(), found));
                if (joint_index < 0 || joint_index >= selection.Count()) {
                    continue;
                }
                selection.attributes[static_cast<std::size_t>(joint_index)] =
                    ToVertexAttribute(override.attribute);
            }
            mesh->ApplySelectionAttribute(selection, true);
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
    scene.mc2_cloth_manager.Dispose();
    scene.mc2_wind_manager.Dispose();
    scene.mc2_collider_manager.Dispose();
    scene.mc2_virtual_mesh_manager.Dispose();
    scene.mc2_simulation_manager.Dispose();
    scene.mc2_transform_manager.Dispose();
    scene.mc2_team_manager.Dispose();
    scene.mc2_cloth_manager.Initialize();
    scene.mc2_wind_manager.Initialize();
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
        const bool is_bone_spring = IsBoneSpring(chain);
        parameters.gravity = IsBoneCloth(chain) ? std::max(0.0f, chain.gravity_strength) : 0.0f;
        parameters.world_gravity_direction = ToMc2Float3(Normalize(chain.gravity_direction));
        parameters.gravity_falloff = Clamp01(chain.gravity_falloff);
        parameters.stabilization_time_after_reset = Clamp01(chain.stabilization_time_after_reset);
        parameters.blend_weight = Clamp01(chain.blend_weight);
        parameters.radius_curve_data = ToCurveMatrix(chain.radius_curve, std::max(0.0001f, chain.joint_radius));
        parameters.damping_curve_data = ScaleCurveMatrix(
            ToCurveMatrix(chain.damping_curve, Clamp01(chain.damping), true),
            0.2f,
            true
        );
        parameters.distance_constraint.restoration_stiffness =
            is_bone_spring
                ? mc2::ConstantCurve(kMc2BoneSpringDistanceStiffness)
                : ToCurveMatrix(chain.distance_stiffness_curve, Clamp01(chain.distance_stiffness), true);
        parameters.tether_constraint.compression_limit = is_bone_spring
            ? kMc2BoneSpringTetherCompressionLimit
            : Clamp01(chain.tether_distance_compression);
        parameters.triangle_bending_constraint.stiffness = Clamp01(chain.triangle_bending_stiffness);
        parameters.spring_constraint.spring_power =
            is_bone_spring && chain.use_spring ? Clamp(chain.spring_power, 0.001f, 1.0f) : 0.0f;
        parameters.spring_constraint.limit_distance = std::max(0.0f, chain.limit_distance);
        parameters.spring_constraint.normal_limit_ratio = Clamp01(chain.normal_limit_ratio);
        parameters.spring_constraint.spring_noise = Clamp01(chain.spring_noise);
        parameters.angle_constraint.use_angle_restoration = chain.angle_restoration_enabled ? 1 : 0;
        parameters.angle_constraint.restoration_stiffness = ScaleCurveMatrix(
            ToCurveMatrix(chain.angle_restoration_stiffness_curve, Clamp01(chain.angle_restoration_stiffness), true),
            0.2f,
            true
        );
        parameters.angle_constraint.restoration_velocity_attenuation =
            Clamp01(chain.angle_restoration_velocity_attenuation);
        parameters.angle_constraint.restoration_gravity_falloff =
            Clamp01(chain.angle_restoration_gravity_falloff);
        parameters.angle_constraint.use_angle_limit = chain.angle_limit_enabled ? 1 : 0;
        parameters.angle_constraint.limit_curve_data =
            ToCurveMatrix(chain.angle_limit_angle_curve, Clamp(chain.angle_limit_angle, 0.0f, 180.0f));
        parameters.angle_constraint.limit_stiffness = Clamp01(chain.angle_limit_stiffness);
        parameters.inertia_constraint.world_inertia = Clamp01(chain.inertia_world_inertia);
        parameters.inertia_constraint.movement_inertia_smoothing =
            Clamp01(chain.inertia_movement_inertia_smoothing);
        parameters.inertia_constraint.local_inertia = Clamp01(chain.inertia_local_inertia);
        parameters.inertia_constraint.local_movement_speed_limit =
            chain.inertia_local_movement_speed_limit_enabled
                ? std::max(0.0f, chain.inertia_local_movement_speed_limit)
                : -1.0f;
        parameters.inertia_constraint.local_rotation_speed_limit =
            chain.inertia_local_rotation_speed_limit_enabled
                ? std::max(0.0f, chain.inertia_local_rotation_speed_limit)
                : -1.0f;
        parameters.inertia_constraint.depth_inertia = Clamp01(chain.inertia_depth_inertia);
        parameters.inertia_constraint.centrifugal_acceleration =
            std::max(0.0f, chain.inertia_centrifugal_acceleration);
        parameters.inertia_constraint.particle_speed_limit =
            chain.inertia_particle_speed_limit_enabled
                ? std::max(0.0f, chain.inertia_particle_speed_limit)
                : -1.0f;
        if (!chain.collider_collision_enabled || chain.collider_ids.empty()) {
            parameters.collider_collision_constraint.mode = mc2::ColliderCollisionMode::None;
        } else if (is_bone_spring) {
            parameters.collider_collision_constraint.mode = mc2::ColliderCollisionMode::Point;
        } else if (chain.collider_collision_mode == "Edge") {
            parameters.collider_collision_constraint.mode = mc2::ColliderCollisionMode::Edge;
        } else {
            parameters.collider_collision_constraint.mode = mc2::ColliderCollisionMode::Point;
        }
        const float collider_friction = is_bone_spring
            ? kMc2BoneSpringCollisionFriction
            : Clamp(chain.collider_friction, 0.0f, 1.0f);
        parameters.collider_collision_constraint.dynamic_friction = collider_friction;
        parameters.collider_collision_constraint.static_friction = collider_friction;
        parameters.collider_collision_constraint.limit_distance =
            ToCurveMatrix(chain.collider_limit_distance_curve, std::max(0.0001f, chain.collider_limit_distance));

        const int team_id = scene.mc2_team_manager.CreateTeam(parameters, true, is_bone_spring);
        auto proxy_mesh = BuildRuntimeProxyMesh(chain);
        mc2::VirtualMeshContainer proxy_container(proxy_mesh);
        scene.mc2_virtual_mesh_manager.RegisterProxyMesh(
            team_id,
            proxy_container,
            scene.mc2_team_manager,
            scene.mc2_transform_manager
        );
        scene.mc2_cloth_manager.Distance().Register(
            team_id,
            mc2::DistanceConstraint::CreateData(*proxy_mesh, parameters),
            scene.mc2_team_manager
        );
        scene.mc2_cloth_manager.TriangleBending().Register(
            team_id,
            mc2::TriangleBendingConstraint::CreateData(*proxy_mesh, parameters),
            scene.mc2_team_manager
        );
        scene.mc2_cloth_manager.Inertia().Register(
            team_id,
            mc2::InertiaConstraint::CreateData(*proxy_mesh, parameters),
            scene.mc2_team_manager
        );

        mc2::SimulationManager::ParticleChunkSet particle_chunks =
            scene.mc2_simulation_manager.RegisterParticleRange(
                team_id,
                proxy_mesh->VertexCount()
            );
        mc2::TeamManager::TeamData& team_data = scene.mc2_team_manager.GetTeamData(team_id);
        team_data.particle_chunk = particle_chunks.team_id_chunk;
        team_data.proxy_mesh_type = proxy_mesh->mesh_type;
        team_data.init_scale = proxy_mesh->init_scale;
        scene.mc2_team_manager.SetUpdateMode(team_id, mc2::ClothUpdateMode::Normal);
        scene.mc2_team_manager.SetAnimationPoseRatio(team_id, is_bone_spring ? 1.0f : 0.0f);
        scene.mc2_team_manager.SetTimeScale(team_id, 1.0f);
        scene.mc2_team_manager.SetSkipWriting(team_id, false);

        for (std::size_t joint_index = 0; joint_index < chain.joints.size()
             && joint_index < static_cast<std::size_t>(proxy_mesh->VertexCount());
             ++joint_index) {
            const int particle_index = particle_chunks.next_pos_chunk.start_index + static_cast<int>(joint_index);
            if (particle_index < 0 || particle_index >= scene.mc2_simulation_manager.NextPositions().Length()) {
                continue;
            }

            const CompiledSpringJoint& joint = chain.joints[joint_index];
            const mc2::float3 position = ToMc2Float3(joint.rest_head_local);
            const mc2::quaternion rotation = ToMc2Quaternion(
                joint.has_rest_world_rotation
                    ? joint.rest_world_rotation
                    : joint.rest_local_rotation
            );
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
            const mc2::DataChunk collider_chunk = scene.mc2_collider_manager.RegisterColliderDataRange(
                team_id,
                scene.mc2_team_manager,
                scene.mc2_transform_manager,
                colliders
            );
            (void)collider_chunk;
        }
        scene.mc2_cloth_manager.SelfCollision().Register(
            team_id,
            scene.mc2_team_manager,
            scene.mc2_virtual_mesh_manager
        );

        scene.mc2_chain_team_ids.push_back(team_id);
        scene.mc2_chain_collider_indices.push_back(std::move(collider_indices));
        scene.mc2_chain_particle_chunks.push_back(particle_chunks);
        scene.mc2_chain_proxy_meshes.push_back(std::move(proxy_mesh));
    }
}

void ApplyRuntimeInputsToMc2Transforms(RuntimeModule::SceneState& scene)
{
    scene.mc2_transform_manager.RestoreTransform(scene.mc2_team_manager);

    mc2::TransformData& transform_data = scene.mc2_transform_manager.Data();
    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        if (chain_index >= scene.mc2_chain_team_ids.size()) {
            continue;
        }

        const int team_id = scene.mc2_chain_team_ids[chain_index];
        if (!scene.mc2_team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const CompiledSpringBone& chain = scene.compiled_scene.spring_bones[chain_index];
        const RuntimeChainInput* runtime_input =
            FindRuntimeChainInput(scene.runtime_inputs, chain.component_id);
        mc2::TeamManager::TeamData& team_data =
            scene.mc2_team_manager.GetTeamData(team_id);
        const mc2::DataChunk transform_chunk = team_data.proxy_transform_chunk;
        if (!transform_chunk.IsValid()) {
            continue;
        }

        const int joint_count = static_cast<int>(chain.joints.size());
        for (int joint_index = 0; joint_index < joint_count; ++joint_index) {
            const int transform_index = transform_chunk.start_index + joint_index;
            if (transform_index < 0
                || transform_index >= transform_data.position_array.Length()
                || transform_index >= transform_data.rotation_array.Length()
                || transform_index >= transform_data.inverse_rotation_array.Length()
                || transform_index >= transform_data.scale_array.Length()
                || transform_index >= transform_data.local_position_array.Length()
                || transform_index >= transform_data.local_rotation_array.Length()
                || transform_index >= transform_data.local_to_world_matrix_array.Length()) {
                continue;
            }

            const CompiledSpringJoint& joint = chain.joints[static_cast<std::size_t>(joint_index)];
            mc2::float3 position = ToMc2Float3(joint.rest_head_local);
            mc2::quaternion rotation = ToMc2Quaternion(
                joint.has_rest_world_rotation
                    ? joint.rest_world_rotation
                    : joint.rest_local_rotation
            );
            if (runtime_input != nullptr
                && static_cast<std::size_t>(joint_index) < runtime_input->basic_head_positions.size()) {
                position = ToMc2Float3(runtime_input->basic_head_positions[static_cast<std::size_t>(joint_index)]);
            }
            if (runtime_input != nullptr
                && static_cast<std::size_t>(joint_index) < runtime_input->basic_rotations.size()) {
                rotation = ToMc2Quaternion(runtime_input->basic_rotations[static_cast<std::size_t>(joint_index)]);
            }

            const mc2::float4x4 local_to_world_matrix =
                runtime_input != nullptr
                    && static_cast<std::size_t>(joint_index)
                        < runtime_input->basic_local_to_world_matrices.size()
                    ? ToMc2Float4x4(
                        runtime_input
                            ->basic_local_to_world_matrices[static_cast<std::size_t>(joint_index)]
                    )
                    : mc2::TRS(position, rotation, ScaleToMc2(joint.rest_world_scale));
            const mc2::float3 scale =
                runtime_input != nullptr
                    && static_cast<std::size_t>(joint_index)
                        < runtime_input->basic_local_to_world_matrices.size()
                    ? LossyScaleFromLocalToWorld(local_to_world_matrix, rotation)
                    : ScaleToMc2(joint.rest_world_scale);
            transform_data.position_array[transform_index] = position;
            transform_data.rotation_array[transform_index] = rotation;
            transform_data.inverse_rotation_array[transform_index] = mc2::Inverse(rotation);
            transform_data.scale_array[transform_index] = scale;
            transform_data.local_position_array[transform_index] =
                ToMc2Float3(ResolveRuntimeLocalPosition(
                    chain,
                    runtime_input,
                    static_cast<std::size_t>(joint_index)
                ));
            transform_data.local_rotation_array[transform_index] =
                ToMc2Quaternion(ResolveRuntimeLocalRotation(
                    chain,
                    runtime_input,
                    static_cast<std::size_t>(joint_index)
                ));
            transform_data.local_to_world_matrix_array[transform_index] = local_to_world_matrix;
        }

        const int center_transform_index = team_data.center_transform_index;
        if (center_transform_index >= 0
            && center_transform_index < transform_data.position_array.Length()
            && center_transform_index < transform_data.rotation_array.Length()
            && center_transform_index < transform_data.inverse_rotation_array.Length()
            && center_transform_index < transform_data.scale_array.Length()
            && center_transform_index < transform_data.local_to_world_matrix_array.Length()) {
            const mc2::float3 position = runtime_input != nullptr
                ? ToMc2Float3(runtime_input->center_translation)
                : mc2::float3{};
            const mc2::quaternion rotation = runtime_input != nullptr
                ? ToMc2Quaternion(runtime_input->center_rotation_quaternion)
                : mc2::quaternion{};
            const mc2::float3 scale = runtime_input != nullptr
                ? AverageScaleToMc2(runtime_input->center_scale)
                : mc2::float3{1.0f, 1.0f, 1.0f};
            transform_data.position_array[center_transform_index] = position;
            transform_data.rotation_array[center_transform_index] = rotation;
            transform_data.inverse_rotation_array[center_transform_index] = mc2::Inverse(rotation);
            transform_data.scale_array[center_transform_index] = scale;
            transform_data.local_to_world_matrix_array[center_transform_index] =
                mc2::TRS(position, rotation, scale);
        }
    }
    scene.mc2_transform_manager.SetDirty(true);
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
    const bool is_bone_cloth = IsBoneCloth(chain);

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
        const Vec3 point = is_bone_cloth
            ? Vec3{}
            : ToAnglePoint(cache.joint_states[static_cast<std::size_t>(driven_joint_index)]);
        const float point_scale = std::max(0.05f, driven_joint.length);
        const mc2::float3 base_position = ResolveParticleBasePosition(chain, runtime_input, joint_index);
        Quat basis_rotation = driven_joint.has_rest_world_rotation
            ? driven_joint.rest_world_rotation
            : driven_joint.rest_local_rotation;
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
        const mc2::float3 animated_next_position{
            base_position.x + offset.x,
            base_position.y + offset.y,
            base_position.z + offset.z,
        };
        mc2::float3 next_position = animated_next_position;
        if (is_bone_cloth && scene.steps > 0) {
            next_position = scene.mc2_simulation_manager.OldPositions()[particle_index];
        }

        scene.mc2_simulation_manager.BasePositions()[particle_index] = base_position;
        scene.mc2_simulation_manager.BaseRotations()[particle_index] = ToMc2Quaternion(basis_rotation);
        scene.mc2_simulation_manager.StepBasicRotations()[particle_index] = ToMc2Quaternion(basis_rotation);
        scene.mc2_simulation_manager.NextPositions()[particle_index] = next_position;
        if (is_bone_cloth && scene.steps > 0) {
            scene.mc2_simulation_manager.VelocityPositions()[particle_index] =
                scene.mc2_simulation_manager.OldPositions()[particle_index];
        } else {
            scene.mc2_simulation_manager.VelocityPositions()[particle_index] = next_position;
        }
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
    const bool is_bone_cloth = IsBoneCloth(chain);
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
        Quat basis_rotation = driven_joint.has_rest_world_rotation
            ? driven_joint.rest_world_rotation
            : driven_joint.rest_local_rotation;
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
        if (is_bone_cloth) {
            point = ClampVectorLength(point, 0.8f);
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

            const float depth_ratio = static_cast<float>(path_index) / path_depth;
            const float stiffness = Clamp01(joint.stiffness > 0.0f
                ? joint.stiffness
                : EvaluateCurve(chain.distance_stiffness_curve, chain.stiffness, depth_ratio, true));
            const float damping = Clamp01(joint.damping > 0.0f
                ? joint.damping
                : EvaluateCurve(chain.damping_curve, chain.damping, depth_ratio, true)) * 0.2f;
            const float drag = Clamp01(joint.drag > 0.0f ? joint.drag : chain.drag);
            const float angle_restoration_stiffness =
                EvaluateCurve(
                    chain.angle_restoration_stiffness_curve,
                    chain.angle_restoration_stiffness,
                    depth_ratio,
                    true
                );
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

            const float tether_compression = is_bone_cloth
                ? Clamp01(chain.tether_distance_compression)
                : kMc2BoneSpringTetherCompressionLimit;
            const float distance_stiffness = is_bone_cloth
                ? stiffness
                : kMc2BoneSpringDistanceStiffness;
            const float inherit = tether_compression * (0.55f + stiffness * 0.25f);
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

            const float bone_spring_power_scale = is_bone_cloth
                ? 1.0f
                : (chain.use_spring ? Clamp01(chain.spring_power) : 0.0f);
            const float spring_gain = is_bone_cloth
                ? (
                    chain.angle_restoration_enabled
                        ? angle_restoration_stiffness
                            * (0.10f + Clamp01(chain.angle_restoration_velocity_attenuation) * 0.08f)
                            * (0.85f + scene.time_state.simulation_power[1] * 0.35f)
                        : 0.0f
                )
                : (
                    (0.45f + stiffness * 0.55f)
                    * distance_stiffness
                    * bone_spring_power_scale
                    * (0.85f + scene.time_state.simulation_power[1] * 0.35f)
                );
            const float damping_gain = chain.angle_restoration_enabled
                ? (
                    is_bone_cloth
                        ? angle_restoration_stiffness
                        : (0.25f + angle_restoration_stiffness * 0.65f)
                )
                : 0.0f;
            const float drag_factor = std::max(
                0.0f,
                1.0f
                - (
                    (drag * 0.5f)
                    + (damping * scene.time_state.simulation_power[2])
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
        const float relax = is_bone_cloth
            ? 0.0f
            : (0.08f + kMc2BoneSpringDistanceStiffness * 0.2f)
                * (chain.use_spring ? Clamp01(chain.spring_power) : 0.0f);
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

    BuildSceneResult result;
    result.handle = handle;
    result.summary = scene->compiled_scene.Summary();
    result.backend = "native_mc2_particle_bridge";
    result.build_message = "HoCloth native MC2 particle/collider bridge active; BoneCloth chains use cloth gravity and non-spring teams.";
    result.backend_status = RuntimeBackendStatus(*scene);
    result.build_output = MakeBuildSceneOutput(*scene);

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
    return true;
}

bool RuntimeModule::SetRuntimeInputs(SceneHandle handle, RuntimeInputs runtime_inputs)
{
    SceneState& scene = RequireScene(handle);
    scene.runtime_inputs = std::move(runtime_inputs);
    ApplyCollisionObjectInputs(scene);
    ApplyRuntimeInputsToMc2Transforms(scene);
    return true;
}

StepSceneResult RuntimeModule::StepScene(SceneHandle handle, float dt, int simulation_frequency)
{
    SceneState& scene = RequireScene(handle);
    scene.time_state.FrameUpdate(simulation_frequency);
    ApplyRuntimeInputsToMc2Transforms(scene);

    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        if (!IsBoneCloth(scene.compiled_scene.spring_bones[chain_index])) {
            StepChain(scene, scene.compiled_scene.spring_bones[chain_index], scene.chain_caches[chain_index]);
        }
    }

    const mc2::float4 simulation_power{
        scene.time_state.simulation_power[0],
        scene.time_state.simulation_power[1],
        scene.time_state.simulation_power[2],
        scene.time_state.simulation_power[3],
    };

    const int max_update_count = scene.mc2_team_manager.AlwaysTeamUpdate(
        dt,
        dt,
        dt,
        scene.time_state.global_time_scale,
        scene.time_state.simulation_delta_time,
        scene.time_state.max_simulation_count_per_frame
    );

    scene.mc2_virtual_mesh_manager.PreProxyMeshUpdate(
        scene.mc2_team_manager,
        scene.mc2_transform_manager
    );
    scene.mc2_team_manager.UpdateCenterAndInertia(
        scene.time_state.simulation_delta_time,
        scene.mc2_transform_manager,
        scene.mc2_virtual_mesh_manager,
        scene.mc2_wind_manager,
        scene.mc2_cloth_manager.Inertia().FixedArray()
    );
    scene.mc2_simulation_manager.PreSimulationUpdate(
        scene.mc2_team_manager,
        scene.mc2_virtual_mesh_manager
    );
    scene.mc2_collider_manager.PreSimulationUpdate(
        scene.mc2_team_manager,
        scene.mc2_transform_manager
    );

    int executed_steps = 0;
    if (max_update_count > 0) {
        for (int update_index = 0; update_index < max_update_count; ++update_index) {
            scene.mc2_simulation_manager.BeginSimulationStep();
            scene.mc2_team_manager.SimulationStepTeamUpdate(
                update_index,
                scene.time_state.simulation_delta_time
            );
            scene.mc2_simulation_manager.CreateStepParticleList(scene.mc2_team_manager);
            scene.mc2_collider_manager.CreateUpdateColliderList(
                update_index,
                scene.mc2_team_manager,
                scene.mc2_simulation_manager
            );
            scene.mc2_collider_manager.StartSimulationStep(
                scene.mc2_team_manager,
                scene.mc2_simulation_manager
            );
            scene.mc2_cloth_manager.PrepareStepWorkBuffers(scene.mc2_simulation_manager);
            scene.mc2_simulation_manager.StartSimulationStep(
                simulation_power,
                scene.time_state.simulation_delta_time,
                scene.mc2_team_manager,
                scene.mc2_virtual_mesh_manager
            );
            scene.mc2_simulation_manager.UpdateStepBasicPosture(
                scene.mc2_team_manager,
                scene.mc2_virtual_mesh_manager
            );
            scene.mc2_cloth_manager.SolveStepConstraints(
                update_index,
                simulation_power,
                scene.mc2_team_manager,
                scene.mc2_virtual_mesh_manager,
                scene.mc2_collider_manager,
                scene.mc2_simulation_manager
            );
            scene.mc2_simulation_manager.EndSimulationStepSolve(
                scene.time_state.simulation_delta_time,
                scene.mc2_team_manager,
                scene.mc2_virtual_mesh_manager
            );
            scene.mc2_collider_manager.EndSimulationStep(scene.mc2_simulation_manager);
            scene.mc2_simulation_manager.EndSimulationStep();
            ++executed_steps;
        }
    }

    scene.mc2_simulation_manager.CalcDisplayPosition(
        scene.time_state.simulation_delta_time,
        scene.mc2_team_manager,
        scene.mc2_virtual_mesh_manager
    );
    scene.mc2_virtual_mesh_manager.PostProxyMeshUpdate(
        scene.mc2_team_manager,
        scene.mc2_transform_manager
    );
    scene.mc2_collider_manager.PostSimulationUpdate(scene.mc2_team_manager);
    scene.mc2_team_manager.PostTeamUpdate();

    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        if (IsBoneSpring(scene.compiled_scene.spring_bones[chain_index])) {
            SyncJointStatesToMc2Particles(scene, chain_index);
            ApplyMc2ParticlesToJointStates(scene, chain_index);
        }
    }

    if (executed_steps == 0) {
        for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
            if (IsBoneSpring(scene.compiled_scene.spring_bones[chain_index])) {
                SyncJointStatesToMc2Particles(scene, chain_index);
            }
        }
    }

    scene.steps += 1;

    StepSceneResult result;
    result.handle = handle;
    result.dt = dt;
    result.simulation_frequency = scene.time_state.simulation_frequency;
    result.executed_steps = executed_steps;
    result.steps = scene.steps;
    result.summary = scene.compiled_scene.Summary();
    return result;
}

std::vector<BoneTransform> RuntimeModule::GetBoneTransforms(SceneHandle handle) const
{
    const SceneState& scene = RequireScene(handle);
    std::vector<BoneTransform> transforms;
    const mc2::TransformData& transform_data = scene.mc2_transform_manager.Data();

    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        const CompiledSpringBone& chain = scene.compiled_scene.spring_bones[chain_index];
        const ChainCache& cache = scene.chain_caches[chain_index];
        const bool is_bone_cloth = IsBoneCloth(chain);
        const RuntimeChainInput* runtime_input =
            FindRuntimeChainInput(scene.runtime_inputs, chain.component_id);
        const mc2::DataChunk transform_chunk =
            chain_index < scene.mc2_chain_team_ids.size()
                && scene.mc2_team_manager.IsValidTeam(scene.mc2_chain_team_ids[chain_index])
                ? scene.mc2_team_manager.GetTeamData(scene.mc2_chain_team_ids[chain_index]).proxy_transform_chunk
                : mc2::DataChunk::Empty();
        for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
            const CompiledSpringJoint& joint = chain.joints[joint_index];
            if (!is_bone_cloth && joint.parent_index < 0) {
                continue;
            }

            if (is_bone_cloth && transform_chunk.IsValid()) {
                const int transform_index = transform_chunk.start_index + static_cast<int>(joint_index);
                if (transform_index >= 0
                    && transform_index < transform_data.flag_array.Length()
                    && transform_index < transform_data.position_array.Length()
                    && transform_index < transform_data.rotation_array.Length()
                    && transform_index < transform_data.local_position_array.Length()
                    && transform_index < transform_data.local_rotation_array.Length()) {
                    const mc2::BitFlag8 transform_flag = transform_data.flag_array[transform_index];
                    const Quat world_rotation =
                        Mc2ToQuat(transform_data.rotation_array[transform_index]);
                    Quat output_rotation =
                        Mc2ToQuat(transform_data.local_rotation_array[transform_index]);
                    const Vec3 output_world_position =
                        Mc2ToVec3(transform_data.position_array[transform_index]);
                    const Vec3 output_local_position =
                        Mc2ToVec3(transform_data.local_position_array[transform_index]);
                    const int team_id_for_debug =
                        chain_index < scene.mc2_chain_team_ids.size()
                            ? scene.mc2_chain_team_ids[chain_index]
                            : 0;
                    const mc2::TeamManager::TeamData* team_data_for_debug =
                        scene.mc2_team_manager.IsValidTeam(team_id_for_debug)
                            ? &scene.mc2_team_manager.GetTeamData(team_id_for_debug)
                            : nullptr;
                    Quat proxy_vertex_rotation;
                    Quat vertex_to_transform_rotation;
                    Vec3 proxy_local_position;
                    Vec3 proxy_local_normal;
                    Vec3 proxy_local_tangent;
                    Vec3 proxy_posed_position;
                    Vec3 proxy_posed_normal;
                    Vec3 proxy_posed_tangent;
                    Vec3 proxy_world_normal;
                    Vec3 proxy_world_tangent;
                    if (team_data_for_debug != nullptr) {
                        const int vertex_index =
                            team_data_for_debug->proxy_common_chunk.start_index
                            + static_cast<int>(joint_index);
                        if (vertex_index >= 0
                            && vertex_index < scene.mc2_virtual_mesh_manager.Rotations().Length()) {
                            proxy_vertex_rotation =
                                Mc2ToQuat(scene.mc2_virtual_mesh_manager.Rotations()[vertex_index]);
                        }
                        const int mesh_index =
                            team_data_for_debug->proxy_mesh_chunk.start_index
                            + static_cast<int>(joint_index);
                        if (mesh_index >= 0
                            && mesh_index < scene.mc2_virtual_mesh_manager.LocalPositions().Length()
                            && mesh_index < scene.mc2_virtual_mesh_manager.LocalNormals().Length()
                            && mesh_index < scene.mc2_virtual_mesh_manager.LocalTangents().Length()
                            && mesh_index < scene.mc2_virtual_mesh_manager.BoneWeights().Length()) {
                            const mc2::VirtualMeshBoneWeight& bone_weight =
                                scene.mc2_virtual_mesh_manager.BoneWeights()[mesh_index];
                            proxy_local_position =
                                Mc2ToVec3(scene.mc2_virtual_mesh_manager.LocalPositions()[mesh_index]);
                            proxy_local_normal =
                                Mc2ToVec3(scene.mc2_virtual_mesh_manager.LocalNormals()[mesh_index]);
                            proxy_local_tangent =
                                Mc2ToVec3(scene.mc2_virtual_mesh_manager.LocalTangents()[mesh_index]);
                            for (int weight_index = 0; weight_index < bone_weight.Count(); ++weight_index) {
                                const float weight =
                                    bone_weight.weights[static_cast<std::size_t>(weight_index)];
                                const int local_bone_index =
                                    bone_weight.bone_indices[static_cast<std::size_t>(weight_index)];
                                const int skin_index =
                                    team_data_for_debug->proxy_skin_bone_chunk.start_index
                                    + local_bone_index;
                                if (weight <= 0.0f
                                    || local_bone_index < 0
                                    || skin_index < 0
                                    || skin_index
                                        >= scene.mc2_virtual_mesh_manager.SkinBoneBindPoses().Length()
                                    || skin_index
                                        >= scene.mc2_virtual_mesh_manager.SkinBoneTransformIndices().Length()) {
                                    continue;
                                }
                                const int skin_transform_index =
                                    team_data_for_debug->proxy_transform_chunk.start_index
                                    + scene.mc2_virtual_mesh_manager
                                          .SkinBoneTransformIndices()[skin_index];
                                if (skin_transform_index < 0
                                    || skin_transform_index
                                        >= transform_data.local_to_world_matrix_array.Length()) {
                                    continue;
                                }
                                const mc2::float4x4 bone_pose =
                                    scene.mc2_virtual_mesh_manager.SkinBoneBindPoses()[skin_index];
                                const mc2::float4x4 local_to_world =
                                    transform_data.local_to_world_matrix_array[skin_transform_index];
                                const mc2::float3 local_position =
                                    scene.mc2_virtual_mesh_manager.LocalPositions()[mesh_index];
                                const mc2::float3 local_normal =
                                    scene.mc2_virtual_mesh_manager.LocalNormals()[mesh_index];
                                const mc2::float3 local_tangent =
                                    scene.mc2_virtual_mesh_manager.LocalTangents()[mesh_index];
                                const mc2::float3 posed_position =
                                    mc2::TransformPoint(local_position, bone_pose);
                                const mc2::float3 posed_normal =
                                    mc2::TransformVector(local_normal, bone_pose);
                                const mc2::float3 posed_tangent =
                                    mc2::TransformVector(local_tangent, bone_pose);
                                proxy_posed_position = Mc2ToVec3(posed_position);
                                proxy_posed_normal = Mc2ToVec3(posed_normal);
                                proxy_posed_tangent = Mc2ToVec3(posed_tangent);
                                proxy_world_normal = Mc2ToVec3(
                                    mc2::Normalize(
                                        mc2::TransformVector(posed_normal, local_to_world),
                                        mc2::float3{0.0f, 1.0f, 0.0f}
                                    )
                                );
                                proxy_world_tangent = Mc2ToVec3(
                                    mc2::Normalize(
                                        mc2::TransformVector(posed_tangent, local_to_world),
                                        mc2::float3{0.0f, 0.0f, 1.0f}
                                    )
                                );
                                break;
                            }
                        }
                        const int bone_index =
                            team_data_for_debug->proxy_bone_chunk.start_index
                            + static_cast<int>(joint_index);
                        if (team_data_for_debug->proxy_bone_chunk.IsValid()
                            && bone_index >= 0
                            && bone_index
                                < scene.mc2_virtual_mesh_manager.VertexToTransformRotations().Length()) {
                            vertex_to_transform_rotation = Mc2ToQuat(
                                scene.mc2_virtual_mesh_manager.VertexToTransformRotations()[bone_index]
                            );
                        }
                    }
                    const int vertex_attribute =
                        chain_index < scene.mc2_chain_team_ids.size()
                            && scene.mc2_team_manager.IsValidTeam(scene.mc2_chain_team_ids[chain_index])
                            ? [&scene, chain_index, joint_index]() {
                                const mc2::TeamManager::TeamData& team_data =
                                    scene.mc2_team_manager.GetTeamData(
                                        scene.mc2_chain_team_ids[chain_index]
                                    );
                                const int vertex_index =
                                    team_data.proxy_common_chunk.start_index
                                    + static_cast<int>(joint_index);
                                return vertex_index >= 0
                                    && vertex_index < scene.mc2_virtual_mesh_manager.Attributes().Length()
                                    ? static_cast<int>(
                                        scene.mc2_virtual_mesh_manager.Attributes()[vertex_index].Value()
                                    )
                                    : 0;
                            }()
                            : 0;
                    if (transform_flag.IsSet(mc2::TransformManager::FlagWorldRotWrite)) {
                        transforms.push_back(MakeBoneTransformDiagnostic(
                            chain,
                            runtime_input,
                            joint_index,
                            Vec3{0.0f, 0.0f, 0.0f},
                            NormalizeQuat(world_rotation),
                            NormalizeQuat(world_rotation),
                            "mc2_world_rotation",
                            transform_flag.Value(),
                            vertex_attribute,
                            output_world_position,
                            output_local_position,
                            proxy_vertex_rotation,
                            vertex_to_transform_rotation,
                            proxy_local_position,
                            proxy_local_normal,
                            proxy_local_tangent,
                            proxy_posed_position,
                            proxy_posed_normal,
                            proxy_posed_tangent,
                            proxy_world_normal,
                            proxy_world_tangent
                        ));
                        continue;
                    }
                    if (!transform_flag.IsSet(mc2::TransformManager::FlagLocalPosRotWrite)) {
                        if (joint.parent_index < 0) {
                            continue;
                        }
                        const JointState& state = cache.joint_states[joint_index];
                        transforms.push_back(MakeBoneTransformDiagnostic(
                            chain,
                            runtime_input,
                            joint_index,
                            Vec3{0.0f, 0.0f, 0.0f},
                            QuaternionFromPitchRoll(state.pitch, state.roll),
                            world_rotation,
                            "fallback_pitch_roll",
                            transform_flag.Value(),
                            vertex_attribute,
                            output_world_position,
                            output_local_position,
                            proxy_vertex_rotation,
                            vertex_to_transform_rotation,
                            proxy_local_position,
                            proxy_local_normal,
                            proxy_local_tangent,
                            proxy_posed_position,
                            proxy_posed_normal,
                            proxy_posed_tangent,
                            proxy_world_normal,
                            proxy_world_tangent
                        ));
                        continue;
                    }

                    transforms.push_back(MakeBoneTransformDiagnostic(
                        chain,
                        runtime_input,
                        joint_index,
                        Vec3{0.0f, 0.0f, 0.0f},
                        NormalizeQuat(output_rotation),
                        NormalizeQuat(world_rotation),
                        "mc2_local_rotation",
                        transform_flag.Value(),
                        vertex_attribute,
                        output_world_position,
                        output_local_position,
                        proxy_vertex_rotation,
                        vertex_to_transform_rotation,
                        proxy_local_position,
                        proxy_local_normal,
                        proxy_local_tangent,
                        proxy_posed_position,
                        proxy_posed_normal,
                        proxy_posed_tangent,
                        proxy_world_normal,
                        proxy_world_tangent
                    ));
                    continue;
                }
            }

            const JointState& state = cache.joint_states[joint_index];
            transforms.push_back(MakeBoneTransformDiagnostic(
                chain,
                runtime_input,
                joint_index,
                Vec3{0.0f, 0.0f, 0.0f},
                QuaternionFromPitchRoll(state.pitch, state.roll),
                Quat{},
                is_bone_cloth ? "fallback_pitch_roll" : "bone_spring_pitch_roll",
                0,
                0,
                ResolveRuntimeWorldPosition(chain, runtime_input, joint_index),
                ResolveRuntimeLocalPosition(chain, runtime_input, joint_index)
            ));
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
