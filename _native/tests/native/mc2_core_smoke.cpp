#include "hocloth/manager/magica_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_container.hpp"

#include <iostream>
#include <memory>
#include <vector>

int main()
{
    hocloth::mc2::MagicaManager manager;
    manager.Initialize();

    hocloth::mc2::ClothParameters cloth_parameters;
    cloth_parameters.distance_constraint = hocloth::mc2::DistanceConstraintParams::BoneSpringDefaults();
    const int team_id = manager.Team().CreateTeam(cloth_parameters, true, true);
    hocloth::mc2::TransformRecord root_transform;
    root_transform.id = 1001;
    root_transform.name = "smoke-root";
    root_transform.position = hocloth::mc2::float3{1.0f, 2.0f, 3.0f};
    root_transform.local_position = root_transform.position;
    root_transform.scale = hocloth::mc2::float3{1.0f, 1.0f, 1.0f};
    root_transform.local_to_world_matrix =
        hocloth::mc2::TRS(root_transform.position, root_transform.rotation, root_transform.scale);

    auto vmesh = std::make_shared<hocloth::mc2::VirtualMesh>();
    vmesh->name = "smoke-vmesh";
    vmesh->mesh_type = hocloth::mc2::VirtualMesh::MeshType::ProxyBoneMesh;
    vmesh->center_transform_index = 0;
    vmesh->transform_data.Initialize(1);
    for (int index = 0; index < 2; ++index) {
        vmesh->local_positions.Add(hocloth::mc2::float3{static_cast<float>(index), 0.0f, 0.0f});
        vmesh->local_normals.Add(hocloth::mc2::float3{0.0f, 0.0f, 1.0f});
        vmesh->local_tangents.Add(hocloth::mc2::float3{1.0f, 0.0f, 0.0f});
        vmesh->attributes.Add(hocloth::mc2::VertexAttribute{hocloth::mc2::VertexAttribute::FlagMove});
        vmesh->uv.Add(hocloth::mc2::float2{static_cast<float>(index), 0.0f});
        vmesh->bone_weights.Add(hocloth::mc2::VirtualMeshBoneWeight{});
        vmesh->vertex_bind_pose_positions.Add(hocloth::mc2::float3{static_cast<float>(index), 0.0f, 0.0f});
        vmesh->vertex_bind_pose_rotations.Add(hocloth::mc2::quaternion{});
        vmesh->vertex_depths.Add(static_cast<float>(index) * 0.25f);
        vmesh->vertex_root_indices.Add(-1);
        vmesh->vertex_parent_indices.Add(index == 0 ? -1 : 0);
        vmesh->vertex_local_positions.Add(hocloth::mc2::float3{static_cast<float>(index), 0.0f, 0.0f});
        vmesh->vertex_local_rotations.Add(hocloth::mc2::quaternion{});
        vmesh->vertex_child_index_array.Add(0);
        vmesh->normal_adjustment_rotations.Add(hocloth::mc2::quaternion{});
        vmesh->vertex_to_transform_rotations.Add(hocloth::mc2::quaternion{});
    }
    const int mesh_id = manager.VirtualMesh().RegisterMesh(vmesh);

    hocloth::mc2::VirtualMeshContainer container{vmesh};
    container.SetUniqueTransformRecords(std::vector<hocloth::mc2::TransformRecord>{root_transform});
    manager.VirtualMesh().RegisterProxyMesh(team_id, container, manager.Team(), manager.Transform());
    const auto transform_chunk = manager.Team().GetTeamData(team_id).proxy_transform_chunk;
    manager.Team().GetTeamData(team_id).center_transform_index = transform_chunk.start_index;

    auto chunks = manager.Simulation().RegisterParticleRange(team_id, 2);
    manager.Team().GetTeamData(team_id).particle_chunk = chunks.next_pos_chunk;
    manager.Simulation().NextPositions()[chunks.next_pos_chunk.start_index] =
        hocloth::mc2::float3{0.0f, 0.0f, 0.0f};
    manager.Simulation().NextPositions()[chunks.next_pos_chunk.start_index + 1] =
        hocloth::mc2::float3{1.25f, 0.0f, 0.0f};
    manager.Simulation().BasePositions()[chunks.base_pos_chunk.start_index] =
        hocloth::mc2::float3{0.0f, 0.0f, 0.0f};
    manager.Simulation().BasePositions()[chunks.base_pos_chunk.start_index + 1] =
        hocloth::mc2::float3{1.0f, 0.0f, 0.0f};

    hocloth::mc2::DistanceConstraint::ConstraintData distance_data;
    distance_data.index_array = {
        hocloth::mc2::data::Pack12_20(1, 0),
        hocloth::mc2::data::Pack12_20(1, 1),
    };
    distance_data.data_array = {1u, 0u};
    distance_data.distance_array = {1.0f, 1.0f};
    manager.Cloth().Distance().Register(team_id, distance_data, manager.Team());

    hocloth::mc2::InertiaConstraint::ConstraintData inertia_data;
    inertia_data.fixed_array = {0u};
    inertia_data.center_data.frame_local_position = hocloth::mc2::float3{0.5f, 0.0f, 0.0f};
    inertia_data.center_data.old_frame_world_position = hocloth::mc2::float3{0.0f, 0.0f, 0.0f};
    inertia_data.center_data.frame_world_position = hocloth::mc2::float3{0.2f, 0.0f, 0.0f};
    inertia_data.center_data.old_world_position = hocloth::mc2::float3{0.0f, 0.0f, 0.0f};
    inertia_data.center_data.now_world_position = hocloth::mc2::float3{0.0f, 0.0f, 0.0f};
    manager.Cloth().Inertia().Register(team_id, inertia_data, manager.Team());
    manager.Team().GetTeamData(team_id).time = 1.0f / 60.0f;
    manager.Team().GetTeamData(team_id).frame_old_time = 0.0f;
    manager.Team().GetTeamData(team_id).now_update_time = 0.0f;
    manager.Team().GetTeamData(team_id).update_count = 1;

    manager.Simulation().BeginSimulationStep();
    manager.Team().SimulationStepTeamUpdate(0, 1.0f / 60.0f);
    for (int index = 0; index < chunks.ParticleCount(); ++index) {
        manager.Simulation().MarkStepParticle(chunks.next_pos_chunk.start_index + index);
    }
    manager.Cloth().Distance().Solve(
        hocloth::mc2::float4{1.0f, 1.0f, 1.0f, 1.0f},
        manager.Team(),
        manager.VirtualMesh(),
        manager.Simulation()
    );
    manager.Simulation().EndSimulationStep();

    std::cout << "team_id=" << team_id
              << " mesh_id=" << mesh_id
              << " transforms=" << manager.Transform().Count()
              << " meshes=" << manager.VirtualMesh().MeshCount()
              << " proxy_vertices=" << manager.VirtualMesh().ProxyVertexCount()
              << " distance_connections=" << manager.Cloth().Distance().ConnectionCount()
              << " fixed=" << manager.Cloth().Inertia().FixedCount()
              << " center_data=" << manager.Team().CenterDataCount()
              << " center_transform=" << manager.Team().GetCenterData(team_id).center_transform_index
              << " inertia_x=" << manager.Team().GetCenterData(team_id).inertia_vector.x
              << " gravity_dot=" << manager.Team().GetTeamData(team_id).gravity_dot
              << " particles=" << manager.Simulation().ParticleCount()
              << " steps=" << manager.Simulation().SimulationStepCount()
              << " p0.x=" << manager.Simulation().NextPositions()[chunks.next_pos_chunk.start_index].x
              << " p1.x=" << manager.Simulation().NextPositions()[chunks.next_pos_chunk.start_index + 1].x
              << "\n";

    manager.Dispose();
    return 0;
}
