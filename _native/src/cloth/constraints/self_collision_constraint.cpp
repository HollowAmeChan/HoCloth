#include "hocloth/cloth/constraints/self_collision_constraint.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace hocloth::mc2 {

namespace {

constexpr float InterlockToFixed = 1000000.0f;
constexpr float InterlockToFloat = 0.000001f;

int ToFixed(float value)
{
    return static_cast<int>(value * InterlockToFixed);
}

void AddAggregateFloat3(ExNativeArray<int>& count_array, ExNativeArray<int>& sum_array, int index, const float3& value)
{
    if (index < 0 || index >= count_array.Length()) {
        return;
    }

    ++count_array[index];
    const int data_index = index * 3;
    if (data_index + 2 >= sum_array.Length()) {
        return;
    }

    sum_array[data_index] += ToFixed(value.x);
    sum_array[data_index + 1] += ToFixed(value.y);
    sum_array[data_index + 2] += ToFixed(value.z);
}

float3 ReadAverageFloat3(const ExNativeArray<int>& count_array, const ExNativeArray<int>& sum_array, int index)
{
    if (index < 0 || index >= count_array.Length()) {
        return float3{};
    }

    const int count = count_array[index];
    const int data_index = index * 3;
    if (count <= 0 || data_index + 2 >= sum_array.Length()) {
        return float3{};
    }

    return Scale(
        float3{
            static_cast<float>(sum_array[data_index]),
            static_cast<float>(sum_array[data_index + 1]),
            static_cast<float>(sum_array[data_index + 2]),
        },
        InterlockToFloat / static_cast<float>(count)
    );
}

void ClearPrimitiveChunk(
    ExNativeArray<SelfCollisionConstraint::Primitive>& primitives,
    ExNativeArray<SelfCollisionConstraint::SortData>& sort_data,
    DataChunk& chunk
)
{
    if (!chunk.IsValid()) {
        return;
    }
    primitives.Remove(chunk);
    sort_data.Remove(chunk);
    chunk.Clear();
}

DataChunk SelfChunkForKind(const TeamManager::TeamData& team_data, std::uint32_t kind)
{
    switch (kind) {
    case SelfCollisionConstraint::KindPoint:
        return team_data.self_point_chunk;
    case SelfCollisionConstraint::KindEdge:
        return team_data.self_edge_chunk;
    case SelfCollisionConstraint::KindTriangle:
        return team_data.self_triangle_chunk;
    default:
        return DataChunk::Empty();
    }
}

int AxisCountForKind(std::uint32_t kind)
{
    return static_cast<int>(kind) + 1;
}

}  // namespace

const float3& SelfCollisionConstraint::float3x3::operator[](int index) const
{
    switch (index) {
    case 0:
        return c0;
    case 1:
        return c1;
    default:
        return c2;
    }
}

float3& SelfCollisionConstraint::float3x3::operator[](int index)
{
    switch (index) {
    case 0:
        return c0;
    case 1:
        return c1;
    default:
        return c2;
    }
}

bool SelfCollisionConstraint::Primitive::IsIgnore() const
{
    return (flag_and_team_id & FlagIgnore) != 0;
}

bool SelfCollisionConstraint::Primitive::HasParticle(int particle_index) const
{
    return particle_index >= 0
        && (particle_indices.x == particle_index
            || particle_indices.y == particle_index
            || particle_indices.z == particle_index);
}

std::uint32_t SelfCollisionConstraint::Primitive::Kind() const
{
    return (flag_and_team_id & FlagKindMask) >> 24;
}

int SelfCollisionConstraint::Primitive::TeamId() const
{
    return static_cast<int>(flag_and_team_id & 0x00ffffff);
}

float SelfCollisionConstraint::Primitive::SolveThickness(const Primitive& other) const
{
    return thickness + other.thickness;
}

bool SelfCollisionConstraint::Primitive::AnyParticle(const Primitive& other) const
{
    const int particles[3] = {particle_indices.x, particle_indices.y, particle_indices.z};
    for (int particle : particles) {
        if (other.HasParticle(particle)) {
            return true;
        }
    }
    return false;
}

std::uint32_t SelfCollisionConstraint::SortData::Kind() const
{
    return (flag_and_team_id & FlagKindMask) >> 24;
}

bool SelfCollisionConstraint::SortData::operator<(const SortData& other) const
{
    return first_min_max.x < other.first_min_max.x;
}

void SelfCollisionConstraint::Dispose()
{
    primitive_array_.Dispose();
    sort_and_sweep_array_.Dispose();
    edge_edge_contact_array_.Dispose();
    point_triangle_contact_array_.Dispose();
    intersect_flag_array_.Dispose();
    point_primitive_count_ = 0;
    edge_primitive_count_ = 0;
    triangle_primitive_count_ = 0;
    intersect_count_ = 0;
}

int SelfCollisionConstraint::PointPrimitiveCount() const
{
    return point_primitive_count_;
}

int SelfCollisionConstraint::EdgePrimitiveCount() const
{
    return edge_primitive_count_;
}

int SelfCollisionConstraint::TrianglePrimitiveCount() const
{
    return triangle_primitive_count_;
}

int SelfCollisionConstraint::IntersectCount() const
{
    return intersect_count_;
}

bool SelfCollisionConstraint::HasPrimitive() const
{
    return point_primitive_count_ + edge_primitive_count_ + triangle_primitive_count_ > 0;
}

void SelfCollisionConstraint::Clear()
{
    primitive_array_.Clear();
    sort_and_sweep_array_.Clear();
    edge_edge_contact_array_.Clear();
    point_triangle_contact_array_.Clear();
    intersect_flag_array_.Clear();
    point_primitive_count_ = 0;
    edge_primitive_count_ = 0;
    triangle_primitive_count_ = 0;
    intersect_count_ = 0;
}

void SelfCollisionConstraint::WorkBufferUpdate(int particle_count)
{
    if (particle_count <= 0) {
        intersect_flag_array_.Dispose();
        return;
    }

    if (intersect_flag_array_.Length() < particle_count) {
        intersect_flag_array_.Dispose();
        intersect_flag_array_ = ExNativeArray<std::uint8_t>(particle_count);
        intersect_flag_array_.AddRange(particle_count, 0);
    } else {
        intersect_flag_array_.Fill(0);
    }
}

void SelfCollisionConstraint::RegisterTeamPrimitives(
    int team_id,
    bool use_point_primitive,
    bool use_edge_primitive,
    bool use_triangle_primitive,
    TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager
)
{
    if (!team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);

    if (use_point_primitive && !team_data.self_point_chunk.IsValid()) {
        const int point_count = team_data.ParticleCount();
        team_data.self_point_chunk = primitive_array_.AddRange(point_count);
        sort_and_sweep_array_.AddRange(point_count);
        InitPrimitive(
            team_id,
            team_data,
            KindPoint,
            team_data.self_point_chunk.start_index,
            team_data.self_point_chunk.start_index,
            point_count,
            virtual_mesh_manager
        );
        point_primitive_count_ += point_count;
    } else if (!use_point_primitive && team_data.self_point_chunk.IsValid()) {
        point_primitive_count_ -= team_data.self_point_chunk.data_length;
        ClearPrimitiveChunk(primitive_array_, sort_and_sweep_array_, team_data.self_point_chunk);
    }

    if (use_edge_primitive && !team_data.self_edge_chunk.IsValid()) {
        const int edge_count = team_data.EdgeCount();
        team_data.self_edge_chunk = primitive_array_.AddRange(edge_count);
        sort_and_sweep_array_.AddRange(edge_count);
        InitPrimitive(
            team_id,
            team_data,
            KindEdge,
            team_data.self_edge_chunk.start_index,
            team_data.self_edge_chunk.start_index,
            edge_count,
            virtual_mesh_manager
        );
        edge_primitive_count_ += edge_count;
    } else if (!use_edge_primitive && team_data.self_edge_chunk.IsValid()) {
        edge_primitive_count_ -= team_data.self_edge_chunk.data_length;
        ClearPrimitiveChunk(primitive_array_, sort_and_sweep_array_, team_data.self_edge_chunk);
    }

    if (use_triangle_primitive && !team_data.self_triangle_chunk.IsValid()) {
        const int triangle_count = team_data.TriangleCount();
        team_data.self_triangle_chunk = primitive_array_.AddRange(triangle_count);
        sort_and_sweep_array_.AddRange(triangle_count);
        InitPrimitive(
            team_id,
            team_data,
            KindTriangle,
            team_data.self_triangle_chunk.start_index,
            team_data.self_triangle_chunk.start_index,
            triangle_count,
            virtual_mesh_manager
        );
        triangle_primitive_count_ += triangle_count;
    } else if (!use_triangle_primitive && team_data.self_triangle_chunk.IsValid()) {
        triangle_primitive_count_ -= team_data.self_triangle_chunk.data_length;
        ClearPrimitiveChunk(primitive_array_, sort_and_sweep_array_, team_data.self_triangle_chunk);
    }

    RecalculateIntersectCount(team_manager);
}

