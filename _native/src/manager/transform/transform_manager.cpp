#include "hocloth/manager/transform/transform_manager.hpp"

namespace hocloth::mc2 {

Result TransformManager::Initialize()
{
    initialized_ = true;
    return Result::Ok();
}

void TransformManager::Dispose()
{
    initialized_ = false;
}

ManagerStatus TransformManager::Status() const
{
    return ManagerStatus{"TransformManager", initialized_, 0};
}

}  // namespace hocloth::mc2
