#pragma once

#include "hocloth/manager/transform/transform_data.hpp"
#include "hocloth/manager/render/render_setup_data_serialization.hpp"
#include "hocloth/cloth/selection_data.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/grid/grid_map.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/ex_simple_native_array.hpp"
#include "hocloth/utility/native_collection/fixed_list.hpp"
#include "hocloth/utility/result_code/result_code.hpp"
#include "hocloth/virtual_mesh/vertex_attribute.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_bone_weight.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_raycast_hit.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

struct ReductionSettings;
struct ReductionWorkData;

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VirtualMesh.cs
class VirtualMesh {
public:
    static constexpr std::uint8_t EdgeFlagCut = 0x01;

    using VertexTriangleList = FixedList<std::uint32_t, 7>;
    using EdgeToTrianglesMap =
        std::unordered_map<std::uint32_t, std::vector<std::uint16_t>>;
    using ShareSerializationData = VirtualMeshSerializationData::ShareSerializationData;
    using UniqueSerializationData = VirtualMeshSerializationData::UniqueSerializationData;

    enum class MeshType {
        NormalMesh = 0,
        NormalBoneMesh = 1,
        ProxyMesh = 2,
        ProxyBoneMesh = 3,
        Mapping = 4,
    };

    std::string name;
    Result result = Result::Ok();
    bool is_managed = false;
    MeshType mesh_type = MeshType::NormalMesh;
    bool is_bone_cloth = false;

    ExSimpleNativeArray<int> reference_indices;
    ExSimpleNativeArray<VertexAttribute> attributes;
    ExSimpleNativeArray<float3> local_positions;
    ExSimpleNativeArray<float3> local_normals;
    ExSimpleNativeArray<float3> local_tangents;
    ExSimpleNativeArray<float2> uv;
    ExSimpleNativeArray<VertexTriangleList> vertex_to_triangles;
    ExSimpleNativeArray<std::uint32_t> vertex_to_vertex_index_array;
    ExSimpleNativeArray<std::uint16_t> vertex_to_vertex_data_array;
    ExSimpleNativeArray<VirtualMeshBoneWeight> bone_weights;
    ExSimpleNativeArray<int3> triangles;
    ExSimpleNativeArray<int2> lines;
    ExSimpleNativeArray<int2> edges;
    ExSimpleNativeArray<BitFlag8> edge_flags;
    EdgeToTrianglesMap edge_to_triangles;

    int center_transform_index = -1;
    float4x4 init_local_to_world{};
    float4x4 init_world_to_local{};
    quaternion init_rotation{};
    quaternion init_inverse_rotation{};
    float3 init_scale{1.0f, 1.0f, 1.0f};

    int skin_root_index = -1;
    ExSimpleNativeArray<int> skin_bone_transform_indices;
    ExSimpleNativeArray<float4x4> skin_bone_bind_poses;
    TransformData transform_data;
    AABB bounding_box;
    float average_vertex_distance = 0.0f;
    float max_vertex_distance = 0.0f;
    DataChunk merge_chunk;
    float4x4 to_proxy_matrix{};
    quaternion to_proxy_rotation{};
    int mapping_id = -1;

    ExSimpleNativeArray<std::uint32_t> vertex_child_index_array;
    ExSimpleNativeArray<std::uint16_t> vertex_child_data_array;
    ExSimpleNativeArray<float3> vertex_bind_pose_positions;
    ExSimpleNativeArray<quaternion> vertex_bind_pose_rotations;
    ExSimpleNativeArray<float> vertex_depths;
    ExSimpleNativeArray<int> vertex_root_indices;
    ExSimpleNativeArray<int> vertex_parent_indices;
    ExSimpleNativeArray<std::uint16_t> center_fixed_list;
    ExSimpleNativeArray<float3> vertex_local_positions;
    ExSimpleNativeArray<quaternion> vertex_local_rotations;
    ExSimpleNativeArray<BitFlag8> base_line_flags;
    ExSimpleNativeArray<std::uint16_t> base_line_start_data_indices;
    ExSimpleNativeArray<std::uint16_t> base_line_data_counts;
    ExSimpleNativeArray<std::uint16_t> base_line_data;
    ExSimpleNativeArray<quaternion> normal_adjustment_rotations;
    ExSimpleNativeArray<quaternion> vertex_to_transform_rotations;
    std::vector<int> custom_skinning_bone_indices;
    float3 local_center_position{};
    float3 center_world_position{};
    quaternion center_world_rotation{};
    float3 center_world_scale{1.0f, 1.0f, 1.0f};

