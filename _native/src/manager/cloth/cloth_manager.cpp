#include "hocloth/manager/cloth/cloth_manager.hpp"

namespace hocloth::mc2 {

Result ClothManager::Initialize()
{
    initialized_ = true;
    return Result::Ok();
}

void ClothManager::Dispose()
{
    initialized_ = false;
}

ManagerStatus ClothManager::Status() const
{
    return ManagerStatus{"ClothManager", initialized_, 0};
}

}  // namespace hocloth::mc2
