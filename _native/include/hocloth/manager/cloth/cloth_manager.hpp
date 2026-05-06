#pragma once

#include "hocloth/cloth/constraints/angle_constraint.hpp"
#include "hocloth/cloth/constraints/collider_collision_constraint.hpp"
#include "hocloth/cloth/constraints/distance_constraint.hpp"
#include "hocloth/cloth/constraints/inertia_constraint.hpp"
#include "hocloth/cloth/constraints/motion_constraint.hpp"
#include "hocloth/cloth/constraints/tether_constraint.hpp"
#include "hocloth/cloth/constraints/triangle_bending_constraint.hpp"
#include "hocloth/manager/i_manager.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Cloth/ClothManager.cs
class ClothManager final : public IManager {
public:
    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] AngleConstraint& Angle();
    [[nodiscard]] const AngleConstraint& Angle() const;
    [[nodiscard]] ColliderCollisionConstraint& ColliderCollision();
    [[nodiscard]] const ColliderCollisionConstraint& ColliderCollision() const;
    [[nodiscard]] DistanceConstraint& Distance();
    [[nodiscard]] const DistanceConstraint& Distance() const;
    [[nodiscard]] InertiaConstraint& Inertia();
    [[nodiscard]] const InertiaConstraint& Inertia() const;
    [[nodiscard]] MotionConstraint& Motion();
    [[nodiscard]] const MotionConstraint& Motion() const;
    [[nodiscard]] TetherConstraint& Tether();
    [[nodiscard]] const TetherConstraint& Tether() const;
    [[nodiscard]] TriangleBendingConstraint& TriangleBending();
    [[nodiscard]] const TriangleBendingConstraint& TriangleBending() const;

private:
    bool initialized_ = false;
    AngleConstraint angle_constraint_;
    ColliderCollisionConstraint collider_collision_constraint_;
    DistanceConstraint distance_constraint_;
    InertiaConstraint inertia_constraint_;
    MotionConstraint motion_constraint_;
    TetherConstraint tether_constraint_;
    TriangleBendingConstraint triangle_bending_constraint_;
};

}  // namespace hocloth::mc2
