#pragma once

#include <array>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VirtualMeshBoneWeight.cs
struct VirtualMeshBoneWeight {
    std::array<int, 4> bone_indices{-1, -1, -1, -1};
    std::array<float, 4> weights{0.0f, 0.0f, 0.0f, 0.0f};
};

}  // namespace hocloth::mc2