void SelfCollisionConstraint::RemoveTeamPrimitives(int team_id, TeamManager& team_manager)
{
    if (!team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    if (team_data.self_point_chunk.IsValid()) {
        point_primitive_count_ -= team_data.self_point_chunk.data_length;
        ClearPrimitiveChunk(primitive_array_, sort_and_sweep_array_, team_data.self_point_chunk);
    }
    if (team_data.self_edge_chunk.IsValid()) {
        edge_primitive_count_ -= team_data.self_edge_chunk.data_length;
        ClearPrimitiveChunk(primitive_array_, sort_and_sweep_array_, team_data.self_edge_chunk);
    }
    if (team_data.self_triangle_chunk.IsValid()) {
        triangle_primitive_count_ -= team_data.self_triangle_chunk.data_length;
        ClearPrimitiveChunk(primitive_array_, sort_and_sweep_array_, team_data.self_triangle_chunk);
    }
    RecalculateIntersectCount(team_manager);
}

void SelfCollisionConstraint::InitPrimitive(
    int team_id,
    const TeamManager::TeamData& team_data,
    std::uint32_t kind,
    int start_primitive,
    int start_sort,
    int length,
    const VirtualMeshManager& virtual_mesh_manager
)
{
    const auto& edges = virtual_mesh_manager.Edges();
    const auto& triangles = virtual_mesh_manager.Triangles();
    const int particle_start = team_data.particle_chunk.start_index;
    for (int index = 0; index < length; ++index) {
        const int primitive_index = start_primitive + index;
        const int sort_index = start_sort + index;
        if (primitive_index < 0 || primitive_index >= primitive_array_.Length()
            || sort_index < 0 || sort_index >= sort_and_sweep_array_.Length()) {
            continue;
        }

        Primitive primitive = primitive_array_[primitive_index];
        int3 particle_indices{-1, -1, -1};
        if (kind == KindPoint) {
            particle_indices.x = particle_start + index;
        } else if (kind == KindEdge) {
            const int edge_index = team_data.proxy_edge_chunk.start_index + index;
            if (edge_index >= 0 && edge_index < edges.Length()) {
                const int2 edge = edges[edge_index];
                particle_indices.x = particle_start + edge.x;
                particle_indices.y = particle_start + edge.y;
            }
        } else if (kind == KindTriangle) {
            const int triangle_index = team_data.proxy_triangle_chunk.start_index + index;
            if (triangle_index >= 0 && triangle_index < triangles.Length()) {
                const int3 triangle = triangles[triangle_index];
                particle_indices.x = particle_start + triangle.x;
                particle_indices.y = particle_start + triangle.y;
                particle_indices.z = particle_start + triangle.z;
            }
        }

        primitive.flag_and_team_id = static_cast<std::uint32_t>(team_id) | (kind << 24);
        primitive.sort_index = sort_index;
        primitive.particle_indices = particle_indices;
        primitive_array_[primitive_index] = primitive;

        SortData sort_data = sort_and_sweep_array_[sort_index];
        sort_data.primitive_index = primitive_index;
        sort_data.flag_and_team_id = static_cast<std::uint32_t>(team_id);
        sort_and_sweep_array_[sort_index] = sort_data;
    }
}

void SelfCollisionConstraint::UpdatePrimitives(
    std::uint32_t kind,
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    const SimulationManager& simulation_manager
)
{
    const auto& processing = kind == KindPoint
        ? simulation_manager.ProcessingSelfPointTriangles()
        : kind == KindEdge
            ? simulation_manager.ProcessingSelfEdgeEdges()
            : simulation_manager.ProcessingSelfTrianglePoints();
    const auto& buffer = processing.Buffer();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& depths = virtual_mesh_manager.VertexDepths();
    const auto& next_positions = simulation_manager.NextPositions();
    const auto& old_positions = simulation_manager.OldPositions();
    const auto& frictions = simulation_manager.Frictions();

    for (int step_index = 0; step_index < processing.Count(); ++step_index) {
        const std::uint32_t pack = buffer[static_cast<std::size_t>(step_index)];
        const int team_id = data::Unpack32Hi(pack);
        const int local_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const DataChunk primitive_chunk = SelfChunkForKind(team_data, kind);
        const int primitive_index = primitive_chunk.start_index + local_index;
        if (!primitive_chunk.IsValid()
            || primitive_index < primitive_chunk.start_index
            || primitive_index >= primitive_chunk.EndIndex()
            || primitive_index >= primitive_array_.Length()) {
            continue;
        }

        const ClothParameters& parameters = team_manager.GetParameters(team_id);
        Primitive primitive = primitive_array_[primitive_index];
        std::uint32_t flag = primitive.flag_and_team_id;
        const int axis_count = AxisCountForKind(kind);
        std::uint32_t fix_flag = FlagFix0;
        bool ignore = false;
        int fixed_count = 0;
        float depth = 0.0f;

        for (int axis = 0; axis < axis_count; ++axis) {
            const int particle_index = primitive.particle_indices[axis];
            if (particle_index < 0 || particle_index >= next_positions.Length()
                || particle_index >= old_positions.Length()
                || particle_index >= frictions.Length()) {
                ignore = true;
                fix_flag <<= 1;
                continue;
            }

            primitive.next_position[axis] = next_positions[particle_index];
            primitive.old_position[axis] = old_positions[particle_index];

            const int vertex_index =
                team_data.proxy_common_chunk.start_index
                + particle_index
                - team_data.particle_chunk.start_index;
            if (vertex_index < 0 || vertex_index >= attributes.Length() || vertex_index >= depths.Length()) {
                ignore = true;
                fix_flag <<= 1;
                continue;
            }

            const VertexAttribute attr = attributes[vertex_index];
            if (attr.IsMove()) {
                flag &= ~fix_flag;
            } else {
                flag |= fix_flag;
                ++fixed_count;
            }
            if (attr.IsInvalid()) {
                ignore = true;
            }

            primitive.inverse_mass[axis] = CalcSelfCollisionInverseMass(
                frictions[particle_index],
                attr.IsDontMove(),
                parameters.self_collision_constraint.cloth_mass
            );
            depth += depths[vertex_index];
            fix_flag <<= 1;
        }

        if (fixed_count == axis_count) {
            flag |= FlagAllFix;
        } else {
            flag &= ~FlagAllFix;
        }
        if (ignore) {
            flag |= FlagIgnore;
        } else {
            flag &= ~FlagIgnore;
        }

        primitive.flag_and_team_id = flag;
        depth /= static_cast<float>(axis_count);
        primitive.thickness =
            MC2EvaluateCurve(parameters.self_collision_constraint.surface_thickness_curve_data, depth)
            * team_data.scale_ratio;
        primitive_array_[primitive_index] = primitive;

        AABB bounds{
            float3{
                std::min(primitive.next_position[0].x, primitive.old_position[0].x),
                std::min(primitive.next_position[0].y, primitive.old_position[0].y),
                std::min(primitive.next_position[0].z, primitive.old_position[0].z),
            },
            float3{
                std::max(primitive.next_position[0].x, primitive.old_position[0].x),
                std::max(primitive.next_position[0].y, primitive.old_position[0].y),
                std::max(primitive.next_position[0].z, primitive.old_position[0].z),
            },
        };
        for (int axis = 1; axis < axis_count; ++axis) {
            Encapsulate(bounds, primitive.next_position[axis]);
            Encapsulate(bounds, primitive.old_position[axis]);
        }
        Expand(bounds, primitive.thickness);

        const int sort_index = primitive.sort_index;
        if (sort_index >= 0 && sort_index < sort_and_sweep_array_.Length()) {
            SortData sort_data = sort_and_sweep_array_[sort_index];
            sort_data.flag_and_team_id = primitive.flag_and_team_id;
            sort_data.first_min_max = float2{bounds.min.y, bounds.max.y};
            sort_data.second_min_max = float2{bounds.min.x, bounds.max.x};
            sort_data.third_min_max = float2{bounds.min.z, bounds.max.z};
            sort_and_sweep_array_[sort_index] = sort_data;
        }
    }
}

void SelfCollisionConstraint::IntersectUpdatePrimitives(
    std::uint32_t kind,
    const TeamManager& team_manager,
    const SimulationManager& simulation_manager
)
{
    const auto& processing = kind == KindEdge
        ? simulation_manager.ProcessingSelfEdgeEdges()
        : simulation_manager.ProcessingSelfTrianglePoints();
    const auto& buffer = processing.Buffer();
    const auto& next_positions = simulation_manager.NextPositions();

    for (int step_index = 0; step_index < processing.Count(); ++step_index) {
        const std::uint32_t pack = buffer[static_cast<std::size_t>(step_index)];
        const int team_id = data::Unpack32Hi(pack);
        const int local_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (kind == KindEdge
            && !team_data.flag.TestAny(TeamManager::FlagSelfEdgeTriangleIntersect, 3)) {
            continue;
        }
        if (kind == KindTriangle
            && !team_data.flag.TestAny(TeamManager::FlagSelfTriangleEdgeIntersect, 3)) {
            continue;
        }

        const DataChunk primitive_chunk = SelfChunkForKind(team_data, kind);
        const int primitive_index = primitive_chunk.start_index + local_index;
        if (!primitive_chunk.IsValid()
            || primitive_index < primitive_chunk.start_index
            || primitive_index >= primitive_chunk.EndIndex()
            || primitive_index >= primitive_array_.Length()) {
            continue;
        }

        Primitive primitive = primitive_array_[primitive_index];
        const int axis_count = AxisCountForKind(kind);
        for (int axis = 0; axis < axis_count; ++axis) {
            const int particle_index = primitive.particle_indices[axis];
            if (particle_index >= 0 && particle_index < next_positions.Length()) {
                primitive.next_position[axis] = next_positions[particle_index];
            }
        }
        primitive_array_[primitive_index] = primitive;
    }
}

void SelfCollisionConstraint::SortTeamPrimitives(const TeamManager& team_manager)
{
    for (int team_id = 1; team_id < team_manager.TeamCount(); ++team_id) {
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }
        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const DataChunk chunks[3] = {
            team_data.self_point_chunk,
            team_data.self_edge_chunk,
            team_data.self_triangle_chunk,
        };
        for (const DataChunk& chunk : chunks) {
            if (!chunk.IsValid() || chunk.EndIndex() > sort_and_sweep_array_.Length()) {
                continue;
            }
            auto begin = sort_and_sweep_array_.Data().begin() + chunk.start_index;
            auto end = sort_and_sweep_array_.Data().begin() + chunk.EndIndex();
            std::sort(begin, end);
            for (int index = chunk.start_index; index < chunk.EndIndex(); ++index) {
                const int primitive_index = sort_and_sweep_array_[index].primitive_index;
                if (primitive_index >= 0 && primitive_index < primitive_array_.Length()) {
                    primitive_array_[primitive_index].sort_index = index;
                }
            }
        }
    }
}

