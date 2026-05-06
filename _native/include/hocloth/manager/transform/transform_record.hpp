#pragma once

#include "hocloth/utility/math/math_types.hpp"

#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/TransformManager/TransformRecord.cs
struct TransformRecord {
    int id = 0;
    int parent_id = 0;
    std::string name;

    float3 local_position{};
    quaternion local_rotation{};
    float3 position{};
    quaternion rotation{};
    float3 scale{1.0f, 1.0f, 1.0f};
    float4x4 local_to_world_matrix{};
    float4x4 world_to_local_matrix{};

    [[nodiscard]] bool IsValid() const
    {
        return id != 0;
    }

    [[nodiscard]] float3 InverseTransformDirection(const float3& direction) const;
};

}  // namespace hocloth::mc2
