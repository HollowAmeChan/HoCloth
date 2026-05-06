#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include "hocloth/manager/transform/transform_data_serialization.hpp"
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
    if (mesh.edges.Count() <= 0 && mesh.lines.Count() > 0) {
        mesh.edges.AddRange(mesh.lines);
        mesh.edge_flags.AddRange(mesh.lines.Count(), BitFlag8{});
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
