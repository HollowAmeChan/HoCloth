#include "hocloth/virtual_mesh/virtual_mesh.hpp"

namespace hocloth::mc2 {

void VirtualMesh::Dispose()
{
    reference_indices.Dispose();
    attributes.Dispose();
    local_positions.Dispose();
    local_normals.Dispose();
    local_tangents.Dispose();
    uv.Dispose();
    vertex_to_triangles.Dispose();
    vertex_to_vertex_index_array.Dispose();
    vertex_to_vertex_data_array.Dispose();
    bone_weights.Dispose();
    triangles.Dispose();
    lines.Dispose();
    edges.Dispose();
    edge_flags.Dispose();
    edge_to_triangles.clear();
    skin_bone_transform_indices.Dispose();
    skin_bone_bind_poses.Dispose();
    transform_data.Dispose();
    vertex_child_index_array.Dispose();
    vertex_child_data_array.Dispose();
    vertex_bind_pose_positions.Dispose();
    vertex_bind_pose_rotations.Dispose();
    vertex_depths.Dispose();
    vertex_root_indices.Dispose();
    vertex_parent_indices.Dispose();
    center_fixed_list.Dispose();
    vertex_local_positions.Dispose();
    vertex_local_rotations.Dispose();
    base_line_flags.Dispose();
    base_line_start_data_indices.Dispose();
    base_line_data_counts.Dispose();
    base_line_data.Dispose();
    normal_adjustment_rotations.Dispose();
    vertex_to_transform_rotations.Dispose();
    custom_skinning_bone_indices.clear();
    local_center_position = float3{};
    center_world_position = float3{};
    center_world_rotation = quaternion{};
    center_world_scale = float3{1.0f, 1.0f, 1.0f};
    result = Result::Ok();
    is_managed = false;
    mesh_type = MeshType::NormalMesh;
    is_bone_cloth = false;
    center_transform_index = -1;
    init_local_to_world = float4x4{};
    init_world_to_local = float4x4{};
    init_rotation = quaternion{};
    init_inverse_rotation = quaternion{};
    init_scale = float3{1.0f, 1.0f, 1.0f};
    skin_root_index = -1;
    bounding_box = AABB{};
    average_vertex_distance = 0.0f;
    max_vertex_distance = 0.0f;
    merge_chunk.Clear();
    join_indices.Dispose();
    to_proxy_matrix = float4x4{};
    to_proxy_rotation = quaternion{};
    mapping_id = -1;
}

bool VirtualMesh::IsValid() const
{
    return result.Succeeded();
}

int VirtualMesh::VertexCount() const
{
    return local_positions.Count();
}

int VirtualMesh::TriangleCount() const
{
    return triangles.Count();
}

int VirtualMesh::LineCount() const
{
    return lines.Count();
}

int VirtualMesh::SkinBoneCount() const
{
    return skin_bone_transform_indices.Count();
}

int VirtualMesh::TransformCount() const
{
    return transform_data.Count();
}

int VirtualMesh::CustomSkinningBoneCount() const
{
    return static_cast<int>(custom_skinning_bone_indices.size());
}

int VirtualMesh::CenterFixedPointCount() const
{
    return center_fixed_list.Count();
}

int VirtualMesh::BaseLineCount() const
{
    return base_line_start_data_indices.Count();
}

bool VirtualMesh::IsProxy() const
{
    return mesh_type == MeshType::ProxyMesh || mesh_type == MeshType::ProxyBoneMesh;
}

bool VirtualMesh::IsMapping() const
{
    return mesh_type == MeshType::Mapping;
}

}  // namespace hocloth::mc2
