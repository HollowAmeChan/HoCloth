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

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/TriangleBendingConstraint.cs
class TriangleBendingConstraint {
public:
    static constexpr std::int8_t VolumeSign = 100;
    static constexpr float VolumeScale = 1000.0f;

    struct ConstraintData {
        std::vector<std::uint64_t> triangle_pair_array;
        std::vector<float> rest_angle_or_volume_array;
        std::vector<std::int8_t> sign_or_volume_array;
        int write_buffer_count = 0;
        std::vector<std::uint32_t> write_data_array;
        std::vector<std::uint32_t> write_index_array;

        [[nodiscard]] bool IsValid() const
        {
            return !triangle_pair_array.empty();
        }
    };

    void Dispose();
    [[nodiscard]] int DataCount() const;
    [[nodiscard]] int WriteBufferCount() const;

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
    );

private:
    [[nodiscard]] bool SolveVolume(
        const float3 next_positions[4],
        const float inv_masses[4],
        float volume_rest,
        float stiffness,
        float3 add_positions[4]
    ) const;
    [[nodiscard]] bool SolveDihedralAngle(
        float sign,
        const float3 next_positions[4],
        const float inv_masses[4],
        float rest_angle,
        float stiffness,
        float3 add_positions[4]
    ) const;

    ExNativeArray<std::uint64_t> triangle_pair_array_;
    ExNativeArray<float> rest_angle_or_volume_array_;
    ExNativeArray<std::int8_t> sign_or_volume_array_;
    ExNativeArray<std::uint32_t> write_data_array_;
    ExNativeArray<std::uint32_t> write_index_array_;
    ExNativeArray<float3> write_buffer_;
};

}  // namespace hocloth::mc2