void SelfCollisionConstraint::ClearContacts()
{
    edge_edge_contact_array_.Clear();
    point_triangle_contact_array_.Clear();
}

int SelfCollisionConstraint::BinarySearchSortAndSweep(
    const SortData& sort_data,
    const DataChunk& chunk
) const
{
    if (!chunk.IsValid()) {
        return -1;
    }

    int left = chunk.start_index;
    int right = chunk.EndIndex();
    while (left < right) {
        const int mid = left + (right - left) / 2;
        if (sort_and_sweep_array_[mid].first_min_max.x < sort_data.first_min_max.x) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return left;
}

void SelfCollisionConstraint::GenerateBroadPhaseContacts(
    const TeamManager& team_manager,
    const SimulationManager& simulation_manager
)
{
    ClearContacts();

    const auto& intersect_flags = intersect_flag_array_;
    const auto& edge_processing = simulation_manager.ProcessingSelfEdgeEdges();
    const auto& edge_buffer = edge_processing.Buffer();
    for (int step_index = 0; step_index < edge_processing.Count(); ++step_index) {
        const std::uint32_t pack = edge_buffer[static_cast<std::size_t>(step_index)];
        const int team_id = data::Unpack32Hi(pack);
        const int local_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const int primitive_index = team_data.self_edge_chunk.start_index + local_index;
        if (!team_data.self_edge_chunk.IsValid()
            || primitive_index < team_data.self_edge_chunk.start_index
            || primitive_index >= team_data.self_edge_chunk.EndIndex()
            || primitive_index >= primitive_array_.Length()) {
            continue;
        }

        const Primitive primitive0 = primitive_array_[primitive_index];
        if (primitive0.IsIgnore()) {
            continue;
        }
        if (team_data.flag.TestAny(TeamManager::FlagSelfEdgeTriangleIntersect, 6)) {
            const int p0 = primitive0.particle_indices.x;
            const int p1 = primitive0.particle_indices.y;
            if ((p0 >= 0 && p0 < intersect_flags.Length() && intersect_flags[p0] > 0)
                || (p1 >= 0 && p1 < intersect_flags.Length() && intersect_flags[p1] > 0)) {
                continue;
            }
        }

        const SortData sort_data0 = sort_and_sweep_array_[primitive0.sort_index];
        if (team_data.flag.IsSet(TeamManager::FlagSelfEdgeEdge)) {
            SweepEdgeEdge(
                team_data,
                primitive0,
                sort_data0,
                primitive0.sort_index + 1,
                team_data.self_edge_chunk,
                true
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagSyncEdgeEdge)
            && team_manager.IsValidTeam(team_data.sync_team_id)) {
            const TeamManager::TeamData& sync_team_data =
                team_manager.GetTeamData(team_data.sync_team_id);
            SweepEdgeEdge(
                team_data,
                primitive0,
                sort_data0,
                -1,
                sync_team_data.self_edge_chunk,
                false
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagPSyncEdgeEdge)) {
            for (int parent_index = 0; parent_index < team_data.sync_parent_team_ids.Length();
                 ++parent_index) {
                const int parent_team_id = team_data.sync_parent_team_ids[parent_index];
                if (!team_manager.IsValidTeam(parent_team_id)) {
                    continue;
                }
                const TeamManager::TeamData& parent_team_data =
                    team_manager.GetTeamData(parent_team_id);
                if (!parent_team_data.flag.IsSet(TeamManager::FlagSyncEdgeEdge)) {
                    continue;
                }
                SweepEdgeEdge(
                    team_data,
                    primitive0,
                    sort_data0,
                    -1,
                    parent_team_data.self_edge_chunk,
                    false
                );
            }
        }
    }

    const auto& point_processing = simulation_manager.ProcessingSelfPointTriangles();
    const auto& point_buffer = point_processing.Buffer();
    for (int step_index = 0; step_index < point_processing.Count(); ++step_index) {
        const std::uint32_t pack = point_buffer[static_cast<std::size_t>(step_index)];
        const int team_id = data::Unpack32Hi(pack);
        const int local_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const int primitive_index = team_data.self_point_chunk.start_index + local_index;
        if (!team_data.self_point_chunk.IsValid()
            || primitive_index < team_data.self_point_chunk.start_index
            || primitive_index >= team_data.self_point_chunk.EndIndex()
            || primitive_index >= primitive_array_.Length()) {
            continue;
        }

        const Primitive primitive0 = primitive_array_[primitive_index];
        if (primitive0.IsIgnore()) {
            continue;
        }
        if (team_data.flag.TestAny(TeamManager::FlagSelfEdgeTriangleIntersect, 6)) {
            const int p0 = primitive0.particle_indices.x;
            if (p0 >= 0 && p0 < intersect_flags.Length() && intersect_flags[p0] > 0) {
                continue;
            }
        }

        const SortData sort_data0 = sort_and_sweep_array_[primitive0.sort_index];
        if (team_data.flag.IsSet(TeamManager::FlagSelfPointTriangle)) {
            SweepPointTriangle(
                team_data,
                KindPoint,
                primitive0,
                sort_data0,
                -1,
                team_data.self_triangle_chunk,
                true
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagSyncPointTriangle)
            && team_manager.IsValidTeam(team_data.sync_team_id)) {
            const TeamManager::TeamData& sync_team_data =
                team_manager.GetTeamData(team_data.sync_team_id);
            SweepPointTriangle(
                team_data,
                KindPoint,
                primitive0,
                sort_data0,
                -1,
                sync_team_data.self_triangle_chunk,
                false
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagPSyncPointTriangle)) {
            for (int parent_index = 0; parent_index < team_data.sync_parent_team_ids.Length();
                 ++parent_index) {
                const int parent_team_id = team_data.sync_parent_team_ids[parent_index];
                if (!team_manager.IsValidTeam(parent_team_id)) {
                    continue;
                }
                const TeamManager::TeamData& parent_team_data =
                    team_manager.GetTeamData(parent_team_id);
                if (!parent_team_data.flag.IsSet(TeamManager::FlagSyncTrianglePoint)) {
                    continue;
                }
                SweepPointTriangle(
                    team_data,
                    KindPoint,
                    primitive0,
                    sort_data0,
                    -1,
                    parent_team_data.self_triangle_chunk,
                    false
                );
            }
        }
    }

    const auto& triangle_processing = simulation_manager.ProcessingSelfTrianglePoints();
    const auto& triangle_buffer = triangle_processing.Buffer();
    for (int step_index = 0; step_index < triangle_processing.Count(); ++step_index) {
        const std::uint32_t pack = triangle_buffer[static_cast<std::size_t>(step_index)];
        const int team_id = data::Unpack32Hi(pack);
        const int local_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const int primitive_index = team_data.self_triangle_chunk.start_index + local_index;
        if (!team_data.self_triangle_chunk.IsValid()
            || primitive_index < team_data.self_triangle_chunk.start_index
            || primitive_index >= team_data.self_triangle_chunk.EndIndex()
            || primitive_index >= primitive_array_.Length()) {
            continue;
        }

        const Primitive primitive0 = primitive_array_[primitive_index];
        if (primitive0.IsIgnore()) {
            continue;
        }
        if (team_data.flag.TestAny(TeamManager::FlagSelfEdgeTriangleIntersect, 6)) {
            bool intersected = false;
            for (int axis = 0; axis < 3; ++axis) {
                const int particle_index = primitive0.particle_indices[axis];
                if (particle_index >= 0
                    && particle_index < intersect_flags.Length()
                    && intersect_flags[particle_index] > 0) {
                    intersected = true;
                    break;
                }
            }
            if (intersected) {
                continue;
            }
        }

        const SortData sort_data0 = sort_and_sweep_array_[primitive0.sort_index];
        if (team_data.flag.IsSet(TeamManager::FlagSelfTrianglePoint)) {
            SweepPointTriangle(
                team_data,
                KindTriangle,
                primitive0,
                sort_data0,
                -1,
                team_data.self_point_chunk,
                true
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagSyncTrianglePoint)
            && team_manager.IsValidTeam(team_data.sync_team_id)) {
            const TeamManager::TeamData& sync_team_data =
                team_manager.GetTeamData(team_data.sync_team_id);
            SweepPointTriangle(
                team_data,
                KindTriangle,
                primitive0,
                sort_data0,
                -1,
                sync_team_data.self_point_chunk,
                false
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagPSyncTrianglePoint)) {
            for (int parent_index = 0; parent_index < team_data.sync_parent_team_ids.Length();
                 ++parent_index) {
                const int parent_team_id = team_data.sync_parent_team_ids[parent_index];
                if (!team_manager.IsValidTeam(parent_team_id)) {
                    continue;
                }
                const TeamManager::TeamData& parent_team_data =
                    team_manager.GetTeamData(parent_team_id);
                if (!parent_team_data.flag.IsSet(TeamManager::FlagSyncPointTriangle)) {
                    continue;
                }
                SweepPointTriangle(
                    team_data,
                    KindTriangle,
                    primitive0,
                    sort_data0,
                    -1,
                    parent_team_data.self_point_chunk,
                    false
                );
            }
        }
    }
}

void SelfCollisionConstraint::SweepEdgeEdge(
    const TeamManager::TeamData&,
    const Primitive& primitive0,
    const SortData& sort_data0,
    int sort_index,
    const DataChunk& sub_chunk,
    bool connection_check
)
{
    if (!sub_chunk.IsValid()) {
        return;
    }
    if (sort_index < 0) {
        sort_index = BinarySearchSortAndSweep(sort_data0, sub_chunk);
    }

    const float first_end = sort_data0.first_min_max.y;
    const int end_index = sub_chunk.EndIndex();
    while (sort_index < end_index) {
        const SortData sort_data1 = sort_and_sweep_array_[sort_index];
        ++sort_index;

        if (sort_data1.first_min_max.x > first_end) {
            break;
        }
        if (sort_data1.second_min_max.y < sort_data0.second_min_max.x
            || sort_data1.second_min_max.x > sort_data0.second_min_max.y) {
            continue;
        }
        if (sort_data1.third_min_max.y < sort_data0.third_min_max.x
            || sort_data1.third_min_max.x > sort_data0.third_min_max.y) {
            continue;
        }

        if (sort_data1.primitive_index < 0 || sort_data1.primitive_index >= primitive_array_.Length()) {
            continue;
        }
        const Primitive primitive1 = primitive_array_[sort_data1.primitive_index];
        if (primitive1.IsIgnore()) {
            continue;
        }
        if (connection_check && primitive0.AnyParticle(primitive1)) {
            continue;
        }
        if ((primitive0.flag_and_team_id & FlagAllFix) != 0
            && (primitive1.flag_and_team_id & FlagAllFix) != 0) {
            continue;
        }

        const float solve_thickness = primitive0.SolveThickness(primitive1);
        if (solve_thickness < 0.0001f) {
            continue;
        }
        BroadEdgeEdge(
            primitive0,
            primitive1,
            solve_thickness,
            solve_thickness * define::system::SelfCollisionSCR
        );
    }
}

void SelfCollisionConstraint::SweepPointTriangle(
    const TeamManager::TeamData&,
    std::uint32_t main_kind,
    const Primitive& primitive0,
    const SortData& sort_data0,
    int sort_index,
    const DataChunk& sub_chunk,
    bool connection_check
)
{
    if (!sub_chunk.IsValid()) {
        return;
    }
    if (sort_index < 0) {
        sort_index = BinarySearchSortAndSweep(sort_data0, sub_chunk);
    }

    const float first_end = sort_data0.first_min_max.y;
    const int end_index = sub_chunk.EndIndex();
    while (sort_index < end_index) {
        const SortData sort_data1 = sort_and_sweep_array_[sort_index];
        ++sort_index;

        if (sort_data1.first_min_max.x > first_end) {
            break;
        }
        if (sort_data1.second_min_max.y < sort_data0.second_min_max.x
            || sort_data1.second_min_max.x > sort_data0.second_min_max.y) {
            continue;
        }
        if (sort_data1.third_min_max.y < sort_data0.third_min_max.x
            || sort_data1.third_min_max.x > sort_data0.third_min_max.y) {
            continue;
        }

        if (sort_data1.primitive_index < 0 || sort_data1.primitive_index >= primitive_array_.Length()) {
            continue;
        }
        const Primitive primitive1 = primitive_array_[sort_data1.primitive_index];
        if (primitive1.IsIgnore()) {
            continue;
        }
        if (connection_check && primitive0.AnyParticle(primitive1)) {
            continue;
        }
        if ((primitive0.flag_and_team_id & FlagAllFix) != 0
            && (primitive1.flag_and_team_id & FlagAllFix) != 0) {
            continue;
        }

        const float solve_thickness = primitive0.SolveThickness(primitive1);
        if (solve_thickness < 0.0001f) {
            continue;
        }
        const float scr = solve_thickness * define::system::SelfCollisionSCR;
        if (main_kind == KindPoint) {
            BroadPointTriangle(
                primitive0,
                primitive1,
                solve_thickness,
                scr,
                define::system::SelfCollisionPointTriangleAngleCos
            );
        } else {
            BroadPointTriangle(
                primitive1,
                primitive0,
                solve_thickness,
                scr,
                define::system::SelfCollisionPointTriangleAngleCos
            );
        }
    }
}

void SelfCollisionConstraint::BroadEdgeEdge(
    const Primitive& primitive0,
    const Primitive& primitive1,
    float thickness,
    float scr
)
{
    float s = 0.0f;
    float t = 0.0f;
    float3 closest_a{};
    float3 closest_b{};
    const float closest_sq_length = ClosestPtSegmentSegment(
        primitive0.old_position[0],
        primitive0.old_position[1],
        primitive1.old_position[0],
        primitive1.old_position[1],
        s,
        t,
        closest_a,
        closest_b
    );
    const float closest_length = std::sqrt(closest_sq_length);
    if (closest_length < 1.0e-9f) {
        return;
    }

    const float3 normal = Scale(Subtract(closest_a, closest_b), 1.0f / closest_length);
    const float3 delta_a = Lerp(
        Subtract(primitive0.next_position[0], primitive0.old_position[0]),
        Subtract(primitive0.next_position[1], primitive0.old_position[1]),
        s
    );
    const float3 delta_b = Lerp(
        Subtract(primitive1.next_position[0], primitive1.old_position[0]),
        Subtract(primitive1.next_position[1], primitive1.old_position[1]),
        t
    );
    const float distance =
        closest_length + Dot(normal, delta_a) - Dot(normal, delta_b);
    if (distance > thickness + scr) {
        return;
    }

    EdgeEdgeContact contact;
    contact.flag_and_team_id0 = primitive0.flag_and_team_id | FlagEnable;
    contact.flag_and_team_id1 = primitive1.flag_and_team_id;
    contact.thickness = thickness;
    contact.s = s;
    contact.t = t;
    contact.normal = normal;
    contact.edge_inverse_mass0 = float2{primitive0.inverse_mass.x, primitive0.inverse_mass.y};
    contact.edge_inverse_mass1 = float2{primitive1.inverse_mass.x, primitive1.inverse_mass.y};
    contact.edge_particle_index0 = int2{primitive0.particle_indices.x, primitive0.particle_indices.y};
    contact.edge_particle_index1 = int2{primitive1.particle_indices.x, primitive1.particle_indices.y};
    edge_edge_contact_array_.Add(contact);
}

void SelfCollisionConstraint::BroadPointTriangle(
    const Primitive& point_primitive,
    const Primitive& triangle_primitive,
    float thickness,
    float scr,
    float angle_cos
)
{
    const float3 delta_point =
        Subtract(point_primitive.next_position.c0, point_primitive.old_position.c0);
    const float3 delta_tri0 =
        Subtract(triangle_primitive.next_position.c0, triangle_primitive.old_position.c0);
    const float3 delta_tri1 =
        Subtract(triangle_primitive.next_position.c1, triangle_primitive.old_position.c1);
    const float3 delta_tri2 =
        Subtract(triangle_primitive.next_position.c2, triangle_primitive.old_position.c2);

    float3 uvw{};
    const float3 closest_point = ClosestPtPointTriangle(
        point_primitive.old_position.c0,
        triangle_primitive.old_position.c0,
        triangle_primitive.old_position.c1,
        triangle_primitive.old_position.c2,
        uvw
    );
    const float3 delta_triangle = Add(
        Add(Scale(delta_tri0, uvw.x), Scale(delta_tri1, uvw.y)),
        Scale(delta_tri2, uvw.z)
    );

    const float3 closest_vector =
        Subtract(closest_point, point_primitive.old_position.c0);
    const float closest_length = Length(closest_vector);
    if (closest_length <= define::system::Epsilon) {
        return;
    }

    float3 normal = Scale(closest_vector, 1.0f / closest_length);
    const float distance =
        closest_length - Dot(normal, delta_point) + Dot(normal, delta_triangle);
    if (distance >= thickness + scr) {
        return;
    }

    const float3 triangle_normal = TriangleNormal(
        triangle_primitive.old_position.c0,
        triangle_primitive.old_position.c1,
        triangle_primitive.old_position.c2
    );
    normal = Normalize(Subtract(point_primitive.old_position.c0, closest_point));
    const float dot = Dot(triangle_normal, normal);
    if (std::abs(dot) < angle_cos) {
        return;
    }

    PointTriangleContact contact;
    contact.flag_and_team_id0 = point_primitive.flag_and_team_id | FlagEnable;
    contact.flag_and_team_id1 = triangle_primitive.flag_and_team_id;
    contact.thickness = thickness;
    contact.sign = dot > 0.0f ? 1.0f : -1.0f;
    contact.point_particle_index = point_primitive.particle_indices.x;
    contact.triangle_particle_index = triangle_primitive.particle_indices;
    contact.point_inverse_mass = point_primitive.inverse_mass.x;
    contact.triangle_inverse_mass = triangle_primitive.inverse_mass;
    point_triangle_contact_array_.Add(contact);
}

void SelfCollisionConstraint::UpdateBroadPhaseContacts(const SimulationManager& simulation_manager)
{
    for (int index = 0; index < edge_edge_contact_array_.Count(); ++index) {
        UpdateEdgeEdgeBroadPhaseContact(edge_edge_contact_array_[index], simulation_manager);
    }
    for (int index = 0; index < point_triangle_contact_array_.Count(); ++index) {
        UpdatePointTriangleBroadPhaseContact(point_triangle_contact_array_[index], simulation_manager);
    }
}

void SelfCollisionConstraint::SolveContacts(SimulationManager& simulation_manager)
{
    for (int iteration = 0; iteration < define::system::SelfCollisionSolverIteration; ++iteration) {
        for (int index = 0; index < edge_edge_contact_array_.Count(); ++index) {
            SolveEdgeEdgeContact(edge_edge_contact_array_[index], simulation_manager);
        }
        for (int index = 0; index < point_triangle_contact_array_.Count(); ++index) {
            SolvePointTriangleContact(point_triangle_contact_array_[index], simulation_manager);
        }
        SolveAggregateBufferAndClear(simulation_manager);
    }
}

void SelfCollisionConstraint::SolveRuntimeSelfCollision(
    int update_index,
    const TeamManager& team_manager,
    SimulationManager& simulation_manager
)
{
    if (!HasPrimitive()) {
        return;
    }

    if (update_index == 0) {
        GenerateBroadPhaseContacts(team_manager, simulation_manager);
    } else {
        UpdateBroadPhaseContacts(simulation_manager);
    }
    SolveContacts(simulation_manager);
    SolveIntersect(team_manager, simulation_manager);
}

void SelfCollisionConstraint::SolveIntersect(
    const TeamManager& team_manager,
    SimulationManager& simulation_manager
)
{
    if (intersect_count_ <= 0) {
        return;
    }

    intersect_flag_array_.Fill(0);
    IntersectUpdatePrimitives(KindEdge, team_manager, simulation_manager);
    IntersectUpdatePrimitives(KindTriangle, team_manager, simulation_manager);

    const int div = define::system::SelfCollisionIntersectDiv;
    const int exec_number = div > 0 ? simulation_manager.SimulationStepCount() % div : 0;

    const auto& edge_processing = simulation_manager.ProcessingSelfEdgeEdges();
    const auto& edge_buffer = edge_processing.Buffer();
    for (int step_index = 0; step_index < edge_processing.Count(); ++step_index) {
        if (div > 0 && step_index % div != exec_number) {
            continue;
        }

        const std::uint32_t pack = edge_buffer[static_cast<std::size_t>(step_index)];
        const int team_id = data::Unpack32Hi(pack);
        const int local_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.flag.TestAny(TeamManager::FlagSelfEdgeTriangleIntersect, 3)) {
            continue;
        }

        const int primitive_index = team_data.self_edge_chunk.start_index + local_index;
        if (!team_data.self_edge_chunk.IsValid()
            || primitive_index < team_data.self_edge_chunk.start_index
            || primitive_index >= team_data.self_edge_chunk.EndIndex()
            || primitive_index >= primitive_array_.Length()) {
            continue;
        }

        const Primitive primitive0 = primitive_array_[primitive_index];
        if (primitive0.sort_index < 0 || primitive0.sort_index >= sort_and_sweep_array_.Length()) {
            continue;
        }
        if (team_data.flag.IsSet(TeamManager::FlagSelfEdgeTriangleIntersect)) {
            SweepIntersectEdgeTriangle(
                primitive0,
                sort_and_sweep_array_[primitive0.sort_index],
                KindEdge,
                team_data.self_triangle_chunk,
                true
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagSyncEdgeTriangleIntersect)
            && team_manager.IsValidTeam(team_data.sync_team_id)) {
            const TeamManager::TeamData& sync_team_data =
                team_manager.GetTeamData(team_data.sync_team_id);
            SweepIntersectEdgeTriangle(
                primitive0,
                sort_and_sweep_array_[primitive0.sort_index],
                KindEdge,
                sync_team_data.self_triangle_chunk,
                false
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagPSyncEdgeTriangleIntersect)) {
            for (int parent_index = 0; parent_index < team_data.sync_parent_team_ids.Length();
                 ++parent_index) {
                const int parent_team_id = team_data.sync_parent_team_ids[parent_index];
                if (!team_manager.IsValidTeam(parent_team_id)) {
                    continue;
                }
                const TeamManager::TeamData& parent_team_data =
                    team_manager.GetTeamData(parent_team_id);
                if (!parent_team_data.flag.IsSet(TeamManager::FlagSyncTriangleEdgeIntersect)) {
                    continue;
                }
                SweepIntersectEdgeTriangle(
                    primitive0,
                    sort_and_sweep_array_[primitive0.sort_index],
                    KindEdge,
                    parent_team_data.self_triangle_chunk,
                    false
                );
            }
        }
    }

    const auto& triangle_processing = simulation_manager.ProcessingSelfTrianglePoints();
    const auto& triangle_buffer = triangle_processing.Buffer();
    for (int step_index = 0; step_index < triangle_processing.Count(); ++step_index) {
        if (div > 0 && step_index % div != exec_number) {
            continue;
        }

        const std::uint32_t pack = triangle_buffer[static_cast<std::size_t>(step_index)];
        const int team_id = data::Unpack32Hi(pack);
        const int local_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.flag.TestAny(TeamManager::FlagSelfTriangleEdgeIntersect, 3)) {
            continue;
        }

        const int primitive_index = team_data.self_triangle_chunk.start_index + local_index;
        if (!team_data.self_triangle_chunk.IsValid()
            || primitive_index < team_data.self_triangle_chunk.start_index
            || primitive_index >= team_data.self_triangle_chunk.EndIndex()
            || primitive_index >= primitive_array_.Length()) {
            continue;
        }

        const Primitive primitive0 = primitive_array_[primitive_index];
        if (primitive0.sort_index < 0 || primitive0.sort_index >= sort_and_sweep_array_.Length()) {
            continue;
        }
        if (team_data.flag.IsSet(TeamManager::FlagSelfTriangleEdgeIntersect)) {
            SweepIntersectEdgeTriangle(
                primitive0,
                sort_and_sweep_array_[primitive0.sort_index],
                KindTriangle,
                team_data.self_edge_chunk,
                true
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagSyncTriangleEdgeIntersect)
            && team_manager.IsValidTeam(team_data.sync_team_id)) {
            const TeamManager::TeamData& sync_team_data =
                team_manager.GetTeamData(team_data.sync_team_id);
            SweepIntersectEdgeTriangle(
                primitive0,
                sort_and_sweep_array_[primitive0.sort_index],
                KindTriangle,
                sync_team_data.self_edge_chunk,
                false
            );
        }
        if (team_data.flag.IsSet(TeamManager::FlagPSyncTriangleEdgeIntersect)) {
            for (int parent_index = 0; parent_index < team_data.sync_parent_team_ids.Length();
                 ++parent_index) {
                const int parent_team_id = team_data.sync_parent_team_ids[parent_index];
                if (!team_manager.IsValidTeam(parent_team_id)) {
                    continue;
                }
                const TeamManager::TeamData& parent_team_data =
                    team_manager.GetTeamData(parent_team_id);
                if (!parent_team_data.flag.IsSet(TeamManager::FlagSyncEdgeTriangleIntersect)) {
                    continue;
                }
                SweepIntersectEdgeTriangle(
                    primitive0,
                    sort_and_sweep_array_[primitive0.sort_index],
                    KindTriangle,
                    parent_team_data.self_edge_chunk,
                    false
                );
            }
        }
    }
}

