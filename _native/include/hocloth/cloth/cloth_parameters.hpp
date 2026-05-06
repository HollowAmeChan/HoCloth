#pragma once

#include "hocloth/cloth/cloth_force_mode.hpp"
#include "hocloth/cloth/cloth_normal_axis.hpp"
#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"

namespace hocloth::mc2 {

constexpr float4x4 ConstantCurve(float value)
{
    return float4x4{
        float4{value, value, value, value},
        float4{value, value, value, value},
        float4{value, value, value, value},
        float4{value, value, value, value},
    };
}

// Port target for Magica Cloth 2: Scripts/Core/Cloth/ClothParameters.cs
struct DistanceConstraintParams {
    float4x4 restoration_stiffness{};
    float velocity_attenuation = define::system::DistanceVelocityAttenuation;

    static DistanceConstraintParams BoneSpringDefaults()
    {
        DistanceConstraintParams params;
        params.restoration_stiffness.c0.x = define::system::BoneSpringDistanceStiffness;
        params.restoration_stiffness.c1.y = define::system::BoneSpringDistanceStiffness;
        params.restoration_stiffness.c2.z = define::system::BoneSpringDistanceStiffness;
        params.restoration_stiffness.c3.w = 1.0f;
        params.velocity_attenuation = define::system::DistanceVelocityAttenuation;
        return params;
    }
};

struct InertiaConstraintParams {
    float anchor_inertia = 0.0f;
    float world_inertia = 1.0f;
    float movement_inertia_smoothing = 0.5f;
    float movement_speed_limit = 5.0f;
    float rotation_speed_limit = 720.0f;
    float local_inertia = 1.0f;
    float local_movement_speed_limit = -1.0f;
    float local_rotation_speed_limit = -1.0f;
    float depth_inertia = 0.0f;
    float centrifugal_acceleration = 0.0f;
    float particle_speed_limit = 4.0f;
    int teleport_mode = 0;
    float teleport_distance = 0.5f;
    float teleport_rotation = 90.0f;
};

struct SpringConstraintParams {
    float spring_power = 0.0f;
    float limit_distance = 0.1f;
    float normal_limit_ratio = 1.0f;
    float spring_noise = 0.0f;

    static SpringConstraintParams BoneSpringDefaults()
    {
        SpringConstraintParams params;
        params.spring_power = 0.04f;
        params.limit_distance = 0.1f;
        params.normal_limit_ratio = 1.0f;
        params.spring_noise = 0.0f;
        return params;
    }
};

struct MotionConstraintParams {
    int use_max_distance = 0;
    float stiffness = 1.0f;
    float4x4 max_distance_curve_data = ConstantCurve(0.3f);
    int use_backstop = 0;
    float backstop_radius = 10.0f;
    float4x4 backstop_distance_curve_data = ConstantCurve(0.0f);
};

struct TetherConstraintParams {
    float compression_limit = define::system::BoneSpringTetherCompressionLimit;
    float stretch_limit = define::system::TetherStretchLimit;
};

enum class TriangleBendingMethod {
    None = 0,
    DihedralAngle = 1,
    DirectionDihedralAngle = 2,
};

struct TriangleBendingConstraintParams {
    TriangleBendingMethod method = TriangleBendingMethod::DirectionDihedralAngle;
    float stiffness = 1.0f;
};

struct AngleConstraintParams {
    int use_angle_restoration = 0;
    float4x4 restoration_stiffness = ConstantCurve(0.04f);
    float restoration_velocity_attenuation = 0.8f;
    float restoration_gravity_falloff = 0.0f;
    int use_angle_limit = 0;
    float4x4 limit_curve_data = ConstantCurve(60.0f);
    float limit_stiffness = 1.0f;
};

struct ColliderCollisionConstraintParams {
    ColliderManager::ColliderType mode = ColliderManager::ColliderType::Point;
    float dynamic_friction = define::system::BoneSpringCollisionFriction;
    float static_friction = define::system::BoneSpringCollisionFriction;
    float4x4 limit_distance = ConstantCurve(0.05f);
};

struct ClothParameters {
    int simulation_frequency = define::system::DefaultSimulationFrequency;
    float gravity = 5.0f;
    float3 world_gravity_direction{0.0f, -1.0f, 0.0f};
    float gravity_falloff = 0.0f;
    float stabilization_time_after_reset = 0.1f;
    float blend_weight = 1.0f;
    float4x4 radius_curve_data = ConstantCurve(0.02f);
    float4x4 damping_curve_data = ConstantCurve(0.0f);
    ClothNormalAxis normal_axis = ClothNormalAxis::Up;
    InertiaConstraintParams inertia_constraint;
    SpringConstraintParams spring_constraint;
    MotionConstraintParams motion_constraint;
    TetherConstraintParams tether_constraint;
    TriangleBendingConstraintParams triangle_bending_constraint;
    AngleConstraintParams angle_constraint;
    ColliderCollisionConstraintParams collider_collision_constraint;
    DistanceConstraintParams distance_constraint = DistanceConstraintParams::BoneSpringDefaults();
};

}  // namespace hocloth::mc2
