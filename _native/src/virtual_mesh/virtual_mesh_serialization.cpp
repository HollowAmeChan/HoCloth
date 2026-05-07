#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include "hocloth/manager/transform/transform_data_serialization.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/native_collection/native_array_extensions.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <algorithm>

namespace hocloth::mc2 {

namespace {

template <typename T>
void DeserializeSimpleArray(
    ExSimpleNativeArray<T>& target,
    const VirtualMeshSerializationData::SimpleArrayData<T>& source
)
{
    target.Dispose();
    if (source.count <= 0 || source.data.empty()) {
        return;
    }
    const int count = std::min(source.count, static_cast<int>(source.data.size()));
    target.AddRange(source.data, count);
}

template <typename T>
void DeserializeRawArray(ExSimpleNativeArray<T>& target, const std::vector<std::uint8_t>& bytes)
{
    target.Dispose();
    const std::vector<T> values = native_array_extensions::FromRawBytes<T>(bytes);
    if (!values.empty()) {
        target.AddRange(values);
    }
}

void DeserializeRawBitFlag8Array(
    ExSimpleNativeArray<BitFlag8>& target,
    const std::vector<std::uint8_t>& bytes
)
{
    target.Dispose();
    const std::vector<BitFlag8> values =
        native_array_extensions::FromRawBitFlag8Bytes(bytes);
    if (!values.empty()) {
        target.AddRange(values);
    }
}

void DeserializeFixedList32UIntArray(
    ExSimpleNativeArray<VirtualMesh::VertexTriangleList>& target,
    const std::vector<std::uint8_t>& bytes
)
{
    target.Dispose();

    constexpr std::size_t kUnityFixedList32Bytes = 32;
    constexpr std::size_t kUnityLengthBytes = 2;
    constexpr std::size_t kUIntBytes = sizeof(std::uint32_t);
    if (bytes.size() < kUnityFixedList32Bytes) {
        return;
    }

    const std::size_t count = bytes.size() / kUnityFixedList32Bytes;
    for (std::size_t list_index = 0; list_index < count; ++list_index) {
        const std::size_t offset = list_index * kUnityFixedList32Bytes;
        const auto length = static_cast<int>(
            static_cast<std::uint16_t>(bytes[offset])
            | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8)
        );

        VirtualMesh::VertexTriangleList list;
        const int value_count =
            std::min(length, VirtualMesh::VertexTriangleList::CapacityValue);
        for (int value_index = 0; value_index < value_count; ++value_index) {
            const std::size_t value_offset =
                offset + kUnityLengthBytes + static_cast<std::size_t>(value_index) * kUIntBytes;
            if (value_offset + kUIntBytes > offset + kUnityFixedList32Bytes) {
                break;
            }
            const std::uint32_t value =
                static_cast<std::uint32_t>(bytes[value_offset])
                | (static_cast<std::uint32_t>(bytes[value_offset + 1]) << 8)
                | (static_cast<std::uint32_t>(bytes[value_offset + 2]) << 16)
                | (static_cast<std::uint32_t>(bytes[value_offset + 3]) << 24);
            list.Add(value);
        }
        target.Add(list);
    }
}

}  // namespace

