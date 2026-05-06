#pragma once

#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/ex_simple_native_array.hpp"
#include "hocloth/virtual_mesh/vertex_attribute.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_bone_weight.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hocloth::mc2 {

class VirtualMesh;

// Native data-layer port for Scripts/Core/Reduction/ReductionWorkData.cs.
struct ReductionWorkData {
    VirtualMesh* vmesh = nullptr;

    std::vector<int> vertex_join_indices;
    std::unordered_map<std::uint16_t, std::vector<std::uint16_t>> vertex_to_vertex_map;

    std::vector<int> vertex_remap_indices;
    std::vector<int> old_vertex_to_new_vertex_indices;
    std::unordered_map<int, int> use_skin_bone_map;
    std::unordered_map<std::uint16_t, std::vector<std::uint16_t>> new_vertex_to_vertex_map;
    std::unordered_set<std::uint64_t> edge_set;
    std::unordered_set<std::uint64_t> triangle_set;

    int old_vertex_count = 0;
    int new_vertex_count = 0;
    int remove_vertex_count = 0;

    ExSimpleNativeArray<VertexAttribute> new_attributes;
    ExSimpleNativeArray<float3> new_local_positions;
    ExSimpleNativeArray<float3> new_local_normals;
    ExSimpleNativeArray<float3> new_local_tangents;
    ExSimpleNativeArray<float2> new_uv;
    ExSimpleNativeArray<VirtualMeshBoneWeight> new_bone_weights;

    int new_skin_bone_count = 0;
    std::vector<int> new_skin_bone_transform_indices;
    std::vector<float4x4> new_skin_bone_bind_pose_list;

    std::vector<int2> new_line_list;
    std::vector<int3> new_triangle_list;

    ReductionWorkData() = default;
    explicit ReductionWorkData(VirtualMesh* mesh)
        : vmesh(mesh)
    {
    }

    void Dispose()
    {
        vertex_join_indices.clear();
        vertex_to_vertex_map.clear();
        vertex_remap_indices.clear();
        old_vertex_to_new_vertex_indices.clear();
        use_skin_bone_map.clear();
        new_vertex_to_vertex_map.clear();
        edge_set.clear();
        triangle_set.clear();
        new_attributes.Dispose();
        new_local_positions.Dispose();
        new_local_normals.Dispose();
        new_local_tangents.Dispose();
        new_uv.Dispose();
        new_bone_weights.Dispose();
        new_skin_bone_count = 0;
        new_skin_bone_transform_indices.clear();
        new_skin_bone_bind_pose_list.clear();
        new_line_list.clear();
        new_triangle_list.clear();
        old_vertex_count = 0;
        new_vertex_count = 0;
        remove_vertex_count = 0;
    }
};

}  // namespace hocloth::mc2
