#pragma once

#include "hocloth/cloth/cloth_parameters.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <cmath>

namespace hocloth::mc2 {

enum class WindZoneMode {
    GlobalDirection = 0,
    SphereDirection = 1,
    BoxDirection = 2,
    SphereRadial = 10,
};

// Data-form native port of Magica Cloth 2: Scripts/Core/Cloth/Wind/MagicaWindZone.cs.
struct MagicaWindZone {
    WindZoneMode mode = WindZoneMode::GlobalDirection;
    float3 size{10.0f, 10.0f, 10.0f};
    float radius = 10.0f;
    float main = 5.0f;
    float turbulence = 1.0f;
    float direction_angle_x = 0.0f;
    float direction_angle_y = 0.0f;
    float4x4 attenuation = ConstantCurve(1.0f);
    bool is_addition = false;

    float3 world_position{};
    quaternion world_rotation{};
    float3 world_scale{1.0f, 1.0f, 1.0f};
    float4x4 world_to_local_matrix{};

    [[nodiscard]] bool IsDirection() const
    {
        return mode == WindZoneMode::GlobalDirection
            || mode == WindZoneMode::SphereDirection
            || mode == WindZoneMode::BoxDirection;
    }

    [[nodiscard]] bool IsRadial() const
    {
        return mode == WindZoneMode::SphereRadial;
    }

    [[nodiscard]] bool IsAddition() const
    {
        return is_addition;
    }

    [[nodiscard]] float3 GetWindDirection(bool local_space = false) const
    {
        constexpr float degrees_to_radians = 0.017453292519943295f;
        const quaternion x_rotation =
            AxisAngle(float3{1.0f, 0.0f, 0.0f}, direction_angle_x * degrees_to_radians);
        const quaternion y_rotation =
            AxisAngle(float3{0.0f, 1.0f, 0.0f}, direction_angle_y * degrees_to_radians);
        const float3 local_direction =
            Normalize(Rotate(Multiply(y_rotation, x_rotation), float3{0.0f, 0.0f, 1.0f}));
        return local_space ? local_direction : Rotate(world_rotation, local_direction);
    }

    void SetWindDirection(const float3& direction, bool local_space = false)
    {
        constexpr float radians_to_degrees = 57.29577951308232f;
        const float3 local_direction =
            Normalize(local_space ? direction : Rotate(Inverse(world_rotation), direction));
        direction_angle_y = std::atan2(local_direction.x, local_direction.z) * radians_to_degrees;
        const float xz_length = Length(float3{local_direction.x, 0.0f, local_direction.z});
        direction_angle_x = std::atan2(-local_direction.y, xz_length) * radians_to_degrees;
    }
};

}  // namespace hocloth::mc2