VirtualMesh VirtualMeshSerializationData::ShareDeserialize(
    const ShareSerializationData& data
)
{
    // Port target: Scripts/Core/VirtualMesh/Function/VirtualMeshSerialization.cs
    // ShareDeserialize(). Packed byte dictionaries stay as serialized payload until the native
    // binary decoder is ported; structured MC2 arrays are restored here.
    VirtualMesh mesh;
    mesh.name = data.name;
    mesh.mesh_type = static_cast<VirtualMesh::MeshType>(data.mesh_type);
    mesh.is_bone_cloth = data.is_bone_cloth;
    mesh.center_transform_index = data.center_transform_index;
    mesh.init_local_to_world = data.init_local_to_world;
    mesh.init_world_to_local = data.init_world_to_local;
    mesh.init_rotation = data.init_rotation;
    mesh.init_inverse_rotation = data.init_inverse_rotation;
    mesh.init_scale = data.init_scale;
    mesh.skin_root_index = data.skin_root_index;
    mesh.transform_data = data.transform_data;
    mesh.bounding_box = data.bounding_box;
    mesh.average_vertex_distance = data.average_vertex_distance;
    mesh.max_vertex_distance = data.max_vertex_distance;
    mesh.to_proxy_matrix = data.to_proxy_matrix;
    mesh.to_proxy_rotation = data.to_proxy_rotation;

    DeserializeSimpleArray(mesh.reference_indices, data.reference_indices);
    DeserializeSimpleArray(mesh.attributes, data.attributes);
    DeserializeSimpleArray(mesh.local_positions, data.local_positions);
    DeserializeSimpleArray(mesh.local_normals, data.local_normals);
    DeserializeSimpleArray(mesh.local_tangents, data.local_tangents);
    DeserializeSimpleArray(mesh.uv, data.uv);
    DeserializeSimpleArray(mesh.bone_weights, data.bone_weights);
    DeserializeSimpleArray(mesh.triangles, data.triangles);
    DeserializeSimpleArray(mesh.lines, data.lines);
    DeserializeSimpleArray(mesh.skin_bone_transform_indices, data.skin_bone_transform_indices);
    DeserializeSimpleArray(mesh.skin_bone_bind_poses, data.skin_bone_bind_poses);

    DeserializeFixedList32UIntArray(mesh.vertex_to_triangles, data.vertex_to_triangles);
    DeserializeRawArray(mesh.vertex_to_vertex_index_array, data.vertex_to_vertex_index_array);
    DeserializeRawArray(mesh.vertex_to_vertex_data_array, data.vertex_to_vertex_data_array);
    DeserializeRawArray(mesh.edges, data.edges);
    DeserializeRawBitFlag8Array(mesh.edge_flags, data.edge_flags);
    DeserializeRawArray(mesh.vertex_bind_pose_positions, data.vertex_bind_pose_positions);
    DeserializeRawArray(mesh.vertex_bind_pose_rotations, data.vertex_bind_pose_rotations);
    DeserializeRawArray(mesh.vertex_to_transform_rotations, data.vertex_to_transform_rotations);
    DeserializeRawArray(mesh.vertex_depths, data.vertex_depths);
    DeserializeRawArray(mesh.vertex_root_indices, data.vertex_root_indices);
    DeserializeRawArray(mesh.vertex_parent_indices, data.vertex_parent_indices);
    DeserializeRawArray(mesh.vertex_child_index_array, data.vertex_child_index_array);
    DeserializeRawArray(mesh.vertex_child_data_array, data.vertex_child_data_array);
    DeserializeRawArray(mesh.vertex_local_positions, data.vertex_local_positions);
    DeserializeRawArray(mesh.vertex_local_rotations, data.vertex_local_rotations);
    DeserializeRawArray(mesh.normal_adjustment_rotations, data.normal_adjustment_rotations);
    DeserializeRawBitFlag8Array(mesh.base_line_flags, data.base_line_flags);
    DeserializeRawArray(mesh.base_line_start_data_indices, data.base_line_start_data_indices);
    DeserializeRawArray(mesh.base_line_data_counts, data.base_line_data_counts);
    DeserializeRawArray(mesh.base_line_data, data.base_line_data);

    if (!data.center_fixed_list.empty()) {
        mesh.center_fixed_list.AddRange(data.center_fixed_list);
    }
    const std::size_t edge_triangle_count =
        std::min(data.edge_to_triangles_keys.size(), data.edge_to_triangles_values.size());
    mesh.edge_to_triangles.clear();
    mesh.edge_to_triangles.reserve(edge_triangle_count);
    for (std::size_t index = 0; index < edge_triangle_count; ++index) {
        const int2 edge = data.edge_to_triangles_keys[index];
        mesh.edge_to_triangles[data::Pack32(edge.x, edge.y)].push_back(
            data.edge_to_triangles_values[index]
        );
    }
    mesh.custom_skinning_bone_indices = data.custom_skinning_bone_indices;
    mesh.local_center_position = data.local_center_position;
    mesh.center_world_position = data.center_world_position;
    mesh.center_world_rotation = data.center_world_rotation;
    mesh.center_world_scale = data.center_world_scale;

    if (mesh.edges.Count() <= 0 && mesh.lines.Count() > 0) {
        mesh.edges.AddRange(mesh.lines);
        mesh.edge_flags.AddRange(mesh.lines.Count(), BitFlag8{});
    }
    if (mesh.edge_to_triangles.empty() && mesh.TriangleCount() > 0) {
        mesh.BuildEdgeToTriangles();
    }
    if (mesh.IsProxy() && mesh.base_line_start_data_indices.Count() <= 0) {
        if (mesh.is_bone_cloth) {
            mesh.BuildTransformBaseLines();
        } else {
            mesh.BuildMeshBaseLinesFromEdges();
        }
    }

    mesh.result = Result::Ok();
    return mesh;
}

std::vector<TransformRecord> VirtualMeshSerializationData::UniqueTransformRecords(
    const UniqueSerializationData& data
)
{
    return TransformDataSerialization::UniqueSerialize(data.transform_data).transform_array;
}

}  // namespace hocloth::mc2
