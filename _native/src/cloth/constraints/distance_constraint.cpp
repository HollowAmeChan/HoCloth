#include "hocloth/cloth/constraints/distance_constraint.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/data/multi_data_builder.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <cstddef>
#include <unordered_set>

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

DistanceConstraint::ConstraintData DistanceConstraint::CreateData(
    const VirtualMesh& proxy_mesh,
    const ClothParameters& parameters
)
{
    // Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/DistanceConstraint.cs
    (void)parameters;
    ConstraintData constraint_data;
    const int vertex_count = proxy_mesh.VertexCount();
    if (vertex_count <= 0
        || proxy_mesh.attributes.Count() < vertex_count
        || proxy_mesh.local_positions.Count() < vertex_count) {
        return constraint_data;
    }

    data::MultiDataBuilder<std::uint16_t> vertical_connection(vertex_count, vertex_count * 2);
    data::MultiDataBuilder<std::uint16_t> horizontal_connection(vertex_count, vertex_count * 2);
    std::unordered_set<std::uint32_t> connect_set;

    const int edge_count = proxy_mesh.edges.Count();
    for (int edge_index = 0; edge_index < edge_count; ++edge_index) {
        const int2 edge = proxy_mesh.edges[edge_index];
        if (edge.x < 0 || edge.y < 0 || edge.x >= vertex_count || edge.y >= vertex_count) {
            continue;
        }
        const VertexAttribute attr0 = proxy_mesh.attributes[edge.x];
        const VertexAttribute attr1 = proxy_mesh.attributes[edge.y];
        if ((attr0.IsMove() == false && attr1.IsMove() == false)
            || attr0.IsInvalid()
            || attr1.IsInvalid()) {
            continue;
        }

        const int parent0 =
            edge.x < proxy_mesh.vertex_parent_indices.Count()
                ? proxy_mesh.vertex_parent_indices[edge.x]
                : -1;
        const int parent1 =
            edge.y < proxy_mesh.vertex_parent_indices.Count()
                ? proxy_mesh.vertex_parent_indices[edge.y]
                : -1;

        if (edge.y == parent0 || edge.x == parent1) {
            vertical_connection.Add(edge.x, static_cast<std::uint16_t>(edge.y));
            vertical_connection.Add(edge.y, static_cast<std::uint16_t>(edge.x));
        } else {
            horizontal_connection.Add(edge.x, static_cast<std::uint16_t>(edge.y));
            horizontal_connection.Add(edge.y, static_cast<std::uint16_t>(edge.x));
        }
        connect_set.insert(data::Pack32Sort(edge.x, edge.y));
    }

    const int triangle_count = proxy_mesh.TriangleCount();
    for (int triangle_index0 = 0; triangle_index0 < triangle_count; ++triangle_index0) {
        const int3 tri0 = proxy_mesh.triangles[triangle_index0];
        const int tri0_values[3] = {tri0.x, tri0.y, tri0.z};
        for (int triangle_index1 = triangle_index0 + 1; triangle_index1 < triangle_count; ++triangle_index1) {
            const int3 tri1 = proxy_mesh.triangles[triangle_index1];
            int common[2] = {-1, -1};
            int common_count = 0;
            const int tri1_values[3] = {tri1.x, tri1.y, tri1.z};
            for (int value0 : tri0_values) {
                for (int value1 : tri1_values) {
                    if (value0 == value1 && common_count < 2) {
                        common[common_count++] = value0;
                    }
                }
            }
            if (common_count != 2) {
                continue;
            }

            const int2 edge{common[0], common[1]};
            const int3 packed_edge_tri0 = tri0;
            const int3 packed_edge_tri1 = tri1;
            const int2 diagonal = GetRestTriangleVertex(packed_edge_tri0, packed_edge_tri1, edge);
            const int e3 = diagonal.x;
            const int e4 = diagonal.y;
            if (e3 < 0 || e4 < 0 || e3 >= vertex_count || e4 >= vertex_count) {
                continue;
            }
            if (connect_set.find(data::Pack32Sort(e3, e4)) != connect_set.end()) {
                continue;
            }

            const float3 p1 = proxy_mesh.local_positions[edge.x];
            const float3 p2 = proxy_mesh.local_positions[edge.y];
            const float3 p3 = proxy_mesh.local_positions[e3];
            const float3 p4 = proxy_mesh.local_positions[e4];
            const float edge_length1 = Distance(p1, p2);
            if (edge_length1 < define::system::Epsilon) {
                continue;
            }

            const VertexAttribute attr3 = proxy_mesh.attributes[e3];
            const VertexAttribute attr4 = proxy_mesh.attributes[e4];
            if ((attr3.IsMove() == false && attr4.IsMove() == false)
                || attr3.IsInvalid()
                || attr4.IsInvalid()) {
                continue;
            }

            const float dot = Abs(Dot(TriangleNormal(p1, p2, p3), TriangleNormal(p1, p2, p4)));
            if (dot < 0.9396926f) {
                continue;
            }

            const float edge_length2 = Distance(p3, p4);
            const float ratio = Abs(edge_length2 / edge_length1 - 1.0f);
            if (ratio <= 0.3f) {
                connect_set.insert(data::Pack32Sort(e3, e4));
                horizontal_connection.Add(e3, static_cast<std::uint16_t>(e4));
                horizontal_connection.Add(e4, static_cast<std::uint16_t>(e3));
            }
        }
    }

    const auto [vertical_data, vertical_index] = vertical_connection.ToArray();
    const auto [horizontal_data, horizontal_index] = horizontal_connection.ToArray();
    const int total_count =
        static_cast<int>(vertical_data.size() + horizontal_data.size());
    if (total_count <= 0) {
        return constraint_data;
    }

    constraint_data.index_array.reserve(static_cast<std::size_t>(vertex_count));
    constraint_data.data_array.reserve(static_cast<std::size_t>(total_count));
    constraint_data.distance_array.reserve(static_cast<std::size_t>(total_count));

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const int start = static_cast<int>(constraint_data.data_array.size());
        int count = 0;
        const float3 position = proxy_mesh.local_positions[vertex_index];

        for (int type = 0; type < TypeCount; ++type) {
            int data_count = 0;
            int data_start = 0;
            data::Unpack12_20(
                type == 0 ? vertical_index[static_cast<std::size_t>(vertex_index)]
                          : horizontal_index[static_cast<std::size_t>(vertex_index)],
                data_count,
                data_start
            );
            for (int index = 0; index < data_count; ++index) {
                const std::uint16_t target =
                    type == 0
                        ? vertical_data[static_cast<std::size_t>(data_start + index)]
                        : horizontal_data[static_cast<std::size_t>(data_start + index)];
                const float distance = Distance(position, proxy_mesh.local_positions[target]);
                if (distance < 1.0e-6f) {
                    continue;
                }
                constraint_data.data_array.push_back(target);
                constraint_data.distance_array.push_back(type == 0 ? distance : -distance);
                ++count;
            }
        }

        constraint_data.index_array.push_back(data::Pack12_20(count, start));
    }

    return constraint_data;
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
