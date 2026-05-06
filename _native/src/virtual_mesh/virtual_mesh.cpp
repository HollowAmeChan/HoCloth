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
    bone_weights.Dispose();
    triangles.Dispose();
    lines.Dispose();
    edges.Dispose();
    edge_flags.Dispose();
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
    vertex_local_positions.Dispose();
    vertex_local_rotations.Dispose();
    base_line_start_data_indices.Dispose();
    base_line_data_counts.Dispose();
    base_line_data.Dispose();
    normal_adjustment_rotations.Dispose();
    vertex_to_transform_rotations.Dispose();
    merge_chunk.Clear();
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

bool VirtualMesh::IsProxy() const
{
    return mesh_type == MeshType::ProxyMesh || mesh_type == MeshType::ProxyBoneMesh;
}

bool VirtualMesh::IsMapping() const
{
    return mesh_type == MeshType::Mapping;
}

}  // namespace hocloth::mc2
