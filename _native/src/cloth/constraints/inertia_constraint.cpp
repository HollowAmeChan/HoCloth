#include "hocloth/cloth/constraints/inertia_constraint.hpp"

#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <cstddef>
#include <cstdint>

namespace hocloth::mc2 {

void InertiaConstraint::Dispose()
{
    fixed_array_.Dispose();
}

int InertiaConstraint::FixedCount() const
{
    return fixed_array_.Count();
}

InertiaConstraint::ConstraintData InertiaConstraint::CreateData(
    const VirtualMesh& proxy_mesh,
    const ClothParameters& parameters
)
{
    // Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/InertiaConstraint.cs
    ConstraintData constraint_data;
    InertiaCenterData center_data;
    center_data.center_transform_index = proxy_mesh.center_transform_index;
    constraint_data.center_data = center_data;

    float3 normal{};
    float3 tangent{};
    const int fixed_count = proxy_mesh.CenterFixedPointCount();
    if (fixed_count <= 0) {
        constraint_data.init_local_gravity_direction = float3{0.0f, -1.0f, 0.0f};
        return constraint_data;
    }
    constraint_data.fixed_array.reserve(static_cast<std::size_t>(fixed_count));
    for (int index = 0; index < fixed_count; ++index) {
        const int fixed_index = proxy_mesh.center_fixed_list[index];
        if (fixed_index < 0 || fixed_index >= proxy_mesh.VertexCount()) {
            continue;
        }

        constraint_data.fixed_array.push_back(static_cast<std::uint16_t>(fixed_index));
        const float3 local_normal =
            fixed_index < proxy_mesh.local_normals.Count()
                ? proxy_mesh.local_normals[fixed_index]
                : float3{0.0f, 1.0f, 0.0f};
        const float3 local_tangent =
            fixed_index < proxy_mesh.local_tangents.Count()
                ? proxy_mesh.local_tangents[fixed_index]
                : float3{0.0f, 0.0f, 1.0f};
        const quaternion local_rotation = ToRotation(local_normal, local_tangent);
        const quaternion bind_rotation =
            fixed_index < proxy_mesh.vertex_bind_pose_rotations.Count()
                ? proxy_mesh.vertex_bind_pose_rotations[fixed_index]
                : quaternion{};
        const quaternion rotation = Multiply(local_rotation, bind_rotation);
        float3 fixed_normal;
        float3 fixed_tangent;
        ToNormalTangent(rotation, fixed_normal, fixed_tangent);
        normal = Add(normal, fixed_normal);
        tangent = Add(tangent, fixed_tangent);
    }

    float3 local_gravity_direction{0.0f, -1.0f, 0.0f};
    if (!constraint_data.fixed_array.empty()) {
        const quaternion rotation = ToRotation(Normalize(normal), Normalize(tangent));
        local_gravity_direction =
            Rotate(Inverse(rotation), parameters.world_gravity_direction);
    }
    constraint_data.init_local_gravity_direction = local_gravity_direction;
    return constraint_data;
}

void InertiaConstraint::Register(int team_id, const ConstraintData& data, TeamManager& team_manager)
{
    if (!team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    InertiaCenterData center_data = data.center_data;
    center_data.center_transform_index = team_data.center_transform_index;
    center_data.init_local_gravity_direction = data.init_local_gravity_direction;
    team_manager.SetCenterData(team_id, center_data);

    if (!data.fixed_array.empty()) {
        team_data.fixed_data_chunk = fixed_array_.AddRange(data.fixed_array);
    }
}

void InertiaConstraint::Exit(int team_id, TeamManager& team_manager)
{
    if (!team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    fixed_array_.Remove(team_data.fixed_data_chunk);
    team_data.fixed_data_chunk.Clear();
    team_manager.SetCenterData(team_id, InertiaCenterData{});
}

const ExNativeArray<std::uint16_t>& InertiaConstraint::FixedArray() const
{
    return fixed_array_;
}

}  // namespace hocloth::mc2
