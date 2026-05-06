#pragma once

#include "hocloth/utility/data/data_utility.hpp"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hocloth::mc2::data {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Data/MultiDataBuilder.cs
template <typename T>
class MultiDataBuilder {
public:
    MultiDataBuilder(int index_count, int data_capacity = 0)
        : index_count_(index_count)
    {
        map_.reserve(static_cast<std::size_t>(data_capacity));
    }

    [[nodiscard]] int Count() const
    {
        int count = 0;
        for (const auto& [key, values] : map_) {
            (void)key;
            count += static_cast<int>(values.size());
        }
        return count;
    }

    [[nodiscard]] int GetDataCount(int index) const
    {
        const auto found = map_.find(index);
        return found == map_.end() ? 0 : static_cast<int>(found->second.size());
    }

    void Add(int key, const T& data)
    {
        map_[key].push_back(data);
    }

    [[nodiscard]] int CountValuesForKey(int key) const
    {
        return GetDataCount(key);
    }

    [[nodiscard]] std::pair<std::vector<T>, std::vector<std::uint32_t>> ToArray() const
    {
        if (index_count_ <= 0) {
            return {};
        }

        std::vector<std::uint32_t> index_array(static_cast<std::size_t>(index_count_));
        std::vector<T> data_array;
        data_array.reserve(static_cast<std::size_t>(Count()));

        for (int index = 0; index < index_count_; ++index) {
            const int start = static_cast<int>(data_array.size());
            int count = 0;
            const auto found = map_.find(index);
            if (found != map_.end()) {
                data_array.insert(data_array.end(), found->second.begin(), found->second.end());
                count = static_cast<int>(found->second.size());
            }
            index_array[static_cast<std::size_t>(index)] = Pack12_20(count, start);
        }
        return {data_array, index_array};
    }

    [[nodiscard]] std::vector<std::uint32_t> ToIndexArray() const
    {
        return ToArray().second;
    }

private:
    int index_count_ = 0;
    std::unordered_map<int, std::vector<T>> map_;
};

}  // namespace hocloth::mc2::data
