#include "hocloth/cloth/constraints/motion_constraint.hpp"

#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cstddef>

namespace hocloth::mc2 {

namespace {

float3 NormalAxisVector(ClothNormalAxis normal_axis)
{
    switch (normal_axis) {
    case ClothNormalAxis::Right:
        return float3{1.0f, 0.0f, 0.0f};
    case ClothNormalAxis::Forward:
        return float3{0.0f, 0.0f, 1.0f};
    case ClothNormalAxis::InverseRight:
        return float3{-1.0f, 0.0f, 0.0f};
    case ClothNormalAxis::InverseUp:
        return float3{0.0f, -1.0f, 0.0f};
    case ClothNormalAxis::InverseForward:
        return float3{0.0f, 0.0f, -1.0f};
    case ClothNormalAxis::Up:
    default:
        return float3{0.0f, 1.0f, 0.0f};
    }
}

}  // namespace

void MotionConstraint::Dispose()
{
}

void MotionConstraint::Solve(
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager
) const
{
    // Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/MotionConstraint.cs
    const ExProcessingList<int>& motion_particles = simulation_manager.ProcessingStepMotionParticles();
    const auto& motion_buffer = motion_particles.Buffer();
    const auto& team_ids = simulation_manager.TeamIds();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& depths = virtual_mesh_manager.VertexDepths();
    const auto& base_positions = simulation_manager.BasePositions();
    const auto& base_rotations = simulation_manager.BaseRotations();
    auto& next_positions = simulation_manager.NextPositions();
    auto& velocity_positions = simulation_manager.VelocityPositions();

    for (int step_index = 0; step_index < motion_particles.Count(); ++step_index) {
        const int particle_index = motion_buffer[static_cast<std::size_t>(step_index)];
        if (particle_index < 0 || particle_index >= team_ids.Length()) {
            continue;
        }

        const int team_id = team_ids[particle_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const ClothParameters& parameters = team_manager.GetParameters(team_id);
        const MotionConstraintParams& motion_params = parameters.motion_constraint;
        if (!motion_params.use_max_distance && !motion_params.use_backstop) {
            continue;
        }

        const int local_index = particle_index - team_data.particle_chunk.start_index;
        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (local_index < 0
            || vertex_index < 0
            || vertex_index >= attributes.Length()
            || vertex_index >= depths.Length()) {
            continue;
        }

        const VertexAttribute attr = attributes[vertex_index];
        if (!attr.IsMove() || !attr.IsMotion()) {
            continue;
        }

        float3 next_position = next_positions[particle_index];
        const float3 old_next_position = next_position;
        const float3 base_position = base_positions[particle_index];
        float depth = depths[vertex_index];
        float stiffness = Clamp01(motion_params.stiffness);
        const float radius = std::max(
            MC2EvaluateCurve(parameters.radius_curve_data, depth),
            0.0001f
        );
        const float collision_friction_range = radius;
        depth *= depth;

        const quaternion base_rotation =
            particle_index < base_rotations.Length() ? base_rotations[particle_index] : quaternion{};
        const float3 normal_direction =
            Rotate(base_rotation, NormalAxisVector(parameters.normal_axis));

        if (motion_params.use_max_distance) {
            const float max_distance =
                MC2EvaluateCurve(motion_params.max_distance_curve_data, depth);
            next_position = Add(
                base_position,
                ClampVector(Subtract(next_position, base_position), max_distance * team_data.scale_ratio)
            );
        }

        if (motion_params.use_backstop && motion_params.backstop_radius > 0.0f) {
            const float backstop_radius = motion_params.backstop_radius;
            const float backstop_distance =
                MC2EvaluateCurve(motion_params.backstop_distance_curve_data, depth);
            const float3 center = Subtract(
                base_position,
                Scale(normal_direction, backstop_distance + backstop_radius)
            );
            const float3 vector = Subtract(next_position, center);
            const float length = Length(vector);
            if (length > define::system::Epsilon
                && length < backstop_radius + collision_friction_range) {
                const float3 normal = Scale(vector, 1.0f / length);
                if (length < backstop_radius) {
                    next_position = Add(center, Scale(normal, backstop_radius));
                }
            }
        }

        next_position = Lerp(old_next_position, next_position, stiffness);
        next_positions[particle_index] = next_position;

        const float3 add = Subtract(next_position, old_next_position);
        constexpr float velocity_attenuation = 0.95f;
        velocity_positions[particle_index] =
            Add(velocity_positions[particle_index], Scale(add, velocity_attenuation));
    }
}

}  // namespace hocloth::mc2
