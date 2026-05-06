#include "hocloth/manager/simulation/simulation_manager.hpp"

#include <stdexcept>

namespace hocloth::mc2 {

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
    disp_pos_array_ = ExNativeArray<float3>(capacity);
    velocity_array_ = ExNativeArray<float3>(capacity);
    real_velocity_array_ = ExNativeArray<float3>(capacity);
    friction_array_ = ExNativeArray<float>(capacity);
    static_friction_array_ = ExNativeArray<float>(capacity);
    collision_normal_array_ = ExNativeArray<float3>(capacity);

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
    disp_pos_array_.Dispose();
    velocity_array_.Dispose();
    real_velocity_array_.Dispose();
    friction_array_.Dispose();
    static_friction_array_.Dispose();
    collision_normal_array_.Dispose();

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
    return ManagerStatus{
        "SimulationManager",
        initialized_,
        static_cast<std::uint32_t>(ParticleCount())
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

const ExNativeArray<float3>& SimulationManager::NextPositions() const
{
    return next_pos_array_;
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
    chunks.next_pos_chunk = next_pos_array_.AddRange(particle_count);
    chunks.old_pos_chunk = old_pos_array_.AddRange(particle_count);
    chunks.old_rot_chunk = old_rot_array_.AddRange(particle_count);
    chunks.base_pos_chunk = base_pos_array_.AddRange(particle_count);
    chunks.base_rot_chunk = base_rot_array_.AddRange(particle_count);
    chunks.old_position_chunk = old_position_array_.AddRange(particle_count);
    chunks.old_rotation_chunk = old_rotation_array_.AddRange(particle_count);
    chunks.velocity_pos_chunk = velocity_pos_array_.AddRange(particle_count);
    chunks.disp_pos_chunk = disp_pos_array_.AddRange(particle_count);
    chunks.velocity_chunk = velocity_array_.AddRange(particle_count);
    chunks.real_velocity_chunk = real_velocity_array_.AddRange(particle_count);
    chunks.friction_chunk = friction_array_.AddRange(particle_count);
    chunks.static_friction_chunk = static_friction_array_.AddRange(particle_count);
    chunks.collision_normal_chunk = collision_normal_array_.AddRange(particle_count);
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
    disp_pos_array_.Remove(chunks.disp_pos_chunk);
    velocity_array_.Remove(chunks.velocity_chunk);
    real_velocity_array_.Remove(chunks.real_velocity_chunk);
    friction_array_.Remove(chunks.friction_chunk);
    static_friction_array_.Remove(chunks.static_friction_chunk);
    collision_normal_array_.Remove(chunks.collision_normal_chunk);
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

void SimulationManager::MarkStepParticle(int particle_index)
{
    processing_step_particle_.Add(particle_index);
}

void SimulationManager::EndSimulationStep()
{
    ++simulation_step_count_;
}

}  // namespace hocloth::mc2
