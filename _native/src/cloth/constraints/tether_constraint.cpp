#include "hocloth/cloth/constraints/tether_constraint.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <cstddef>

namespace hocloth::mc2 {

void TetherConstraint::Dispose()
{
}

void TetherConstraint::Solve(
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager
) const
{
    // Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/TetherConstraint.cs
    const ExProcessingList<int>& step_particles = simulation_manager.ProcessingStepParticles();
    const auto& step_buffer = step_particles.Buffer();
    const auto& team_ids = simulation_manager.TeamIds();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& vertex_depths = virtual_mesh_manager.VertexDepths();
    const auto& vertex_root_indices = virtual_mesh_manager.VertexRootIndices();
    auto& next_positions = simulation_manager.NextPositions();
    auto& velocity_positions = simulation_manager.VelocityPositions();
    const auto& frictions = simulation_manager.Frictions();
    const auto& step_basic_positions = simulation_manager.StepBasicPositions();

    for (int step_index = 0; step_index < step_particles.Count(); ++step_index) {
        const int particle_index = step_buffer[static_cast<std::size_t>(step_index)];
        if (particle_index < 0 || particle_index >= team_ids.Length()) {
            continue;
        }

        const int team_id = team_ids[particle_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const TetherConstraintParams& params =
            team_manager.GetParameters(team_id).tether_constraint;

        const int particle_start = team_data.particle_chunk.start_index;
        const int local_index = particle_index - particle_start;
        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (local_index < 0
            || vertex_index < 0
            || vertex_index >= attributes.Length()
            || vertex_index >= vertex_depths.Length()
            || vertex_index >= vertex_root_indices.Length()) {
            continue;
        }

        const VertexAttribute attr = attributes[vertex_index];
        if (!attr.IsMove()) {
            continue;
        }

        const int root_index = vertex_root_indices[vertex_index];
        if (root_index < 0) {
            continue;
        }

        const int root_particle_index = root_index + particle_start;
        if (root_particle_index < 0
            || root_particle_index >= next_positions.Length()
            || root_particle_index >= step_basic_positions.Length()
            || particle_index >= next_positions.Length()
            || particle_index >= velocity_positions.Length()
            || particle_index >= step_basic_positions.Length()
            || particle_index >= frictions.Length()) {
            continue;
        }

        float3 next_position = next_positions[particle_index];
        const float3 root_position = next_positions[root_particle_index];
        [[maybe_unused]] const float depth = vertex_depths[vertex_index];
        [[maybe_unused]] const float friction = frictions[particle_index];

        const float3 vector = Subtract(root_position, next_position);
        const float distance = Length(vector);
        if (distance < define::system::Epsilon) {
            continue;
        }

        const float3 calc_position = step_basic_positions[particle_index];
        const float3 calc_root_position = step_basic_positions[root_particle_index];
        const float calc_distance = Distance(calc_position, calc_root_position);
        if (calc_distance == 0.0f) {
            continue;
        }

        const float ratio = distance / calc_distance;
        float dist = 0.0f;
        float stiffness = 0.0f;
        float attenuation = 0.0f;
        const float compression_limit = 1.0f - params.compression_limit;
        const float stretch_limit = 1.0f + params.stretch_limit;

        if (ratio < compression_limit) {
            dist = distance - compression_limit * calc_distance;
            const float t = Clamp01(
                (compression_limit - ratio) / define::system::TetherStiffnessWidth
            );
            stiffness = define::system::TetherCompressionStiffness * t;
            attenuation = define::system::TetherCompressionVelocityAttenuation;
        } else if (ratio > stretch_limit) {
            dist = distance - stretch_limit * calc_distance;
            const float t = Clamp01(
                (ratio - stretch_limit) / define::system::TetherStiffnessWidth
            );
            stiffness = define::system::TetherStretchStiffness * t;
            attenuation = define::system::TetherStretchVelocityAttenuation;
        } else {
            continue;
        }

        const float3 add = Scale(vector, (dist * stiffness) / distance);
        next_position = Add(next_position, add);
        next_positions[particle_index] = next_position;
        velocity_positions[particle_index] =
            Add(velocity_positions[particle_index], Scale(add, attenuation));
    }
}

}  // namespace hocloth::mc2
