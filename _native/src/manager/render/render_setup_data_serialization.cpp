#include "hocloth/manager/render/render_setup_data_serialization.hpp"

#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <iterator>

namespace hocloth::mc2 {

bool RenderSetupData::IsSuccess() const
{
    return result.IsSuccess();
}

bool RenderSetupData::IsFailed() const
{
    return result.IsFailed();
}

bool RenderSetupData::HasLocalPositions() const
{
    return !local_positions.empty();
}

void RenderSetupData::Dispose()
{
    bind_pose_list.clear();
    bones_per_vertex_array.clear();
    bone_weight_array.clear();
    local_positions.clear();
    local_normals.clear();
    root_transform_ids.clear();
    transform_ids.clear();
    transform_parent_ids.clear();
    transform_child_ids.clear();
    collision_bone_indices.clear();
    transform_names.clear();
    transform_positions.clear();
    transform_rotations.clear();
    transform_scales.clear();
    transform_local_positions.clear();
    transform_local_rotations.clear();
    transform_inverse_rotations.clear();
    is_managed = false;
    result.Clear();
}

int RenderSetupData::TransformCount() const
{
    return static_cast<int>(transform_ids.size());
}

int RenderSetupData::GetTransformIndexFromId(int id) const
{
    const auto found = std::find(transform_ids.begin(), transform_ids.end(), id);
    if (found == transform_ids.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(transform_ids.begin(), found));
}

TransformRecord RenderSetupData::GetTransformRecordFromIndex(int index) const
{
    if (index < 0 || index >= TransformCount()) {
        return TransformRecord{};
    }

    const auto read_int = [index](const std::vector<int>& values, int fallback) {
        return index < static_cast<int>(values.size())
            ? values[static_cast<std::size_t>(index)]
            : fallback;
    };
    const auto read_name = [index](const std::vector<std::string>& values) {
        return index < static_cast<int>(values.size())
            ? values[static_cast<std::size_t>(index)]
            : std::string{};
    };
    const auto read_float3 = [index](const std::vector<float3>& values, const float3& fallback) {
        return index < static_cast<int>(values.size())
            ? values[static_cast<std::size_t>(index)]
            : fallback;
    };
    const auto read_quaternion =
        [index](const std::vector<quaternion>& values, const quaternion& fallback) {
            return index < static_cast<int>(values.size())
                ? values[static_cast<std::size_t>(index)]
                : fallback;
        };

    TransformRecord record;
    record.id = read_int(transform_ids, 0);
    record.parent_id = read_int(transform_parent_ids, 0);
    record.name = read_name(transform_names);
    record.local_position = read_float3(transform_local_positions, float3{});
    record.local_rotation = read_quaternion(transform_local_rotations, quaternion{});
    record.position = read_float3(transform_positions, record.local_position);
    record.rotation = read_quaternion(transform_rotations, record.local_rotation);
    record.scale = read_float3(transform_scales, float3{1.0f, 1.0f, 1.0f});
    record.local_to_world_matrix = TRS(record.position, record.rotation, record.scale);
    record.world_to_local_matrix = InverseAffine(record.local_to_world_matrix);
    return record;
}

TransformRecord RenderSetupData::GetTransformRecordFromId(int id) const
{
    return GetTransformRecordFromIndex(GetTransformIndexFromId(id));
}

int RenderSetupData::GetParentTransformIndex(int index, bool center_excluded) const
{
    if (index < 0 || index >= static_cast<int>(transform_parent_ids.size())) {
        return -1;
    }
    int parent_index = GetTransformIndexFromId(transform_parent_ids[static_cast<std::size_t>(index)]);
    if (center_excluded && parent_index == render_transform_index) {
        parent_index = -1;
    }
    return parent_index;
}

RenderSetupData RenderSetupData::ShareDeserialize(
    const ShareSerializationData& data
)
{
    RenderSetupData setup;
    setup.is_managed = true;

    if (data.result.IsFailed()) {
        setup.result = data.result;
        return setup;
    }

    setup.name = data.name;
    setup.setup_type = data.setup_type;
    setup.original_mesh_id = data.original_mesh_id;
    setup.vertex_count = data.vertex_count;
    setup.has_skinned_mesh = data.has_skinned_mesh;
    setup.has_bone_weight = data.has_bone_weight;
    setup.skin_root_bone_index = data.skin_root_bone_index;
    setup.skin_bone_count = data.skin_bone_count;
    setup.bind_pose_list = data.bind_pose_list;
    setup.bones_per_vertex_array = data.bones_per_vertex_array;
    setup.bone_weight_array = data.bone_weight_array;
    setup.local_positions = data.local_positions;
    setup.local_normals = data.local_normals;
    setup.bone_connection_mode = data.bone_connection_mode;
    setup.root_transform_ids = data.root_transform_ids;
    setup.transform_ids = data.transform_ids;
    setup.transform_parent_ids = data.transform_parent_ids;
    setup.transform_child_ids = data.transform_child_ids;
    setup.collision_bone_indices = data.collision_bone_indices;
    setup.transform_names = data.transform_names;
    setup.transform_positions = data.transform_positions;
    setup.transform_rotations = data.transform_rotations;
    setup.transform_scales = data.transform_scales;
    setup.transform_local_positions = data.transform_local_positions;
    setup.transform_local_rotations = data.transform_local_rotations;
    setup.transform_inverse_rotations = data.transform_inverse_rotations;
    setup.init_render_local_to_world = data.init_render_local_to_world;
    setup.init_render_world_to_local = data.init_render_world_to_local;
    setup.init_render_rotation = data.init_render_rotation;
    setup.init_render_scale = data.init_render_scale;
    setup.render_transform_index = data.render_transform_index;
    setup.result.SetSuccess();
    return setup;
}

}  // namespace hocloth::mc2
