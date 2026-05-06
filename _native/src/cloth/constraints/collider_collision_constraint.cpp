#include "hocloth/cloth/constraints/collider_collision_constraint.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>

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

void AddBufferFloat3(ExNativeArray<int>& buffer, int index, const float3& value)
{
    const int data_index = index * 3;
    if (index < 0 || data_index + 2 >= buffer.Length()) {
        return;
    }

    buffer[data_index] += ToFixed(value.x);
    buffer[data_index + 1] += ToFixed(value.y);
    buffer[data_index + 2] += ToFixed(value.z);
}

void MaxBufferFloat(ExNativeArray<int>& buffer, int index, float value)
{
    if (index < 0 || index >= buffer.Length()) {
        return;
    }
    buffer[index] = std::max(buffer[index], ToFixed(value));
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

float3 ReadFloat3(const ExNativeArray<int>& buffer, int index)
{
    const int data_index = index * 3;
    if (index < 0 || data_index + 2 >= buffer.Length()) {
        return float3{};
    }
    return Scale(
        float3{
            static_cast<float>(buffer[data_index]),
            static_cast<float>(buffer[data_index + 1]),
            static_cast<float>(buffer[data_index + 2]),
        },
        InterlockToFloat
    );
}

float ReadFloat(const ExNativeArray<int>& buffer, int index)
{
    if (index < 0 || index >= buffer.Length()) {
        return 0.0f;
    }
    return static_cast<float>(buffer[index]) * InterlockToFloat;
}

}  // namespace

void ColliderCollisionConstraint::Dispose()
{
    temp_friction_array_.Dispose();
    temp_normal_array_.Dispose();
}

void ColliderCollisionConstraint::WorkBufferUpdate(
    int particle_count,
    int edge_collider_collision_count
)
{
    if (edge_collider_collision_count <= 0) {
        return;
    }

    temp_friction_array_.Dispose();
    temp_normal_array_.Dispose();
    temp_friction_array_ = ExNativeArray<int>(particle_count);
    temp_normal_array_ = ExNativeArray<int>(particle_count * 3);
    temp_friction_array_.AddRange(particle_count, 0);
    temp_normal_array_.AddRange(particle_count * 3, 0);
}