void SelfCollisionConstraint::UpdateEdgeEdgeBroadPhaseContact(
    EdgeEdgeContact& contact,
    const SimulationManager& simulation_manager
) const
{
    const auto& old_positions = simulation_manager.OldPositions();
    const auto& next_positions = simulation_manager.NextPositions();
    const int particles[4] = {
        contact.edge_particle_index0.x,
        contact.edge_particle_index0.y,
        contact.edge_particle_index1.x,
        contact.edge_particle_index1.y,
    };
    for (int particle_index : particles) {
        if (particle_index < 0
            || particle_index >= old_positions.Length()
            || particle_index >= next_positions.Length()) {
            contact.flag_and_team_id0 &= ~FlagEnable;
            return;
        }
    }

    const float3 old_pos_a0 = old_positions[contact.edge_particle_index0.x];
    const float3 old_pos_a1 = old_positions[contact.edge_particle_index0.y];
    const float3 old_pos_b0 = old_positions[contact.edge_particle_index1.x];
    const float3 old_pos_b1 = old_positions[contact.edge_particle_index1.y];
    const float3 next_pos_a0 = next_positions[contact.edge_particle_index0.x];
    const float3 next_pos_a1 = next_positions[contact.edge_particle_index0.y];
    const float3 next_pos_b0 = next_positions[contact.edge_particle_index1.x];
    const float3 next_pos_b1 = next_positions[contact.edge_particle_index1.y];

    float s = 0.0f;
    float t = 0.0f;
    float3 closest_a{};
    float3 closest_b{};
    const float closest_sq_length = ClosestPtSegmentSegment(
        old_pos_a0,
        old_pos_a1,
        old_pos_b0,
        old_pos_b1,
        s,
        t,
        closest_a,
        closest_b
    );
    const float closest_length = std::sqrt(closest_sq_length);
    if (closest_length < 1.0e-9f) {
        contact.flag_and_team_id0 &= ~FlagEnable;
        return;
    }

    const float3 normal = Scale(Subtract(closest_a, closest_b), 1.0f / closest_length);
    const float3 delta_a = Lerp(Subtract(next_pos_a0, old_pos_a0), Subtract(next_pos_a1, old_pos_a1), s);
    const float3 delta_b = Lerp(Subtract(next_pos_b0, old_pos_b0), Subtract(next_pos_b1, old_pos_b1), t);
    const float distance = closest_length + Dot(normal, delta_a) - Dot(normal, delta_b);
    const float scr = contact.thickness;
    if (distance > contact.thickness + scr) {
        contact.flag_and_team_id0 &= ~FlagEnable;
        return;
    }

    contact.flag_and_team_id0 |= FlagEnable;
    contact.s = s;
    contact.t = t;
    contact.normal = normal;
}

