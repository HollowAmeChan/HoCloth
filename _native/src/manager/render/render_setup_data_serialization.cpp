#include "hocloth/manager/render/render_setup_data_serialization.hpp"

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
    is_managed = false;
    result.Clear();
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
    setup.render_transform_index = data.render_transform_index;
    setup.result.SetSuccess();
    return setup;
}

}  // namespace hocloth::mc2
