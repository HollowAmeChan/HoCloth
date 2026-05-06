#pragma once

#include "hocloth/cloth/constraints/distance_constraint.hpp"
#include "hocloth/cloth/constraints/inertia_constraint.hpp"
#include "hocloth/manager/i_manager.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Cloth/ClothManager.cs
class ClothManager final : public IManager {
public:
    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] DistanceConstraint& Distance();
    [[nodiscard]] const DistanceConstraint& Distance() const;
    [[nodiscard]] InertiaConstraint& Inertia();
    [[nodiscard]] const InertiaConstraint& Inertia() const;

private:
    bool initialized_ = false;
    DistanceConstraint distance_constraint_;
    InertiaConstraint inertia_constraint_;
};

}  // namespace hocloth::mc2