void SelfCollisionConstraint::UpdatePointTriangleBroadPhaseContact(
    PointTriangleContact& contact,
    const SimulationManager& simulation_manager
) const
{
    const auto& old_positions = simulation_manager.OldPositions();
    const auto& next_positions = simulation_manager.NextPositions();
    const int particles[4] = {
        contact.point_particle_index,
        contact.triangle_particle_index.x,
        contact.triangle_particle_index.y,
        contact.triangle_particle_index.z,
    };
    for (int particle_index : particles) {
        if (particle_index < 0
            || particle_index >= old_positions.Length()
            || particle_index >= next_positions.Length()) {
            contact.flag_and_team_id0 &= ~FlagEnable;
            return;
        }
    }

    const float3 old_pos_a = old_positions[contact.point_particle_index];
    const float3 old_pos_b0 = old_positions[contact.triangle_particle_index.x];
    const float3 old_pos_b1 = old_positions[contact.triangle_particle_index.y];
    const float3 old_pos_b2 = old_positions[contact.triangle_particle_index.z];
    const float3 next_pos_a = next_positions[contact.point_particle_index];
    const float3 next_pos_b0 = next_positions[contact.triangle_particle_index.x];
    const float3 next_pos_b1 = next_positions[contact.triangle_particle_index.y];
    const float3 next_pos_b2 = next_positions[contact.triangle_particle_index.z];

    const float3 delta_a = Subtract(next_pos_a, old_pos_a);
    const float3 delta_b0 = Subtract(next_pos_b0, old_pos_b0);
    const float3 delta_b1 = Subtract(next_pos_b1, old_pos_b1);
    const float3 delta_b2 = Subtract(next_pos_b2, old_pos_b2);

    float3 uvw{};
    const float3 closest_point =
        ClosestPtPointTriangle(old_pos_a, old_pos_b0, old_pos_b1, old_pos_b2, uvw);
    const float3 delta_triangle = Add(
        Add(Scale(delta_b0, uvw.x), Scale(delta_b1, uvw.y)),
        Scale(delta_b2, uvw.z)
    );

    const float3 closest_vector = Subtract(closest_point, old_pos_a);
    const float closest_length = Length(closest_vector);
    bool enable = false;
    if (closest_length > define::system::Epsilon) {
        const float3 normal = Scale(closest_vector, 1.0f / closest_length);
        const float distance = closest_length - Dot(normal, delta_a) + Dot(normal, delta_triangle);
        const float scr = contact.thickness;
        enable = distance < contact.thickness + scr;
    }

    if (enable) {
        contact.flag_and_team_id0 |= FlagEnable;
    } else {
        contact.flag_and_team_id0 &= ~FlagEnable;
    }
}

