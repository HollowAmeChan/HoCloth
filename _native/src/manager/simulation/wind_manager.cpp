#include "hocloth/manager/simulation/wind_manager.hpp"

namespace hocloth::mc2 {

Result WindManager::Initialize()
{
    initialized_ = true;
    return Result::Ok();
}

void WindManager::Dispose()
{
    initialized_ = false;
}

ManagerStatus WindManager::Status() const
{
    return ManagerStatus{"WindManager", initialized_, 0};
}

}  // namespace hocloth::mc2
