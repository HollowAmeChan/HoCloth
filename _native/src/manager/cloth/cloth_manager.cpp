#include "hocloth/manager/cloth/cloth_manager.hpp"

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
    distance_constraint_.Dispose();
    inertia_constraint_.Dispose();
    motion_constraint_.Dispose();
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

}  // namespace hocloth::mc2
