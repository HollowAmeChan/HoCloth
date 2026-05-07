#pragma once

#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace hocloth::mc2::interlock_utility {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Jobs/InterlockUtility.cs
inline constexpr float ToFixedScale = 1000000.0f;
inline constexpr float ToFloatScale = 0.000001f;

[[nodiscard]] inline int ToFixed(float value)
{
    return static_cast<int>(value * ToFixedScale);
}

[[nodiscard]] inline float ToFloat(int value)
{
    return static_cast<float>(value) * ToFloatScale;
}

[[nodiscard]] inline int3 ToFixed(const float3& value)
{
    return int3{ToFixed(value.x), ToFixed(value.y), ToFixed(value.z)};
}

[[nodiscard]] inline float3 ToFloat3(int x, int y, int z)
{
    return float3{ToFloat(x), ToFloat(y), ToFloat(z)};
}

template <typename TCountArray, typename TSumArray>
void AddFloat3(int index, const float3& add, TCountArray& count_array, TSumArray& sum_array)
{
    if (index < 0 || index >= static_cast<int>(count_array.size())) {
        return;
    }
    ++count_array[static_cast<std::size_t>(index)];

    const int data_index = index * 3;
    if (data_index < 0 || data_index + 2 >= static_cast<int>(sum_array.size())) {
        return;
    }
    const int3 iadd = ToFixed(add);
    sum_array[static_cast<std::size_t>(data_index)] += iadd.x;
    sum_array[static_cast<std::size_t>(data_index + 1)] += iadd.y;
    sum_array[static_cast<std::size_t>(data_index + 2)] += iadd.z;
}

inline void AddFloat3(
    int index,
    const float3& add,
    ExNativeArray<int>& count_array,
    ExNativeArray<int>& sum_array
)
{
    if (index < 0 || index >= count_array.Length()) {
        return;
    }
    ++count_array[index];

    const int data_index = index * 3;
    if (data_index < 0 || data_index + 2 >= sum_array.Length()) {
        return;
    }
    const int3 iadd = ToFixed(add);
    sum_array[data_index] += iadd.x;
    sum_array[data_index + 1] += iadd.y;
    sum_array[data_index + 2] += iadd.z;
}

template <typename TSumArray>
void AddFloat3(int index, const float3& add, TSumArray& sum_array)
{
    const int data_index = index * 3;
    if (index < 0 || data_index + 2 >= static_cast<int>(sum_array.size())) {
        return;
    }
    const int3 iadd = ToFixed(add);
    sum_array[static_cast<std::size_t>(data_index)] += iadd.x;
    sum_array[static_cast<std::size_t>(data_index + 1)] += iadd.y;
    sum_array[static_cast<std::size_t>(data_index + 2)] += iadd.z;
}

inline void AddFloat3(int index, const float3& add, ExNativeArray<int>& sum_array)
{
    const int data_index = index * 3;
    if (index < 0 || data_index + 2 >= sum_array.Length()) {
        return;
    }
    const int3 iadd = ToFixed(add);
    sum_array[data_index] += iadd.x;
    sum_array[data_index + 1] += iadd.y;
    sum_array[data_index + 2] += iadd.z;
}

template <typename TCountArray>
void Increment(int index, TCountArray& count_array)
{
    if (index >= 0 && index < static_cast<int>(count_array.size())) {
        ++count_array[static_cast<std::size_t>(index)];
    }
}

inline void Increment(int index, ExNativeArray<int>& count_array)
{
    if (index >= 0 && index < count_array.Length()) {
        ++count_array[index];
    }
}

template <typename TArray>
void Max(int index, float value, TArray& buffer)
{
    if (index >= 0 && index < static_cast<int>(buffer.size())) {
        buffer[static_cast<std::size_t>(index)] =
            std::max(buffer[static_cast<std::size_t>(index)], ToFixed(value));
    }
}

inline void Max(int index, float value, ExNativeArray<int>& buffer)
{
    if (index >= 0 && index < buffer.Length()) {
        buffer[index] = std::max(buffer[index], ToFixed(value));
    }
}

template <typename TCountArray, typename TSumArray>
[[nodiscard]] float3 ReadAverageFloat3(int index, const TCountArray& count_array, const TSumArray& sum_array)
{
    if (index < 0 || index >= static_cast<int>(count_array.size())) {
        return float3{};
    }
    const int count = count_array[static_cast<std::size_t>(index)];
    const int data_index = index * 3;
    if (count <= 0 || data_index + 2 >= static_cast<int>(sum_array.size())) {
        return float3{};
    }
    return float3{
        ToFloat(sum_array[static_cast<std::size_t>(data_index)]) / static_cast<float>(count),
        ToFloat(sum_array[static_cast<std::size_t>(data_index + 1)]) / static_cast<float>(count),
        ToFloat(sum_array[static_cast<std::size_t>(data_index + 2)]) / static_cast<float>(count),
    };
}

