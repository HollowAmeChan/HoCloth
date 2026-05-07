#pragma once

#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/fixed_list.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hocloth::mc2::native_multi_hash_map_extensions {

struct Int2Hash {
    [[nodiscard]] std::size_t operator()(const int2& value) const
    {
        std::size_t seed = 0;
        seed ^= std::hash<int>{}(value.x) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(value.y) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct Int2Equal {
    [[nodiscard]] bool operator()(const int2& a, const int2& b) const
    {
        return a.x == b.x && a.y == b.y;
    }
};

template <typename TKey, typename TValue, typename THash = std::hash<TKey>, typename TEqual = std::equal_to<TKey>>
using NativeMultiHashMap = std::unordered_multimap<TKey, TValue, THash, TEqual>;

using Int2UShortMultiHashMap = NativeMultiHashMap<int2, std::uint16_t, Int2Hash, Int2Equal>;

namespace detail {

template <int Bytes, typename T>
inline constexpr int FixedListByteCapacity =
    (Bytes / static_cast<int>(sizeof(T))) > 0 ? (Bytes / static_cast<int>(sizeof(T))) : 1;

}  // namespace detail

// Port target for Magica Cloth 2:
// Scripts/Core/Utility/NativeCollection/NativeMultiHashMapExtensions.cs
template <typename TMap, typename TKey, typename TValue>
[[nodiscard]] bool MC2Contains(const TMap& map, const TKey& key, const TValue& value)
{
    const auto range = map.equal_range(key);
    for (auto iter = range.first; iter != range.second; ++iter) {
        if (iter->second == value) {
            return true;
        }
    }
    return false;
}

template <typename TMap, typename TKey, typename TValue>
bool MC2UniqueAdd(TMap& map, const TKey& key, const TValue& value)
{
    if (MC2Contains(map, key, value)) {
        return false;
    }
    map.emplace(key, value);
    return true;
}

template <typename TMap, typename TKey, typename TValue>
bool MC2RemoveValue(TMap& map, const TKey& key, const TValue& value)
{
    const auto range = map.equal_range(key);
    for (auto iter = range.first; iter != range.second; ++iter) {
        if (iter->second == value) {
            map.erase(iter);
            return true;
        }
    }
    return false;
}

template <int Capacity, typename TMap, typename TKey>
[[nodiscard]] auto MC2ToFixedList(const TMap& map, const TKey& key)
    -> FixedList<typename TMap::mapped_type, Capacity>
{
    FixedList<typename TMap::mapped_type, Capacity> fixed_list;
    const auto range = map.equal_range(key);
    for (auto iter = range.first; iter != range.second; ++iter) {
        if (!fixed_list.Add(iter->second)) {
            break;
        }
    }
    return fixed_list;
}

template <typename TMap, typename TKey>
[[nodiscard]] auto MC2ToFixedList512Bytes(const TMap& map, const TKey& key)
{
    return MC2ToFixedList<detail::FixedListByteCapacity<512, typename TMap::mapped_type>>(map, key);
}

template <typename TMap, typename TKey>
[[nodiscard]] auto MC2ToFixedList128Bytes(const TMap& map, const TKey& key)
{
    return MC2ToFixedList<detail::FixedListByteCapacity<128, typename TMap::mapped_type>>(map, key);
}

template <typename TMap>
[[nodiscard]] auto MC2Serialize(const TMap& map)
    -> std::pair<std::vector<typename TMap::key_type>, std::vector<typename TMap::mapped_type>>
{
    std::vector<typename TMap::key_type> keys;
    std::vector<typename TMap::mapped_type> values;
    keys.reserve(map.size());
    values.reserve(map.size());
    for (const auto& [key, value] : map) {
        keys.push_back(key);
        values.push_back(value);
    }
    return {keys, values};
}

template <
    typename TKey,
    typename TValue,
    typename THash = std::hash<TKey>,
    typename TEqual = std::equal_to<TKey>>
[[nodiscard]] NativeMultiHashMap<TKey, TValue, THash, TEqual> MC2Deserialize(
    const std::vector<TKey>& keys,
    const std::vector<TValue>& values
)
{
    NativeMultiHashMap<TKey, TValue, THash, TEqual> map;
    const std::size_t count = std::min(keys.size(), values.size());
    map.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        map.emplace(keys[index], values[index]);
    }
    return map;
}

[[nodiscard]] inline Int2UShortMultiHashMap MC2Deserialize(
    const std::vector<int2>& keys,
    const std::vector<std::uint16_t>& values
)
{
    Int2UShortMultiHashMap map;
    const std::size_t count = std::min(keys.size(), values.size());
    map.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        map.emplace(keys[index], values[index]);
    }
    return map;
}

}  // namespace hocloth::mc2::native_multi_hash_map_extensions