void ColliderCollisionConstraint::Solve(
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    const ColliderManager& collider_manager,
    SimulationManager& simulation_manager
)
{
    // Ported from MC2 PointColliderCollisionConstraintJob and EdgeColliderCollisionConstraintJob.
    const ExProcessingList<int>& step_particles = simulation_manager.ProcessingStepParticles();
    const auto& step_buffer = step_particles.Buffer();
    const auto& team_ids = simulation_manager.TeamIds();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& vertex_depths = virtual_mesh_manager.VertexDepths();
    const auto& edge_team_ids = virtual_mesh_manager.EdgeTeamIds();
    const auto& edges = virtual_mesh_manager.Edges();
    const auto& collider_flags = collider_manager.Flags();
    const auto& collider_work_array = collider_manager.WorkDataArray();

    auto& next_positions = simulation_manager.NextPositions();
    auto& frictions = simulation_manager.Frictions();
    auto& collision_normals = simulation_manager.CollisionNormals();
    auto& velocity_positions = simulation_manager.VelocityPositions();
    const auto& base_positions = simulation_manager.BasePositions();
    auto& count_array = simulation_manager.CountArray();
    auto& sum_array = simulation_manager.SumArray();

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
        if (team_data.ColliderCount() == 0) {
            continue;
        }

        const ClothParameters& parameters = team_manager.GetParameters(team_id);
        if (parameters.collider_collision_constraint.mode != ColliderCollisionMode::Point) {
            continue;
        }

        const int local_index = particle_index - team_data.particle_chunk.start_index;
        const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
        if (local_index < 0
            || vertex_index < 0
            || vertex_index >= attributes.Length()
            || vertex_index >= vertex_depths.Length()) {
            continue;
        }

        const VertexAttribute attr = attributes[vertex_index];
        if (attr.IsInvalid() || attr.IsDisableCollision()) {
            continue;
        }

        const bool is_spring = team_data.IsSpring();
        if (!attr.IsMove() && !is_spring) {
            continue;
        }

        const float depth = vertex_depths[vertex_index];
        float3 next_position = next_positions[particle_index];
        const float3 base_position = is_spring ? base_positions[particle_index] : float3{};
        float radius = std::max(MC2EvaluateCurve(parameters.radius_curve_data, depth), 0.0001f);
        radius *= team_data.scale_ratio;

        float min_distance = std::numeric_limits<float>::max();
        int collision_collider_id = -1;
        float3 collision_normal{};

        float3 add_position{};
        int add_count = 0;
        float3 add_normal{};

        const float collision_friction_range = radius;
        AABB particle_bounds{
            Subtract(next_position, float3{radius, radius, radius}),
            Add(next_position, float3{radius, radius, radius}),
        };
        Expand(particle_bounds, collision_friction_range);

        const float max_length = is_spring
            ? std::max(
                  MC2EvaluateCurve(parameters.collider_collision_constraint.limit_distance, depth),
                  0.0001f
              ) * team_data.scale_ratio
            : -1.0f;

        int collider_index = team_data.collider_chunk.start_index;
        for (int collider_offset = 0; collider_offset < team_data.ColliderCount();
             ++collider_offset, ++collider_index) {
            if (collider_index < 0
                || collider_index >= collider_flags.Length()
                || collider_index >= collider_work_array.Length()) {
                continue;
            }

            const BitFlag8 collider_flag = collider_flags[collider_index];
            if (!collider_flag.IsSet(ColliderManager::FlagValid)
                || !collider_flag.IsSet(ColliderManager::FlagEnable)) {
                continue;
            }

            const ColliderManager::ColliderType collider_type =
                ColliderManager::TypeFromFlag(collider_flag);
            const ColliderManager::WorkData& collider_work =
                collider_work_array[collider_index];
            float3 solved_position = next_position;
            float3 normal{};
            float distance = 100.0f;

            switch (collider_type) {
            case ColliderManager::ColliderType::Sphere:
                distance = PointSphereColliderDetection(
                    solved_position,
                    base_position,
                    radius,
                    particle_bounds,
                    collider_work,
                    max_length,
                    normal
                );
                break;
            case ColliderManager::ColliderType::CapsuleXCenter:
            case ColliderManager::ColliderType::CapsuleYCenter:
            case ColliderManager::ColliderType::CapsuleZCenter:
            case ColliderManager::ColliderType::CapsuleXStart:
            case ColliderManager::ColliderType::CapsuleYStart:
            case ColliderManager::ColliderType::CapsuleZStart:
                distance = PointCapsuleColliderDetection(
                    solved_position,
                    radius,
                    particle_bounds,
                    collider_work,
                    normal
                );
                break;
            case ColliderManager::ColliderType::Plane:
                distance = PointPlaneColliderDetection(
                    solved_position,
                    radius,
                    collider_work,
                    normal
                );
                break;
            default:
                break;
            }

            if (distance <= 0.0f) {
                add_position = Add(add_position, Subtract(solved_position, next_position));
                add_normal = Add(add_normal, normal);
                ++add_count;
            }

            if (distance <= collision_friction_range) {
                collision_collider_id = collider_index;
                collision_normal = Add(collision_normal, normal);
                min_distance = std::min(min_distance, distance);
            }
        }

        if (add_count > 0) {
            add_normal = Scale(add_normal, 1.0f / static_cast<float>(add_count));
            const float normal_length = Length(add_normal);
            if (normal_length <= define::system::Epsilon) {
                add_position = float3{};
            } else {
                const float t = std::min(normal_length, 1.0f);
                add_position = Scale(add_position, 1.0f / static_cast<float>(add_count));
                next_position = Add(next_position, Scale(add_position, t));
            }
        }

        if (collision_collider_id >= 0
            && collision_friction_range > 0.0f
            && LengthSquared(collision_normal) > 1.0e-6f) {
            const float friction = 1.0f - Clamp01(min_distance / collision_friction_range);
            frictions[particle_index] = std::max(friction, frictions[particle_index]);
            collision_normal = Normalize(collision_normal);
        }

        collision_normals[particle_index] = collision_normal;
        next_positions[particle_index] = next_position;

        if (is_spring && add_count > 0) {
            velocity_positions[particle_index] =
                Add(velocity_positions[particle_index], add_position);
        }
    }

    const ExProcessingList<int>& step_edges = simulation_manager.ProcessingStepEdgeCollision();
    const auto& edge_step_buffer = step_edges.Buffer();
    for (int step_index = 0; step_index < step_edges.Count(); ++step_index) {
        const int edge_index = edge_step_buffer[static_cast<std::size_t>(step_index)];
        if (edge_index < 0 || edge_index >= edge_team_ids.Length() || edge_index >= edges.Length()) {
            continue;
        }

        const int team_id = edge_team_ids[edge_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (team_data.ColliderCount() == 0) {
            continue;
        }

        const ClothParameters& parameters = team_manager.GetParameters(team_id);
        if (parameters.collider_collision_constraint.mode != ColliderCollisionMode::Edge) {
            continue;
        }

        const int2 edge = edges[edge_index];
        const int vertex_start = team_data.proxy_common_chunk.start_index;
        const int particle_start = team_data.particle_chunk.start_index;
        const int vertex_indices[2] = {vertex_start + edge.x, vertex_start + edge.y};
        const int particle_indices[2] = {particle_start + edge.x, particle_start + edge.y};
        if (vertex_indices[0] < 0 || vertex_indices[1] < 0
            || vertex_indices[0] >= attributes.Length()
            || vertex_indices[1] >= attributes.Length()
            || vertex_indices[0] >= vertex_depths.Length()
            || vertex_indices[1] >= vertex_depths.Length()
            || particle_indices[0] < 0 || particle_indices[1] < 0
            || particle_indices[0] >= next_positions.Length()
            || particle_indices[1] >= next_positions.Length()) {
            continue;
        }

        const VertexAttribute attr0 = attributes[vertex_indices[0]];
        const VertexAttribute attr1 = attributes[vertex_indices[1]];
        if (!attr0.IsMove() && !attr1.IsMove()) {
            continue;
        }

        float3 edge_positions[2] = {
            next_positions[particle_indices[0]],
            next_positions[particle_indices[1]],
        };
        float radius[2] = {
            MC2EvaluateCurve(parameters.radius_curve_data, vertex_depths[vertex_indices[0]]),
            MC2EvaluateCurve(parameters.radius_curve_data, vertex_depths[vertex_indices[1]]),
        };
        radius[0] *= team_data.scale_ratio;
        radius[1] *= team_data.scale_ratio;

        const float collision_friction_range = (radius[0] + radius[1]) * 0.5f;
        AABB edge_bounds{
            Subtract(edge_positions[0], float3{radius[0], radius[0], radius[0]}),
            Add(edge_positions[0], float3{radius[0], radius[0], radius[0]}),
        };
        AABB edge_bounds1{
            Subtract(edge_positions[1], float3{radius[1], radius[1], radius[1]}),
            Add(edge_positions[1], float3{radius[1], radius[1], radius[1]}),
        };
        Encapsulate(edge_bounds, edge_bounds1);
        Expand(edge_bounds, collision_friction_range);

        float min_distance = std::numeric_limits<float>::max();
        int collision_collider_id = -1;
        float3 collision_normal{};
        float3 add_positions[2]{};
        int add_count = 0;
        float3 add_normal{};

        int collider_index = team_data.collider_chunk.start_index;
        for (int collider_offset = 0; collider_offset < team_data.ColliderCount();
             ++collider_offset, ++collider_index) {
            if (collider_index < 0
                || collider_index >= collider_flags.Length()
                || collider_index >= collider_work_array.Length()) {
                continue;
            }

            const BitFlag8 collider_flag = collider_flags[collider_index];
            if (!collider_flag.IsSet(ColliderManager::FlagValid)
                || !collider_flag.IsSet(ColliderManager::FlagEnable)) {
                continue;
            }

            float3 solved_positions[2] = {edge_positions[0], edge_positions[1]};
            float3 normal{};
            float distance = 100.0f;
            const ColliderManager::ColliderType collider_type =
                ColliderManager::TypeFromFlag(collider_flag);
            const ColliderManager::WorkData& collider_work =
                collider_work_array[collider_index];

            switch (collider_type) {
            case ColliderManager::ColliderType::Sphere:
                distance = EdgeSphereColliderDetection(
                    solved_positions,
                    radius,
                    edge_bounds,
                    collision_friction_range,
                    collider_work,
                    normal
                );
                break;
            case ColliderManager::ColliderType::CapsuleXCenter:
            case ColliderManager::ColliderType::CapsuleYCenter:
            case ColliderManager::ColliderType::CapsuleZCenter:
            case ColliderManager::ColliderType::CapsuleXStart:
            case ColliderManager::ColliderType::CapsuleYStart:
            case ColliderManager::ColliderType::CapsuleZStart:
                distance = EdgeCapsuleColliderDetection(
                    solved_positions,
                    radius,
                    edge_bounds,
                    collision_friction_range,
                    collider_work,
                    normal
                );
                break;
            case ColliderManager::ColliderType::Plane:
                distance = EdgePlaneColliderDetection(
                    solved_positions,
                    radius,
                    collider_work,
                    normal
                );
                break;
            default:
                break;
            }

            if (distance <= 0.0f) {
                add_positions[0] = Add(add_positions[0], Subtract(solved_positions[0], edge_positions[0]));
                add_positions[1] = Add(add_positions[1], Subtract(solved_positions[1], edge_positions[1]));
                add_normal = Add(add_normal, normal);
                ++add_count;
            }

            if (distance <= collision_friction_range) {
                collision_collider_id = collider_index;
                collision_normal = Add(collision_normal, normal);
                min_distance = std::min(min_distance, distance);
            }
        }

        if (add_count > 0) {
            add_normal = Scale(add_normal, 1.0f / static_cast<float>(add_count));
            const float normal_length = Length(add_normal);
            if (normal_length > define::system::Epsilon) {
                const float scale =
                    std::min(normal_length, 1.0f) / static_cast<float>(add_count);
                add_positions[0] = Scale(add_positions[0], scale);
                add_positions[1] = Scale(add_positions[1], scale);
                AddAggregateFloat3(count_array, sum_array, particle_indices[0], add_positions[0]);
                AddAggregateFloat3(count_array, sum_array, particle_indices[1], add_positions[1]);
            }
        }

        if (collision_collider_id >= 0
            && collision_friction_range > 0.0f
            && LengthSquared(collision_normal) > 1.0e-6f) {
            const float friction = 1.0f - Clamp01(min_distance / collision_friction_range);
            MaxBufferFloat(temp_friction_array_, particle_indices[0], friction);
            MaxBufferFloat(temp_friction_array_, particle_indices[1], friction);

            collision_normal = Normalize(collision_normal);
            AddBufferFloat3(temp_normal_array_, particle_indices[0], collision_normal);
            AddBufferFloat3(temp_normal_array_, particle_indices[1], collision_normal);
        }
    }

    if (step_edges.Count() > 0) {
        SolveEdgeBufferAndClear(simulation_manager);
    }
}

