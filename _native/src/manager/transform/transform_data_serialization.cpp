#include "hocloth/manager/transform/transform_data_serialization.hpp"

#include <algorithm>

namespace hocloth::mc2 {

namespace {

template <typename T>
std::vector<T> CopyArrayPrefix(const ExNativeArray<T>& array)
{
    const int count = array.Count();
    if (count <= 0) {
        return {};
    }
    const int copy_count = std::min(count, array.Length());
    return std::vector<T>(array.Data().begin(), array.Data().begin() + copy_count);
}

template <typename T>
TransformDataSerialization::SimpleArrayData<T> SerializeArray(
    const ExNativeArray<T>& array
)
{
    TransformDataSerialization::SimpleArrayData<T> result;
    result.count = array.Count();
    result.length = array.Length();
    result.data = CopyArrayPrefix(array);
    return result;
}

template <typename T>
void DeserializeArray(
    ExNativeArray<T>& target,
    const TransformDataSerialization::SimpleArrayData<T>& source
)
{
    target.Dispose();
    const int capacity = std::max(source.length, source.count);
    if (capacity > 0) {
        target = ExNativeArray<T>(capacity);
    }
    const int count = std::min(source.count, static_cast<int>(source.data.size()));
    if (count > 0) {
        target.AddRange(source.data, count);
    }
}

}  // namespace

TransformDataSerialization::ShareSerializationData TransformDataSerialization::ShareSerialize(
    const TransformData& data
)
{
    ShareSerializationData result;
    result.flag_array = SerializeArray(data.flag_array);
    result.init_local_position_array = SerializeArray(data.init_local_position_array);
    result.init_local_rotation_array = SerializeArray(data.init_local_rotation_array);
    return result;
}

TransformData TransformDataSerialization::ShareDeserialize(
    const ShareSerializationData& data
)
{
    TransformData result;
    DeserializeArray(result.flag_array, data.flag_array);
    DeserializeArray(result.init_local_position_array, data.init_local_position_array);
    DeserializeArray(result.init_local_rotation_array, data.init_local_rotation_array);
    result.EnsureRecordCapacity(result.Count());
    return result;
}

TransformDataSerialization::UniqueSerializationData TransformDataSerialization::UniqueSerialize(
    const TransformData& data
)
{
    UniqueSerializationData result;
    const int count = data.Count();
    result.transform_array.reserve(static_cast<std::size_t>(count));
    const auto read_array = [](const auto& array, int index, const auto& fallback) {
        return index >= 0 && index < array.Length() ? array[index] : fallback;
    };
    for (int index = 0; index < count; ++index) {
        const int id =
            index < static_cast<int>(data.id_array.size())
                ? data.id_array[static_cast<std::size_t>(index)]
                : 0;
        const int parent_id =
            index < static_cast<int>(data.parent_id_array.size())
                ? data.parent_id_array[static_cast<std::size_t>(index)]
                : 0;
        const std::string name =
            index < static_cast<int>(data.name_array.size())
                ? data.name_array[static_cast<std::size_t>(index)]
                : std::string{};
        result.transform_array.push_back(TransformRecord{
            id,
            parent_id,
            name,
            read_array(data.local_position_array, index, float3{}),
            read_array(data.local_rotation_array, index, quaternion{}),
            read_array(data.position_array, index, float3{}),
            read_array(data.rotation_array, index, quaternion{}),
            read_array(data.scale_array, index, float3{1.0f, 1.0f, 1.0f}),
            read_array(data.local_to_world_matrix_array, index, float4x4{}),
            float4x4{},
        });
    }
    return result;
}

}  // namespace hocloth::mc2
