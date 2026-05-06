#include "hocloth/virtual_mesh/virtual_mesh_transform.hpp"

namespace hocloth::mc2 {

VirtualMeshTransform VirtualMeshTransform::Origin()
{
    VirtualMeshTransform transform;
    transform.name = "VirtualMesh Origin";
    transform.index = -1;
    transform.parent_index = -1;
    return transform;
}

}  // namespace hocloth::mc2
