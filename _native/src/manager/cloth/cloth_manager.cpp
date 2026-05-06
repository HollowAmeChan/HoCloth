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
    initialized_ = false;
}

ManagerStatus ClothManager::Status() const
{
    std::ostringstream detail;
    detail << "distance_connections=" << distance_constraint_.ConnectionCount()
           << " fixed=" << inertia_constraint_.FixedCount();
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

}  // namespace hocloth::mc2
