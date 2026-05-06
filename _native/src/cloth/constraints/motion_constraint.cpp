#include "hocloth/cloth/constraints/motion_constraint.hpp"

#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <cstddef>

namespace hocloth::mc2 {

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

        if (motion_params.use_max_distance) {
            depth *= depth;
            const float max_distance =
                MC2EvaluateCurveClamp01(motion_params.max_distance_curve_data, depth);
            next_position = Add(
                base_position,
                ClampVector(Subtract(next_position, base_position), max_distance * team_data.scale_ratio)
            );
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
