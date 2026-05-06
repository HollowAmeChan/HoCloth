#pragma once

#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"

namespace hocloth::mc2 {

class SimulationManager;
class VirtualMeshManager;

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/MotionConstraint.cs
class MotionConstraint {
public:
    void Dispose();
    void Solve(
        const TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager,
        SimulationManager& simulation_manager
    ) const;
};

}  // namespace hocloth::mc2