void SelfCollisionConstraint::SolveEdgeEdgeContact(
    const EdgeEdgeContact& contact,
    SimulationManager& simulation_manager
) const
{
    if ((contact.flag_and_team_id0 & FlagEnable) == 0) {
        return;
    }

    auto& next_positions = simulation_manager.NextPositions();
    auto& count_array = simulation_manager.CountArray();
    auto& sum_array = simulation_manager.SumArray();
    const int particles[4] = {
        contact.edge_particle_index0.x,
        contact.edge_particle_index0.y,
        contact.edge_particle_index1.x,
        contact.edge_particle_index1.y,
    };
    for (int particle_index : particles) {
        if (particle_index < 0 || particle_index >= next_positions.Length()) {
            return;
        }
    }

    const float3 next_pos_a0 = next_positions[contact.edge_particle_index0.x];
    const float3 next_pos_a1 = next_positions[contact.edge_particle_index0.y];
    const float3 next_pos_b0 = next_positions[contact.edge_particle_index1.x];
    const float3 next_pos_b1 = next_positions[contact.edge_particle_index1.y];
    const float s = contact.s;
    const float t = contact.t;
    const float3 normal = contact.normal;
    const float thickness = contact.thickness;

    const float3 a = Lerp(next_pos_a0, next_pos_a1, s);
    const float3 b = Lerp(next_pos_b0, next_pos_b1, t);
    const float distance = Dot(normal, Subtract(a, b));
    if (distance > thickness) {
        return;
    }

    const float inv_mass_a0 = contact.edge_inverse_mass0.x;
    const float inv_mass_a1 = contact.edge_inverse_mass0.y;
    const float inv_mass_b0 = contact.edge_inverse_mass1.x;
    const float inv_mass_b1 = contact.edge_inverse_mass1.y;
    const float constraint = thickness - distance;
    const float b0 = 1.0f - s;
    const float b1 = s;
    const float b2 = 1.0f - t;
    const float b3 = t;
    const float scale_sum =
        inv_mass_a0 * b0 * b0
        + inv_mass_a1 * b1 * b1
        + inv_mass_b0 * b2 * b2
        + inv_mass_b1 * b3 * b3;
    if (scale_sum == 0.0f) {
        return;
    }

    const float scale = constraint / scale_sum;
    const float3 add_a0 = Scale(Scale(normal, b0), scale * inv_mass_a0);
    const float3 add_a1 = Scale(Scale(normal, b1), scale * inv_mass_a1);
    const float3 add_b0 = Scale(Scale(normal, -b2), scale * inv_mass_b0);
    const float3 add_b1 = Scale(Scale(normal, -b3), scale * inv_mass_b1);

    if ((contact.flag_and_team_id0 & FlagFix0) == 0) {
        AddAggregateFloat3(count_array, sum_array, contact.edge_particle_index0.x, add_a0);
    }
    if ((contact.flag_and_team_id0 & FlagFix1) == 0) {
        AddAggregateFloat3(count_array, sum_array, contact.edge_particle_index0.y, add_a1);
    }
    if ((contact.flag_and_team_id1 & FlagFix0) == 0) {
        AddAggregateFloat3(count_array, sum_array, contact.edge_particle_index1.x, add_b0);
    }
    if ((contact.flag_and_team_id1 & FlagFix1) == 0) {
        AddAggregateFloat3(count_array, sum_array, contact.edge_particle_index1.y, add_b1);
    }
}

