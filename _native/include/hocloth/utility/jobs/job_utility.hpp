#pragma once

#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/native_multi_hash_map_extensions.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <vector>

namespace hocloth::mc2::job_utility {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Jobs/JobUtility.cs
// The native backend runs these helpers synchronously; call sites can later swap in a scheduler.
inline constexpr float Pi = 3.14159265358979323846f;

template <typename T>
void Fill(std::vector<T>& array, int length, const T& value)
{
    const int count = std::min(length, static_cast<int>(array.size()));
    if (count > 0) {
        std::fill(array.begin(), array.begin() + count, value);
    }
}

template <typename T>
void Fill(std::vector<T>& array, int start_index, int length, const T& value)
{
    if (start_index < 0 || length <= 0 || start_index >= static_cast<int>(array.size())) {
        return;
    }
    const int count = std::min(length, static_cast<int>(array.size()) - start_index);
    std::fill(array.begin() + start_index, array.begin() + start_index + count, value);
}

template <typename T>
void FillRun(std::vector<T>& array, int length, const T& value)
{
    Fill(array, length, value);
}

template <typename T>
void FillRun(std::vector<T>& array, int start_index, int length, const T& value)
{
    Fill(array, start_index, length, value);
}

inline void Fill(int& reference, int value)
{
    reference = value;
}

inline void ClearReference(int& reference)
{
    reference = 0;
}

inline void SerialNumber(std::vector<int>& array, int length)
{
    const int count = std::min(length, static_cast<int>(array.size()));
    for (int index = 0; index < count; ++index) {
        array[static_cast<std::size_t>(index)] = index;
    }
}

inline void SerialNumberRun(std::vector<int>& array, int length)
{
    SerialNumber(array, length);
}

template <typename T, typename THash = std::hash<T>, typename TEqual = std::equal_to<T>>
[[nodiscard]] std::vector<T> ConvertHashSetToNativeList(
    const std::unordered_set<T, THash, TEqual>& hash_set
)
{
    std::vector<T> list;
    list.reserve(hash_set.size());
    for (const T& key : hash_set) {
        list.push_back(key);
    }
    return list;
}

template <typename T, typename THash = std::hash<T>, typename TEqual = std::equal_to<T>>
[[nodiscard]] std::vector<T> ConvertHashSetKeyToNativeList(
    const std::unordered_set<T, THash, TEqual>& hash_set
)
{
    return ConvertHashSetToNativeList(hash_set);
}

[[nodiscard]] inline AABB CalcAABB(const std::vector<float3>& positions, int length)
{
    if (positions.empty() || length <= 0) {
        return AABB{};
    }

    const int count = std::min(length, static_cast<int>(positions.size()));
    float3 min_pos{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    float3 max_pos{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };

    for (int index = 0; index < count; ++index) {
        const float3& position = positions[static_cast<std::size_t>(index)];
        min_pos.x = std::min(min_pos.x, position.x);
        min_pos.y = std::min(min_pos.y, position.y);
        min_pos.z = std::min(min_pos.z, position.z);
        max_pos.x = std::max(max_pos.x, position.x);
        max_pos.y = std::max(max_pos.y, position.y);
        max_pos.z = std::max(max_pos.z, position.z);
    }

    return AABB{min_pos, max_pos};
}

inline void CalcAABBRun(const std::vector<float3>& positions, int length, AABB& out_aabb)
{
    out_aabb = CalcAABB(positions, length);
}

[[nodiscard]] inline std::vector<float2> CalcUVWithSphereMapping(
    const std::vector<float3>& positions,
    int length,
    const AABB& aabb
)
{
    const int count = std::min(length, static_cast<int>(positions.size()));
    std::vector<float2> uvs(static_cast<std::size_t>(std::max(count, 0)));
    const float3 center = Center(aabb);
    for (int index = 0; index < count; ++index) {
        float3 local_vector = Subtract(positions[static_cast<std::size_t>(index)], center);
        local_vector = Normalize(local_vector);

        const float u = Clamp01((std::atan2(local_vector.x, local_vector.z) + Pi) / (2.0f * Pi));
        const float v = Clamp01((1.0f - local_vector.y) * 0.5f);
        const float add = static_cast<float>(index) * 0.0001234f;
        uvs[static_cast<std::size_t>(index)] = float2{v * 10.0f + add, u * 10.0f + add};
    }
    return uvs;
}

inline void CalcUVWithSphereMappingRun(
    const std::vector<float3>& positions,
    int length,
    const AABB& aabb,
    std::vector<float2>& out_uvs
)
{
    out_uvs = CalcUVWithSphereMapping(positions, length, aabb);
}

inline void TransformPosition(std::vector<float3>& positions, int length, const float4x4& to_matrix)
{
    const int count = std::min(length, static_cast<int>(positions.size()));
    for (int index = 0; index < count; ++index) {
        positions[static_cast<std::size_t>(index)] =
            TransformPoint(positions[static_cast<std::size_t>(index)], to_matrix);
    }
}

inline void TransformPositionRun(std::vector<float3>& positions, int length, const float4x4& to_matrix)
{
    TransformPosition(positions, length, to_matrix);
}

inline void TransformPosition(
    const std::vector<float3>& src_positions,
    std::vector<float3>& dst_positions,
    int length,
    const float4x4& to_matrix
)
{
    const int count = std::min(length, static_cast<int>(src_positions.size()));
    dst_positions.resize(static_cast<std::size_t>(std::max(count, 0)));
    for (int index = 0; index < count; ++index) {
        dst_positions[static_cast<std::size_t>(index)] =
            TransformPoint(src_positions[static_cast<std::size_t>(index)], to_matrix);
    }
}

[[nodiscard]] inline std::vector<float3> TransformPosition(
    const std::vector<float3>& src_positions,
    int length,
    const float4x4& to_matrix
)
{
    const int count = std::min(length, static_cast<int>(src_positions.size()));
    std::vector<float3> dst_positions(static_cast<std::size_t>(std::max(count, 0)));
    for (int index = 0; index < count; ++index) {
        dst_positions[static_cast<std::size_t>(index)] =
            TransformPoint(src_positions[static_cast<std::size_t>(index)], to_matrix);
    }
    return dst_positions;
}

inline void TransformPositionRun(
    const std::vector<float3>& src_positions,
    std::vector<float3>& dst_positions,
    int length,
    const float4x4& to_matrix
)
{
    dst_positions = TransformPosition(src_positions, length, to_matrix);
}

template <typename T>
void AddArithmeticDataCopy(
    const std::vector<T>& src_data,
    std::vector<T>& dst_data,
    int dst_offset,
    int length,
    const T& add_data
)
{
    if (dst_offset < 0 || length <= 0) {
        return;
    }
    const int count = std::min(length, static_cast<int>(src_data.size()));
    if (dst_offset + count > static_cast<int>(dst_data.size())) {
        dst_data.resize(static_cast<std::size_t>(dst_offset + count));
    }
    for (int index = 0; index < count; ++index) {
        dst_data[static_cast<std::size_t>(dst_offset + index)] =
            src_data[static_cast<std::size_t>(index)] + add_data;
    }
}

inline void AddIntDataCopy(
    const std::vector<int>& src_data,
    std::vector<int>& dst_data,
    int dst_offset,
    int length,
    int add_data
)
{
    AddArithmeticDataCopy(src_data, dst_data, dst_offset, length, add_data);
}

inline void AddInt2DataCopy(
    const std::vector<int2>& src_data,
    std::vector<int2>& dst_data,
    int dst_offset,
    int length,
    const int2& add_data
)
{
    if (dst_offset < 0 || length <= 0) {
        return;
    }
    const int count = std::min(length, static_cast<int>(src_data.size()));
    if (dst_offset + count > static_cast<int>(dst_data.size())) {
        dst_data.resize(static_cast<std::size_t>(dst_offset + count));
    }
    for (int index = 0; index < count; ++index) {
        const int2& data = src_data[static_cast<std::size_t>(index)];
        dst_data[static_cast<std::size_t>(dst_offset + index)] =
            int2{data.x + add_data.x, data.y + add_data.y};
    }
}

inline void AddInt3DataCopy(
    const std::vector<int3>& src_data,
    std::vector<int3>& dst_data,
    int dst_offset,
    int length,
    const int3& add_data
)
{
    if (dst_offset < 0 || length <= 0) {
        return;
    }
    const int count = std::min(length, static_cast<int>(src_data.size()));
    if (dst_offset + count > static_cast<int>(dst_data.size())) {
        dst_data.resize(static_cast<std::size_t>(dst_offset + count));
    }
    for (int index = 0; index < count; ++index) {
        const int3& data = src_data[static_cast<std::size_t>(index)];
        dst_data[static_cast<std::size_t>(dst_offset + index)] =
            int3{data.x + add_data.x, data.y + add_data.y, data.z + add_data.z};
    }
}

[[nodiscard]] inline native_multi_hash_map_extensions::NativeMultiHashMap<int, std::uint16_t>
ToNativeMultiHashMap(const std::vector<std::uint32_t>& index_array, const std::vector<std::uint16_t>& data_array)
{
    native_multi_hash_map_extensions::NativeMultiHashMap<int, std::uint16_t> map;
    map.reserve(data_array.size());
    for (int index = 0; index < static_cast<int>(index_array.size()); ++index) {
        int data_count = 0;
        int data_start = 0;
        Unpack12_20(index_array[static_cast<std::size_t>(index)], data_count, data_start);
        for (int data_index = 0; data_index < data_count; ++data_index) {
            const int source_index = data_start + data_index;
            if (source_index >= 0 && source_index < static_cast<int>(data_array.size())) {
                map.emplace(index, data_array[static_cast<std::size_t>(source_index)]);
            }
        }
    }
    return map;
}

}  // namespace hocloth::mc2::job_utility
