#pragma once

#include "hocloth/utility/math/math_types.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Math/MathUtility.cs
[[nodiscard]] float Clamp(float value, float low, float high);
[[nodiscard]] float Clamp01(float value);
[[nodiscard]] float Clamp1(float value);
[[nodiscard]] float Dot(const float3& a, const float3& b);
[[nodiscard]] float3 Cross(const float3& a, const float3& b);
[[nodiscard]] float Length(const float3& value);
[[nodiscard]] float LengthSquared(const float3& value);
[[nodiscard]] float3 Add(const float3& a, const float3& b);
[[nodiscard]] float3 Subtract(const float3& a, const float3& b);
[[nodiscard]] float3 Scale(const float3& value, float scalar);
[[nodiscard]] float3 Lerp(const float3& a, const float3& b, float t);
[[nodiscard]] float3 Normalize(const float3& value, const float3& fallback = float3{});
[[nodiscard]] float3 Project(const float3& value, const float3& normal);
[[nodiscard]] float3 ProjectOnPlane(const float3& value, const float3& normal);
[[nodiscard]] float3 ClampVector(const float3& value, float max_length);
[[nodiscard]] float Distance(const float3& a, const float3& b);
[[nodiscard]] float Abs(float value);
[[nodiscard]] float CalcMass(float depth);
[[nodiscard]] float CalcInverseMass(float friction);
[[nodiscard]] float CalcInverseMass(float friction, float depth);
[[nodiscard]] float CalcInverseMass(float friction, float depth, bool fixed, float fixed_mass);
[[nodiscard]] quaternion Normalize(const quaternion& value);
[[nodiscard]] quaternion Inverse(const quaternion& value);
[[nodiscard]] quaternion Multiply(const quaternion& a, const quaternion& b);
[[nodiscard]] quaternion Slerp(const quaternion& a, const quaternion& b, float t);
[[nodiscard]] quaternion FromToRotation(const quaternion& from, const quaternion& to);
[[nodiscard]] float Angle(const quaternion& a, const quaternion& b);
void ToAngleAxis(const quaternion& value, float& angle, float3& axis);
[[nodiscard]] float3 Rotate(const quaternion& rotation, const float3& vector);
[[nodiscard]] float4x4 TRS(const float3& position, const quaternion& rotation, const float3& scale);
void Encapsulate(AABB& bounds, const float3& point);

}  // namespace hocloth::mc2
