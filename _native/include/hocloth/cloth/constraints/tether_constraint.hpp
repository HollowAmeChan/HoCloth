#pragma once

namespace hocloth::mc2 {

class TeamManager;
class VirtualMeshManager;
class SimulationManager;

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/TetherConstraint.cs
class TetherConstraint {
public:
    void Dispose();

    void Solve(
        const TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager,
        SimulationManager& simulation_manager
    ) const;
};

}  // namespace hocloth::mc2