float ColliderCollisionConstraint::PointSphereColliderDetection(
    float3& next_position,
    const float3& base_position,
    float radius,
    const AABB& particle_bounds,
    const ColliderManager::WorkData& collider_work,
    float max_length,
    float3& normal
)
{
    normal = float3{};
    if (!Overlaps(particle_bounds, collider_work.aabb)) {
        return std::numeric_limits<float>::max();
    }

    const float3 old_position = next_position;
    const float3 collider_old_position = collider_work.old_positions[0];
    const float3 collider_position = collider_work.next_positions[0];
    const float collider_radius = collider_work.radius.x;

    const float3 v = Subtract(next_position, collider_old_position);
    const float3 n = Normalize(v);
    const float3 plane_position =
        Add(collider_position, Scale(n, collider_radius + radius));
    normal = n;

    float distance = IntersectPointPlaneDist(plane_position, n, next_position, next_position);
    if (max_length > 0.0f) {
        next_position = ClampDistance(base_position, next_position, max_length);

        const float length = Distance(base_position, next_position);
        const float t = Clamp01(length / radius) * 0.85f;
        next_position = Lerp(next_position, old_position, t);
        distance *= 3.0f;
    }

    return distance;
}

float ColliderCollisionConstraint::PointPlaneColliderDetection(
    float3& next_position,
    float radius,
    const ColliderManager::WorkData& collider_work,
    float3& normal
)
{
    const float3 collider_position = collider_work.next_positions[0];
    const float3 n = collider_work.old_positions[0];
    normal = n;
    return IntersectPointPlaneDist(
        Add(collider_position, Scale(n, radius)),
        n,
        next_position,
        next_position
    );
}

