#include "hocloth/utility/math/math_utility.hpp"

#include "hocloth/core/define/system_define.hpp"

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

float Clamp1(float value)
{
    return Clamp(value, -1.0f, 1.0f);
}

float Dot(const float3& a, const float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float3 Cross(const float3& a, const float3& b)
{
    return float3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float Length(const float3& value)
{
    return std::sqrt(Dot(value, value));
}

float LengthSquared(const float3& value)
{
    return Dot(value, value);
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

float3 Lerp(const float3& a, const float3& b, float t)
{
    return float3{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

float3 Normalize(const float3& value, const float3& fallback)
{
    const float length = Length(value);
    if (length <= 1.0e-8f) {
        return fallback;
    }
    return Scale(value, 1.0f / length);
}

float3 Project(const float3& value, const float3& normal)
{
    const float normal_length_squared = LengthSquared(normal);
    if (normal_length_squared <= define::system::Epsilon) {
        return float3{};
    }
    return Scale(normal, Dot(value, normal) / normal_length_squared);
}

float3 ProjectOnPlane(const float3& value, const float3& normal)
{
    return Subtract(value, Project(value, normal));
}

float3 ClampVector(const float3& value, float max_length)
{
    const float length = Length(value);
    if (length <= max_length || length <= define::system::Epsilon) {
        return value;
    }
    return Scale(value, max_length / length);
}

float3 ClampDistance(const float3& from, const float3& to, float max_length)
{
    const float length = Distance(from, to);
    if (length <= max_length || length <= define::system::Epsilon) {
        return to;
    }
    return Lerp(from, to, max_length / length);
}

float Distance(const float3& a, const float3& b)
{
    return Length(Subtract(a, b));
}

float Abs(float value)
{
    return std::abs(value);
}

float Angle(const float3& a, const float3& b)
{
    const float length_a = Length(a);
    const float length_b = Length(b);
    const float denominator = length_a * length_b;
    if (denominator <= define::system::Epsilon) {
        return 0.0f;
    }
    return std::acos(Clamp1(Dot(a, b) / denominator));
}

float CalcMass(float depth)
{
    const float a = 1.0f - depth;
    return 1.0f + a * a * define::system::DepthMass;
}

float CalcInverseMass(float friction)
{
    const float mass = 1.0f + friction * define::system::FrictionMass;
    if (mass <= define::system::Epsilon) {
        return 0.0f;
    }
    return 1.0f / mass;
}

float CalcInverseMass(float friction, float depth)
{
    float mass = 1.0f;
    mass += friction * define::system::FrictionMass;
    const float a = 1.0f - depth;
    mass += a * a * define::system::DepthMass;
    if (mass <= define::system::Epsilon) {
        return 0.0f;
    }
    return 1.0f / mass;
}

float CalcInverseMass(float friction, float depth, bool fixed, float fixed_mass)
{
    if (fixed) {
        return fixed_mass > define::system::Epsilon ? 1.0f / fixed_mass : 0.0f;
    }
    return CalcInverseMass(friction, depth);
}

float CalcSelfCollisionInverseMass(float friction, bool fixed, float cloth_mass)
{
    float mass = fixed
        ? define::system::SelfCollisionFixedMass
        : 1.0f + friction * define::system::SelfCollisionFrictionMass;
    mass += cloth_mass * define::system::SelfCollisionClothMass;
    if (mass <= define::system::Epsilon) {
        return 0.0f;
    }
    return 1.0f / mass;
}

quaternion Normalize(const quaternion& value)
{
    const float length =
        std::sqrt(value.w * value.w + value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= define::system::Epsilon) {
        return quaternion{};
    }
    const float inv_length = 1.0f / length;
    return quaternion{
        value.w * inv_length,
        value.x * inv_length,
        value.y * inv_length,
        value.z * inv_length,
    };
}

quaternion Inverse(const quaternion& value)
{
    const float length_squared =
        value.w * value.w + value.x * value.x + value.y * value.y + value.z * value.z;
    if (length_squared <= 1.0e-8f) {
        return quaternion{};
    }
    const float inv_length_squared = 1.0f / length_squared;
    return quaternion{
        value.w * inv_length_squared,
        -value.x * inv_length_squared,
        -value.y * inv_length_squared,
        -value.z * inv_length_squared,
    };
}

quaternion Multiply(const quaternion& a, const quaternion& b)
{
    return quaternion{
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

quaternion AxisAngle(const float3& axis, float angle)
{
    const float3 normalized_axis = Normalize(axis, float3{1.0f, 0.0f, 0.0f});
    const float half_angle = angle * 0.5f;
    const float sin_half_angle = std::sin(half_angle);
    return Normalize(quaternion{
        std::cos(half_angle),
        normalized_axis.x * sin_half_angle,
        normalized_axis.y * sin_half_angle,
        normalized_axis.z * sin_half_angle,
    });
}

quaternion Slerp(const quaternion& a, const quaternion& b, float t)
{
    t = Clamp01(t);
    quaternion end = b;
    float dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (dot < 0.0f) {
        dot = -dot;
        end = quaternion{-b.w, -b.x, -b.y, -b.z};
    }
    if (dot > 0.9995f) {
        return Normalize(quaternion{
            a.w + (end.w - a.w) * t,
            a.x + (end.x - a.x) * t,
            a.y + (end.y - a.y) * t,
            a.z + (end.z - a.z) * t,
        });
    }

    const float theta_0 = std::acos(Clamp(dot, -1.0f, 1.0f));
    const float theta = theta_0 * t;
    const float sin_theta = std::sin(theta);
    const float sin_theta_0 = std::sin(theta_0);
    const float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    const float s1 = sin_theta / sin_theta_0;
    return Normalize(quaternion{
        a.w * s0 + end.w * s1,
        a.x * s0 + end.x * s1,
        a.y * s0 + end.y * s1,
        a.z * s0 + end.z * s1,
    });
}

quaternion FromToRotation(const float3& from, const float3& to, float t)
{
    const float3 v1 = Normalize(from);
    const float3 v2 = Normalize(to);
    const float c = Clamp1(Dot(v1, v2));
    float angle = std::acos(c);
    float3 axis = Cross(v1, v2);

    if (std::abs(1.0f + c) < 1.0e-6f) {
        constexpr float pi = 3.14159265358979323846f;
        angle = pi;
        axis = v1.x > v1.y && v1.x > v1.z
            ? Cross(v1, float3{0.0f, 1.0f, 0.0f})
            : Cross(v1, float3{1.0f, 0.0f, 0.0f});
    } else if (std::abs(1.0f - c) < 1.0e-6f) {
        return quaternion{};
    }

    return AxisAngle(Normalize(axis, float3{1.0f, 0.0f, 0.0f}), angle * Clamp01(t));
}

quaternion FromToRotation(const quaternion& from, const quaternion& to)
{
    return Multiply(to, Inverse(from));
}

float Angle(const quaternion& a, const quaternion& b)
{
    constexpr float pi = 3.14159265358979323846f;
    constexpr float two_pi = pi * 2.0f;
    const float dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (std::abs(dot) >= 0.9999f) {
        return 0.0f;
    }
    const float angle = std::acos(Clamp(dot, -1.0f, 1.0f)) * 2.0f;
    return angle > pi ? two_pi - angle : angle;
}

bool ClampAngle(
    const float3& direction,
    const float3& base_direction,
    float max_angle,
    float3& out_direction
)
{
    const float3 v1 = Normalize(direction);
    const float3 v2 = Normalize(base_direction);
    const float c = Clamp1(Dot(v1, v2));
    float angle = std::acos(c);
    if (angle <= max_angle) {
        out_direction = direction;
        return false;
    }

    const float t = (angle - max_angle) / angle;
    float3 axis = Cross(v1, v2);
    if (std::abs(1.0f + c) < 1.0e-6f) {
        constexpr float pi = 3.14159265358979323846f;
        angle = pi;
        axis = v1.x > v1.y && v1.x > v1.z
            ? Cross(v1, float3{0.0f, 1.0f, 0.0f})
            : Cross(v1, float3{1.0f, 0.0f, 0.0f});
    } else if (std::abs(1.0f - c) < 1.0e-6f) {
        out_direction = direction;
        return false;
    }

    const quaternion rotation = AxisAngle(Normalize(axis, float3{1.0f, 0.0f, 0.0f}), angle * t);
    out_direction = Rotate(rotation, direction);
    return true;
}

void ToAngleAxis(const quaternion& value, float& angle, float3& axis)
{
    const quaternion q = Normalize(value);
    const float half_angle = std::abs(q.w) < 0.9999f ? std::acos(q.w) : 0.0f;
    angle = half_angle * 2.0f;
    const float sin_half_angle = std::sin(half_angle);
    if (std::abs(sin_half_angle) < 1.0e-6f) {
        axis = float3{};
        return;
    }
    axis = float3{q.x / sin_half_angle, q.y / sin_half_angle, q.z / sin_half_angle};
}

void ToNormalTangent(const quaternion& rotation, float3& normal, float3& tangent)
{
    normal = Rotate(rotation, float3{0.0f, 1.0f, 0.0f});
    tangent = Rotate(rotation, float3{0.0f, 0.0f, 1.0f});
}

quaternion ToRotation(const float3& normal, const float3& tangent)
{
    return LookRotation(tangent, normal);
}

quaternion LookRotation(const float3& forward, const float3& up)
{
    const float3 f = Normalize(forward, float3{0.0f, 0.0f, 1.0f});
    const float3 u0 = Normalize(up, float3{0.0f, 1.0f, 0.0f});
    float3 r = Normalize(Cross(u0, f), float3{1.0f, 0.0f, 0.0f});
    float3 u = Cross(f, r);

    const float m00 = r.x;
    const float m01 = u.x;
    const float m02 = f.x;
    const float m10 = r.y;
    const float m11 = u.y;
    const float m12 = f.y;
    const float m20 = r.z;
    const float m21 = u.z;
    const float m22 = f.z;
    const float trace = m00 + m11 + m22;

    quaternion q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }

    return Normalize(q);
}

float3 Rotate(const quaternion& rotation, const float3& vector)
{
    const quaternion q = Normalize(rotation);
    const quaternion v{0.0f, vector.x, vector.y, vector.z};
    const quaternion result = Multiply(Multiply(q, v), Inverse(q));
    return float3{result.x, result.y, result.z};
}

float4x4 TRS(const float3& position, const quaternion& rotation, const float3& scale)
{
    const float x = rotation.x;
    const float y = rotation.y;
    const float z = rotation.z;
    const float w = rotation.w;

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    float4x4 matrix;
    matrix.c0 = float4{
        (1.0f - 2.0f * (yy + zz)) * scale.x,
        (2.0f * (xy + wz)) * scale.x,
        (2.0f * (xz - wy)) * scale.x,
        0.0f,
    };
    matrix.c1 = float4{
        (2.0f * (xy - wz)) * scale.y,
        (1.0f - 2.0f * (xx + zz)) * scale.y,
        (2.0f * (yz + wx)) * scale.y,
        0.0f,
    };
    matrix.c2 = float4{
        (2.0f * (xz + wy)) * scale.z,
        (2.0f * (yz - wx)) * scale.z,
        (1.0f - 2.0f * (xx + yy)) * scale.z,
        0.0f,
    };
    matrix.c3 = float4{position.x, position.y, position.z, 1.0f};
    return matrix;
}

float4x4 Multiply(const float4x4& a, const float4x4& b)
{
    auto transform_column = [&a](const float4& c) {
        return float4{
            a.c0.x * c.x + a.c1.x * c.y + a.c2.x * c.z + a.c3.x * c.w,
            a.c0.y * c.x + a.c1.y * c.y + a.c2.y * c.z + a.c3.y * c.w,
            a.c0.z * c.x + a.c1.z * c.y + a.c2.z * c.z + a.c3.z * c.w,
            a.c0.w * c.x + a.c1.w * c.y + a.c2.w * c.z + a.c3.w * c.w,
        };
    };

    return float4x4{
        transform_column(b.c0),
        transform_column(b.c1),
        transform_column(b.c2),
        transform_column(b.c3),
    };
}

float4x4 InverseAffine(const float4x4& matrix)
{
    const float a00 = matrix.c0.x;
    const float a01 = matrix.c1.x;
    const float a02 = matrix.c2.x;
    const float a10 = matrix.c0.y;
    const float a11 = matrix.c1.y;
    const float a12 = matrix.c2.y;
    const float a20 = matrix.c0.z;
    const float a21 = matrix.c1.z;
    const float a22 = matrix.c2.z;

    const float b01 = a22 * a11 - a12 * a21;
    const float b11 = -a22 * a10 + a12 * a20;
    const float b21 = a21 * a10 - a11 * a20;
    float det = a00 * b01 + a01 * b11 + a02 * b21;
    if (std::abs(det) <= define::system::Epsilon) {
        return float4x4{};
    }
    det = 1.0f / det;

    float4x4 inverse;
    inverse.c0 = float4{
        b01 * det,
        (-a22 * a01 + a02 * a21) * det,
        (a12 * a01 - a02 * a11) * det,
        0.0f,
    };
    inverse.c1 = float4{
        b11 * det,
        (a22 * a00 - a02 * a20) * det,
        (-a12 * a00 + a02 * a10) * det,
        0.0f,
    };
    inverse.c2 = float4{
        b21 * det,
        (-a21 * a00 + a01 * a20) * det,
        (a11 * a00 - a01 * a10) * det,
        0.0f,
    };

    const float3 translation{matrix.c3.x, matrix.c3.y, matrix.c3.z};
    const float3 inverse_translation = TransformVector(Scale(translation, -1.0f), inverse);
    inverse.c3 = float4{
        inverse_translation.x,
        inverse_translation.y,
        inverse_translation.z,
        1.0f,
    };
    return inverse;
}

float3 TransformPoint(const float3& position, const float4x4& matrix)
{
    return float3{
        matrix.c0.x * position.x + matrix.c1.x * position.y + matrix.c2.x * position.z + matrix.c3.x,
        matrix.c0.y * position.x + matrix.c1.y * position.y + matrix.c2.y * position.z + matrix.c3.y,
        matrix.c0.z * position.x + matrix.c1.z * position.y + matrix.c2.z * position.z + matrix.c3.z,
    };
}

float3 TransformPoint(
    const float3& position,
    const float3& world_position,
    const quaternion& world_rotation,
    const float3& world_scale
)
{
    return TransformPoint(position, TRS(world_position, world_rotation, world_scale));
}

float3 TransformVector(const float3& vector, const float4x4& matrix)
{
    return float3{
        matrix.c0.x * vector.x + matrix.c1.x * vector.y + matrix.c2.x * vector.z,
        matrix.c0.y * vector.x + matrix.c1.y * vector.y + matrix.c2.y * vector.z,
        matrix.c0.z * vector.x + matrix.c1.z * vector.y + matrix.c2.z * vector.z,
    };
}

float3 TransformDirection(const float3& direction, const float4x4& matrix)
{
    const float length = Length(direction);
    if (length <= define::system::Epsilon) {
        return direction;
    }
    return Scale(Normalize(TransformVector(direction, matrix)), length);
}

float TransformDistance(float distance, const float4x4& matrix)
{
    const float3 transformed = TransformVector(float3{distance, distance, distance}, matrix);
    return (transformed.x + transformed.y + transformed.z) / 3.0f;
}

float TransformLength(float length, const float4x4& matrix)
{
    constexpr float inv_sqrt_three = 0.5773502691896258f;
    return Length(TransformVector(float3{length, length, length}, matrix)) * inv_sqrt_three;
}

quaternion TransformRotation(
    const quaternion& rotation,
    const float4x4& matrix,
    const float3& normal_tangent_flip
)
{
    float3 normal;
    float3 tangent;
    ToNormalTangent(rotation, normal, tangent);
    normal = Scale(TransformVector(normal, matrix), normal_tangent_flip.y);
    tangent = Scale(TransformVector(tangent, matrix), normal_tangent_flip.z);
    return LookRotation(tangent, normal);
}

float3 InverseTransformPoint(const float3& position, const float4x4& world_to_local_matrix)
{
    return TransformPoint(position, world_to_local_matrix);
}

float3 InverseTransformPoint(
    const float3& position,
    const float3& world_position,
    const quaternion& world_rotation,
    const float3& world_scale
)
{
    const float3 local = Rotate(Inverse(world_rotation), Subtract(position, world_position));
    return float3{
        std::abs(world_scale.x) > define::system::Epsilon ? local.x / world_scale.x : 0.0f,
        std::abs(world_scale.y) > define::system::Epsilon ? local.y / world_scale.y : 0.0f,
        std::abs(world_scale.z) > define::system::Epsilon ? local.z / world_scale.z : 0.0f,
    };
}

float3 ShiftPosition(
    const float3& old_position,
    const float3& old_pivot_position,
    const float3& shift_vector,
    const quaternion& shift_rotation
)
{
    const float3 local_position = Subtract(old_position, old_pivot_position);
    return Add(
        old_pivot_position,
        Add(Rotate(shift_rotation, local_position), shift_vector)
    );
}

bool Overlaps(const AABB& a, const AABB& b)
{
    return a.max.x >= b.min.x && a.min.x <= b.max.x
        && a.max.y >= b.min.y && a.min.y <= b.max.y
        && a.max.z >= b.min.z && a.min.z <= b.max.z;
}

void Expand(AABB& bounds, float signed_distance)
{
    bounds.min.x -= signed_distance;
    bounds.min.y -= signed_distance;
    bounds.min.z -= signed_distance;
    bounds.max.x += signed_distance;
    bounds.max.y += signed_distance;
    bounds.max.z += signed_distance;
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

void Encapsulate(AABB& bounds, const AABB& other)
{
    Encapsulate(bounds, other.min);
    Encapsulate(bounds, other.max);
}

float ClosestPtPointSegmentRatio(const float3& point, const float3& a, const float3& b)
{
    const float3 ab = Subtract(b, a);
    const float denominator = Dot(ab, ab);
    if (denominator <= define::system::Epsilon) {
        return 0.0f;
    }
    return Clamp01(Dot(Subtract(point, a), ab) / denominator);
}

float ClosestPtSegmentSegment(
    const float3& p1,
    const float3& q1,
    const float3& p2,
    const float3& q2,
    float& s,
    float& t,
    float3& c1,
    float3& c2
)
{
    const float3 d1 = Subtract(q1, p1);
    const float3 d2 = Subtract(q2, p2);
    const float3 r = Subtract(p1, p2);
    const float a = Dot(d1, d1);
    const float e = Dot(d2, d2);
    const float f = Dot(d2, r);

    if (a <= 1.0e-8f && e <= 1.0e-8f) {
        s = 0.0f;
        t = 0.0f;
        c1 = p1;
        c2 = p2;
        return LengthSquared(Subtract(c1, c2));
    }

    if (a <= 1.0e-8f) {
        s = 0.0f;
        t = Clamp01(f / e);
    } else {
        const float c = Dot(d1, r);
        if (e <= 1.0e-8f) {
            t = 0.0f;
            s = Clamp01(-c / a);
        } else {
            const float b = Dot(d1, d2);
            const float denom = a * e - b * b;
            s = denom != 0.0f ? Clamp01((b * f - c * e) / denom) : 0.0f;

            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = Clamp01(-c / a);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = Clamp01((b - c) / a);
            }
        }
    }

    c1 = Add(p1, Scale(d1, s));
    c2 = Add(p2, Scale(d2, t));
    return LengthSquared(Subtract(c1, c2));
}

void ClosestPtSegmentSegment2(
    const float3& p1,
    const float3& q1,
    const float3& p2,
    const float3& q2,
    float& s,
    float& t
)
{
    const float3 d1 = Subtract(q1, p1);
    const float3 d2 = Subtract(q2, p2);
    const float3 r = Subtract(p1, p2);
    const float a = Dot(d1, d1);
    const float e = Dot(d2, d2);
    const float f = Dot(d2, r);

    if (a <= 1.0e-8f && e <= 1.0e-8f) {
        s = 0.0f;
        t = 0.0f;
    } else if (a <= 1.0e-8f) {
        s = 0.0f;
        t = Clamp01(f / e);
    } else {
        const float c = Dot(d1, r);
        if (e <= 1.0e-8f) {
            t = 0.0f;
            s = Clamp01(-c / a);
        } else {
            const float b = Dot(d1, d2);
            const float denom = a * e - b * b;
            s = denom != 0.0f ? Clamp01((b * f - c * e) / denom) : 0.0f;

            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = Clamp01(-c / a);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = Clamp01((b - c) / a);
            }
        }
    }
}

float3 ClosestPtPointTriangle(
    const float3& point,
    const float3& a,
    const float3& b,
    const float3& c,
    float3& uvw
)
{
    uvw = float3{};
    float v = 0.0f;
    float w = 0.0f;

    const float3 ab = Subtract(b, a);
    const float3 ac = Subtract(c, a);
    const float3 ap = Subtract(point, a);
    const float d1 = Dot(ab, ap);
    const float d2 = Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        uvw.x = 1.0f;
        return a;
    }

    const float3 bp = Subtract(point, b);
    const float d3 = Dot(ab, bp);
    const float d4 = Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        uvw.y = 1.0f;
        return b;
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float denominator = d1 - d3;
        v = std::abs(denominator) > define::system::Epsilon ? d1 / denominator : 0.0f;
        uvw = float3{1.0f - v, v, 0.0f};
        return Add(a, Scale(ab, v));
    }

    const float3 cp = Subtract(point, c);
    const float d5 = Dot(ab, cp);
    const float d6 = Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        uvw.z = 1.0f;
        return c;
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float denominator = d2 - d6;
        w = std::abs(denominator) > define::system::Epsilon ? d2 / denominator : 0.0f;
        uvw = float3{1.0f - w, 0.0f, w};
        return Add(a, Scale(ac, w));
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float denominator = (d4 - d3) + (d5 - d6);
        w = std::abs(denominator) > define::system::Epsilon
            ? (d4 - d3) / denominator
            : 0.0f;
        uvw = float3{0.0f, 1.0f - w, w};
        return Add(b, Scale(Subtract(c, b), w));
    }

    const float h = va + vb + vc;
    const float denominator = std::abs(h) > define::system::Epsilon ? 1.0f / h : 0.0f;
    v = vb * denominator;
    w = vc * denominator;
    uvw = float3{1.0f - v - w, v, w};
    return Add(Add(a, Scale(ab, v)), Scale(ac, w));
}

bool PointInTriangleUVW(const float3& uvw)
{
    return uvw.x != 0.0f && uvw.y != 0.0f && uvw.z != 0.0f;
}

float3 TriangleCenter(const float3& p0, const float3& p1, const float3& p2)
{
    return Scale(Add(Add(p0, p1), p2), 1.0f / 3.0f);
}

float3 TriangleNormal(const float3& p0, const float3& p1, const float3& p2)
{
    return Normalize(Cross(Subtract(p1, p0), Subtract(p2, p0)));
}

float TriangleArea(const float3& p0, const float3& p1, const float3& p2)
{
    return Length(Cross(Subtract(p1, p0), Subtract(p2, p0)));
}

bool IsSafeTriangle(const float3& p0, const float3& p1, const float3& p2)
{
    return TriangleArea(p0, p1, p2) > 1.0e-6f;
}

float3 TriangleTangent(
    const float3& p0,
    const float3& p1,
    const float3& p2,
    const float2& uv0,
    const float2& uv1,
    const float2& uv2
)
{
    const float3 dist_ba = Subtract(p1, p0);
    const float3 dist_ca = Subtract(p2, p0);
    const float2 tdist_ba{uv1.x - uv0.x, uv1.y - uv0.y};
    const float2 tdist_ca{uv2.x - uv0.x, uv2.y - uv0.y};
    float area = tdist_ba.x * tdist_ca.y - tdist_ba.y * tdist_ca.x;
    if (area == 0.0f) {
        area = 1.0f;
    }

    const float delta = 1.0f / area;
    const float3 tangent{
        -((dist_ba.x * tdist_ca.y) + (dist_ca.x * -tdist_ba.y)) * delta,
        -((dist_ba.y * tdist_ca.y) + (dist_ca.y * -tdist_ba.y)) * delta,
        -((dist_ba.z * tdist_ca.y) + (dist_ca.z * -tdist_ba.y)) * delta,
    };
    return Normalize(tangent);
}

quaternion TriangleRotation(const float3& p0, const float3& p1, const float3& p2)
{
    const float3 normal = TriangleNormal(p0, p1, p2);
    const float3 center = TriangleCenter(p0, p1, p2);
    const float3 tangent = Normalize(Subtract(p0, center), float3{0.0f, 0.0f, 1.0f});
    return LookRotation(tangent, normal);
}

quaternion TriangleCenterRotation(
    const float3& p0,
    const float3& p1,
    const float3& p2,
    const float3& p3
)
{
    const float3 n0 = TriangleNormal(p0, p2, p3);
    const float3 n1 = TriangleNormal(p1, p3, p2);
    const float3 normal = Scale(Add(n0, n1), 0.5f);
    const float3 tangent = Normalize(Subtract(p3, p2), float3{0.0f, 0.0f, 1.0f});
    return LookRotation(tangent, normal);
}

float TriangleAngle(
    const float3& v0,
    const float3& v1,
    const float3& v2,
    const float3& v3
)
{
    const float3 edge = Subtract(v1, v0);
    const float3 va = Subtract(v2, v0);
    const float3 vb = Subtract(v3, v0);
    const float3 na = Cross(va, edge);
    const float3 nb = Cross(edge, vb);
    return Angle(na, nb);
}

float DistanceTriangleCenter(
    const float3& point,
    const float3& p0,
    const float3& p1,
    const float3& p2
)
{
    return Distance(point, TriangleCenter(p0, p1, p2));
}

float DirectionPointTriangle(
    const float3& point,
    const float3& a,
    const float3& b,
    const float3& c
)
{
    const float3 ab = Subtract(b, a);
    const float3 ac = Subtract(c, a);
    const float3 ap = Subtract(point, a);
    const float d = Dot(ap, Cross(ab, ac));
    if (d > 0.0f) {
        return 1.0f;
    }
    if (d < 0.0f) {
        return -1.0f;
    }
    return 0.0f;
}

int2 GetRestTriangleVertex(const int3& tri1, const int3& tri2, const int2& edge)
{
    int2 result{-1, -1};
    const int values1[3] = {tri1.x, tri1.y, tri1.z};
    const int values2[3] = {tri2.x, tri2.y, tri2.z};
    for (int value : values1) {
        if (value != edge.x && value != edge.y) {
            result.x = value;
            break;
        }
    }
    for (int value : values2) {
        if (value != edge.x && value != edge.y) {
            result.y = value;
            break;
        }
    }
    return result;
}

int2 GetCommonEdgeFromTrianglePair(const int3& tri1, const int3& tri2)
{
    int2 edge{};
    int edge_index = 0;
    const int values1[3] = {tri1.x, tri1.y, tri1.z};
    const int values2[3] = {tri2.x, tri2.y, tri2.z};
    for (int value1 : values1) {
        for (int value2 : values2) {
            if (value1 == value2 && edge_index < 2) {
                if (edge_index == 0) {
                    edge.x = value1;
                } else {
                    edge.y = value1;
                }
                ++edge_index;
                break;
            }
        }
    }
    if (edge_index != 2) {
        return int2{};
    }
    if (edge.x > edge.y) {
        std::swap(edge.x, edge.y);
    }
    return edge;
}

int4 GetTrianglePairIndices(const int3& tri1, const int3& tri2)
{
    const int2 edge = GetCommonEdgeFromTrianglePair(tri1, tri2);
    int4 result{0, 0, edge.x, edge.y};
    const int values1[3] = {tri1.x, tri1.y, tri1.z};
    const int values2[3] = {tri2.x, tri2.y, tri2.z};
    for (int value : values1) {
        if (value != edge.x && value != edge.y) {
            result.x = value;
        }
    }
    for (int value : values2) {
        if (value != edge.x && value != edge.y) {
            result.y = value;
        }
    }
    return result;
}

int GetUnuseTriangleIndex(const int3& tri, const int2& edge)
{
    if (tri.x != edge.x && tri.x != edge.y) {
        return tri.x;
    }
    if (tri.y != edge.x && tri.y != edge.y) {
        return tri.y;
    }
    if (tri.z != edge.x && tri.z != edge.y) {
        return tri.z;
    }
    return -1;
}

float GetTrianglePairAngle(
    const float3& pos0,
    const float3& pos1,
    const float3& pos2,
    const float3& pos3
)
{
    const float3 va = Subtract(pos3, pos2);
    const float3 vb = Subtract(pos0, pos2);
    const float3 vc = Subtract(pos1, pos2);
    const float3 n0 = Cross(va, vb);
    const float3 n1 = Cross(vc, va);
    return Angle(n0, n1);
}

int3 FlipTriangle(const int3& tri)
{
    return int3{tri.x, tri.z, tri.y};
}

void GetTriangleSphere(
    const float3& pos0,
    const float3& pos1,
    const float3& pos2,
    float3& sphere_center,
    float& sphere_radius
)
{
    const float3 max_value{
        std::max({pos0.x, pos1.x, pos2.x}),
        std::max({pos0.y, pos1.y, pos2.y}),
        std::max({pos0.z, pos1.z, pos2.z}),
    };
    const float3 min_value{
        std::min({pos0.x, pos1.x, pos2.x}),
        std::min({pos0.y, pos1.y, pos2.y}),
        std::min({pos0.z, pos1.z, pos2.z}),
    };
    sphere_center = Scale(Add(min_value, max_value), 0.5f);
    sphere_radius = Distance(min_value, max_value) * 0.5f;
}

bool IntersectSegmentTriangle(
    const float3& p,
    const float3& q,
    float3 a,
    float3 b,
    float3 c,
    bool double_side,
    float& u,
    float& v,
    float& w,
    float& t
)
{
    t = 0.0f;
    u = 0.0f;
    v = 0.0f;
    w = 0.0f;

    float3 ab = Subtract(b, a);
    float3 ac = Subtract(c, a);
    const float3 qp = Subtract(p, q);
    float3 n = Cross(ab, ac);
    float d = Dot(qp, n);

    if (std::abs(d) < 1.0e-9f) {
        return false;
    }
    if (!double_side) {
        if (d <= 0.0f) {
            return false;
        }
    } else if (d < 0.0f) {
        n = Scale(n, -1.0f);
        const float3 swap = b;
        b = c;
        c = swap;
        ab = Subtract(b, a);
        ac = Subtract(c, a);
        d = Dot(qp, n);
    }

    const float3 ap = Subtract(p, a);
    t = Dot(ap, n);
    if (t < 0.0f || t > d) {
        return false;
    }

    const float3 e = Cross(qp, ap);
    v = Dot(ac, e);
    if (v < 0.0f || v > d) {
        return false;
    }
    w = -Dot(ab, e);
    if (w < 0.0f || v + w > d) {
        return false;
    }

    const float inv_d = 1.0f / d;
    t *= inv_d;
    v *= inv_d;
    w *= inv_d;
    u = 1.0f - v - w;
    return true;
}

bool IntersectSegmentTriangle(
    const float3& p,
    const float3& q,
    float3 a,
    float3 b,
    float3 c
)
{
    float u = 0.0f;
    float v = 0.0f;
    float w = 0.0f;
    float t = 0.0f;
    return IntersectSegmentTriangle(p, q, a, b, c, true, u, v, w, t);
}

bool IntersectRaySphere(
    const float3& position,
    const float3& direction,
    const float3& sphere_center,
    float sphere_radius,
    float& t,
    float3& q
)
{
    const float3 m = Subtract(position, sphere_center);
    const float b = Dot(m, direction);
    const float c = Dot(m, m) - sphere_radius * sphere_radius;
    if (c > 0.0f && b > 0.0f) {
        return false;
    }

    const float discriminant = b * b - c;
    if (discriminant < 0.0f) {
        return false;
    }

    t = -b - std::sqrt(discriminant);
    if (t < 0.0f) {
        t = 0.0f;
    }
    q = Add(position, Scale(direction, t));
    return true;
}

float IntersectPointPlaneDist(
    const float3& plane_position,
    const float3& plane_direction,
    const float3& position,
    float3& out_position
)
{
    const float3 v = Subtract(position, plane_position);
    const float3 projected = Project(v, plane_direction);
    const float length = Length(projected);
    if (Dot(plane_direction, v) < 0.0f) {
        out_position = Subtract(position, projected);
        return -length;
    }

    out_position = position;
    return length;
}

}  // namespace hocloth::mc2
