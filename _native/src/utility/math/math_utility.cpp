#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cmath>

namespace hocloth::mc2 {

float Clamp(float value, float low, float high)
{
    return std::clamp(value, low, high);
}

float Clamp01(float value)
{
    return Clamp(value, 0.0f, 1.0f);
}

float Dot(const float3& a, const float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length(const float3& value)
{
    return std::sqrt(Dot(value, value));
}

float3 Add(const float3& a, const float3& b)
{
    return float3{a.x + b.x, a.y + b.y, a.z + b.z};
}

float3 Subtract(const float3& a, const float3& b)
{
    return float3{a.x - b.x, a.y - b.y, a.z - b.z};
}

float3 Scale(const float3& value, float scalar)
{
    return float3{value.x * scalar, value.y * scalar, value.z * scalar};
}

float3 Normalize(const float3& value, const float3& fallback)
{
    const float length = Length(value);
    if (length <= 1.0e-8f) {
        return fallback;
    }
    return Scale(value, 1.0f / length);
}

void Encapsulate(AABB& bounds, const float3& point)
{
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.min.z = std::min(bounds.min.z, point.z);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    bounds.max.z = std::max(bounds.max.z, point.z);
}

}  // namespace hocloth::mc2