void SelfCollisionConstraint::SolvePointTriangleContact(
    const PointTriangleContact& contact,
    SimulationManager& simulation_manager
) const
{
    if ((contact.flag_and_team_id0 & FlagEnable) == 0) {
        return;
    }

    auto& next_positions = simulation_manager.NextPositions();
    auto& count_array = simulation_manager.CountArray();
    auto& sum_array = simulation_manager.SumArray();
    const int particles[4] = {
        contact.point_particle_index,
        contact.triangle_particle_index.x,
        contact.triangle_particle_index.y,
        contact.triangle_particle_index.z,
    };
    for (int particle_index : particles) {
        if (particle_index < 0 || particle_index >= next_positions.Length()) {
            return;
        }
    }

    const int3 triangle_particles = contact.triangle_particle_index;
    const float3 next_pos0 = next_positions[triangle_particles.x];
    const float3 next_pos1 = next_positions[triangle_particles.y];
    const float3 next_pos2 = next_positions[triangle_particles.z];
    const float3 triangle_normal = TriangleNormal(next_pos0, next_pos1, next_pos2);
    const int point_particle_index = contact.point_particle_index;
    const float3 next_pos = next_positions[point_particle_index];

    float3 uvw{};
    (void)ClosestPtPointTriangle(next_pos, next_pos0, next_pos1, next_pos2, uvw);
    const float3 normal = Scale(triangle_normal, contact.sign);
    const float distance = Dot(normal, Subtract(next_pos, next_pos0));
    if (distance >= contact.thickness) {
        return;
    }

    const float constraint = distance - contact.thickness;
    const float point_inverse_mass = contact.point_inverse_mass;
    const float3 triangle_inverse_mass = contact.triangle_inverse_mass;
    float scale =
        point_inverse_mass
        + triangle_inverse_mass.x * uvw.x * uvw.x
        + triangle_inverse_mass.y * uvw.y * uvw.y
        + triangle_inverse_mass.z * uvw.z * uvw.z;
    if (scale == 0.0f) {
        return;
    }
    scale = constraint / scale;

    const float3 point_add = Scale(normal, -scale * point_inverse_mass);
    const float3 tri_add0 = Scale(normal, scale * triangle_inverse_mass.x * uvw.x);
    const float3 tri_add1 = Scale(normal, scale * triangle_inverse_mass.y * uvw.y);
    const float3 tri_add2 = Scale(normal, scale * triangle_inverse_mass.z * uvw.z);

    if ((contact.flag_and_team_id0 & FlagFix0) == 0) {
        AddAggregateFloat3(count_array, sum_array, point_particle_index, point_add);
    }
    if ((contact.flag_and_team_id1 & FlagFix0) == 0) {
        AddAggregateFloat3(count_array, sum_array, triangle_particles.x, tri_add0);
    }
    if ((contact.flag_and_team_id1 & FlagFix1) == 0) {
        AddAggregateFloat3(count_array, sum_array, triangle_particles.y, tri_add1);
    }
    if ((contact.flag_and_team_id1 & FlagFix2) == 0) {
        AddAggregateFloat3(count_array, sum_array, triangle_particles.z, tri_add2);
    }
}

