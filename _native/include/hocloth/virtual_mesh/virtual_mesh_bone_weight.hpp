#pragma once

#include "hocloth/utility/math/math_types.hpp"

#include <array>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VirtualMeshBoneWeight.cs
struct VirtualMeshBoneWeight {
    std::array<int, 4> bone_indices{-1, -1, -1, -1};
    std::array<float, 4> weights{0.0f, 0.0f, 0.0f, 0.0f};

    VirtualMeshBoneWeight() = default;
    VirtualMeshBoneWeight(const int4& bone_indices, const float4& weights);

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] int Count() const;
    void AddWeight(int bone_index, float weight);
    void AddWeight(const VirtualMeshBoneWeight& bone_weight);
    void AdjustWeight();
    [[nodiscard]] std::string ToString() const;
};

}  // namespace hocloth::mc2
