#pragma once

#include "hocloth/manager/i_manager.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace hocloth::mc2 {

class TeamManager;
class TransformManager;
class VirtualMeshContainer;

// Port target for Magica Cloth 2: Scripts/Core/Manager/VirtualMesh/VirtualMeshManager.cs
class VirtualMeshManager final : public IManager {
public:
    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] int MeshCount() const;
    [[nodiscard]] int RegisterMesh(std::shared_ptr<VirtualMesh> mesh);
    void ReleaseMesh(int mesh_id);
    void ClearMeshes();
    [[nodiscard]] VirtualMesh* GetMesh(int mesh_id);
    [[nodiscard]] const VirtualMesh* GetMesh(int mesh_id) const;

    void RegisterProxyMesh(
        int team_id,
        const VirtualMeshContainer& proxy_mesh_container,
        TeamManager& team_manager,
        TransformManager& transform_manager
    );
    void ExitProxyMesh(
        int team_id,
        TeamManager& team_manager,
        TransformManager& transform_manager
    );
    [[nodiscard]] DataChunk RegisterMappingMesh(
        int team_id,
        VirtualMeshContainer& mapping_mesh_container,
        TeamManager& team_manager,
        TransformManager& transform_manager
    );
    void ExitMappingMesh(
        int team_id,
        int mapping_index,
        TeamManager& team_manager,
        TransformManager& transform_manager
    );

    [[nodiscard]] int ProxyVertexCount() const;
    [[nodiscard]] int ProxyTriangleCount() const;
    [[nodiscard]] int ProxyEdgeCount() const;
    [[nodiscard]] int ProxyBaseLineCount() const;
    [[nodiscard]] int ProxyLocalPositionCount() const;
    [[nodiscard]] int MappingVertexCount() const;
    [[nodiscard]] const ExNativeArray<VertexAttribute>& Attributes() const;
    [[nodiscard]] const ExNativeArray<quaternion>& VertexBindPoseRotations() const;
    [[nodiscard]] const ExNativeArray<float>& VertexDepths() const;
    [[nodiscard]] const ExNativeArray<int>& VertexRootIndices() const;
    [[nodiscard]] const ExNativeArray<int>& VertexParentIndices() const;
    [[nodiscard]] const ExNativeArray<float3>& VertexLocalPositions() const;
    [[nodiscard]] const ExNativeArray<quaternion>& VertexLocalRotations() const;
    [[nodiscard]] const ExNativeArray<std::int16_t>& TriangleTeamIds() const;
    [[nodiscard]] const ExNativeArray<int3>& Triangles() const;
    [[nodiscard]] const ExNativeArray<std::int16_t>& EdgeTeamIds() const;
    [[nodiscard]] const ExNativeArray<int2>& Edges() const;
    [[nodiscard]] const ExNativeArray<BitFlag8>& EdgeFlags() const;
    [[nodiscard]] const ExNativeArray<BitFlag8>& BaseLineFlags() const;
    [[nodiscard]] const ExNativeArray<std::int16_t>& BaseLineTeamIds() const;
    [[nodiscard]] const ExNativeArray<std::uint16_t>& BaseLineStartDataIndices() const;
    [[nodiscard]] const ExNativeArray<std::uint16_t>& BaseLineDataCounts() const;
    [[nodiscard]] const ExNativeArray<std::uint16_t>& BaseLineData() const;
    [[nodiscard]] const ExNativeArray<float3>& Positions() const;
    [[nodiscard]] const ExNativeArray<quaternion>& Rotations() const;
    [[nodiscard]] ExNativeArray<float3>& Positions();
    [[nodiscard]] ExNativeArray<quaternion>& Rotations();
    [[nodiscard]] const ExNativeArray<short>& MappingIds() const;
    [[nodiscard]] const ExNativeArray<int>& MappingReferenceIndices() const;
    [[nodiscard]] const ExNativeArray<VertexAttribute>& MappingAttributes() const;
    [[nodiscard]] const ExNativeArray<float3>& MappingLocalPositions() const;
    [[nodiscard]] const ExNativeArray<float3>& MappingLocalNormals() const;
    [[nodiscard]] const ExNativeArray<VirtualMeshBoneWeight>& MappingBoneWeights() const;
    [[nodiscard]] const ExNativeArray<float3>& MappingPositions() const;
    [[nodiscard]] const ExNativeArray<float3>& MappingNormals() const;
    [[nodiscard]] ExNativeArray<float3>& MappingPositions();
    [[nodiscard]] ExNativeArray<float3>& MappingNormals();

private:
    bool initialized_ = false;
    std::vector<std::shared_ptr<VirtualMesh>> meshes_;
    std::vector<int> free_mesh_ids_;

    ExNativeArray<std::int16_t> team_ids_;
    ExNativeArray<VertexAttribute> attributes_;
    ExNativeArray<float2> uv_;
    ExNativeArray<float3> vertex_bind_pose_positions_;
    ExNativeArray<quaternion> vertex_bind_pose_rotations_;
    ExNativeArray<float> vertex_depths_;
    ExNativeArray<int> vertex_root_indices_;
    ExNativeArray<float3> vertex_local_positions_;
    ExNativeArray<quaternion> vertex_local_rotations_;
    ExNativeArray<int> vertex_parent_indices_;
    ExNativeArray<std::uint32_t> vertex_child_index_array_;
    ExNativeArray<std::uint16_t> vertex_child_data_array_;
    ExNativeArray<quaternion> normal_adjustment_rotations_;

    ExNativeArray<std::int16_t> triangle_team_ids_;
    ExNativeArray<int3> triangles_;
    ExNativeArray<float3> triangle_normals_;
    ExNativeArray<float3> triangle_tangents_;

    ExNativeArray<std::int16_t> edge_team_ids_;
    ExNativeArray<int2> edges_;
    ExNativeArray<BitFlag8> edge_flags_;

    ExNativeArray<BitFlag8> base_line_flags_;
    ExNativeArray<std::int16_t> base_line_team_ids_;
    ExNativeArray<std::uint16_t> base_line_start_data_indices_;
    ExNativeArray<std::uint16_t> base_line_data_counts_;
    ExNativeArray<std::uint16_t> base_line_data_;

    ExNativeArray<float3> local_positions_;
    ExNativeArray<float3> local_normals_;
    ExNativeArray<float3> local_tangents_;
    ExNativeArray<VirtualMeshBoneWeight> bone_weights_;
    ExNativeArray<int> skin_bone_transform_indices_;
    ExNativeArray<float4x4> skin_bone_bind_poses_;
    ExNativeArray<quaternion> vertex_to_transform_rotations_;
    ExNativeArray<float3> positions_;
    ExNativeArray<quaternion> rotations_;

    ExNativeArray<short> mapping_id_array_;
    ExNativeArray<int> mapping_reference_indices_;
    ExNativeArray<VertexAttribute> mapping_attributes_;
    ExNativeArray<float3> mapping_local_positions_;
    ExNativeArray<float3> mapping_local_normals_;
    ExNativeArray<VirtualMeshBoneWeight> mapping_bone_weights_;
    ExNativeArray<float3> mapping_positions_;
    ExNativeArray<float3> mapping_normals_;
};

}  // namespace hocloth::mc2
