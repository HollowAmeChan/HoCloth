#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"

namespace hocloth::mc2 {

Result VirtualMeshManager::Initialize()
{
    initialized_ = true;
    return Result::Ok();
}

void VirtualMeshManager::Dispose()
{
    initialized_ = false;
}

ManagerStatus VirtualMeshManager::Status() const
{
    return ManagerStatus{"VirtualMeshManager", initialized_, 0};
}

}  // namespace hocloth::mc2