    void Dispose();
    void CreateProxyFixedListAndAABB();
    void CreateVertexBindPose();
    void CreateVertexToTransformRotations();
    void BuildVertexToTriangles();
    void BuildEdgeToTriangles();
    void BuildEdgeFlags();
    void ConvertInvalidToFixed();
    void ApplySelectionAttribute(const SelectionData& selection_data);
    void ApplyBoneClothDefaultSelection();
    void BuildBoneConnection(
        RenderSetupData::BoneConnectionMode connection_mode,
        bool setup_as_bone_spring = false
    );
    void BuildMeshBaseLinesFromEdges();
    void BuildTransformBaseLines();
    void BuildBaseLinesFromParents();
    void CalcAverageAndMaxVertexDistanceRun();
    [[nodiscard]] GridMap<int> CreateVertexIndexGridMapRun(float grid_size) const;
    [[nodiscard]] VirtualMeshRaycastHit IntersectRayMesh(
        const float3& ray_position,
        const float3& ray_direction,
        bool double_side,
        float point_radius
    ) const;
    void Optimization();
    void RemoveDuplicateTriangles();
    void Reduction(const ReductionSettings& settings);
    void InitReductionWorkData(ReductionWorkData& work_data);
    void Organization(const ReductionSettings& settings, ReductionWorkData& work_data);
    void OrganizationInit(const ReductionSettings& settings, ReductionWorkData& work_data);
    void OrganizationCreateRemapData(ReductionWorkData& work_data);
    void OrganizationCreateBasicData(ReductionWorkData& work_data);
    void OrganizationCreateLineTriangle(ReductionWorkData& work_data);
    void OrganizeStoreVirtualMesh(ReductionWorkData& work_data);
    void Mapping(VirtualMesh& proxy_mesh);

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] int VertexCount() const;
    [[nodiscard]] int TriangleCount() const;
    [[nodiscard]] int LineCount() const;
    [[nodiscard]] int SkinBoneCount() const;
    [[nodiscard]] int TransformCount() const;
    [[nodiscard]] int CustomSkinningBoneCount() const;
    [[nodiscard]] int CenterFixedPointCount() const;
    [[nodiscard]] int BaseLineCount() const;
    [[nodiscard]] bool IsProxy() const;
    [[nodiscard]] bool IsMapping() const;
    [[nodiscard]] bool CompareSpace(const VirtualMesh& target) const;
    [[nodiscard]] float4x4 CenterTransformTo(const VirtualMesh& target) const;

private:
    struct MappingWorkData {
        float3 position{};
        int vertex_index = -1;
        int proxy_vertex_index = -1;
        float proxy_vertex_distance = 0.0f;
    };

    static float4 CalcMappingVertexWeights(float4 distances);
    void DirectMapping(VirtualMesh& proxy_mesh, const float4x4& to_proxy, std::vector<MappingWorkData>& mapping_work_data);
    void SearchMapping(VirtualMesh& proxy_mesh, const float4x4& to_proxy, std::vector<MappingWorkData>& mapping_work_data);
    void CalcDirectMappingWeights(VirtualMesh& proxy_mesh, const std::vector<MappingWorkData>& mapping_work_data, float weight_length);
    void CalcSearchMappingWeights(VirtualMesh& proxy_mesh, const std::vector<MappingWorkData>& mapping_work_data);
};

}  // namespace hocloth::mc2
