#pragma once

#include "hocloth/utility/math/math_utility.hpp"
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

    [[nodiscard]] VirtualMeshTransform Clone() const
    {
        return *this;
    }

    [[nodiscard]] static VirtualMeshTransform Origin();

    void Update(const float4x4& local_to_world, const float4x4& world_to_local)
    {
        local_to_world_matrix = local_to_world;
        world_to_local_matrix = world_to_local;
    }

    [[nodiscard]] float3 TransformPoint(const float3& position) const
    {
        return mc2::TransformPoint(position, local_to_world_matrix);
    }

    [[nodiscard]] float3 TransformVector(const float3& vector) const
    {
        return mc2::TransformVector(vector, local_to_world_matrix);
    }

    [[nodiscard]] float3 TransformDirection(const float3& direction) const
    {
        return mc2::TransformDirection(direction, local_to_world_matrix);
    }

    [[nodiscard]] float3 InverseTransformPoint(const float3& position) const
    {
        return mc2::InverseTransformPoint(position, world_to_local_matrix);
    }

    [[nodiscard]] float3 InverseTransformVector(const float3& vector) const
    {
        return mc2::InverseTransformVector(vector, world_to_local_matrix);
    }

    [[nodiscard]] float3 InverseTransformDirection(const float3& direction) const
    {
        return mc2::InverseTransformDirection(direction, world_to_local_matrix);
    }

    [[nodiscard]] quaternion InverseTransformRotation(const quaternion& rotation) const
    {
        return mc2::TransformRotation(rotation, world_to_local_matrix, float3{1.0f, 1.0f, 1.0f});
    }

    [[nodiscard]] VirtualMeshTransform Transform(const VirtualMeshTransform& to) const
    {
        VirtualMeshTransform transform;
        transform.name = "__(temporary)__";
        transform.index = -1;
        transform.local_to_world_matrix =
            mc2::Transform(local_to_world_matrix, to.world_to_local_matrix);
        transform.world_to_local_matrix =
            mc2::Transform(to.local_to_world_matrix, world_to_local_matrix);
        transform.parent_index = -1;
        return transform;
    }
};

}  // namespace hocloth::mc2
