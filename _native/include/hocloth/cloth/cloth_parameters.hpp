#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/utility/math/math_types.hpp"

namespace hocloth::mc2 {

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

struct ClothParameters {
    int simulation_frequency = define::system::DefaultSimulationFrequency;
    float3 world_gravity_direction{0.0f, -1.0f, 0.0f};
    InertiaConstraintParams inertia_constraint;
    DistanceConstraintParams distance_constraint = DistanceConstraintParams::BoneSpringDefaults();
};

}  // namespace hocloth::mc2
