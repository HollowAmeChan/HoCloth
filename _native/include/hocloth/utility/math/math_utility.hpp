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
[[nodiscard]] float3 ClampDistance(const float3& from, const float3& to, float max_length);
[[nodiscard]] float Distance(const float3& a, const float3& b);
[[nodiscard]] float Abs(float value);
[[nodiscard]] float CalcMass(float depth);
[[nodiscard]] float CalcInverseMass(float friction);
[[nodiscard]] float CalcInverseMass(float friction, float depth);
[[nodiscard]] float CalcInverseMass(float friction, float depth, bool fixed, float fixed_mass);
[[nodiscard]] quaternion Normalize(const quaternion& value);
[[nodiscard]] quaternion Inverse(const quaternion& value);
[[nodiscard]] quaternion Multiply(const quaternion& a, const quaternion& b);
[[nodiscard]] quaternion AxisAngle(const float3& axis, float angle);
[[nodiscard]] quaternion Slerp(const quaternion& a, const quaternion& b, float t);
[[nodiscard]] quaternion FromToRotation(const float3& from, const float3& to, float t = 1.0f);
[[nodiscard]] quaternion FromToRotation(const quaternion& from, const quaternion& to);
[[nodiscard]] float Angle(const quaternion& a, const quaternion& b);
[[nodiscard]] bool ClampAngle(const float3& direction, const float3& base_direction, float max_angle, float3& out_direction);
void ToAngleAxis(const quaternion& value, float& angle, float3& axis);
void ToNormalTangent(const quaternion& rotation, float3& normal, float3& tangent);
[[nodiscard]] quaternion LookRotation(const float3& forward, const float3& up);
[[nodiscard]] float3 Rotate(const quaternion& rotation, const float3& vector);
[[nodiscard]] float4x4 TRS(const float3& position, const quaternion& rotation, const float3& scale);
[[nodiscard]] float3 TransformPoint(const float3& position, const float4x4& matrix);
[[nodiscard]] float3 TransformPoint(
    const float3& position,
    const float3& world_position,
    const quaternion& world_rotation,
    const float3& world_scale
);
[[nodiscard]] float3 TransformVector(const float3& vector, const float4x4& matrix);
[[nodiscard]] float3 TransformDirection(const float3& direction, const float4x4& matrix);
[[nodiscard]] float TransformDistance(float distance, const float4x4& matrix);
[[nodiscard]] float TransformLength(float length, const float4x4& matrix);
[[nodiscard]] quaternion TransformRotation(
    const quaternion& rotation,
    const float4x4& matrix,
    const float3& normal_tangent_flip
);
[[nodiscard]] float3 InverseTransformPoint(const float3& position, const float4x4& world_to_local_matrix);
[[nodiscard]] float3 InverseTransformPoint(
    const float3& position,
    const float3& world_position,
    const quaternion& world_rotation,
    const float3& world_scale
);
[[nodiscard]] float3 ShiftPosition(
    const float3& old_position,
    const float3& old_pivot_position,
    const float3& shift_vector,
    const quaternion& shift_rotation
);
[[nodiscard]] bool Overlaps(const AABB& a, const AABB& b);
void Expand(AABB& bounds, float signed_distance);
void Encapsulate(AABB& bounds, const float3& point);
void Encapsulate(AABB& bounds, const AABB& other);
[[nodiscard]] float ClosestPtPointSegmentRatio(const float3& point, const float3& a, const float3& b);
[[nodiscard]] float ClosestPtSegmentSegment(
    const float3& p1,
    const float3& q1,
    const float3& p2,
    const float3& q2,
    float& s,
    float& t,
    float3& c1,
    float3& c2
);
void ClosestPtSegmentSegment2(
    const float3& p1,
    const float3& q1,
    const float3& p2,
    const float3& q2,
    float& s,
    float& t
);
[[nodiscard]] float IntersectPointPlaneDist(
    const float3& plane_position,
    const float3& plane_direction,
    const float3& position,
    float3& out_position
);

}  // namespace hocloth::mc2
