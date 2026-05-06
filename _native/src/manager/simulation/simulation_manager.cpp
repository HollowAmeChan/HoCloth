#include "hocloth/manager/simulation/simulation_manager.hpp"

#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace hocloth::mc2 {

namespace {

float3 NormalAxisVector(ClothNormalAxis normal_axis)
{
    switch (normal_axis) {
    case ClothNormalAxis::Right:
        return float3{1.0f, 0.0f, 0.0f};
    case ClothNormalAxis::Forward:
        return float3{0.0f, 0.0f, 1.0f};
    case ClothNormalAxis::InverseRight:
        return float3{-1.0f, 0.0f, 0.0f};
    case ClothNormalAxis::InverseUp:
        return float3{0.0f, -1.0f, 0.0f};
    case ClothNormalAxis::InverseForward:
        return float3{0.0f, 0.0f, -1.0f};
    case ClothNormalAxis::Up:
    default:
        return float3{0.0f, 1.0f, 0.0f};
    }
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

float3 ApplySpring(
    const SpringConstraintParams& spring_params,
    ClothNormalAxis normal_axis,
    float3 next_position,
    const float3& base_position,
    const quaternion& base_rotation,
    float noise_time,
    float scale_ratio
)
{
    float3 offset = Subtract(next_position, base_position);
    const float3 direction = Rotate(base_rotation, NormalAxisVector(normal_axis));
    const float limit_distance = spring_params.limit_distance * scale_ratio;

    if (limit_distance > 1.0e-8f) {
        const float offset_length = Length(offset);
        if (offset_length > limit_distance) {
            offset = Scale(offset, limit_distance / offset_length);
        }

        if (spring_params.normal_limit_ratio < 1.0f) {
            const float y_length = Dot(direction, offset);
            const float3 tangent = Subtract(offset, Scale(direction, y_length));
            const float tangent_length = Length(tangent);
            const float t = Clamp01(tangent_length / limit_distance);
            float y = std::cos(std::asin(t));
            y *= limit_distance * spring_params.normal_limit_ratio;
            if (std::abs(y_length) > y) {
                offset = Subtract(offset, Scale(direction, (std::abs(y_length) - y) * Sign(y_length)));
            }
        }
    } else {
        offset = float3{};
    }

    float power = spring_params.spring_power;
    if (spring_params.spring_noise > 0.0f) {
        float noise = std::sin(noise_time);
        noise *= spring_params.spring_noise * 0.6f;
        power = std::max(power + power * noise, 0.0f);
    }

    offset = Subtract(offset, Scale(offset, Clamp01(power)));
    return Add(base_position, offset);
}

quaternion ApplyNegativeScaleQuaternion(const quaternion& rotation, const float4& negative_scale_quaternion)
{
    return quaternion{
        rotation.w * negative_scale_quaternion.w,
        rotation.x * negative_scale_quaternion.x,
        rotation.y * negative_scale_quaternion.y,
        rotation.z * negative_scale_quaternion.z,
    };
}

quaternion ApplyNegativeScaleRotation(const quaternion& rotation, const float3& negative_scale_direction)
{
    const float4x4 local_to_world = TRS(float3{}, rotation, negative_scale_direction);
    const float3 normal{local_to_world.c1.x, local_to_world.c1.y, local_to_world.c1.z};
    const float3 tangent{local_to_world.c2.x, local_to_world.c2.y, local_to_world.c2.z};
    return ToRotation(normal, tangent);
}

}  // namespace

int SimulationManager::ParticleChunkSet::ParticleCount() const
{
    return next_pos_chunk.data_length;
}

Result SimulationManager::Initialize()
{
    Dispose();
    initialized_ = true;
    simulation_step_count_ = 0;

    constexpr int capacity = 0;
    team_id_array_ = ExNativeArray<short>(capacity);
    next_pos_array_ = ExNativeArray<float3>(capacity);
    old_pos_array_ = ExNativeArray<float3>(capacity);
    old_rot_array_ = ExNativeArray<quaternion>(capacity);
    base_pos_array_ = ExNativeArray<float3>(capacity);
    base_rot_array_ = ExNativeArray<quaternion>(capacity);
    old_position_array_ = ExNativeArray<float3>(capacity);
    old_rotation_array_ = ExNativeArray<quaternion>(capacity);
    velocity_pos_array_ = ExNativeArray<float3>(capacity);
    step_basic_position_array_ = ExNativeArray<float3>(capacity);
    step_basic_rotation_array_ = ExNativeArray<quaternion>(capacity);
    disp_pos_array_ = ExNativeArray<float3>(capacity);
    velocity_array_ = ExNativeArray<float3>(capacity);
    real_velocity_array_ = ExNativeArray<float3>(capacity);
    friction_array_ = ExNativeArray<float>(capacity);
    static_friction_array_ = ExNativeArray<float>(capacity);
    collision_normal_array_ = ExNativeArray<float3>(capacity);
    count_array_ = ExNativeArray<int>(capacity);
    sum_array_ = ExNativeArray<int>(capacity);

    return Result::Ok();
}

void SimulationManager::Dispose()
{
    team_id_array_.Dispose();
    next_pos_array_.Dispose();
    old_pos_array_.Dispose();
    old_rot_array_.Dispose();
    base_pos_array_.Dispose();
    base_rot_array_.Dispose();
    old_position_array_.Dispose();
    old_rotation_array_.Dispose();
    velocity_pos_array_.Dispose();
    step_basic_position_array_.Dispose();
    step_basic_rotation_array_.Dispose();
    disp_pos_array_.Dispose();
    velocity_array_.Dispose();
    real_velocity_array_.Dispose();
    friction_array_.Dispose();
    static_friction_array_.Dispose();
    collision_normal_array_.Dispose();
    count_array_.Dispose();
    sum_array_.Dispose();

    processing_step_particle_.Dispose();
    processing_step_triangle_bending_.Dispose();
    processing_step_edge_collision_.Dispose();
    processing_step_collider_.Dispose();
    processing_step_baseline_.Dispose();
    processing_step_motion_particle_.Dispose();
    processing_self_particle_.Dispose();
    processing_self_point_triangle_.Dispose();
    processing_self_edge_edge_.Dispose();
    processing_self_triangle_point_.Dispose();

    simulation_step_count_ = 0;
    initialized_ = false;
}

ManagerStatus SimulationManager::Status() const
{
    std::ostringstream detail;
    detail << "steps=" << simulation_step_count_
           << " next_pos_length=" << next_pos_array_.Length();
    return ManagerStatus{
        "SimulationManager",
        initialized_,
        static_cast<std::uint32_t>(ParticleCount()),
        detail.str(),
    };
}

int SimulationManager::ParticleCount() const
{
    return next_pos_array_.Count();
}

int SimulationManager::SimulationStepCount() const
{
    return simulation_step_count_;
}

const ExNativeArray<short>& SimulationManager::TeamIds() const
{
    return team_id_array_;
}

const ExNativeArray<float3>& SimulationManager::NextPositions() const
{
    return next_pos_array_;
}

ExNativeArray<float3>& SimulationManager::NextPositions()
{
    return next_pos_array_;
}

const ExNativeArray<float3>& SimulationManager::BasePositions() const
{
    return base_pos_array_;
}

ExNativeArray<float3>& SimulationManager::BasePositions()
{
    return base_pos_array_;
}

const ExNativeArray<quaternion>& SimulationManager::BaseRotations() const
{
    return base_rot_array_;
}

ExNativeArray<quaternion>& SimulationManager::BaseRotations()
{
    return base_rot_array_;
}

const ExNativeArray<float3>& SimulationManager::OldPositions() const
{
    return old_pos_array_;
}

ExNativeArray<float3>& SimulationManager::OldPositions()
{
    return old_pos_array_;
}

const ExNativeArray<float3>& SimulationManager::OldFramePositions() const
{
    return old_position_array_;
}

ExNativeArray<float3>& SimulationManager::OldFramePositions()
{
    return old_position_array_;
}

const ExNativeArray<quaternion>& SimulationManager::OldFrameRotations() const
{
    return old_rotation_array_;
}

ExNativeArray<quaternion>& SimulationManager::OldFrameRotations()
{
    return old_rotation_array_;
}

const ExNativeArray<float3>& SimulationManager::VelocityPositions() const
{
    return velocity_pos_array_;
}

ExNativeArray<float3>& SimulationManager::VelocityPositions()
{
    return velocity_pos_array_;
}

const ExNativeArray<float3>& SimulationManager::StepBasicPositions() const
{
    return step_basic_position_array_;
}

ExNativeArray<float3>& SimulationManager::StepBasicPositions()
{
    return step_basic_position_array_;
}

const ExNativeArray<quaternion>& SimulationManager::StepBasicRotations() const
{
    return step_basic_rotation_array_;
}

ExNativeArray<quaternion>& SimulationManager::StepBasicRotations()
{
    return step_basic_rotation_array_;
}

const ExNativeArray<float3>& SimulationManager::DisplayPositions() const
{
    return disp_pos_array_;
}

ExNativeArray<float3>& SimulationManager::DisplayPositions()
{
    return disp_pos_array_;
}

const ExNativeArray<float3>& SimulationManager::Velocities() const
{
    return velocity_array_;
}

ExNativeArray<float3>& SimulationManager::Velocities()
{
    return velocity_array_;
}

const ExNativeArray<float3>& SimulationManager::RealVelocities() const
{
    return real_velocity_array_;
}

ExNativeArray<float3>& SimulationManager::RealVelocities()
{
    return real_velocity_array_;
}

const ExNativeArray<float>& SimulationManager::Frictions() const
{
    return friction_array_;
}

ExNativeArray<float>& SimulationManager::Frictions()
{
    return friction_array_;
}

const ExNativeArray<float3>& SimulationManager::CollisionNormals() const
{
    return collision_normal_array_;
}

ExNativeArray<float3>& SimulationManager::CollisionNormals()
{
    return collision_normal_array_;
}

const ExNativeArray<int>& SimulationManager::CountArray() const
{
    return count_array_;
}

ExNativeArray<int>& SimulationManager::CountArray()
{
    return count_array_;
}

const ExNativeArray<int>& SimulationManager::SumArray() const
{
    return sum_array_;
}

ExNativeArray<int>& SimulationManager::SumArray()
{
    return sum_array_;
}

const ExProcessingList<int>& SimulationManager::ProcessingStepParticles() const
{
    return processing_step_particle_;
}

const ExProcessingList<int>& SimulationManager::ProcessingStepTriangleBending() const
{
    return processing_step_triangle_bending_;
}

const ExProcessingList<int>& SimulationManager::ProcessingStepEdgeCollision() const
{
    return processing_step_edge_collision_;
}

const ExProcessingList<int>& SimulationManager::ProcessingStepBaseLines() const
{
    return processing_step_baseline_;
}

const ExProcessingList<int>& SimulationManager::ProcessingStepColliders() const
{
    return processing_step_collider_;
}

const ExProcessingList<int>& SimulationManager::ProcessingStepMotionParticles() const
{
    return processing_step_motion_particle_;
}

const ExProcessingList<int>& SimulationManager::ProcessingSelfParticles() const
{
    return processing_self_particle_;
}

const ExProcessingList<unsigned int>& SimulationManager::ProcessingSelfPointTriangles() const
{
    return processing_self_point_triangle_;
}

const ExProcessingList<unsigned int>& SimulationManager::ProcessingSelfEdgeEdges() const
{
    return processing_self_edge_edge_;
}

const ExProcessingList<unsigned int>& SimulationManager::ProcessingSelfTrianglePoints() const
{
    return processing_self_triangle_point_;
}

SimulationManager::ParticleChunkSet SimulationManager::RegisterParticleRange(int team_id, int particle_count)
{
    if (!initialized_) {
        Initialize();
    }
    if (particle_count < 0) {
        throw std::runtime_error("Particle count must be non-negative.");
    }

    // Ported from Magica Cloth 2: Scripts/Core/Manager/Simulation/SimulationManager.cs RegisterProxyMesh(...)
    ParticleChunkSet chunks;
    chunks.team_id_chunk = team_id_array_.AddRange(particle_count, static_cast<short>(team_id));
    chunks.next_pos_chunk = next_pos_array_.AddRange(particle_count, float3{});
    chunks.old_pos_chunk = old_pos_array_.AddRange(particle_count, float3{});
    chunks.old_rot_chunk = old_rot_array_.AddRange(particle_count, quaternion{});
    chunks.base_pos_chunk = base_pos_array_.AddRange(particle_count, float3{});
    chunks.base_rot_chunk = base_rot_array_.AddRange(particle_count, quaternion{});
    chunks.old_position_chunk = old_position_array_.AddRange(particle_count, float3{});
    chunks.old_rotation_chunk = old_rotation_array_.AddRange(particle_count, quaternion{});
    chunks.velocity_pos_chunk = velocity_pos_array_.AddRange(particle_count, float3{});
    chunks.step_basic_position_chunk = step_basic_position_array_.AddRange(particle_count, float3{});
    chunks.step_basic_rotation_chunk = step_basic_rotation_array_.AddRange(particle_count, quaternion{});
    chunks.disp_pos_chunk = disp_pos_array_.AddRange(particle_count, float3{});
    chunks.velocity_chunk = velocity_array_.AddRange(particle_count, float3{});
    chunks.real_velocity_chunk = real_velocity_array_.AddRange(particle_count, float3{});
    chunks.friction_chunk = friction_array_.AddRange(particle_count, 0.0f);
    chunks.static_friction_chunk = static_friction_array_.AddRange(particle_count, 0.0f);
    chunks.collision_normal_chunk = collision_normal_array_.AddRange(particle_count, float3{});
    count_array_.AddRange(particle_count, 0);
    sum_array_.AddRange(particle_count * 3, 0);
    return chunks;
}

void SimulationManager::RemoveParticleRange(const ParticleChunkSet& chunks)
{
    team_id_array_.Remove(chunks.team_id_chunk);
    next_pos_array_.Remove(chunks.next_pos_chunk);
    old_pos_array_.Remove(chunks.old_pos_chunk);
    old_rot_array_.Remove(chunks.old_rot_chunk);
    base_pos_array_.Remove(chunks.base_pos_chunk);
    base_rot_array_.Remove(chunks.base_rot_chunk);
    old_position_array_.Remove(chunks.old_position_chunk);
    old_rotation_array_.Remove(chunks.old_rotation_chunk);
    velocity_pos_array_.Remove(chunks.velocity_pos_chunk);
    step_basic_position_array_.Remove(chunks.step_basic_position_chunk);
    step_basic_rotation_array_.Remove(chunks.step_basic_rotation_chunk);
    disp_pos_array_.Remove(chunks.disp_pos_chunk);
    velocity_array_.Remove(chunks.velocity_chunk);
    real_velocity_array_.Remove(chunks.real_velocity_chunk);
    friction_array_.Remove(chunks.friction_chunk);
    static_friction_array_.Remove(chunks.static_friction_chunk);
    collision_normal_array_.Remove(chunks.collision_normal_chunk);
    count_array_.Remove(DataChunk{chunks.team_id_chunk.start_index, chunks.team_id_chunk.data_length});
    sum_array_.Remove(DataChunk{chunks.team_id_chunk.start_index * 3, chunks.team_id_chunk.data_length * 3});
}

void SimulationManager::PrepareProcessingBuffers(int particle_capacity)
{
    processing_step_particle_.UpdateBuffer(particle_capacity);
    processing_step_triangle_bending_.UpdateBuffer(particle_capacity);
    processing_step_edge_collision_.UpdateBuffer(particle_capacity);
    processing_step_collider_.UpdateBuffer(particle_capacity);
    processing_step_baseline_.UpdateBuffer(particle_capacity);
    processing_step_motion_particle_.UpdateBuffer(particle_capacity);
    processing_self_particle_.UpdateBuffer(particle_capacity);
    processing_self_point_triangle_.UpdateBuffer(particle_capacity);
    processing_self_edge_edge_.UpdateBuffer(particle_capacity);
    processing_self_triangle_point_.UpdateBuffer(particle_capacity);
}

void SimulationManager::PreSimulationUpdate(
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager
)
{
    // Ported from MC2 SimulationManager.PreSimulationUpdateJob.
    const auto& proxy_positions = virtual_mesh_manager.Positions();
    const auto& proxy_rotations = virtual_mesh_manager.Rotations();

    for (int particle_index = 0; particle_index < team_id_array_.Count(); ++particle_index) {
        const int team_id = team_id_array_[particle_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess()) {
            continue;
        }

        const int local_index = particle_index - team_data.particle_chunk.start_index;
        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (local_index < 0
            || vertex_index < 0
            || vertex_index >= proxy_positions.Length()
            || vertex_index >= proxy_rotations.Length()) {
            continue;
        }

        if (team_data.IsReset()) {
            const float3 position = proxy_positions[vertex_index];
            const quaternion rotation = proxy_rotations[vertex_index];

            next_pos_array_[particle_index] = position;
            old_pos_array_[particle_index] = position;
            old_rot_array_[particle_index] = rotation;
            base_pos_array_[particle_index] = position;
            base_rot_array_[particle_index] = rotation;
            old_position_array_[particle_index] = position;
            old_rotation_array_[particle_index] = rotation;
            velocity_pos_array_[particle_index] = position;
            disp_pos_array_[particle_index] = position;
            velocity_array_[particle_index] = float3{};
            real_velocity_array_[particle_index] = float3{};
            friction_array_[particle_index] = 0.0f;
            static_friction_array_[particle_index] = 0.0f;
            collision_normal_array_[particle_index] = float3{};
            continue;
        }

        if (!team_data.IsInertiaShift() && !team_data.IsNegativeScaleTeleport()) {
            continue;
        }

        const InertiaCenterData& center_data = team_manager.GetCenterData(team_id);
        float3 old_position = old_pos_array_[particle_index];
        quaternion old_rotation = old_rot_array_[particle_index];
        float3 old_frame_position = old_position_array_[particle_index];
        quaternion old_frame_rotation = old_rotation_array_[particle_index];
        float3 display_position = disp_pos_array_[particle_index];
        float3 velocity = velocity_array_[particle_index];
        float3 real_velocity = real_velocity_array_[particle_index];

        if (team_data.IsNegativeScaleTeleport()) {
            const float4x4& negative_matrix = center_data.negative_scale_matrix;
            old_position = TransformPoint(old_position, negative_matrix);
            old_rotation = TransformRotation(
                old_rotation,
                negative_matrix,
                team_data.negative_scale_change
            );
            old_frame_position = TransformPoint(old_frame_position, negative_matrix);
            old_frame_rotation = TransformRotation(
                old_frame_rotation,
                negative_matrix,
                team_data.negative_scale_change
            );
            display_position = TransformPoint(display_position, negative_matrix);
            velocity = TransformVector(velocity, negative_matrix);
            real_velocity = TransformVector(real_velocity, negative_matrix);
        }

        if (team_data.IsInertiaShift()) {
            old_position = ShiftPosition(
                old_position,
                center_data.old_component_world_position,
                center_data.frame_component_shift_vector,
                center_data.frame_component_shift_rotation
            );
            old_rotation = Multiply(center_data.frame_component_shift_rotation, old_rotation);
            old_frame_position = ShiftPosition(
                old_frame_position,
                center_data.old_component_world_position,
                center_data.frame_component_shift_vector,
                center_data.frame_component_shift_rotation
            );
            old_frame_rotation =
                Multiply(center_data.frame_component_shift_rotation, old_frame_rotation);
            display_position = ShiftPosition(
                display_position,
                center_data.old_component_world_position,
                center_data.frame_component_shift_vector,
                center_data.frame_component_shift_rotation
            );
            velocity = Rotate(center_data.frame_component_shift_rotation, velocity);
            real_velocity = Rotate(center_data.frame_component_shift_rotation, real_velocity);
        }

        old_pos_array_[particle_index] = old_position;
        old_rot_array_[particle_index] = old_rotation;
        old_position_array_[particle_index] = old_frame_position;
        old_rotation_array_[particle_index] = old_frame_rotation;
        disp_pos_array_[particle_index] = display_position;
        velocity_array_[particle_index] = velocity;
        real_velocity_array_[particle_index] = real_velocity;
    }
}

void SimulationManager::BeginSimulationStep()
{
    PrepareProcessingBuffers(ParticleCount());
    processing_step_particle_.Clear();
    processing_step_triangle_bending_.Clear();
    processing_step_edge_collision_.Clear();
    processing_step_collider_.Clear();
    processing_step_baseline_.Clear();
    processing_step_motion_particle_.Clear();
    processing_self_particle_.Clear();
    processing_self_point_triangle_.Clear();
    processing_self_edge_edge_.Clear();
    processing_self_triangle_point_.Clear();
}

void SimulationManager::CreateStepParticleList(const TeamManager& team_manager)
{
    for (int team_id = 1; team_id < team_manager.TeamCount(); ++team_id) {
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess() || !team_data.IsRunning() || !team_data.IsStepRunning()) {
            continue;
        }

        const ClothParameters& parameters = team_manager.GetParameters(team_id);

        const int particle_count = team_data.ParticleCount();
        const int particle_start = team_data.particle_chunk.start_index;
        for (int index = 0; index < particle_count; ++index) {
            MarkStepParticle(particle_start + index);
        }

        const int baseline_count = team_data.BaseLineCount();
        const int baseline_start = team_data.baseline_chunk.start_index;
        for (int index = 0; index < baseline_count; ++index) {
            MarkStepBaseLine(data::Pack32(team_id, baseline_start + index));
        }

        if (parameters.triangle_bending_constraint.method != TriangleBendingMethod::None) {
            const int bending_count = team_data.bending_pair_chunk.data_length;
            const int bending_start = team_data.bending_pair_chunk.start_index;
            for (int index = 0; index < bending_count; ++index) {
                MarkStepTriangleBending(data::Pack12_20(team_id, bending_start + index));
            }
        }

        if (parameters.collider_collision_constraint.mode == ColliderCollisionMode::Edge
            && team_data.proxy_edge_chunk.IsValid()
            && team_data.ColliderCount() > 0) {
            const int edge_count = team_data.proxy_edge_chunk.data_length;
            const int edge_start = team_data.proxy_edge_chunk.start_index;
            for (int index = 0; index < edge_count; ++index) {
                MarkStepEdgeCollision(edge_start + index);
            }
        }

        if (parameters.motion_constraint.use_max_distance != 0
            || parameters.motion_constraint.use_backstop != 0) {
            for (int index = 0; index < particle_count; ++index) {
                MarkStepMotionParticle(particle_start + index);
            }
        }
    }