void SelfCollisionConstraint::SolveAggregateBufferAndClear(SimulationManager& simulation_manager) const
{
    auto& next_positions = simulation_manager.NextPositions();
    auto& count_array = simulation_manager.CountArray();
    auto& sum_array = simulation_manager.SumArray();
    const auto& self_particles = simulation_manager.ProcessingSelfParticles();
    const auto& self_particle_buffer = self_particles.Buffer();

    for (int step_index = 0; step_index < self_particles.Count(); ++step_index) {
        const int particle_index = self_particle_buffer[static_cast<std::size_t>(step_index)];
        if (particle_index < 0 || particle_index >= next_positions.Length()) {
            continue;
        }

        const int count = particle_index < count_array.Length() ? count_array[particle_index] : 0;
        const int data_index = particle_index * 3;
        if (count > 0) {
            const float3 add = ReadAverageFloat3(count_array, sum_array, particle_index);
            next_positions[particle_index] = Add(next_positions[particle_index], add);

            count_array[particle_index] = 0;
            if (data_index + 2 < sum_array.Length()) {
                sum_array[data_index] = 0;
                sum_array[data_index + 1] = 0;
                sum_array[data_index + 2] = 0;
            }
        }
    }
}

void SelfCollisionConstraint::SweepIntersectEdgeTriangle(
    const Primitive& primitive0,
    const SortData& sort_data0,
    std::uint32_t main_kind,
    const DataChunk& sub_chunk,
    bool connection_check
)
{
    if (!sub_chunk.IsValid()) {
        return;
    }

    int sort_index = BinarySearchSortAndSweep(sort_data0, sub_chunk);
    const float first_end = sort_data0.first_min_max.y;
    const int end_index = sub_chunk.EndIndex();
    while (sort_index < end_index) {
        const SortData sort_data1 = sort_and_sweep_array_[sort_index];
        ++sort_index;

        if (sort_data1.first_min_max.x > first_end) {
            break;
        }
        if (sort_data1.second_min_max.y < sort_data0.second_min_max.x
            || sort_data1.second_min_max.x > sort_data0.second_min_max.y) {
            continue;
        }
        if (sort_data1.third_min_max.y < sort_data0.third_min_max.x
            || sort_data1.third_min_max.x > sort_data0.third_min_max.y) {
            continue;
        }

        if (sort_data1.primitive_index < 0 || sort_data1.primitive_index >= primitive_array_.Length()) {
            continue;
        }
        const Primitive primitive1 = primitive_array_[sort_data1.primitive_index];
        if (connection_check && primitive0.AnyParticle(primitive1)) {
            continue;
        }
        if ((primitive0.flag_and_team_id & FlagAllFix) != 0
            && (primitive1.flag_and_team_id & FlagAllFix) != 0) {
            continue;
        }

        if (main_kind == KindEdge) {
            IntersectEdgeTriangle(primitive0, primitive1);
        } else {
            IntersectEdgeTriangle(primitive1, primitive0);
        }
    }
}

void SelfCollisionConstraint::IntersectEdgeTriangle(
    const Primitive& edge_primitive,
    const Primitive& triangle_primitive
)
{
    float3 p = edge_primitive.next_position.c0;
    const float3 q = edge_primitive.next_position.c1;
    float3 qp = Subtract(p, q);

    const float3 a = triangle_primitive.next_position.c0;
    const float3 b = triangle_primitive.next_position.c1;
    const float3 c = triangle_primitive.next_position.c2;
    const float3 ac = Subtract(c, a);
    const float3 ab = Subtract(b, a);
    const float3 normal = Cross(ab, ac);
    float d = Dot(qp, normal);

    if (std::abs(d) < define::system::Epsilon) {
        return;
    }
    if (d < 0.0f) {
        p = edge_primitive.next_position.c1;
        qp = Scale(qp, -1.0f);
        d = -d;
    }

    const float3 ap = Subtract(p, a);
    const float t = Dot(ap, normal);
    if (t < 0.0f || t > d) {
        return;
    }

    const float3 e = Cross(qp, ap);
    const float v = Dot(ac, e);
    if (v < 0.0f || v > d) {
        return;
    }
    const float w = -Dot(ab, e);
    if (w < 0.0f || v + w > d) {
        return;
    }

    const int particles[5] = {
        edge_primitive.particle_indices.x,
        edge_primitive.particle_indices.y,
        triangle_primitive.particle_indices.x,
        triangle_primitive.particle_indices.y,
        triangle_primitive.particle_indices.z,
    };
    for (int particle_index : particles) {
        if (particle_index >= 0 && particle_index < intersect_flag_array_.Length()) {
            intersect_flag_array_[particle_index] = 1;
        }
    }
}

void SelfCollisionConstraint::RecalculateIntersectCount(const TeamManager& team_manager)
{
    int count = 0;
    for (int team_id = 1; team_id < team_manager.TeamCount(); ++team_id) {
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }
        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (team_data.flag.TestAny(TeamManager::FlagSelfEdgeTriangleIntersect, 6)) {
            ++count;
        }
    }
    intersect_count_ = count;
}

const ExNativeArray<SelfCollisionConstraint::Primitive>& SelfCollisionConstraint::Primitives() const
{
    return primitive_array_;
}

ExNativeArray<SelfCollisionConstraint::Primitive>& SelfCollisionConstraint::Primitives()
{
    return primitive_array_;
}

const ExNativeArray<SelfCollisionConstraint::SortData>& SelfCollisionConstraint::SortAndSweep() const
{
    return sort_and_sweep_array_;
}

ExNativeArray<SelfCollisionConstraint::SortData>& SelfCollisionConstraint::SortAndSweep()
{
    return sort_and_sweep_array_;
}

const ExNativeArray<SelfCollisionConstraint::EdgeEdgeContact>&
SelfCollisionConstraint::EdgeEdgeContacts() const
{
    return edge_edge_contact_array_;
}

const ExNativeArray<SelfCollisionConstraint::PointTriangleContact>&
SelfCollisionConstraint::PointTriangleContacts() const
{
    return point_triangle_contact_array_;
}

}  // namespace hocloth::mc2