float ColliderCollisionConstraint::PointCapsuleColliderDetection(
    float3& next_position,
    float radius,
    const AABB& particle_bounds,
    const ColliderManager::WorkData& collider_work,
    float3& normal
)
{
    normal = float3{};
    if (!Overlaps(particle_bounds, collider_work.aabb)) {
        return std::numeric_limits<float>::max();
    }

    const float3 start_old_position = collider_work.old_positions[0];
    const float3 end_old_position = collider_work.old_positions[1];
    const float3 start_position = collider_work.next_positions[0];
    const float3 end_position = collider_work.next_positions[1];
    const float start_radius = collider_work.radius.x;
    const float end_radius = collider_work.radius.y;

    const float t =
        ClosestPtPointSegmentRatio(next_position, start_old_position, end_old_position);
    const float collider_radius = start_radius + (end_radius - start_radius) * t;
    const float3 old_segment_position = Lerp(start_old_position, end_old_position, t);
    const float3 old_vector = Subtract(next_position, old_segment_position);
    const float3 local_vector = Rotate(collider_work.inverse_old_rotation, old_vector);

    const float3 segment_position = Lerp(start_position, end_position, t);
    const float3 vector = Rotate(collider_work.rotation, local_vector);
    const float3 n = Normalize(vector);
    const float3 plane_position =
        Add(segment_position, Scale(n, collider_radius + radius));
    normal = n;

    return IntersectPointPlaneDist(plane_position, n, next_position, next_position);
}

