#pragma once

#include "hocloth/core/interface/i_valid.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_primitive.hpp"

#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VirtualMeshRaycastHit.cs
struct VirtualMeshRaycastHit final : public IValid {
    VirtualMeshPrimitive type = VirtualMeshPrimitive::None;
    int index = 0;
    float3 position{};
    float3 normal{};
    float distance = 0.0f;

    [[nodiscard]] int CompareTo(const VirtualMeshRaycastHit& other) const
    {
        if (distance == other.distance) {
            return 0;
        }
        return distance < other.distance ? -1 : 1;
    }

    [[nodiscard]] bool IsValid() const override
    {
        return type != VirtualMeshPrimitive::None;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << static_cast<int>(type) << " [" << index << "] pos:("
               << position.x << "," << position.y << "," << position.z << "), dist:" << distance
               << ", nor:(" << normal.x << "," << normal.y << "," << normal.z << ")";
        return stream.str();
    }
};

}  // namespace hocloth::mc2