inline float3 ReadAverageFloat3(
    int index,
    const ExNativeArray<int>& count_array,
    const ExNativeArray<int>& sum_array
)
{
    if (index < 0 || index >= count_array.Length()) {
        return float3{};
    }
    const int count = count_array[index];
    const int data_index = index * 3;
    if (count <= 0 || data_index + 2 >= sum_array.Length()) {
        return float3{};
    }
    return float3{
        ToFloat(sum_array[data_index]) / static_cast<float>(count),
        ToFloat(sum_array[data_index + 1]) / static_cast<float>(count),
        ToFloat(sum_array[data_index + 2]) / static_cast<float>(count),
    };
}

template <typename TArray>
[[nodiscard]] float3 ReadFloat3(int index, const TArray& buffer)
{
    const int data_index = index * 3;
    if (index < 0 || data_index + 2 >= static_cast<int>(buffer.size())) {
        return float3{};
    }
    return ToFloat3(
        buffer[static_cast<std::size_t>(data_index)],
        buffer[static_cast<std::size_t>(data_index + 1)],
        buffer[static_cast<std::size_t>(data_index + 2)]
    );
}

inline float3 ReadFloat3(int index, const ExNativeArray<int>& buffer)
{
    const int data_index = index * 3;
    if (index < 0 || data_index + 2 >= buffer.Length()) {
        return float3{};
    }
    return ToFloat3(buffer[data_index], buffer[data_index + 1], buffer[data_index + 2]);
}

template <typename TArray>
[[nodiscard]] float ReadFloat(int index, const TArray& buffer)
{
    if (index < 0 || index >= static_cast<int>(buffer.size())) {
        return 0.0f;
    }
    return ToFloat(buffer[static_cast<std::size_t>(index)]);
}

inline float ReadFloat(int index, const ExNativeArray<int>& buffer)
{
    if (index < 0 || index >= buffer.Length()) {
        return 0.0f;
    }
    return ToFloat(buffer[index]);
}

template <typename TIndexArray>
void SolveAggregateBufferAndClear(
    const TIndexArray& particle_indices,
    std::vector<float3>& next_positions,
    std::vector<float3>* velocity_positions,
    float velocity_attenuation,
    std::vector<int>& count_array,
    std::vector<int>& sum_array
)
{
    for (int particle_index : particle_indices) {
        if (particle_index < 0 || particle_index >= static_cast<int>(next_positions.size())) {
            continue;
        }
        const float3 add = ReadAverageFloat3(particle_index, count_array, sum_array);
        next_positions[static_cast<std::size_t>(particle_index)].x += add.x;
        next_positions[static_cast<std::size_t>(particle_index)].y += add.y;
        next_positions[static_cast<std::size_t>(particle_index)].z += add.z;
        if (velocity_positions != nullptr && velocity_attenuation > 1.0e-6f
            && particle_index < static_cast<int>(velocity_positions->size())) {
            (*velocity_positions)[static_cast<std::size_t>(particle_index)].x +=
                add.x * velocity_attenuation;
            (*velocity_positions)[static_cast<std::size_t>(particle_index)].y +=
                add.y * velocity_attenuation;
            (*velocity_positions)[static_cast<std::size_t>(particle_index)].z +=
                add.z * velocity_attenuation;
        }

        const int data_index = particle_index * 3;
        count_array[static_cast<std::size_t>(particle_index)] = 0;
        if (data_index + 2 < static_cast<int>(sum_array.size())) {
            sum_array[static_cast<std::size_t>(data_index)] = 0;
            sum_array[static_cast<std::size_t>(data_index + 1)] = 0;
            sum_array[static_cast<std::size_t>(data_index + 2)] = 0;
        }
    }
}

template <typename TArray>
void ClearCountArray(TArray& count_array)
{
    std::fill(count_array.begin(), count_array.end(), 0);
}

inline void ClearCountArray(ExNativeArray<int>& count_array)
{
    for (int index = 0; index < count_array.Length(); ++index) {
        count_array[index] = 0;
    }
}

}  // namespace hocloth::mc2::interlock_utility