float ColliderCollisionConstraint::EdgeSphereColliderDetection(
    float3 next_positions[2],
    const float radius[2],
    const AABB& edge_bounds,
    float collision_friction_range,
    const ColliderManager::WorkData& collider_work,
    float3& normal
)
{
    normal = float3{};
    if (!Overlaps(edge_bounds, collider_work.aabb)) {
        return std::numeric_limits<float>::max();
    }

    const float3 collider_old_position = collider_work.old_positions[0];
    const float3 collider_position = collider_work.next_positions[0];
    const float collider_radius = collider_work.radius.x;

    const float s =
        ClosestPtPointSegmentRatio(collider_old_position, next_positions[0], next_positions[1]);
    const float3 closest = Lerp(next_positions[0], next_positions[1], s);
    float3 v = Subtract(closest, collider_old_position);
    const float closest_length = Length(v);
    if (closest_length < 1.0e-9f) {
        return std::numeric_limits<float>::max();
    }

    const float3 n = Scale(v, 1.0f / closest_length);
    normal = n;

    const float3 collider_delta = Subtract(collider_position, collider_old_position);
    const float l1 = Dot(n, collider_delta);
    float length = closest_length - l1;
    const float edge_radius = radius[0] + (radius[1] - radius[0]) * s;
    const float thickness = edge_radius + collider_radius;
    if (length > thickness + collision_friction_range) {
        return std::numeric_limits<float>::max();
    }

    v = Subtract(closest, collider_position);
    length = Dot(n, v);
    if (length > thickness) {
        return length - thickness;
    }

    const float constraint = thickness - length;
    const float b[2] = {1.0f - s, s};
    const float denominator = b[0] * b[0] + b[1] * b[1];
    if (denominator == 0.0f) {
        return std::numeric_limits<float>::max();
    }

    const float scale = constraint / denominator;
    next_positions[0] = Add(next_positions[0], Scale(n, b[0] * scale));
    next_positions[1] = Add(next_positions[1], Scale(n, b[1] * scale));
    return -constraint;
}