    CreateSelfProcessingLists(team_manager);
}

void SimulationManager::CreateSelfProcessingLists(const TeamManager& team_manager)
{
    for (int team_id = 1; team_id < team_manager.TeamCount(); ++team_id) {
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess() || !team_data.IsRunning()) {
            continue;
        }

        const bool use_self_edge_edge =
            team_data.flag.TestAny(TeamManager::FlagSelfEdgeEdge, 3);
        const bool use_self_point_triangle =
            team_data.flag.TestAny(TeamManager::FlagSelfPointTriangle, 3);
        const bool use_self_triangle_point =
            team_data.flag.TestAny(TeamManager::FlagSelfTrianglePoint, 3);

        if (use_self_edge_edge) {
            for (int index = 0; index < team_data.EdgeCount(); ++index) {
                MarkSelfEdgeEdge(data::Pack32(team_id, index));
            }
        }
        if (use_self_point_triangle) {
            for (int index = 0; index < team_data.ParticleCount(); ++index) {
                MarkSelfPointTriangle(data::Pack32(team_id, index));
            }
        }
        if (use_self_triangle_point) {
            for (int index = 0; index < team_data.TriangleCount(); ++index) {
                MarkSelfTrianglePoint(data::Pack32(team_id, index));
            }
        }
        if (use_self_edge_edge || use_self_point_triangle || use_self_triangle_point) {
            const int particle_start = team_data.particle_chunk.start_index;
            for (int index = 0; index < team_data.ParticleCount(); ++index) {
                MarkSelfParticle(particle_start + index);
            }
        }
    }
}

