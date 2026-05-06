#pragma once

#include "hocloth/manager/transform/transform_data.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/virtual_mesh/vertex_attribute.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_bone_weight.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

class VirtualMesh;

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/Function/VirtualMeshSerialization.cs
struct VirtualMeshSerializationData {
    template <typename T>
    struct SimpleArrayData {
        std::vector<T> data;
        int count = 0;

        [[nodiscard]] bool IsValid() const
        {
            return count > 0 && count <= static_cast<int>(data.size());
        }
    };

    struct ShareSerializationData {
        std::string name;
        int mesh_type = 0;
        bool is_bone_cloth = false;

        SimpleArrayData<int> reference_indices;
        SimpleArrayData<VertexAttribute> attributes;
        SimpleArrayData<float3> local_positions;
        SimpleArrayData<float3> local_normals;
        SimpleArrayData<float3> local_tangents;
        SimpleArrayData<float2> uv;
        SimpleArrayData<VirtualMeshBoneWeight> bone_weights;
        SimpleArrayData<int3> triangles;
        SimpleArrayData<int2> lines;
        int center_transform_index = -1;
        float4x4 init_local_to_world{};
        float4x4 init_world_to_local{};
        quaternion init_rotation{};
        quaternion init_inverse_rotation{};
        float3 init_scale{1.0f, 1.0f, 1.0f};
        int skin_root_index = -1;
        SimpleArrayData<int> skin_bone_transform_indices;
        SimpleArrayData<float4x4> skin_bone_bind_poses;
        TransformData transform_data;
        AABB bounding_box;
        float average_vertex_distance = 0.0f;
        float max_vertex_distance = 0.0f;

        std::vector<std::uint8_t> vertex_to_triangles;
        std::vector<std::uint8_t> vertex_to_vertex_index_array;
        std::vector<std::uint8_t> vertex_to_vertex_data_array;
        std::vector<std::uint8_t> edges;
        std::vector<std::uint8_t> edge_flags;
        std::vector<int2> edge_to_triangles_keys;
        std::vector<std::uint16_t> edge_to_triangles_values;
        std::vector<std::uint8_t> vertex_bind_pose_positions;
        std::vector<std::uint8_t> vertex_bind_pose_rotations;
        std::vector<std::uint8_t> vertex_to_transform_rotations;
        std::vector<std::uint8_t> vertex_depths;
        std::vector<std::uint8_t> vertex_root_indices;
        std::vector<std::uint8_t> vertex_parent_indices;
        std::vector<std::uint8_t> vertex_child_index_array;
        std::vector<std::uint8_t> vertex_child_data_array;
        std::vector<std::uint8_t> vertex_local_positions;
        std::vector<std::uint8_t> vertex_local_rotations;
        std::vector<std::uint8_t> normal_adjustment_rotations;
        std::vector<std::uint8_t> base_line_flags;
        std::vector<std::uint8_t> base_line_start_data_indices;
        std::vector<std::uint8_t> base_line_data_counts;
        std::vector<std::uint8_t> base_line_data;
        std::vector<int> custom_skinning_bone_indices;
        std::vector<std::uint16_t> center_fixed_list;
        float3 local_center_position{};

        float3 center_world_position{};
        quaternion center_world_rotation{};
        float3 center_world_scale{1.0f, 1.0f, 1.0f};
        float4x4 to_proxy_matrix{};
        quaternion to_proxy_rotation{};

        [[nodiscard]] std::string ToString() const
        {
            std::ostringstream stream;
            stream << "===== VirtualMesh.SerializeData =====\n";
            stream << "name:" << name << '\n';
            stream << "meshType:" << mesh_type << '\n';
            stream << "isBoneCloth:" << (is_bone_cloth ? "true" : "false") << '\n';
            stream << "VertexCount:" << attributes.count << '\n';
            stream << "TriangleCount:" << triangles.count << '\n';
            stream << "LineCount:" << lines.count << '\n';
            return stream.str();
        }
    };

    struct UniqueSerializationData {
        TransformData transform_data;

        [[nodiscard]] std::vector<int> GetUsedTransforms() const
        {
            return transform_data.id_array;
        }

        void ReplaceTransform(const std::unordered_map<int, int>& replace_dict)
        {
            for (int& transform_id : transform_data.id_array) {
                const auto iterator = replace_dict.find(transform_id);
                if (iterator != replace_dict.end()) {
                    transform_id = iterator->second;
                }
            }
        }
    };

    [[nodiscard]] static VirtualMesh ShareDeserialize(const ShareSerializationData& data);
    [[nodiscard]] static std::vector<TransformRecord> UniqueTransformRecords(
        const UniqueSerializationData& data
    );
};

}  // namespace hocloth::mc2
