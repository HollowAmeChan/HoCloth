#include "hocloth/manager/simulation/collider_manager.hpp"

namespace hocloth::mc2 {

Result ColliderManager::Initialize()
{
    initialized_ = true;
    return Result::Ok();
}

void ColliderManager::Dispose()
{
    initialized_ = false;
}

ManagerStatus ColliderManager::Status() const
{
    return ManagerStatus{"ColliderManager", initialized_, 0};
}

}  // namespace hocloth::mc2
