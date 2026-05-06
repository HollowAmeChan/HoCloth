#pragma once

#include "hocloth/utility/math/math_types.hpp"

#include <algorithm>
#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Math/IntAABB.cs
struct IntAABB {
    int3 min{};
    int3 max{};

    IntAABB() = default;
    IntAABB(int3 min_, int3 max_)
        : min(min_)
        , max(max_)
    {
    }

    [[nodiscard]] int3 Extents() const
    {
        return int3{max.x - min.x, max.y - min.y, max.z - min.z};
    }

    [[nodiscard]] int3 Center() const
    {
        return int3{(max.x + min.x) / 2, (max.y + min.y) / 2, (max.z + min.z) / 2};
    }

    [[nodiscard]] bool IsValid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    [[nodiscard]] bool Contains(int3 point) const
    {
        return point.x >= min.x && point.y >= min.y && point.z >= min.z
            && point.x <= max.x && point.y <= max.y && point.z <= max.z;
    }

    [[nodiscard]] bool Contains(const IntAABB& aabb) const
    {
        return min.x <= aabb.min.x && min.y <= aabb.min.y && min.z <= aabb.min.z
            && max.x >= aabb.max.x && max.y >= aabb.max.y && max.z >= aabb.max.z;
    }

    [[nodiscard]] bool Overlaps(const IntAABB& aabb) const
    {
        return max.x >= aabb.min.x && max.y >= aabb.min.y && max.z >= aabb.min.z
            && min.x <= aabb.max.x && min.y <= aabb.max.y && min.z <= aabb.max.z;
    }

    void Expand(int signed_distance)
    {
        min.x -= signed_distance;
        min.y -= signed_distance;
        min.z -= signed_distance;
        max.x += signed_distance;
        max.y += signed_distance;
        max.z += signed_distance;
    }

    void Encapsulate(const IntAABB& aabb)
    {
        min.x = std::min(min.x, aabb.min.x);
        min.y = std::min(min.y, aabb.min.y);
        min.z = std::min(min.z, aabb.min.z);
        max.x = std::max(max.x, aabb.max.x);
        max.y = std::max(max.y, aabb.max.y);
        max.z = std::max(max.z, aabb.max.z);
    }

    void Encapsulate(int3 point)
    {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "AABB((" << min.x << "," << min.y << "," << min.z << "), ("
               << max.x << "," << max.y << "," << max.z << "))";
        return stream.str();
    }

    friend bool operator==(const IntAABB& lhs, const IntAABB& rhs)
    {
        return lhs.min.x == rhs.min.x && lhs.min.y == rhs.min.y && lhs.min.z == rhs.min.z
            && lhs.max.x == rhs.max.x && lhs.max.y == rhs.max.y && lhs.max.z == rhs.max.z;
    }

    friend bool operator!=(const IntAABB& lhs, const IntAABB& rhs)
    {
        return !(lhs == rhs);
    }
};

}  // namespace hocloth::mc2