void SimulationManager::StartSimulationStep(
    const float4& simulation_power,
    float simulation_delta_time,
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager
)
{
    // Ported from Magica Cloth 2: Scripts/Core/Manager/Simulation/SimulationManager.cs StartSimulationStepJob
    if (simulation_delta_time <= 0.0f) {
        return;
    }

    const auto& step_particles = processing_step_particle_;
    const auto& step_buffer = step_particles.Buffer();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& depths = virtual_mesh_manager.VertexDepths();
    const auto& proxy_positions = virtual_mesh_manager.Positions();
    const auto& proxy_rotations = virtual_mesh_manager.Rotations();

    for (int step_index = 0; step_index < step_particles.Count(); ++step_index) {
        const int particle_index = step_buffer[static_cast<std::size_t>(step_index)];
        if (particle_index < 0 || particle_index >= team_id_array_.Length()) {
            continue;
        }

        const int team_id = team_id_array_[particle_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const ClothParameters& parameters = team_manager.GetParameters(team_id);
        const InertiaCenterData& center_data = team_manager.GetCenterData(team_id);
        const int local_index = particle_index - team_data.particle_chunk.start_index;
        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (local_index < 0 || vertex_index < 0 || vertex_index >= attributes.Length()) {
            continue;
        }

        const VertexAttribute attr = attributes[vertex_index];
        const float depth = vertex_index < depths.Length() ? depths[vertex_index] : 0.0f;
        const float3 old_position = old_pos_array_[particle_index];
        float3 next_position = old_position;
        float3 velocity_position = old_position;

        const float3 previous_base_position =
            particle_index < old_position_array_.Length() ? old_position_array_[particle_index] : old_position;
        const quaternion previous_base_rotation =
            particle_index < old_rotation_array_.Length() ? old_rotation_array_[particle_index] : quaternion{};
        const float3 proxy_position =
            vertex_index < proxy_positions.Length() ? proxy_positions[vertex_index] : previous_base_position;
        const quaternion proxy_rotation =
            vertex_index < proxy_rotations.Length() ? proxy_rotations[vertex_index] : previous_base_rotation;

        const float3 base_position =
            Lerp(previous_base_position, proxy_position, team_data.frame_interpolation);
        const quaternion base_rotation =
            Normalize(Slerp(previous_base_rotation, proxy_rotation, team_data.frame_interpolation));
        base_pos_array_[particle_index] = base_position;
        base_rot_array_[particle_index] = base_rotation;
        step_basic_position_array_[particle_index] = base_position;
        step_basic_rotation_array_[particle_index] = base_rotation;

        if (attr.IsMove() || team_data.IsSpring()) {
            float3 velocity = velocity_array_[particle_index];

            float3 inertia_vector = center_data.inertia_vector;
            quaternion inertia_rotation = center_data.inertia_rotation;
            const float inertia_depth =
                parameters.inertia_constraint.depth_inertia * (1.0f - depth * depth);
            inertia_vector = Lerp(inertia_vector, center_data.step_vector, inertia_depth);
            inertia_rotation = Slerp(inertia_rotation, center_data.step_rotation, inertia_depth);

            float3 local_position = Subtract(old_position, center_data.old_world_position);
            local_position = Rotate(inertia_rotation, local_position);
            local_position = Add(local_position, inertia_vector);
            const float3 world_position = Add(center_data.old_world_position, local_position);
            const float3 inertia_offset = Subtract(world_position, next_position);

            next_position = world_position;
            velocity_position = Add(velocity_position, inertia_offset);
            velocity = Rotate(inertia_rotation, velocity);
            velocity = Scale(velocity, team_data.velocity_weight);

            const float damping = MC2EvaluateCurveClamp01(parameters.damping_curve_data, depth);
            velocity = Scale(velocity, Clamp01(1.0f - damping * simulation_power.z));

            float3 force = Scale(
                parameters.world_gravity_direction,
                parameters.gravity * team_data.gravity_ratio
            );
            const float mass = CalcMass(depth);
            float3 external_force{};
            switch (team_data.force_mode) {
            case ClothForceMode::VelocityAdd:
                external_force = mass > 0.0f
                    ? Scale(team_data.impact_force, 1.0f / mass)
                    : float3{};
                break;
            case ClothForceMode::VelocityAddWithoutDepth:
                external_force = team_data.impact_force;
                break;
            case ClothForceMode::VelocityChange:
                external_force = mass > 0.0f
                    ? Scale(team_data.impact_force, 1.0f / mass)
                    : float3{};
                velocity = float3{};
                break;
            case ClothForceMode::VelocityChangeWithoutDepth:
                external_force = team_data.impact_force;
                velocity = float3{};
                break;
            case ClothForceMode::None:
            default:
                break;
            }
            force = Add(force, external_force);
            force = Scale(force, team_data.scale_ratio);

            velocity = Add(velocity, Scale(force, simulation_delta_time * simulation_power.x));
            next_position = Add(next_position, Scale(velocity, simulation_delta_time));
        } else {
            next_position = base_position;
            velocity_position = base_position;
        }

        if (team_data.IsSpring() && attr.IsFixed()) {
            const float noise_time =
                (team_data.time + static_cast<float>(step_index) * 49.6198f) * 2.4512f
                + next_position.x
                + next_position.y
                + next_position.z;
            next_position = ApplySpring(
                parameters.spring_constraint,
                parameters.normal_axis,
                next_position,
                base_position,
                base_rotation,
                noise_time,
                team_data.scale_ratio
            );
        }

        velocity_pos_array_[particle_index] = velocity_position;
        next_pos_array_[particle_index] = next_position;
    }
}

void SimulationManager::UpdateStepBasicPosture(
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager
)
{
    // Ported from MC2 SimulationManager.UpdateStepBasicPotureJob.
    const auto& baseline_steps = processing_step_baseline_;
    if (baseline_steps.Count() <= 0) {
        return;
    }

    const auto& baseline_buffer = baseline_steps.Buffer();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& vertex_parent_indices = virtual_mesh_manager.VertexParentIndices();
    const auto& vertex_local_positions = virtual_mesh_manager.VertexLocalPositions();
    const auto& vertex_local_rotations = virtual_mesh_manager.VertexLocalRotations();
    const auto& baseline_start_indices = virtual_mesh_manager.BaseLineStartDataIndices();
    const auto& baseline_data_counts = virtual_mesh_manager.BaseLineDataCounts();
    const auto& baseline_data = virtual_mesh_manager.BaseLineData();

    for (int step_index = 0; step_index < baseline_steps.Count(); ++step_index) {
        const std::uint32_t pack = static_cast<std::uint32_t>(
            baseline_buffer[static_cast<std::size_t>(step_index)]
        );
        const int team_id = data::Unpack32Hi(pack);
        const int baseline_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)
            || baseline_index < 0
            || baseline_index >= baseline_start_indices.Length()
            || baseline_index >= baseline_data_counts.Length()) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const float blend_ratio = team_data.animation_pose_ratio;
        if (blend_ratio > 0.99f) {
            continue;
        }

        const int baseline_data_start = team_data.baseline_data_chunk.start_index;
        const int particle_start = team_data.particle_chunk.start_index;
        const int vertex_start = team_data.proxy_common_chunk.start_index;
        const float3 scale = Scale(team_data.init_scale, team_data.scale_ratio);
        const int start = baseline_start_indices[baseline_index];
        const int data_count = baseline_data_counts[baseline_index];

        int data_index = start + baseline_data_start;
        for (int index = 0; index < data_count; ++index, ++data_index) {
            if (data_index < 0 || data_index >= baseline_data.Length()) {
                continue;
            }

            const int local_index = baseline_data[data_index];
            const int particle_index = particle_start + local_index;
            const int vertex_index = vertex_start + local_index;
            if (particle_index < 0
                || particle_index >= step_basic_position_array_.Length()
                || particle_index >= step_basic_rotation_array_.Length()
                || vertex_index < 0
                || vertex_index >= attributes.Length()
                || vertex_index >= vertex_parent_indices.Length()) {
                continue;
            }

            const int parent_local_index = vertex_parent_indices[vertex_index];
            const int parent_particle_index = parent_local_index + particle_start;
            const VertexAttribute attr = attributes[vertex_index];
            if (attr.IsMove()
                && parent_local_index >= 0
                && parent_particle_index >= 0
                && parent_particle_index < step_basic_position_array_.Length()
                && parent_particle_index < step_basic_rotation_array_.Length()
                && vertex_index < vertex_local_positions.Length()
                && vertex_index < vertex_local_rotations.Length()) {
                float3 local_position = vertex_local_positions[vertex_index];
                local_position = float3{
                    local_position.x * team_data.negative_scale_direction.x * scale.x,
                    local_position.y * team_data.negative_scale_direction.y * scale.y,
                    local_position.z * team_data.negative_scale_direction.z * scale.z,
                };
                const quaternion local_rotation = ApplyNegativeScaleQuaternion(
                    vertex_local_rotations[vertex_index],
                    team_data.negative_scale_quaternion_value
                );
                const float3 parent_position = step_basic_position_array_[parent_particle_index];
                const quaternion parent_rotation = step_basic_rotation_array_[parent_particle_index];

                step_basic_position_array_[particle_index] =
                    Add(parent_position, Rotate(parent_rotation, local_position));
                step_basic_rotation_array_[particle_index] =
                    Normalize(Multiply(parent_rotation, local_rotation));
            } else {
                step_basic_rotation_array_[particle_index] = ApplyNegativeScaleRotation(
                    step_basic_rotation_array_[particle_index],
                    team_data.negative_scale_direction
                );
            }
        }

        if (blend_ratio > define::system::Epsilon) {
            data_index = start + baseline_data_start;
            for (int index = 0; index < data_count; ++index, ++data_index) {
                if (data_index < 0 || data_index >= baseline_data.Length()) {
                    continue;
                }

                const int local_index = baseline_data[data_index];
                const int particle_index = particle_start + local_index;
                if (particle_index < 0
                    || particle_index >= step_basic_position_array_.Length()
                    || particle_index >= step_basic_rotation_array_.Length()
                    || particle_index >= base_pos_array_.Length()
                    || particle_index >= base_rot_array_.Length()) {
                    continue;
                }

                step_basic_position_array_[particle_index] = Lerp(
                    step_basic_position_array_[particle_index],
                    base_pos_array_[particle_index],
                    blend_ratio
                );
                step_basic_rotation_array_[particle_index] = Normalize(Slerp(
                    step_basic_rotation_array_[particle_index],
                    base_rot_array_[particle_index],
                    blend_ratio
                ));
            }
        }
    }
}

