#pragma once

#include "hocloth/manager/i_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"
#include "hocloth/utility/native_collection/ex_processing_list.hpp"

#include <cstdint>

namespace hocloth::mc2 {

class TeamManager;
class VirtualMeshManager;

// Port target for Magica Cloth 2: Scripts/Core/Manager/Simulation/SimulationManager.cs
class SimulationManager final : public IManager {
public:
    struct ParticleChunkSet {
        DataChunk team_id_chunk;
        DataChunk next_pos_chunk;
        DataChunk old_pos_chunk;
        DataChunk old_rot_chunk;
        DataChunk base_pos_chunk;
        DataChunk base_rot_chunk;
        DataChunk old_position_chunk;
        DataChunk old_rotation_chunk;
        DataChunk velocity_pos_chunk;
        DataChunk step_basic_position_chunk;
        DataChunk step_basic_rotation_chunk;
        DataChunk disp_pos_chunk;
        DataChunk velocity_chunk;
        DataChunk real_velocity_chunk;
        DataChunk friction_chunk;
        DataChunk static_friction_chunk;
        DataChunk collision_normal_chunk;

        [[nodiscard]] int ParticleCount() const;
    };

    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] int ParticleCount() const;
    [[nodiscard]] int SimulationStepCount() const;
    [[nodiscard]] const ExNativeArray<short>& TeamIds() const;
    [[nodiscard]] const ExNativeArray<float3>& NextPositions() const;
    [[nodiscard]] ExNativeArray<float3>& NextPositions();
    [[nodiscard]] const ExNativeArray<float3>& BasePositions() const;
    [[nodiscard]] ExNativeArray<float3>& BasePositions();
    [[nodiscard]] const ExNativeArray<quaternion>& BaseRotations() const;
    [[nodiscard]] ExNativeArray<quaternion>& BaseRotations();
    [[nodiscard]] const ExNativeArray<float3>& OldPositions() const;
    [[nodiscard]] ExNativeArray<float3>& OldPositions();
    [[nodiscard]] const ExNativeArray<float3>& OldFramePositions() const;
    [[nodiscard]] ExNativeArray<float3>& OldFramePositions();
    [[nodiscard]] const ExNativeArray<quaternion>& OldFrameRotations() const;
    [[nodiscard]] ExNativeArray<quaternion>& OldFrameRotations();
    [[nodiscard]] const ExNativeArray<float3>& VelocityPositions() const;
    [[nodiscard]] ExNativeArray<float3>& VelocityPositions();
    [[nodiscard]] const ExNativeArray<float3>& StepBasicPositions() const;
    [[nodiscard]] ExNativeArray<float3>& StepBasicPositions();
    [[nodiscard]] const ExNativeArray<quaternion>& StepBasicRotations() const;
    [[nodiscard]] ExNativeArray<quaternion>& StepBasicRotations();
    [[nodiscard]] const ExNativeArray<float3>& DisplayPositions() const;
    [[nodiscard]] ExNativeArray<float3>& DisplayPositions();
    [[nodiscard]] const ExNativeArray<float3>& Velocities() const;
    [[nodiscard]] ExNativeArray<float3>& Velocities();
    [[nodiscard]] const ExNativeArray<float3>& RealVelocities() const;
    [[nodiscard]] ExNativeArray<float3>& RealVelocities();
    [[nodiscard]] const ExNativeArray<float>& Frictions() const;
    [[nodiscard]] ExNativeArray<float>& Frictions();
    [[nodiscard]] const ExNativeArray<float3>& CollisionNormals() const;
    [[nodiscard]] ExNativeArray<float3>& CollisionNormals();
    [[nodiscard]] const ExProcessingList<int>& ProcessingStepParticles() const;
    [[nodiscard]] const ExProcessingList<int>& ProcessingStepTriangleBending() const;
    [[nodiscard]] const ExProcessingList<int>& ProcessingStepBaseLines() const;
    [[nodiscard]] const ExProcessingList<int>& ProcessingStepMotionParticles() const;

    [[nodiscard]] ParticleChunkSet RegisterParticleRange(int team_id, int particle_count);
    void RemoveParticleRange(const ParticleChunkSet& chunks);

    void PrepareProcessingBuffers(int particle_capacity);
    void BeginSimulationStep();
    void StartSimulationStep(
        const float4& simulation_power,
        float simulation_delta_time,
        const TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager
    );
    void EndSimulationStepSolve(
        float simulation_delta_time,
        const TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager
    );
    void CalcDisplayPosition(
        float simulation_delta_time,
        const TeamManager& team_manager,
        VirtualMeshManager& virtual_mesh_manager
    );
    void MarkStepParticle(int particle_index);
    void MarkStepTriangleBending(std::uint32_t packed_team_and_pair_index);
    void MarkStepBaseLine(std::uint32_t packed_team_and_baseline_index);
    void MarkStepMotionParticle(int particle_index);
    void EndSimulationStep();

private:
    bool initialized_ = false;
    int simulation_step_count_ = 0;

    ExNativeArray<short> team_id_array_;
    ExNativeArray<float3> next_pos_array_;
    ExNativeArray<float3> old_pos_array_;
    ExNativeArray<quaternion> old_rot_array_;
    ExNativeArray<float3> base_pos_array_;
    ExNativeArray<quaternion> base_rot_array_;
    ExNativeArray<float3> old_position_array_;
    ExNativeArray<quaternion> old_rotation_array_;
    ExNativeArray<float3> velocity_pos_array_;
    ExNativeArray<float3> step_basic_position_array_;
    ExNativeArray<quaternion> step_basic_rotation_array_;
    ExNativeArray<float3> disp_pos_array_;
    ExNativeArray<float3> velocity_array_;
    ExNativeArray<float3> real_velocity_array_;
    ExNativeArray<float> friction_array_;
    ExNativeArray<float> static_friction_array_;
    ExNativeArray<float3> collision_normal_array_;

    ExProcessingList<int> processing_step_particle_;
    ExProcessingList<int> processing_step_triangle_bending_;
    ExProcessingList<int> processing_step_edge_collision_;
    ExProcessingList<int> processing_step_collider_;
    ExProcessingList<int> processing_step_baseline_;
    ExProcessingList<int> processing_step_motion_particle_;
    ExProcessingList<int> processing_self_particle_;
    ExProcessingList<unsigned int> processing_self_point_triangle_;
    ExProcessingList<unsigned int> processing_self_edge_edge_;
    ExProcessingList<unsigned int> processing_self_triangle_point_;
};

}  // namespace hocloth::mc2
