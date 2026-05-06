#pragma once

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Math/*.cs
struct float3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] float operator[](int index) const
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }

    [[nodiscard]] float& operator[](int index)
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }
};

struct float2 {
    float x = 0.0f;
    float y = 0.0f;

    [[nodiscard]] float operator[](int index) const
    {
        return index == 0 ? x : y;
    }

    [[nodiscard]] float& operator[](int index)
    {
        return index == 0 ? x : y;
    }
};

struct float4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    [[nodiscard]] float operator[](int index) const
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            return w;
        }
    }

    [[nodiscard]] float& operator[](int index)
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            return w;
        }
    }
};

struct quaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct float4x4 {
    float4 c0{1.0f, 0.0f, 0.0f, 0.0f};
    float4 c1{0.0f, 1.0f, 0.0f, 0.0f};
    float4 c2{0.0f, 0.0f, 1.0f, 0.0f};
    float4 c3{0.0f, 0.0f, 0.0f, 1.0f};
};

struct int2 {
    int x = 0;
    int y = 0;

    [[nodiscard]] int operator[](int index) const
    {
        return index == 0 ? x : y;
    }

    [[nodiscard]] int& operator[](int index)
    {
        return index == 0 ? x : y;
    }
};

struct int3 {
    int x = 0;
    int y = 0;
    int z = 0;

    [[nodiscard]] int operator[](int index) const
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }

    [[nodiscard]] int& operator[](int index)
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }
};

struct int4 {
    int x = 0;
    int y = 0;
    int z = 0;
    int w = 0;

    [[nodiscard]] int operator[](int index) const
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            return w;
        }
    }

    [[nodiscard]] int& operator[](int index)
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            return w;
        }
    }
};

struct AABB {
    float3 min{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    float3 max{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };

    AABB() = default;

    AABB(const float3& min_value, const float3& max_value)
        : min(min_value)
        , max(max_value)
    {
    }

    [[nodiscard]] static AABB CreateFromCenterAndExtents(
        const float3& center,
        const float3& extents
    )
    {
        return CreateFromCenterAndHalfExtents(
            center,
            float3{extents.x * 0.5f, extents.y * 0.5f, extents.z * 0.5f}
        );
    }

    [[nodiscard]] static AABB CreateFromCenterAndHalfExtents(
        const float3& center,
        const float3& half_extents
    )
    {
        return AABB{
            float3{center.x - half_extents.x, center.y - half_extents.y, center.z - half_extents.z},
            float3{center.x + half_extents.x, center.y + half_extents.y, center.z + half_extents.z},
        };
    }

    [[nodiscard]] float3 Extents() const
    {
        return float3{max.x - min.x, max.y - min.y, max.z - min.z};
    }

    [[nodiscard]] float3 HalfExtents() const
    {
        const float3 extents = Extents();
        return float3{extents.x * 0.5f, extents.y * 0.5f, extents.z * 0.5f};
    }

    [[nodiscard]] float3 Center() const
    {
        return float3{
            (max.x + min.x) * 0.5f,
            (max.y + min.y) * 0.5f,
            (max.z + min.z) * 0.5f,
        };
    }

    [[nodiscard]] float MaxSideLength() const
    {
        const float3 extents = Extents();
        return std::max({extents.x, extents.y, extents.z});
    }

    [[nodiscard]] bool IsValid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    [[nodiscard]] float SurfaceArea() const
    {
        const float3 diff = Extents();
        return 2.0f * (diff.x * diff.y + diff.y * diff.z + diff.z * diff.x);
    }

    [[nodiscard]] bool Contains(const float3& point) const
    {
        return point.x >= min.x && point.x <= max.x
            && point.y >= min.y && point.y <= max.y
            && point.z >= min.z && point.z <= max.z;
    }

    [[nodiscard]] bool Contains(const AABB& bounds) const
    {
        return min.x <= bounds.min.x && max.x >= bounds.max.x
            && min.y <= bounds.min.y && max.y >= bounds.max.y
            && min.z <= bounds.min.z && max.z >= bounds.max.z;
    }

    [[nodiscard]] bool Overlaps(const AABB& bounds) const
    {
        return max.x >= bounds.min.x && min.x <= bounds.max.x
            && max.y >= bounds.min.y && min.y <= bounds.max.y
            && max.z >= bounds.min.z && min.z <= bounds.max.z;
    }

    void Expand(float signed_distance)
    {
        min.x -= signed_distance;
        min.y -= signed_distance;
        min.z -= signed_distance;
        max.x += signed_distance;
        max.y += signed_distance;
        max.z += signed_distance;
    }

    void Encapsulate(const float3& point)
    {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }

    void Encapsulate(const AABB& bounds)
    {
        Encapsulate(bounds.min);
        Encapsulate(bounds.max);
    }

    void Transform(const float4x4& matrix)
    {
        const auto transform_point = [&matrix](const float3& point) {
            return float3{
                matrix.c0.x * point.x + matrix.c1.x * point.y + matrix.c2.x * point.z + matrix.c3.x,
                matrix.c0.y * point.x + matrix.c1.y * point.y + matrix.c2.y * point.z + matrix.c3.y,
                matrix.c0.z * point.x + matrix.c1.z * point.y + matrix.c2.z * point.z + matrix.c3.z,
            };
        };
        const float3 transformed_min = transform_point(min);
        const float3 transformed_max = transform_point(max);
        min = float3{
            std::min(transformed_min.x, transformed_max.x),
            std::min(transformed_min.y, transformed_max.y),
            std::min(transformed_min.z, transformed_max.z),
        };
        max = float3{
            std::max(transformed_min.x, transformed_max.x),
            std::max(transformed_min.y, transformed_max.y),
            std::max(transformed_min.z, transformed_max.z),
        };
    }

    [[nodiscard]] std::string ToString() const
    {
        const float3 center = Center();
        const float3 half_extents = HalfExtents();
        std::ostringstream stream;
        stream << "AABB Center:(" << center.x << "," << center.y << "," << center.z << ")"
               << " HalfExtents:(" << half_extents.x << "," << half_extents.y << "," << half_extents.z << ")"
               << " Min:(" << min.x << "," << min.y << "," << min.z << ")"
               << " Max:(" << max.x << "," << max.y << "," << max.z << ")";
        return stream.str();
    }

    friend bool operator==(const AABB& lhs, const AABB& rhs)
    {
        return lhs.min.x == rhs.min.x && lhs.min.y == rhs.min.y && lhs.min.z == rhs.min.z
            && lhs.max.x == rhs.max.x && lhs.max.y == rhs.max.y && lhs.max.z == rhs.max.z;
    }

    friend bool operator!=(const AABB& lhs, const AABB& rhs)
    {
        return !(lhs == rhs);
    }
};

}  // namespace hocloth::mc2
