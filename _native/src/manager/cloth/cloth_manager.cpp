#include "hocloth/manager/cloth/cloth_manager.hpp"

#include "hocloth/manager/simulation/simulation_manager.hpp"

#include <sstream>

namespace hocloth::mc2 {

Result ClothManager::Initialize()
{
    Dispose();
    initialized_ = true;
    return Result::Ok();
}

void ClothManager::Dispose()
{
    angle_constraint_.Dispose();
    collider_collision_constraint_.Dispose();
    distance_constraint_.Dispose();
    inertia_constraint_.Dispose();
    motion_constraint_.Dispose();
    self_collision_constraint_.Dispose();
    tether_constraint_.Dispose();
    triangle_bending_constraint_.Dispose();
    initialized_ = false;
}

ManagerStatus ClothManager::Status() const
{
    std::ostringstream detail;
    detail << "distance_connections=" << distance_constraint_.ConnectionCount()
           << " fixed=" << inertia_constraint_.FixedCount()
           << " bending_pairs=" << triangle_bending_constraint_.DataCount();
    return ManagerStatus{"ClothManager", initialized_, 0, detail.str()};
}

AngleConstraint& ClothManager::Angle()
{
    return angle_constraint_;
}

const AngleConstraint& ClothManager::Angle() const
{
    return angle_constraint_;
}

ColliderCollisionConstraint& ClothManager::ColliderCollision()
{
    return collider_collision_constraint_;
}

const ColliderCollisionConstraint& ClothManager::ColliderCollision() const
{
    return collider_collision_constraint_;
}

DistanceConstraint& ClothManager::Distance()
{
    return distance_constraint_;
}

const DistanceConstraint& ClothManager::Distance() const
{
    return distance_constraint_;
}

InertiaConstraint& ClothManager::Inertia()
{
    return inertia_constraint_;
}

const InertiaConstraint& ClothManager::Inertia() const
{
    return inertia_constraint_;
}

MotionConstraint& ClothManager::Motion()
{
    return motion_constraint_;
}

const MotionConstraint& ClothManager::Motion() const
{
    return motion_constraint_;
}

SelfCollisionConstraint& ClothManager::SelfCollision()
{
    return self_collision_constraint_;
}

const SelfCollisionConstraint& ClothManager::SelfCollision() const
{
    return self_collision_constraint_;
}

TetherConstraint& ClothManager::Tether()
{
    return tether_constraint_;
}

const TetherConstraint& ClothManager::Tether() const
{
    return tether_constraint_;
}

TriangleBendingConstraint& ClothManager::TriangleBending()
{
    return triangle_bending_constraint_;
}

const TriangleBendingConstraint& ClothManager::TriangleBending() const
{
    return triangle_bending_constraint_;
}

void ClothManager::PrepareStepWorkBuffers(const SimulationManager& simulation_manager)
{
    const int particle_count = simulation_manager.ParticleCount();
    angle_constraint_.WorkBufferUpdate(particle_count);
    collider_collision_constraint_.WorkBufferUpdate(
        particle_count,
        simulation_manager.ProcessingStepEdgeCollision().Count()
    );
    self_collision_constraint_.WorkBufferUpdate(particle_count);
}

void ClothManager::SolveStepConstraints(
    int update_index,
    const float4& simulation_power,
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    const ColliderManager& collider_manager,
    SimulationManager& simulation_manager
)
{
    // Ported from MC2 SimulationManager.SimulationStepUpdate constraint order.
    tether_constraint_.Solve(team_manager, virtual_mesh_manager, simulation_manager);
    distance_constraint_.Solve(simulation_power, team_manager, virtual_mesh_manager, simulation_manager);
    angle_constraint_.Solve(simulation_power, team_manager, virtual_mesh_manager, simulation_manager);
    triangle_bending_constraint_.Solve(simulation_power, team_manager, virtual_mesh_manager, simulation_manager);
    collider_collision_constraint_.Solve(
        team_manager,
        virtual_mesh_manager,
        collider_manager,
        simulation_manager
    );
    distance_constraint_.Solve(simulation_power, team_manager, virtual_mesh_manager, simulation_manager);
    motion_constraint_.Solve(team_manager, virtual_mesh_manager, simulation_manager);
    self_collision_constraint_.SolveRuntimeSelfCollision(update_index, team_manager, simulation_manager);
}

}  // namespace hocloth::mc2
