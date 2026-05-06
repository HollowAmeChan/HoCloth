#include "hocloth/cloth/constraints/distance_constraint.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <cstddef>

namespace hocloth::mc2 {

void DistanceConstraint::Dispose()
{
    index_array_.Dispose();
    data_array_.Dispose();
    distance_array_.Dispose();
}

int DistanceConstraint::DataCount() const
{
    return index_array_.Count();
}

int DistanceConstraint::ConnectionCount() const
{
    return data_array_.Count();
}

void DistanceConstraint::Register(int team_id, const ConstraintData& data, TeamManager& team_manager)
{
    if (!data.IsValid() || !team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    team_data.distance_start_chunk = index_array_.AddRange(data.index_array);
    team_data.distance_data_chunk = data_array_.AddRange(data.data_array);
    distance_array_.AddRange(data.distance_array);
}

void DistanceConstraint::Exit(int team_id, TeamManager& team_manager)
{
    if (!team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    index_array_.Remove(team_data.distance_start_chunk);
    data_array_.Remove(team_data.distance_data_chunk);
    distance_array_.Remove(team_data.distance_data_chunk);
    team_data.distance_start_chunk.Clear();
    team_data.distance_data_chunk.Clear();
}

void DistanceConstraint::Solve(
    const float4& simulation_power,
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager
) const
{
    // Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/DistanceConstraint.cs
    const ExProcessingList<int>& step_particles = simulation_manager.ProcessingStepParticles();
    const auto& step_buffer = step_particles.Buffer();
    const auto& team_ids = simulation_manager.TeamIds();
    auto& next_positions = simulation_manager.NextPositions();
    const auto& base_positions = simulation_manager.BasePositions();
    auto& velocity_positions = simulation_manager.VelocityPositions();
    const auto& frictions = simulation_manager.Frictions();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& depths = virtual_mesh_manager.VertexDepths();

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
        const ClothParameters& parameters = team_manager.GetParameters(team_id);
        const DataChunk& start_chunk = team_data.distance_start_chunk;
        const DataChunk& data_chunk = team_data.distance_data_chunk;
        if (start_chunk.data_length == 0 || data_chunk.data_length == 0) {
            continue;
        }

        const int particle_start = team_data.particle_chunk.start_index;
        const int local_index = particle_index - particle_start;
        if (local_index < 0 || local_index >= start_chunk.data_length) {
            continue;
        }

        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (vertex_index < 0 || vertex_index >= attributes.Length() || vertex_index >= depths.Length()) {
            continue;
        }

        const VertexAttribute attr = attributes[vertex_index];
        if (attr.IsInvalid()) {
            continue;
        }

        const bool spring = team_data.IsSpring();
        if (attr.IsDontMove() && !spring) {
            continue;
        }

        const float depth = depths[vertex_index];
        const float friction = particle_index < frictions.Length() ? frictions[particle_index] : 0.0f;
        const float fixed_mass = spring ? 10.0f : 50.0f;
        const float inv_mass = CalcInverseMass(friction, depth, attr.IsDontMove(), fixed_mass);
        float stiffness = MC2EvaluateCurveClamp01(parameters.distance_constraint.restoration_stiffness, depth);
        stiffness *= simulation_power.y;

        int connection_count = 0;
        int connection_start = 0;
        data::Unpack12_20(
            index_array_[start_chunk.start_index + local_index],
            connection_count,
            connection_start
        );
        if (connection_count <= 0) {
            continue;
        }

        const float3 next_position = next_positions[particle_index];
        const float3 base_position = base_positions[particle_index];
        float3 add_position{};
        int add_count = 0;
        const int global_connection_start = data_chunk.start_index + connection_start;

        for (int offset = 0; offset < connection_count; ++offset) {
            const int connection_index = global_connection_start + offset;
            if (connection_index < data_chunk.start_index || connection_index >= data_chunk.EndIndex()) {
                continue;
            }

            const int target_local_index = static_cast<int>(data_array_[connection_index]);
            const int target_particle_index = particle_start + target_local_index;
            const int target_vertex_index = team_data.proxy_common_chunk.start_index + target_local_index;
            if (target_particle_index < 0 || target_particle_index >= next_positions.Length()) {
                continue;
            }
            if (target_vertex_index < 0
                || target_vertex_index >= attributes.Length()
                || target_vertex_index >= depths.Length()) {
                continue;
            }

            const float rest_distance = distance_array_[connection_index];
            const float final_stiffness = Clamp01(
                rest_distance >= 0.0f
                    ? stiffness
                    : stiffness * define::system::DistanceHorizontalStiffness
            );

            const VertexAttribute target_attr = attributes[target_vertex_index];
            const float target_depth = depths[target_vertex_index];
            const float target_friction =
                target_particle_index < frictions.Length() ? frictions[target_particle_index] : 0.0f;
            const float target_inv_mass =
                CalcInverseMass(target_friction, target_depth, target_attr.IsDontMove(), fixed_mass);

            const float rest_length = Abs(rest_distance) * team_data.init_scale.x * team_data.scale_ratio
                + (Distance(base_position, base_positions[target_particle_index])
                   - Abs(rest_distance) * team_data.init_scale.x * team_data.scale_ratio)
                    * team_data.animation_pose_ratio;

            const float3 vector = Subtract(next_positions[target_particle_index], next_position);
            const float current_distance = Length(vector);
            if (current_distance < define::system::Epsilon) {
                continue;
            }

            const float inv_mass_sum = inv_mass + target_inv_mass;
            if (inv_mass_sum < define::system::Epsilon) {
                continue;
            }

            const float3 normal = Scale(vector, 1.0f / current_distance);
            const float3 correction =
                Scale(normal, final_stiffness * (current_distance - rest_length) / inv_mass_sum);
            add_position = Add(add_position, Scale(correction, inv_mass));
            ++add_count;
        }

        if (add_count > 0) {
            add_position = Scale(add_position, 1.0f / static_cast<float>(add_count));
            next_positions[particle_index] = Add(next_position, add_position);
            velocity_positions[particle_index] = Add(
                velocity_positions[particle_index],
                Scale(add_position, parameters.distance_constraint.velocity_attenuation)
            );
        }
    }
}

const ExNativeArray<std::uint32_t>& DistanceConstraint::IndexArray() const
{
    return index_array_;
}

const ExNativeArray<std::uint16_t>& DistanceConstraint::DataArray() const
{
    return data_array_;
}

const ExNativeArray<float>& DistanceConstraint::DistanceArray() const
{
    return distance_array_;
}

}  // namespace hocloth::mc2
