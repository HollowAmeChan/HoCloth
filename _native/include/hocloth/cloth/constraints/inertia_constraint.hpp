#pragma once

#include "hocloth/cloth/constraints/inertia_constraint_data.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

#include <cstdint>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/InertiaConstraint.cs
class InertiaConstraint {
public:
    enum class TeleportMode {
        None = 0,
        Reset = 1,
        Keep = 2,
    };

    using CenterData = InertiaCenterData;

    struct ConstraintData {
        CenterData center_data;
        float3 init_local_gravity_direction{0.0f, -1.0f, 0.0f};
        std::vector<std::uint16_t> fixed_array;
    };

    void Dispose();
    [[nodiscard]] int FixedCount() const;

    void Register(int team_id, const ConstraintData& data, TeamManager& team_manager);
    void Exit(int team_id, TeamManager& team_manager);

    [[nodiscard]] const ExNativeArray<std::uint16_t>& FixedArray() const;

private:
    ExNativeArray<std::uint16_t> fixed_array_;
};

}  // namespace hocloth::mc2
