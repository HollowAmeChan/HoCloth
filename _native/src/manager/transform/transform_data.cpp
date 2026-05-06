#include "hocloth/manager/transform/transform_data.hpp"

#include <algorithm>
#include <cstddef>

namespace hocloth::mc2 {

void TransformData::Initialize(int capacity)
{
    flag_array = ExNativeArray<BitFlag8>(capacity);
    init_local_position_array = ExNativeArray<float3>(capacity);
    init_local_rotation_array = ExNativeArray<quaternion>(capacity);
    position_array = ExNativeArray<float3>(capacity);
    rotation_array = ExNativeArray<quaternion>(capacity);
    inverse_rotation_array = ExNativeArray<quaternion>(capacity);
    scale_array = ExNativeArray<float3>(capacity);
    local_position_array = ExNativeArray<float3>(capacity);
    local_rotation_array = ExNativeArray<quaternion>(capacity);
    local_to_world_matrix_array = ExNativeArray<float4x4>(capacity);
    team_id_array = ExNativeArray<std::int16_t>(capacity);
    EnsureRecordCapacity(capacity);
    is_dirty = true;
}

void TransformData::Dispose()
{
    flag_array.Dispose();
    init_local_position_array.Dispose();
    init_local_rotation_array.Dispose();
    position_array.Dispose();
    rotation_array.Dispose();
    inverse_rotation_array.Dispose();
    scale_array.Dispose();
    local_position_array.Dispose();
    local_rotation_array.Dispose();
    local_to_world_matrix_array.Dispose();
    team_id_array.Dispose();
    id_array.clear();
    parent_id_array.clear();
    name_array.clear();
    root_id_list.clear();
    is_dirty = false;
}

void TransformData::EnsureRecordCapacity(int count)
{
    if (count <= 0) {
        return;
    }
    const auto size = static_cast<std::size_t>(count);
    id_array.resize(std::max(id_array.size(), size));
    parent_id_array.resize(std::max(parent_id_array.size(), size));
    name_array.resize(std::max(name_array.size(), size));
}

int TransformData::Count() const
{
    return flag_array.Count();
}

bool TransformData::IsValid() const
{
    return flag_array.IsValid();
}

int TransformData::RootCount() const
{
    return static_cast<int>(root_id_list.size());
}

bool TransformData::IsEmpty() const
{
    return !flag_array.IsValid() || flag_array.Length() == 0;
}

}  // namespace hocloth::mc2
