#include "hocloth_runtime_api.hpp"

#include <algorithm>

namespace hocloth {

namespace {

float Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Sub(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Scale(const Vec3& value, float scalar)
{
    return Vec3{value.x * scalar, value.y * scalar, value.z * scalar};
}

}  // namespace

Vec3 ClosestPointOnSegment(const Vec3& point, const Vec3& a, const Vec3& b)
{
    const Vec3 ab = Sub(b, a);
    const float ab_len_sq = Dot(ab, ab);
    if (ab_len_sq <= 1.0e-8f) {
        return a;
    }

    const float t = std::clamp(Dot(Sub(point, a), ab) / ab_len_sq, 0.0f, 1.0f);
    return Add(a, Scale(ab, t));
}

}  // namespace hocloth
