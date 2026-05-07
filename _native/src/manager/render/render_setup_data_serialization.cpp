#include "hocloth/manager/render/render_setup_data_serialization.hpp"

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
    setup.render_transform_index = data.render_transform_index;
    setup.result.SetSuccess();
    return setup;
}

}  // namespace hocloth::mc2
