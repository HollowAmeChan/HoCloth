#include "hocloth/cloth/constraints/triangle_bending_constraint.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/data/multi_data_builder.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <cmath>
#include <cstddef>
#include <unordered_set>

namespace hocloth::mc2 {

void TriangleBendingConstraint::Dispose()
{
    triangle_pair_array_.Dispose();
    rest_angle_or_volume_array_.Dispose();
    sign_or_volume_array_.Dispose();
    write_data_array_.Dispose();
    write_index_array_.Dispose();
    write_buffer_.Dispose();
}

int TriangleBendingConstraint::DataCount() const
{
    return triangle_pair_array_.Count();
}

int TriangleBendingConstraint::WriteBufferCount() const
{
    return write_buffer_.Count();
}

TriangleBendingConstraint::ConstraintData TriangleBendingConstraint::CreateData(
    const VirtualMesh& proxy_mesh,
    const ClothParameters& parameters
)
{
    // Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/TriangleBendingConstraint.cs
    (void)parameters;
    ConstraintData constraint_data;
    const int vertex_count = proxy_mesh.VertexCount();
    const int triangle_count = proxy_mesh.TriangleCount();
    if (vertex_count <= 0
        || triangle_count <= 0
        || proxy_mesh.attributes.Count() < vertex_count
        || proxy_mesh.local_positions.Count() < vertex_count) {
        return constraint_data;
    }

    data::MultiDataBuilder<std::uint8_t> write_builder(vertex_count, vertex_count * 2);
    std::unordered_set<std::uint64_t> volume_set;

    auto add_triangle_pair = [&](const int2& raw_edge, const int3& tri0, const int3& tri1) {
            const int2 edge = data::PackInt2(raw_edge);
            if (edge.x < 0 || edge.y < 0 || edge.x >= vertex_count || edge.y >= vertex_count) {
                return;
            }

            const int2 diagonal = GetRestTriangleVertex(tri0, tri1, edge);
            const int4 vertices{diagonal.x, diagonal.y, edge.x, edge.y};
            bool valid = true;
            bool all_fixed = true;
            for (int index = 0; index < 4; ++index) {
                const int vertex = vertices[index];
                if (vertex < 0 || vertex >= vertex_count) {
                    valid = false;
                    break;
                }
                const VertexAttribute attr = proxy_mesh.attributes[vertex];
                if (attr.IsInvalid()) {
                    valid = false;
                    break;
                }
                all_fixed = all_fixed && attr.IsDontMove();
            }
            if (!valid || all_fixed) {
                return;
            }

            const float3 pos0 = proxy_mesh.local_positions[vertices.x];
            const float3 pos1 = proxy_mesh.local_positions[vertices.y];
            const float3 pos2 = proxy_mesh.local_positions[vertices.z];
            const float3 pos3 = proxy_mesh.local_positions[vertices.w];
            float3 normal1 = Normalize(Cross(Subtract(pos2, pos0), Subtract(pos3, pos0)));
            float3 normal2 = Normalize(Cross(Subtract(pos3, pos1), Subtract(pos2, pos1)));
            float rest_angle = std::acos(Clamp1(Dot(normal1, normal2)));
            const float direction = Dot(Cross(normal1, normal2), Subtract(pos3, pos2));
            const std::int8_t sign_flag = direction < 0.0f ? static_cast<std::int8_t>(-1) : 1;
            constexpr float radians_to_degrees = 57.29577951308232f;
            const float degree_angle = Abs(rest_angle * radians_to_degrees);
            const std::uint64_t pair = data::Pack64(vertices);

            if (degree_angle < define::system::TriangleBendingMaxAngle) {
                constraint_data.triangle_pair_array.push_back(pair);
                constraint_data.rest_angle_or_volume_array.push_back(rest_angle);
                constraint_data.sign_or_volume_array.push_back(sign_flag);
                constraint_data.write_data_array.push_back(data::Pack32(
                    write_builder.CountValuesForKey(vertices.x),
                    write_builder.CountValuesForKey(vertices.y),
                    write_builder.CountValuesForKey(vertices.z),
                    write_builder.CountValuesForKey(vertices.w)
                ));
                write_builder.Add(vertices.x, 0);
                write_builder.Add(vertices.y, 0);
                write_builder.Add(vertices.z, 0);
                write_builder.Add(vertices.w, 0);
            }

            if (degree_angle >= define::system::VolumeMinAngle
                && degree_angle <= define::system::MaxAngleLimit) {
                const int4 sorted_vertices = data::PackInt4(vertices);
                const std::uint64_t sorted_key = data::Pack64(sorted_vertices);
                if (volume_set.find(sorted_key) != volume_set.end()) {
                    return;
                }
                volume_set.insert(sorted_key);

                const float3 world0 = TransformPoint(pos0, proxy_mesh.init_local_to_world);
                const float3 world1 = TransformPoint(pos1, proxy_mesh.init_local_to_world);
                const float3 world2 = TransformPoint(pos2, proxy_mesh.init_local_to_world);
                const float3 world3 = TransformPoint(pos3, proxy_mesh.init_local_to_world);
                float volume_rest = (1.0f / 6.0f)
                    * Dot(Cross(Subtract(world1, world0), Subtract(world2, world0)), Subtract(world3, world0));
                volume_rest *= VolumeScale;

                constraint_data.triangle_pair_array.push_back(pair);
                constraint_data.rest_angle_or_volume_array.push_back(volume_rest);
                constraint_data.sign_or_volume_array.push_back(VolumeSign);
                constraint_data.write_data_array.push_back(data::Pack32(
                    write_builder.CountValuesForKey(vertices.x),
                    write_builder.CountValuesForKey(vertices.y),
                    write_builder.CountValuesForKey(vertices.z),
                    write_builder.CountValuesForKey(vertices.w)
                ));
                write_builder.Add(vertices.x, 0);
                write_builder.Add(vertices.y, 0);
                write_builder.Add(vertices.z, 0);
                write_builder.Add(vertices.w, 0);
            }
        };

    if (!proxy_mesh.edge_to_triangles.empty()) {
        for (int edge_index = 0; edge_index < proxy_mesh.edges.Count(); ++edge_index) {
            const int2 edge = data::PackInt2(proxy_mesh.edges[edge_index]);
            const auto found = proxy_mesh.edge_to_triangles.find(data::Pack32(edge.x, edge.y));
            if (found == proxy_mesh.edge_to_triangles.end() || found->second.size() < 2) {
                continue;
            }

            const std::vector<std::uint16_t>& triangle_indices = found->second;
            for (std::size_t i = 0; i + 1 < triangle_indices.size(); ++i) {
                const int triangle_index0 = triangle_indices[i];
                if (triangle_index0 < 0 || triangle_index0 >= triangle_count) {
                    continue;
                }
                const int3 tri0 = proxy_mesh.triangles[triangle_index0];
                for (std::size_t j = i + 1; j < triangle_indices.size(); ++j) {
                    const int triangle_index1 = triangle_indices[j];
                    if (triangle_index1 < 0 || triangle_index1 >= triangle_count) {
                        continue;
                    }
                    add_triangle_pair(edge, tri0, proxy_mesh.triangles[triangle_index1]);
                }
            }
        }
    } else {
        for (int triangle_index0 = 0; triangle_index0 < triangle_count; ++triangle_index0) {
            const int3 tri0 = proxy_mesh.triangles[triangle_index0];
            const int tri0_values[3] = {tri0.x, tri0.y, tri0.z};
            for (int triangle_index1 = triangle_index0 + 1; triangle_index1 < triangle_count; ++triangle_index1) {
                const int3 tri1 = proxy_mesh.triangles[triangle_index1];
                const int tri1_values[3] = {tri1.x, tri1.y, tri1.z};
                int common[2] = {-1, -1};
                int common_count = 0;
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

                add_triangle_pair(int2{common[0], common[1]}, tri0, tri1);
            }
        }
    }

    constraint_data.write_buffer_count = write_builder.Count();
    const std::vector<std::uint32_t> write_indices32 = write_builder.ToIndexArray();
    constraint_data.write_index_array.assign(write_indices32.begin(), write_indices32.end());
    return constraint_data;
}

void TriangleBendingConstraint::Register(
    int team_id,
    const ConstraintData& data,
    TeamManager& team_manager
)
{
    if (!data.IsValid() || !team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    team_data.bending_pair_chunk = triangle_pair_array_.AddRange(data.triangle_pair_array);
    rest_angle_or_volume_array_.AddRange(data.rest_angle_or_volume_array);
    sign_or_volume_array_.AddRange(data.sign_or_volume_array);
    write_data_array_.AddRange(data.write_data_array);
    team_data.bending_write_index_chunk = write_index_array_.AddRange(data.write_index_array);
    team_data.bending_buffer_chunk = write_buffer_.AddRange(data.write_buffer_count, float3{});
}

void TriangleBendingConstraint::Exit(int team_id, TeamManager& team_manager)
{
    if (!team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    triangle_pair_array_.Remove(team_data.bending_pair_chunk);
    rest_angle_or_volume_array_.Remove(team_data.bending_pair_chunk);
    sign_or_volume_array_.Remove(team_data.bending_pair_chunk);
    write_data_array_.Remove(team_data.bending_pair_chunk);
    write_index_array_.Remove(team_data.bending_write_index_chunk);
    write_buffer_.Remove(team_data.bending_buffer_chunk);
    team_data.bending_pair_chunk.Clear();
    team_data.bending_write_index_chunk.Clear();
    team_data.bending_buffer_chunk.Clear();
}

void TriangleBendingConstraint::Solve(
    const float4& simulation_power,
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager
)
{
    // Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/TriangleBendingConstraint.cs
    if (DataCount() <= 0) {
        return;
    }

    const auto& bend_steps = simulation_manager.ProcessingStepTriangleBending();
    const auto& bend_buffer = bend_steps.Buffer();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& depths = virtual_mesh_manager.VertexDepths();
    const auto& team_ids = simulation_manager.TeamIds();
    const auto& frictions = simulation_manager.Frictions();
    auto& next_positions = simulation_manager.NextPositions();

    for (int step_index = 0; step_index < bend_steps.Count(); ++step_index) {
        const std::uint32_t pack = static_cast<std::uint32_t>(
            bend_buffer[static_cast<std::size_t>(step_index)]
        );
        const int pair_index = data::Unpack12_20Low(pack);
        const int team_id = data::Unpack12_20Hi(pack);
        if (!team_manager.IsValidTeam(team_id) || pair_index < 0 || pair_index >= DataCount()) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (pair_index < team_data.bending_pair_chunk.start_index
            || pair_index >= team_data.bending_pair_chunk.EndIndex()) {
            continue;
        }

        const ClothParameters& cloth_parameters = team_manager.GetParameters(team_id);
        const TriangleBendingConstraintParams& parameters =
            cloth_parameters.triangle_bending_constraint;
        if (parameters.method == TriangleBendingMethod::None) {
            continue;
        }

        float stiffness = parameters.stiffness;
        if (stiffness < 1.0e-6f) {
            continue;
        }
        stiffness = Clamp01(stiffness * simulation_power.y);

        const int particle_start = team_data.particle_chunk.start_index;
        const int vertex_start = team_data.proxy_common_chunk.start_index;
        const int4 vertices = data::Unpack64(triangle_pair_array_[pair_index]);

        float3 next_position_buffer[4]{};
        float3 add_position_buffer[4]{};
        float inv_mass_buffer[4]{1.0f, 1.0f, 1.0f, 1.0f};
        bool valid = true;
        for (int index = 0; index < 4; ++index) {
            const int local_vertex = vertices[index];
            const int particle_index = particle_start + local_vertex;
            const int vertex_index = vertex_start + local_vertex;
            if (particle_index < 0
                || particle_index >= next_positions.Length()
                || particle_index >= frictions.Length()
                || vertex_index < 0
                || vertex_index >= attributes.Length()
                || vertex_index >= depths.Length()) {
                valid = false;
                break;
            }

            next_position_buffer[index] = next_positions[particle_index];
            const float friction = frictions[particle_index];
            const float depth = depths[vertex_index];
            const bool fixed = attributes[vertex_index].IsDontMove();
            inv_mass_buffer[index] =
                fixed ? 0.01f : CalcInverseMass(friction, depth);
        }
        if (!valid) {
            continue;
        }

        const int local_pair_index = pair_index - team_data.bending_pair_chunk.start_index;
        const int data_index = team_data.bending_pair_chunk.start_index + local_pair_index;
        if (data_index < 0
            || data_index >= rest_angle_or_volume_array_.Length()
            || data_index >= sign_or_volume_array_.Length()
            || data_index >= write_data_array_.Length()) {
            continue;
        }

        float rest_angle_or_volume = rest_angle_or_volume_array_[data_index];
        const std::int8_t sign_or_volume = sign_or_volume_array_[data_index];

        bool solved = false;
        if (sign_or_volume == VolumeSign) {
            float volume_rest = rest_angle_or_volume * team_data.scale_ratio;
            volume_rest *= team_data.negative_scale_sign;
            solved = SolveVolume(
                next_position_buffer,
                inv_mass_buffer,
                volume_rest,
                stiffness,
                add_position_buffer
            );
        } else if (parameters.method == TriangleBendingMethod::DihedralAngle) {
            solved = SolveDihedralAngle(
                0.0f,
                next_position_buffer,
                inv_mass_buffer,
                rest_angle_or_volume,
                stiffness,
                add_position_buffer
            );
        } else if (parameters.method == TriangleBendingMethod::DirectionDihedralAngle) {
            const float sign = sign_or_volume < 0 ? -1.0f : 1.0f;
            rest_angle_or_volume *= sign;
            rest_angle_or_volume *= team_data.negative_scale_sign;
            solved = SolveDihedralAngle(
                sign,
                next_position_buffer,
                inv_mass_buffer,
                rest_angle_or_volume,
                stiffness,
                add_position_buffer
            );
        }

        if (!solved) {
            continue;
        }

        const int4 write_data = data::Unpack32(write_data_array_[data_index]);
        const int index_start = team_data.bending_write_index_chunk.start_index;
        const int buffer_start = team_data.bending_buffer_chunk.start_index;
        for (int index = 0; index < 4; ++index) {
            const int local_vertex = vertices[index];
            const int write_index = index_start + local_vertex;
            if (write_index < 0 || write_index >= write_index_array_.Length()) {
                continue;
            }
            const int start = data::Unpack12_20Low(write_index_array_[write_index]);
            const int buffer_index = buffer_start + start + write_data[index];
            if (buffer_index >= 0 && buffer_index < write_buffer_.Length()) {
                write_buffer_[buffer_index] = add_position_buffer[index];
            }
        }
    }

    const auto& step_particles = simulation_manager.ProcessingStepParticles();
    const auto& step_buffer = step_particles.Buffer();
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
        if (!team_data.bending_pair_chunk.IsValid()) {
            continue;
        }

        const int local_index = particle_index - team_data.particle_chunk.start_index;
        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (local_index < 0 || vertex_index < 0 || vertex_index >= attributes.Length()) {
            continue;
        }
        if (attributes[vertex_index].IsDontMove()) {
            continue;
        }

        const int write_index =
            team_data.bending_write_index_chunk.start_index + local_index;
        if (write_index < 0 || write_index >= write_index_array_.Length()) {
            continue;
        }

        const std::uint32_t write_pack = write_index_array_[write_index];
        const int count = data::Unpack12_20Hi(write_pack);
        const int start = data::Unpack12_20Low(write_pack);
        const int buffer_index = team_data.bending_buffer_chunk.start_index + start;
        float3 add_position{};
        for (int index = 0; index < count; ++index) {
            const int current_index = buffer_index + index;
            if (current_index >= 0 && current_index < write_buffer_.Length()) {
                add_position = Add(add_position, write_buffer_[current_index]);
            }
        }
        if (count > 0 && particle_index < next_positions.Length()) {
            add_position = Scale(add_position, 1.0f / static_cast<float>(count));
            next_positions[particle_index] = Add(next_positions[particle_index], add_position);
        }
    }
}

bool TriangleBendingConstraint::SolveVolume(
    const float3 next_positions[4],
    const float inv_masses[4],
    float volume_rest,
    float stiffness,
    float3 add_positions[4]
) const
{
    const float3 next0 = next_positions[0];
    const float3 next1 = next_positions[1];
    const float3 next2 = next_positions[2];
    const float3 next3 = next_positions[3];

    float volume = (1.0f / 6.0f)
        * Dot(Cross(Subtract(next1, next0), Subtract(next2, next0)), Subtract(next3, next0));
    volume *= VolumeScale;

    const float3 grad0 = Cross(Subtract(next1, next2), Subtract(next3, next2));
    const float3 grad1 = Cross(Subtract(next2, next0), Subtract(next3, next0));
    const float3 grad2 = Cross(Subtract(next0, next1), Subtract(next3, next1));
    const float3 grad3 = Cross(Subtract(next1, next0), Subtract(next2, next0));

    float lambda = inv_masses[0] * LengthSquared(grad0)
        + inv_masses[1] * LengthSquared(grad1)
        + inv_masses[2] * LengthSquared(grad2)
        + inv_masses[3] * LengthSquared(grad3);
    lambda *= VolumeScale;
    if (std::abs(lambda) < 1.0e-6f) {
        return false;
    }

    lambda = stiffness * (volume_rest - volume) / lambda;
    add_positions[0] = Scale(grad0, lambda * inv_masses[0]);
    add_positions[1] = Scale(grad1, lambda * inv_masses[1]);
    add_positions[2] = Scale(grad2, lambda * inv_masses[2]);
    add_positions[3] = Scale(grad3, lambda * inv_masses[3]);
    return true;
}

bool TriangleBendingConstraint::SolveDihedralAngle(
    float sign,
    const float3 next_positions[4],
    const float inv_masses[4],
    float rest_angle,
    float stiffness,
    float3 add_positions[4]
) const
{
    const float3 next0 = next_positions[0];
    const float3 next1 = next_positions[1];
    const float3 next2 = next_positions[2];
    const float3 next3 = next_positions[3];

    const float3 edge = Subtract(next3, next2);
    const float edge_length = Length(edge);
    if (edge_length < 1.0e-8f) {
        return false;
    }
    const float inv_edge_length = 1.0f / edge_length;

    float3 normal1 = Cross(Subtract(next2, next0), Subtract(next3, next0));
    float3 normal2 = Cross(Subtract(next3, next1), Subtract(next2, next1));
    const float normal1_length_squared = LengthSquared(normal1);
    const float normal2_length_squared = LengthSquared(normal2);
    if (normal1_length_squared == 0.0f || normal2_length_squared == 0.0f) {
        return false;
    }

    normal1 = Scale(normal1, 1.0f / normal1_length_squared);
    normal2 = Scale(normal2, 1.0f / normal2_length_squared);

    const float3 d0 = Scale(normal1, edge_length);
    const float3 d1 = Scale(normal2, edge_length);
    const float3 d2 = Add(
        Scale(normal1, Dot(Subtract(next0, next3), edge) * inv_edge_length),
        Scale(normal2, Dot(Subtract(next1, next3), edge) * inv_edge_length)
    );
    const float3 d3 = Add(
        Scale(normal1, Dot(Subtract(next2, next0), edge) * inv_edge_length),
        Scale(normal2, Dot(Subtract(next2, next1), edge) * inv_edge_length)
    );

    normal1 = Normalize(normal1);
    normal2 = Normalize(normal2);
    float dot = Clamp1(Dot(normal1, normal2));
    float phi = std::acos(dot);

    float lambda = inv_masses[0] * LengthSquared(d0)
        + inv_masses[1] * LengthSquared(d1)
        + inv_masses[2] * LengthSquared(d2)
        + inv_masses[3] * LengthSquared(d3);
    if (lambda == 0.0f) {
        return false;
    }

    const float dir_sign = Dot(Cross(normal1, normal2), edge) < 0.0f ? -1.0f : 1.0f;
    if (sign != 0.0f) {
        phi *= dir_sign;
    } else {
        lambda *= dir_sign;
    }

    lambda = (rest_angle - phi) / lambda * stiffness;
    add_positions[0] = Scale(d0, -inv_masses[0] * lambda);
    add_positions[1] = Scale(d1, -inv_masses[1] * lambda);
    add_positions[2] = Scale(d2, -inv_masses[2] * lambda);
    add_positions[3] = Scale(d3, -inv_masses[3] * lambda);
    return true;
}

}  // namespace hocloth::mc2
