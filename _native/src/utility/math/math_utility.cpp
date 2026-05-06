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

float Distance(const float3& a, const float3& b)
{
    return Length(Subtract(a, b));
}

float Abs(float value)
{
    return std::abs(value);
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
