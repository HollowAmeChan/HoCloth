#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"

#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_container.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <utility>

namespace hocloth::mc2 {

Result VirtualMeshManager::Initialize()
{
    Dispose();
    initialized_ = true;
    return Result::Ok();
}

void VirtualMeshManager::Dispose()
{
    ClearMeshes();
    team_ids_.Dispose();
    attributes_.Dispose();
    uv_.Dispose();
    vertex_bind_pose_positions_.Dispose();
    vertex_bind_pose_rotations_.Dispose();
    vertex_depths_.Dispose();
    vertex_root_indices_.Dispose();
    vertex_local_positions_.Dispose();
    vertex_local_rotations_.Dispose();
    vertex_parent_indices_.Dispose();
    vertex_child_index_array_.Dispose();
    vertex_child_data_array_.Dispose();
    normal_adjustment_rotations_.Dispose();
    triangle_team_ids_.Dispose();
    triangles_.Dispose();
    triangle_normals_.Dispose();
    triangle_tangents_.Dispose();
    edge_team_ids_.Dispose();
    edges_.Dispose();
    edge_flags_.Dispose();
    base_line_flags_.Dispose();
    base_line_team_ids_.Dispose();
    base_line_start_data_indices_.Dispose();
    base_line_data_counts_.Dispose();
    base_line_data_.Dispose();
    local_positions_.Dispose();
    local_normals_.Dispose();
    local_tangents_.Dispose();
    bone_weights_.Dispose();
    skin_bone_transform_indices_.Dispose();
    skin_bone_bind_poses_.Dispose();
    vertex_to_transform_rotations_.Dispose();
    positions_.Dispose();
    rotations_.Dispose();
    mapping_id_array_.Dispose();
    mapping_reference_indices_.Dispose();
    mapping_attributes_.Dispose();
    mapping_local_positions_.Dispose();
    mapping_local_normals_.Dispose();
    mapping_bone_weights_.Dispose();
    mapping_positions_.Dispose();
    mapping_normals_.Dispose();
    initialized_ = false;
}

ManagerStatus VirtualMeshManager::Status() const
{
    std::ostringstream detail;
    detail << "proxy_vertices=" << ProxyVertexCount()
           << " proxy_triangles=" << ProxyTriangleCount()
           << " proxy_edges=" << ProxyEdgeCount()
           << " proxy_baselines=" << ProxyBaseLineCount()
           << " local_positions=" << ProxyLocalPositionCount()
           << " mapping_vertices=" << MappingVertexCount();
    return ManagerStatus{
        "VirtualMeshManager",
        initialized_,
        static_cast<std::uint32_t>(MeshCount()),
        detail.str(),
    };
}

bool VirtualMeshManager::IsValid() const
{
    return initialized_;
}

int VirtualMeshManager::MeshCount() const
{
    int count = 0;
    for (const auto& mesh : meshes_) {
        if (mesh != nullptr) {
            ++count;
        }
    }
    return count;
}

int VirtualMeshManager::RegisterMesh(std::shared_ptr<VirtualMesh> mesh)
{
    if (!initialized_ || mesh == nullptr) {
        return -1;
    }

    if (!free_mesh_ids_.empty()) {
        const int mesh_id = free_mesh_ids_.back();
        free_mesh_ids_.pop_back();
        meshes_[static_cast<std::size_t>(mesh_id)] = std::move(mesh);
        return mesh_id;
    }

    meshes_.push_back(std::move(mesh));
    return static_cast<int>(meshes_.size() - 1);
}

void VirtualMeshManager::ReleaseMesh(int mesh_id)
{
    if (mesh_id < 0 || mesh_id >= static_cast<int>(meshes_.size())) {
        return;
    }
    if (meshes_[static_cast<std::size_t>(mesh_id)] == nullptr) {
        return;
    }
    meshes_[static_cast<std::size_t>(mesh_id)].reset();
    free_mesh_ids_.push_back(mesh_id);
}

void VirtualMeshManager::ClearMeshes()
{
    meshes_.clear();
    free_mesh_ids_.clear();
}

VirtualMesh* VirtualMeshManager::GetMesh(int mesh_id)
{
    if (mesh_id < 0 || mesh_id >= static_cast<int>(meshes_.size())) {
        return nullptr;
    }
    return meshes_[static_cast<std::size_t>(mesh_id)].get();
}

const VirtualMesh* VirtualMeshManager::GetMesh(int mesh_id) const
{
    if (mesh_id < 0 || mesh_id >= static_cast<int>(meshes_.size())) {
        return nullptr;
    }
    return meshes_[static_cast<std::size_t>(mesh_id)].get();
}

void VirtualMeshManager::RegisterProxyMesh(
    int team_id,
    const VirtualMeshContainer& proxy_mesh_container,
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    if (!initialized_ || !team_manager.IsValidTeam(team_id)) {
        return;
    }

    const VirtualMesh* proxy_mesh = proxy_mesh_container.SharedVirtualMesh();
    if (proxy_mesh == nullptr) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    team_data.proxy_transform_chunk = transform_manager.AddTransform(proxy_mesh_container, team_id);
    team_data.center_transform_index =
        proxy_mesh->center_transform_index >= 0
            ? team_data.proxy_transform_chunk.start_index + proxy_mesh->center_transform_index
            : -1;

    const int vertex_count = proxy_mesh->VertexCount();
    if (vertex_count > 0) {
        team_data.proxy_common_chunk = team_ids_.AddRange(vertex_count, static_cast<std::int16_t>(team_id));
        attributes_.AddRange(proxy_mesh->attributes);
        uv_.AddRange(proxy_mesh->uv);
        vertex_bind_pose_positions_.AddRange(proxy_mesh->vertex_bind_pose_positions);
        vertex_bind_pose_rotations_.AddRange(proxy_mesh->vertex_bind_pose_rotations);
        vertex_depths_.AddRange(proxy_mesh->vertex_depths);
        vertex_root_indices_.AddRange(proxy_mesh->vertex_root_indices);
        vertex_local_positions_.AddRange(proxy_mesh->vertex_local_positions);
        vertex_local_rotations_.AddRange(proxy_mesh->vertex_local_rotations);
        vertex_parent_indices_.AddRange(proxy_mesh->vertex_parent_indices);
        vertex_child_index_array_.AddRange(proxy_mesh->vertex_child_index_array);
        normal_adjustment_rotations_.AddRange(proxy_mesh->normal_adjustment_rotations);
        positions_.AddRange(vertex_count);
        rotations_.AddRange(vertex_count);
    }

    if (proxy_mesh->vertex_child_data_array.Count() > 0) {
        team_data.proxy_vertex_child_data_chunk =
            vertex_child_data_array_.AddRange(proxy_mesh->vertex_child_data_array);
    }

    if (proxy_mesh->TriangleCount() > 0) {
        team_data.proxy_triangle_chunk =
            triangle_team_ids_.AddRange(proxy_mesh->TriangleCount(), static_cast<std::int16_t>(team_id));
        triangles_.AddRange(proxy_mesh->triangles);
        triangle_normals_.AddRange(proxy_mesh->TriangleCount());
        triangle_tangents_.AddRange(proxy_mesh->TriangleCount());
    }

    if (proxy_mesh->edges.Count() > 0) {
        team_data.proxy_edge_chunk =
            edge_team_ids_.AddRange(proxy_mesh->edges.Count(), static_cast<std::int16_t>(team_id));
        edges_.AddRange(proxy_mesh->edges);
        edge_flags_.AddRange(proxy_mesh->edge_flags);
    }

    if (proxy_mesh->base_line_start_data_indices.Count() > 0
        && proxy_mesh->base_line_data_counts.Count()
            == proxy_mesh->base_line_start_data_indices.Count()) {
        if (proxy_mesh->base_line_flags.Count()
            == proxy_mesh->base_line_start_data_indices.Count()) {
            base_line_flags_.AddRange(proxy_mesh->base_line_flags);
        } else {
            base_line_flags_.AddRange(
                proxy_mesh->base_line_start_data_indices.Count(),
                BitFlag8{}
            );
        }
        base_line_team_ids_.AddRange(
            proxy_mesh->base_line_start_data_indices.Count(),
            static_cast<std::int16_t>(team_id)
        );
        team_data.baseline_chunk =
            base_line_start_data_indices_.AddRange(proxy_mesh->base_line_start_data_indices);
        base_line_data_counts_.AddRange(proxy_mesh->base_line_data_counts);
    }

    if (proxy_mesh->base_line_data.Count() > 0) {
        team_data.baseline_data_chunk = base_line_data_.AddRange(proxy_mesh->base_line_data);
    }

    if (proxy_mesh->VertexCount() > 0) {
        team_data.proxy_mesh_chunk = local_positions_.AddRange(proxy_mesh->local_positions);
        local_normals_.AddRange(proxy_mesh->local_normals);
        local_tangents_.AddRange(proxy_mesh->local_tangents);
        bone_weights_.AddRange(proxy_mesh->bone_weights);
    }

    if (proxy_mesh->SkinBoneCount() > 0) {
        team_data.proxy_skin_bone_chunk =
            skin_bone_transform_indices_.AddRange(proxy_mesh->skin_bone_transform_indices);
        skin_bone_bind_poses_.AddRange(proxy_mesh->skin_bone_bind_poses);
    }

    if (proxy_mesh->vertex_to_transform_rotations.Count() > 0) {
        team_data.proxy_bone_chunk =
            vertex_to_transform_rotations_.AddRange(proxy_mesh->vertex_to_transform_rotations);
    }
}

void VirtualMeshManager::ExitProxyMesh(
    int team_id,
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    if (!initialized_ || !team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    transform_manager.RemoveTransform(team_data.proxy_transform_chunk);
    team_ids_.Remove(team_data.proxy_common_chunk);
    attributes_.Remove(team_data.proxy_common_chunk);
    uv_.Remove(team_data.proxy_common_chunk);
    vertex_bind_pose_positions_.Remove(team_data.proxy_common_chunk);
    vertex_bind_pose_rotations_.Remove(team_data.proxy_common_chunk);
    vertex_depths_.Remove(team_data.proxy_common_chunk);
    vertex_root_indices_.Remove(team_data.proxy_common_chunk);
    vertex_local_positions_.Remove(team_data.proxy_common_chunk);
    vertex_local_rotations_.Remove(team_data.proxy_common_chunk);
    vertex_parent_indices_.Remove(team_data.proxy_common_chunk);
    vertex_child_index_array_.Remove(team_data.proxy_common_chunk);
    normal_adjustment_rotations_.Remove(team_data.proxy_common_chunk);
    positions_.Remove(team_data.proxy_common_chunk);
    rotations_.Remove(team_data.proxy_common_chunk);

    vertex_child_data_array_.Remove(team_data.proxy_vertex_child_data_chunk);
    triangle_team_ids_.Remove(team_data.proxy_triangle_chunk);
    triangles_.Remove(team_data.proxy_triangle_chunk);
    triangle_normals_.Remove(team_data.proxy_triangle_chunk);
    triangle_tangents_.Remove(team_data.proxy_triangle_chunk);
    edge_team_ids_.Remove(team_data.proxy_edge_chunk);
    edges_.Remove(team_data.proxy_edge_chunk);
    edge_flags_.Remove(team_data.proxy_edge_chunk);
    base_line_flags_.Remove(team_data.baseline_chunk);
    base_line_team_ids_.Remove(team_data.baseline_chunk);
    base_line_start_data_indices_.Remove(team_data.baseline_chunk);
    base_line_data_counts_.Remove(team_data.baseline_chunk);
    base_line_data_.Remove(team_data.baseline_data_chunk);
    local_positions_.Remove(team_data.proxy_mesh_chunk);
    local_normals_.Remove(team_data.proxy_mesh_chunk);
    local_tangents_.Remove(team_data.proxy_mesh_chunk);
    bone_weights_.Remove(team_data.proxy_mesh_chunk);
    skin_bone_transform_indices_.Remove(team_data.proxy_skin_bone_chunk);
    skin_bone_bind_poses_.Remove(team_data.proxy_skin_bone_chunk);
    vertex_to_transform_rotations_.Remove(team_data.proxy_bone_chunk);

    team_data.proxy_transform_chunk.Clear();
    team_data.proxy_common_chunk.Clear();
    team_data.proxy_vertex_child_data_chunk.Clear();
    team_data.proxy_triangle_chunk.Clear();
    team_data.proxy_edge_chunk.Clear();
    team_data.proxy_mesh_chunk.Clear();
    team_data.proxy_bone_chunk.Clear();
    team_data.proxy_skin_bone_chunk.Clear();
    team_data.baseline_chunk.Clear();
    team_data.baseline_data_chunk.Clear();
    team_data.center_transform_index = -1;
}

DataChunk VirtualMeshManager::RegisterMappingMesh(
    int team_id,
    VirtualMeshContainer& mapping_mesh_container,
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    if (!initialized_ || !team_manager.IsValidTeam(team_id)) {
        return DataChunk::Empty();
    }

    VirtualMesh* mapping_mesh = mapping_mesh_container.SharedVirtualMesh();
    if (mapping_mesh == nullptr || !mapping_mesh->IsMapping()) {
        return DataChunk::Empty();
    }

    TeamManager::MappingData mapping_data;
    mapping_data.team_id = team_id;

    BitFlag8 transform_flag{TransformManager::FlagRead};
    transform_flag.SetFlag(TransformManager::FlagEnable, true);
    const DataChunk center_transform_chunk = transform_manager.AddTransform(
        mapping_mesh_container.GetCenterTransformRecord(),
        transform_flag,
        team_id
    );
    mapping_data.center_transform_index =
        center_transform_chunk.IsValid() ? center_transform_chunk.start_index : -1;

    mapping_data.to_proxy_matrix = mapping_mesh->to_proxy_matrix;
    mapping_data.to_proxy_rotation = mapping_mesh->to_proxy_rotation;

    const int mapping_index = team_manager.RegisterMappingData(team_id, mapping_data);
    if (mapping_index < 0) {
        if (center_transform_chunk.IsValid()) {
            transform_manager.RemoveTransform(center_transform_chunk);
        }
        return DataChunk::Empty();
    }

    const int vertex_count = mapping_mesh->VertexCount();
    if (vertex_count > 0) {
        mapping_data.mapping_common_chunk =
            mapping_id_array_.AddRange(vertex_count, static_cast<short>(mapping_index + 1));
        mapping_reference_indices_.AddRange(mapping_mesh->reference_indices);
        mapping_attributes_.AddRange(mapping_mesh->attributes);
        mapping_local_positions_.AddRange(mapping_mesh->local_positions);
        mapping_local_normals_.AddRange(mapping_mesh->local_normals);
        mapping_bone_weights_.AddRange(mapping_mesh->bone_weights);
        mapping_positions_.AddRange(vertex_count);
        mapping_normals_.AddRange(vertex_count);
    }

    team_manager.MappingDataArray()[mapping_index] = mapping_data;
    mapping_mesh->mapping_id = mapping_index;
    return mapping_data.mapping_common_chunk;
}

void VirtualMeshManager::ExitMappingMesh(
    int team_id,
    int mapping_index,
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    if (!initialized_ || !team_manager.IsValidTeam(team_id) || mapping_index < 0
        || mapping_index >= team_manager.MappingDataArray().Length()) {
        return;
    }

    const TeamManager::MappingData mapping_data = team_manager.MappingDataArray()[mapping_index];
    if (!mapping_data.IsValid() || mapping_data.team_id != team_id) {
        return;
    }

    if (mapping_data.center_transform_index >= 0) {
        transform_manager.RemoveTransform(DataChunk{mapping_data.center_transform_index, 1});
    }

    mapping_id_array_.RemoveAndFill(mapping_data.mapping_common_chunk, 0);
    mapping_reference_indices_.Remove(mapping_data.mapping_common_chunk);
    mapping_attributes_.Remove(mapping_data.mapping_common_chunk);
    mapping_local_positions_.Remove(mapping_data.mapping_common_chunk);
    mapping_local_normals_.Remove(mapping_data.mapping_common_chunk);
    mapping_bone_weights_.Remove(mapping_data.mapping_common_chunk);
    mapping_positions_.Remove(mapping_data.mapping_common_chunk);
    mapping_normals_.Remove(mapping_data.mapping_common_chunk);

    team_manager.RemoveMappingData(team_id, mapping_index);
}

int VirtualMeshManager::ProxyVertexCount() const
{
    return team_ids_.Count();
}

int VirtualMeshManager::ProxyTriangleCount() const
{
    return triangles_.Count();
}

int VirtualMeshManager::ProxyEdgeCount() const
{
    return edges_.Count();
}

int VirtualMeshManager::ProxyBaseLineCount() const
{
    return base_line_flags_.Count();
}

int VirtualMeshManager::ProxyLocalPositionCount() const
{
    return local_positions_.Count();
}

int VirtualMeshManager::MappingVertexCount() const
{
    return mapping_id_array_.Count();
}

const ExNativeArray<VertexAttribute>& VirtualMeshManager::Attributes() const
{
    return attributes_;
}

const ExNativeArray<quaternion>& VirtualMeshManager::VertexBindPoseRotations() const
{
    return vertex_bind_pose_rotations_;
}

const ExNativeArray<float>& VirtualMeshManager::VertexDepths() const
{
    return vertex_depths_;
}

const ExNativeArray<int>& VirtualMeshManager::VertexRootIndices() const
{
    return vertex_root_indices_;
}

const ExNativeArray<int>& VirtualMeshManager::VertexParentIndices() const
{
    return vertex_parent_indices_;
}

const ExNativeArray<float3>& VirtualMeshManager::VertexLocalPositions() const
{
    return vertex_local_positions_;
}

const ExNativeArray<quaternion>& VirtualMeshManager::VertexLocalRotations() const
{
    return vertex_local_rotations_;
}

const ExNativeArray<std::int16_t>& VirtualMeshManager::TriangleTeamIds() const
{
    return triangle_team_ids_;
}

const ExNativeArray<int3>& VirtualMeshManager::Triangles() const
{
    return triangles_;
}

const ExNativeArray<std::int16_t>& VirtualMeshManager::EdgeTeamIds() const
{
    return edge_team_ids_;
}

const ExNativeArray<int2>& VirtualMeshManager::Edges() const
{
    return edges_;
}

const ExNativeArray<BitFlag8>& VirtualMeshManager::EdgeFlags() const
{
    return edge_flags_;
}

const ExNativeArray<BitFlag8>& VirtualMeshManager::BaseLineFlags() const
{
    return base_line_flags_;
}

const ExNativeArray<std::int16_t>& VirtualMeshManager::BaseLineTeamIds() const
{
    return base_line_team_ids_;
}

const ExNativeArray<std::uint16_t>& VirtualMeshManager::BaseLineStartDataIndices() const
{
    return base_line_start_data_indices_;
}

const ExNativeArray<std::uint16_t>& VirtualMeshManager::BaseLineDataCounts() const
{
    return base_line_data_counts_;
}

const ExNativeArray<std::uint16_t>& VirtualMeshManager::BaseLineData() const
{
    return base_line_data_;
}

const ExNativeArray<float3>& VirtualMeshManager::Positions() const
{
    return positions_;
}

const ExNativeArray<quaternion>& VirtualMeshManager::Rotations() const
{
    return rotations_;
}

ExNativeArray<float3>& VirtualMeshManager::Positions()
{
    return positions_;
}

ExNativeArray<quaternion>& VirtualMeshManager::Rotations()
{
    return rotations_;
}

const ExNativeArray<short>& VirtualMeshManager::MappingIds() const
{
    return mapping_id_array_;
}

const ExNativeArray<int>& VirtualMeshManager::MappingReferenceIndices() const
{
    return mapping_reference_indices_;
}

const ExNativeArray<VertexAttribute>& VirtualMeshManager::MappingAttributes() const
{
    return mapping_attributes_;
}

const ExNativeArray<float3>& VirtualMeshManager::MappingLocalPositions() const
{
    return mapping_local_positions_;
}

const ExNativeArray<float3>& VirtualMeshManager::MappingLocalNormals() const
{
    return mapping_local_normals_;
}

const ExNativeArray<VirtualMeshBoneWeight>& VirtualMeshManager::MappingBoneWeights() const
{
    return mapping_bone_weights_;
}

const ExNativeArray<float3>& VirtualMeshManager::MappingPositions() const
{
    return mapping_positions_;
}

const ExNativeArray<float3>& VirtualMeshManager::MappingNormals() const
{
    return mapping_normals_;
}

ExNativeArray<float3>& VirtualMeshManager::MappingPositions()
{
    return mapping_positions_;
}

ExNativeArray<float3>& VirtualMeshManager::MappingNormals()
{
    return mapping_normals_;
}

}  // namespace hocloth::mc2
