#pragma once

#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

namespace hocloth::mc2 {

class TeamManager;
class VirtualMeshManager;
class SimulationManager;

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
    [[nodiscard]] static float PointSphereColliderDetection(
        float3& next_position,
        const float3& base_position,
        float radius,
        const AABB& particle_bounds,
        const ColliderManager::WorkData& collider_work,
        float max_length,
        float3& normal
    );
    [[nodiscard]] static float PointPlaneColliderDetection(
        float3& next_position,
        float radius,
        const ColliderManager::WorkData& collider_work,
        float3& normal
    );
    [[nodiscard]] static float PointCapsuleColliderDetection(
        float3& next_position,
        float radius,
        const AABB& particle_bounds,
        const ColliderManager::WorkData& collider_work,
        float3& normal
    );

    ExNativeArray<int> temp_friction_array_;
    ExNativeArray<int> temp_normal_array_;
};

}  // namespace hocloth::mc2
