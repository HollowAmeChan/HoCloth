#pragma once

#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

namespace hocloth::mc2 {

class TeamManager;
class VirtualMeshManager;
class SimulationManager;

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/AngleConstraint.cs
class AngleConstraint {
public:
    void Dispose();
    void WorkBufferUpdate(int particle_count);

    void Solve(
        const float4& simulation_power,
        const TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager,
        SimulationManager& simulation_manager
    );

private:
    ExNativeArray<float> length_buffer_;
    ExNativeArray<float3> local_pos_buffer_;
    ExNativeArray<quaternion> local_rot_buffer_;
    ExNativeArray<quaternion> rotation_buffer_;
    ExNativeArray<float3> restoration_vector_buffer_;
};

}  // namespace hocloth::mc2
