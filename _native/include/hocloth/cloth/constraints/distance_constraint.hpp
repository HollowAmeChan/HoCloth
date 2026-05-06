#pragma once

#include "hocloth/cloth/cloth_parameters.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

#include <cstdint>
#include <vector>

namespace hocloth::mc2 {

class SimulationManager;
class VirtualMeshManager;
class VirtualMesh;

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/DistanceConstraint.cs
class DistanceConstraint {
public:
    static constexpr int TypeCount = 2;

    struct ConstraintData {
        std::vector<std::uint32_t> index_array;
        std::vector<std::uint16_t> data_array;
        std::vector<float> distance_array;

        [[nodiscard]] bool IsValid() const
        {
            return !index_array.empty();
        }
    };

    void Dispose();
    [[nodiscard]] int DataCount() const;
    [[nodiscard]] int ConnectionCount() const;

    [[nodiscard]] static ConstraintData CreateData(
        const VirtualMesh& proxy_mesh,
        const ClothParameters& parameters
    );
    void Register(int team_id, const ConstraintData& data, TeamManager& team_manager);
    void Exit(int team_id, TeamManager& team_manager);
    void Solve(
        const float4& simulation_power,
        const TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager,
        SimulationManager& simulation_manager
    ) const;

    [[nodiscard]] const ExNativeArray<std::uint32_t>& IndexArray() const;
    [[nodiscard]] const ExNativeArray<std::uint16_t>& DataArray() const;
    [[nodiscard]] const ExNativeArray<float>& DistanceArray() const;

private:
    ExNativeArray<std::uint32_t> index_array_;
    ExNativeArray<std::uint16_t> data_array_;
    ExNativeArray<float> distance_array_;
};

}  // namespace hocloth::mc2