float ColliderCollisionConstraint::EdgeCapsuleColliderDetection(
    float3 next_positions[2],
    const float radius[2],
    const AABB& edge_bounds,
    float collision_friction_range,
    const ColliderManager::WorkData& collider_work,
    float3& normal
)
{
    normal = float3{};
    if (!Overlaps(edge_bounds, collider_work.aabb)) {
        return std::numeric_limits<float>::max();
    }

    const float3 start_old_position = collider_work.old_positions[0];
    const float3 end_old_position = collider_work.old_positions[1];
    const float3 start_position = collider_work.next_positions[0];
    const float3 end_position = collider_work.next_positions[1];
    const float start_radius = collider_work.radius.x;
    const float end_radius = collider_work.radius.y;

    float s = 0.0f;
    float t = 0.0f;
    float3 closest_a{};
    float3 closest_b{};
    float closest_sq_length = ClosestPtSegmentSegment(
        next_positions[0],
        next_positions[1],
        start_old_position,
        end_old_position,
        s,
        t,
        closest_a,
        closest_b
    );
    float closest_length = std::sqrt(closest_sq_length);
    if (closest_length < 1.0e-9f) {
        return std::numeric_limits<float>::max();
    }

    float3 v = Subtract(closest_a, closest_b);
    float3 n = Scale(v, 1.0f / closest_length);
    normal = n;

    if (start_radius != end_radius) {
        const float3 shifted_start = Add(start_old_position, Scale(n, start_radius));
        const float3 shifted_end = Add(end_old_position, Scale(n, end_radius));
        ClosestPtSegmentSegment2(
            next_positions[0],
            next_positions[1],
            shifted_start,
            shifted_end,
            s,
            t
        );

        closest_a = Lerp(next_positions[0], next_positions[1], s);
        closest_b = Lerp(start_old_position, end_old_position, t);
        v = Subtract(closest_a, closest_b);
        closest_length = Length(v);
        if (closest_length < 1.0e-9f) {
            return std::numeric_limits<float>::max();
        }
        n = Scale(v, 1.0f / closest_length);
        normal = n;
    }

    const float3 delta_start = Subtract(start_position, start_old_position);
    const float3 delta_end = Subtract(end_position, end_old_position);
    const float3 collider_delta = Lerp(delta_start, delta_end, t);
    const float l1 = Dot(n, collider_delta);
    float length = closest_length - l1;

    const float edge_radius = radius[0] + (radius[1] - radius[0]) * s;
    const float collider_radius = start_radius + (end_radius - start_radius) * t;
    const float thickness = edge_radius + collider_radius;
    if (length > thickness + collision_friction_range) {
        return std::numeric_limits<float>::max();
    }

    const float3 collider_position = Lerp(start_position, end_position, t);
    v = Subtract(closest_a, collider_position);
    length = Dot(n, v);
    if (length > thickness) {
        return length - thickness;
    }

    const float constraint = thickness - length;
    const float b[2] = {1.0f - s, s};
    const float denominator = b[0] * b[0] + b[1] * b[1];
    if (denominator == 0.0f) {
        return std::numeric_limits<float>::max();
    }

    const float scale = constraint / denominator;
    next_positions[0] = Add(next_positions[0], Scale(n, b[0] * scale));
    next_positions[1] = Add(next_positions[1], Scale(n, b[1] * scale));
    return -constraint;
}

float ColliderCollisionConstraint::EdgePlaneColliderDetection(
    float3 next_positions[2],
    const float radius[2],
    const ColliderManager::WorkData& collider_work,
    float3& normal
)
{
    const float3 collider_position = collider_work.next_positions[0];
    const float3 n = collider_work.old_positions[0];
    normal = n;

    const float distance0 = IntersectPointPlaneDist(
        Add(collider_position, Scale(n, radius[0])),
        n,
        next_positions[0],
        next_positions[0]
    );
    const float distance1 = IntersectPointPlaneDist(
        Add(collider_position, Scale(n, radius[1])),
        n,
        next_positions[1],
        next_positions[1]
    );
    return std::min(distance0, distance1);
}

void ColliderCollisionConstraint::SolveEdgeBufferAndClear(SimulationManager& simulation_manager)
{
    auto& next_positions = simulation_manager.NextPositions();
    auto& frictions = simulation_manager.Frictions();
    auto& collision_normals = simulation_manager.CollisionNormals();
    auto& count_array = simulation_manager.CountArray();
    auto& sum_array = simulation_manager.SumArray();

    const ExProcessingList<int>& step_particles = simulation_manager.ProcessingStepParticles();
    const auto& step_buffer = step_particles.Buffer();
    for (int step_index = 0; step_index < step_particles.Count(); ++step_index) {
        const int particle_index = step_buffer[static_cast<std::size_t>(step_index)];
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

        const float friction = ReadFloat(temp_friction_array_, particle_index);
        if (friction > 0.0f && particle_index < frictions.Length() && friction > frictions[particle_index]) {
            frictions[particle_index] = friction;
            temp_friction_array_[particle_index] = 0;
        }

        const float3 normal = ReadFloat3(temp_normal_array_, particle_index);
        if (LengthSquared(normal) > 0.0f && particle_index < collision_normals.Length()) {
            collision_normals[particle_index] = Normalize(normal);
            if (data_index + 2 < temp_normal_array_.Length()) {
                temp_normal_array_[data_index] = 0;
                temp_normal_array_[data_index + 1] = 0;
                temp_normal_array_[data_index + 2] = 0;
            }
        }
    }
}

}  // namespace hocloth::mc2