void SimulationManager::EndSimulationStepSolve(
    float simulation_delta_time,
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager
)
{
    // Ported from Magica Cloth 2: Scripts/Core/Manager/Simulation/SimulationManager.cs EndSimulationStepJob
    if (simulation_delta_time <= 0.0f) {
        return;
    }

    const auto& step_particles = processing_step_particle_;
    const auto& step_buffer = step_particles.Buffer();
    const auto& attributes = virtual_mesh_manager.Attributes();

    for (int step_index = 0; step_index < step_particles.Count(); ++step_index) {
        const int particle_index = step_buffer[static_cast<std::size_t>(step_index)];
        if (particle_index < 0 || particle_index >= team_id_array_.Length()) {
            continue;
        }

        const int team_id = team_id_array_[particle_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const ClothParameters& parameters = team_manager.GetParameters(team_id);
        const int local_index = particle_index - team_data.particle_chunk.start_index;
        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (local_index < 0 || vertex_index < 0 || vertex_index >= attributes.Length()) {
            continue;
        }

        const VertexAttribute attr = attributes[vertex_index];
        const float3 next_position = next_pos_array_[particle_index];
        const float3 old_position = old_pos_array_[particle_index];

        if (attr.IsMove() || team_data.IsSpring()) {
            const float3 velocity_old_position = velocity_pos_array_[particle_index];
            float3 velocity =
                Scale(Subtract(next_position, velocity_old_position), 1.0f / simulation_delta_time);

            if (parameters.inertia_constraint.particle_speed_limit >= 0.0f) {
                const float max_speed =
                    parameters.inertia_constraint.particle_speed_limit * team_data.scale_ratio;
                const float speed = Length(velocity);
                if (speed > max_speed && speed > 0.0f) {
                    velocity = Scale(velocity, max_speed / speed);
                }
            }

            velocity_array_[particle_index] = Scale(velocity, team_data.velocity_weight);
        }

        real_velocity_array_[particle_index] =
            Scale(Subtract(next_position, old_position), 1.0f / simulation_delta_time);
        old_pos_array_[particle_index] = next_position;
    }
}

void SimulationManager::CalcDisplayPosition(
    float simulation_delta_time,
    const TeamManager& team_manager,
    VirtualMeshManager& virtual_mesh_manager
)
{
    // Ported from Magica Cloth 2: Scripts/Core/Manager/Simulation/SimulationManager.cs CalcDisplayPositionJob
    if (simulation_delta_time <= 0.0f) {
        return;
    }

    const auto& attributes = virtual_mesh_manager.Attributes();
    auto& proxy_positions = virtual_mesh_manager.Positions();
    auto& proxy_rotations = virtual_mesh_manager.Rotations();

    for (int particle_index = 0; particle_index < team_id_array_.Count(); ++particle_index) {
        const int team_id = team_id_array_[particle_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess()) {
            continue;
        }

        const int local_index = particle_index - team_data.particle_chunk.start_index;
        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (local_index < 0
            || vertex_index < 0
            || vertex_index >= attributes.Length()
            || vertex_index >= proxy_positions.Length()
            || vertex_index >= proxy_rotations.Length()) {
            continue;
        }

        const VertexAttribute attr = attributes[vertex_index];
        const float3 previous_proxy_position = proxy_positions[vertex_index];
        const quaternion previous_proxy_rotation = proxy_rotations[vertex_index];

        if (attr.IsMove() || team_data.IsSpring()) {
            float3 display_position = old_pos_array_[particle_index];
            const float3 velocity_prediction =
                Scale(real_velocity_array_[particle_index], simulation_delta_time);
            const float3 future_position = Add(display_position, velocity_prediction);
            const float interval =
                (team_data.now_update_time + simulation_delta_time) - team_data.old_time;
            const float interpolation =
                interval > 0.0f ? (team_data.time - team_data.old_time) / interval : 0.0f;
            display_position = Lerp(disp_pos_array_[particle_index], future_position, interpolation);
            disp_pos_array_[particle_index] = display_position;

            proxy_positions[vertex_index] =
                Lerp(previous_proxy_position, display_position, team_data.blend_weight);
        } else {
            disp_pos_array_[particle_index] = previous_proxy_position;
        }

        if (team_data.IsRunning()) {
            old_position_array_[particle_index] = previous_proxy_position;
            old_rotation_array_[particle_index] = previous_proxy_rotation;
        }

        if (team_data.IsNegativeScale()) {
            proxy_rotations[vertex_index] = ApplyNegativeScaleRotation(
                previous_proxy_rotation,
                team_data.negative_scale_direction
            );
        }
    }
}

void SimulationManager::MarkStepParticle(int particle_index)
{
    processing_step_particle_.Add(particle_index);
}

void SimulationManager::MarkStepTriangleBending(std::uint32_t packed_team_and_pair_index)
{
    processing_step_triangle_bending_.Add(static_cast<int>(packed_team_and_pair_index));
}

void SimulationManager::MarkStepEdgeCollision(int edge_index)
{
    processing_step_edge_collision_.Add(edge_index);
}

void SimulationManager::MarkStepBaseLine(std::uint32_t packed_team_and_baseline_index)
{
    processing_step_baseline_.Add(static_cast<int>(packed_team_and_baseline_index));
}

void SimulationManager::MarkStepCollider(int collider_index)
{
    processing_step_collider_.Add(collider_index);
}

void SimulationManager::MarkStepMotionParticle(int particle_index)
{
    processing_step_motion_particle_.Add(particle_index);
}

void SimulationManager::MarkSelfParticle(int particle_index)
{
    processing_self_particle_.Add(particle_index);
}

void SimulationManager::MarkSelfPointTriangle(std::uint32_t packed_team_and_local_index)
{
    processing_self_point_triangle_.Add(packed_team_and_local_index);
}

void SimulationManager::MarkSelfEdgeEdge(std::uint32_t packed_team_and_local_index)
{
    processing_self_edge_edge_.Add(packed_team_and_local_index);
}

void SimulationManager::MarkSelfTrianglePoint(std::uint32_t packed_team_and_local_index)
{
    processing_self_triangle_point_.Add(packed_team_and_local_index);
}

void SimulationManager::EndSimulationStep()
{
    ++simulation_step_count_;
}

}  // namespace hocloth::mc2
