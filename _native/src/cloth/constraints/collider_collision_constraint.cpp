#include "hocloth/cloth/constraints/collider_collision_constraint.hpp"

#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"

namespace hocloth::mc2 {

void ColliderCollisionConstraint::Dispose()
{
    temp_friction_array_.Dispose();
    temp_normal_array_.Dispose();
}

void ColliderCollisionConstraint::WorkBufferUpdate(
    int particle_count,
    int edge_collider_collision_count
)
{
    if (edge_collider_collision_count <= 0) {
        return;
    }

    temp_friction_array_.Dispose();
    temp_normal_array_.Dispose();
    temp_friction_array_ = ExNativeArray<int>(particle_count);
    temp_normal_array_ = ExNativeArray<int>(particle_count * 3);
    temp_friction_array_.AddRange(particle_count, 0);
    temp_normal_array_.AddRange(particle_count * 3, 0);
}

void ColliderCollisionConstraint::Solve(
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    const ColliderManager& collider_manager,
    SimulationManager& simulation_manager
)
{
    // Runtime collision detection is intentionally split after the manager/data port.
    // The MC2 source has separate Point, Edge, and aggregation jobs; this shell keeps
    // the same dependencies and ownership boundary for the next bottom-up pass.
    (void)team_manager;
    (void)virtual_mesh_manager;
    (void)collider_manager;
    (void)simulation_manager;
}

}  // namespace hocloth::mc2
