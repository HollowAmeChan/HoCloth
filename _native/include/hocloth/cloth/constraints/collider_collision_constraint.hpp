#pragma once

#include "hocloth/utility/native_collection/ex_native_array.hpp"

namespace hocloth::mc2 {

class TeamManager;
class VirtualMeshManager;
class SimulationManager;
class ColliderManager;

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/ColliderCollisionConstraint.cs
class ColliderCollisionConstraint {
public:
    void Dispose();
    void WorkBufferUpdate(int particle_count, int edge_collider_collision_count);

    void Solve(
        const TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager,
        const ColliderManager& collider_manager,
        SimulationManager& simulation_manager
    );

private:
    ExNativeArray<int> temp_friction_array_;
    ExNativeArray<int> temp_normal_array_;
};

}  // namespace hocloth::mc2
