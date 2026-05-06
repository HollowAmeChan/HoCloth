#pragma once

#include "hocloth/utility/math/math_types.hpp"

#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VirtualMeshTransform.cs
struct VirtualMeshTransform {
    std::string name;
    int index = -1;
    float4x4 local_to_world_matrix{};
    float4x4 world_to_local_matrix{};
    int parent_index = -1;

    [[nodiscard]] static VirtualMeshTransform Origin();
};

}  // namespace hocloth::mc2
