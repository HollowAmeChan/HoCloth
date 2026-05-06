#include "hocloth/cloth/constraints/collider_collision_constraint.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace hocloth::mc2 {

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
    // Ported from MC2 PointColliderCollisionConstraintJob.
    // Edge collision and its aggregation buffers are kept for a later bottom-layer pass.
    const ExProcessingList<int>& step_particles = simulation_manager.ProcessingStepParticles();
    const auto& step_buffer = step_particles.Buffer();
    const auto& team_ids = simulation_manager.TeamIds();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& vertex_depths = virtual_mesh_manager.VertexDepths();
    const auto& collider_flags = collider_manager.Flags();
    const auto& collider_work_array = collider_manager.WorkDataArray();

    auto& next_positions = simulation_manager.NextPositions();
    auto& frictions = simulation_manager.Frictions();
    auto& collision_normals = simulation_manager.CollisionNormals();
    auto& velocity_positions = simulation_manager.VelocityPositions();
    const auto& base_positions = simulation_manager.BasePositions();

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

}  // namespace hocloth::mc2
