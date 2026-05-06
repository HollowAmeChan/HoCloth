#pragma once

#include "hocloth/utility/math/math_types.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Math/MathUtility.cs
[[nodiscard]] float Clamp(float value, float low, float high);
[[nodiscard]] float Clamp01(float value);
[[nodiscard]] float Dot(const float3& a, const float3& b);
[[nodiscard]] float Length(const float3& value);
[[nodiscard]] float3 Add(const float3& a, const float3& b);
[[nodiscard]] float3 Subtract(const float3& a, const float3& b);
[[nodiscard]] float3 Scale(const float3& value, float scalar);
[[nodiscard]] float3 Normalize(const float3& value, const float3& fallback = float3{});
void Encapsulate(AABB& bounds, const float3& point);

}  // namespace hocloth::mc2
