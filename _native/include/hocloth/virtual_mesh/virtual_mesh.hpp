#pragma once

#include "hocloth/manager/transform/transform_data.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/ex_simple_native_array.hpp"
#include "hocloth/utility/result_code/result_code.hpp"
#include "hocloth/virtual_mesh/vertex_attribute.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_bone_weight.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include <cstdint>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VirtualMesh.cs
class VirtualMesh {
public:
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
    ExSimpleNativeArray<VirtualMeshBoneWeight> bone_weights;
    ExSimpleNativeArray<int3> triangles;
    ExSimpleNativeArray<int2> lines;
    ExSimpleNativeArray<int2> edges;
    ExSimpleNativeArray<BitFlag8> edge_flags;

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

    void Dispose();
    void CreateProxyFixedListAndAABB();
    void CreateVertexBindPose();
    void CreateVertexToTransformRotations();
    void BuildMeshBaseLinesFromEdges();
    void BuildTransformBaseLines();
    void BuildBaseLinesFromParents();

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] int VertexCount() const;
    [[nodiscard]] int TriangleCount() const;
    [[nodiscard]] int LineCount() const;
    [[nodiscard]] int SkinBoneCount() const;
    [[nodiscard]] int TransformCount() const;
    [[nodiscard]] int CenterFixedPointCount() const;
    [[nodiscard]] int BaseLineCount() const;
    [[nodiscard]] bool IsProxy() const;
    [[nodiscard]] bool IsMapping() const;
};

}  // namespace hocloth::mc2
