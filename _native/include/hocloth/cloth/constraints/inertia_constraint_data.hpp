#pragma once

#include "hocloth/utility/math/math_types.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/InertiaConstraint.cs CenterData
struct InertiaCenterData {
    float3 anchor_position{};
    quaternion anchor_rotation{};
    float3 old_anchor_position{};
    quaternion old_anchor_rotation{};
    float3 anchor_component_local_position{};
    int center_transform_index = -1;

    float3 component_world_position{};
    quaternion component_world_rotation{};
    float3 component_world_scale{1.0f, 1.0f, 1.0f};
    float3 old_component_world_position{};
    quaternion old_component_world_rotation{};
    float3 old_component_world_scale{1.0f, 1.0f, 1.0f};
    float3 frame_component_shift_vector{};
    quaternion frame_component_shift_rotation{};
    float frame_moving_speed = 0.0f;
    float3 frame_moving_direction{};

    float3 frame_world_position{};
    quaternion frame_world_rotation{};
    float3 frame_world_scale{1.0f, 1.0f, 1.0f};
    float3 frame_local_position{};
    float3 old_frame_world_position{};
    quaternion old_frame_world_rotation{};
    float3 old_frame_world_scale{1.0f, 1.0f, 1.0f};

    float3 now_world_position{};
    quaternion now_world_rotation{};
    float3 old_world_position{};
    quaternion old_world_rotation{};
    float step_move_inertia_ratio = 0.0f;
    float step_rotation_inertia_ratio = 0.0f;
    float3 step_vector{};
    quaternion step_rotation{};
    float3 inertia_vector{};
    quaternion inertia_rotation{};
    float step_moving_speed = 0.0f;
    float3 step_moving_direction{};
    float angular_velocity = 0.0f;
    float3 rotation_axis{};
    float3 init_local_gravity_direction{0.0f, -1.0f, 0.0f};
    float3 smoothing_velocity{};
    float4x4 negative_scale_matrix{};
};

}  // namespace hocloth::mc2
