#pragma once

#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/ex_simple_native_array.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/NativeCollection/ExNativeArray.cs
template <typename T>
class ExNativeArray {
public:
    ExNativeArray() = default;

    explicit ExNativeArray(int empty_length, bool create = false)
    {
        if (empty_length > 0) {
            storage_.resize(static_cast<std::size_t>(empty_length));
            empty_chunks_.push_back(DataChunk{0, empty_length});
            if (create) {
                AddRange(empty_length);
            }
        } else if (create) {
            storage_.resize(0);
        }
    }

    [[nodiscard]] bool IsValid() const
    {
        return !storage_.empty() || use_count_ > 0;
    }

    [[nodiscard]] int Length() const
    {
        return static_cast<int>(storage_.size());
    }

    [[nodiscard]] int Count() const
    {
        return use_count_;
    }

    void Dispose()
    {
        storage_.clear();
        empty_chunks_.clear();
        use_count_ = 0;
    }

    DataChunk AddRange(int data_length)
    {
        if (data_length == 0) {
            return DataChunk::Empty();
        }

        DataChunk chunk = GetEmptyChunk(data_length);
        if (!chunk.IsValid()) {
            const int now_length = Length();
            const int next_length = now_length + std::max(data_length, now_length);
            if (now_length == 0) {
                storage_.resize(static_cast<std::size_t>(next_length));
                chunk = DataChunk{0, data_length};
                if (data_length < next_length) {
                    AddEmptyChunk(DataChunk{data_length, next_length - data_length});
                }
            } else {
                storage_.resize(static_cast<std::size_t>(next_length));
                chunk = DataChunk{now_length, data_length};
                const int last = now_length + data_length;
                if (last < next_length) {
                    AddEmptyChunk(DataChunk{last, next_length - last});
                }
            }
        }

        use_count_ = std::max(use_count_, chunk.EndIndex());
        return chunk;
    }

    DataChunk AddRange(int data_length, const T& fill_data)
    {
        const DataChunk chunk = AddRange(data_length);
        Fill(chunk, fill_data);
        return chunk;
    }

    DataChunk AddRange(const std::vector<T>& values)
    {
        if (values.empty()) {
            return DataChunk::Empty();
        }
        const DataChunk chunk = AddRange(static_cast<int>(values.size()));
        std::copy(values.begin(), values.end(), storage_.begin() + chunk.start_index);
        return chunk;
    }

    DataChunk AddRange(const ExSimpleNativeArray<T>& values)
    {
        const int count = values.Count();
        if (count <= 0) {
            return DataChunk::Empty();
        }
        const DataChunk chunk = AddRange(count);
        std::copy(
            values.Data().begin(),
            values.Data().begin() + count,
            storage_.begin() + chunk.start_index
        );
        return chunk;
    }

    DataChunk Add(const T& value)
    {
        const DataChunk chunk = AddRange(1);
        storage_[static_cast<std::size_t>(chunk.start_index)] = value;
        return chunk;
    }

    DataChunk Expand(DataChunk chunk, int new_data_length)
    {
        if (!chunk.IsValid() || new_data_length <= chunk.data_length) {
            return chunk;
        }

        const DataChunk new_chunk = AddRange(new_data_length);
        std::copy(
            storage_.begin() + chunk.start_index,
            storage_.begin() + chunk.start_index + chunk.data_length,
            storage_.begin() + new_chunk.start_index
        );
        Remove(chunk);
        return new_chunk;
    }

    void AddEmpty(int data_length)
    {
        Remove(AddRange(data_length));
    }

    void Remove(DataChunk chunk)
    {
        if (!chunk.IsValid()) {
            return;
        }
        AddEmptyChunk(chunk);
        if (chunk.EndIndex() == use_count_) {
            RecalculateUseCount();
        }
    }

    void RemoveAndFill(DataChunk chunk, const T& clear_data = T{})
    {
        Fill(chunk, clear_data);
        Remove(chunk);
    }

    void Fill(const T& fill_data = T{})
    {
        std::fill(storage_.begin(), storage_.end(), fill_data);
    }

    void Fill(DataChunk chunk, const T& fill_data = T{})
    {
        if (!chunk.IsValid()) {
            return;
        }
        assert(chunk.start_index >= 0);
        assert(chunk.EndIndex() <= Length());
        std::fill(
            storage_.begin() + chunk.start_index,
            storage_.begin() + chunk.EndIndex(),
            fill_data
        );
    }

    void Clear()
    {
        empty_chunks_.clear();
        use_count_ = 0;
        if (Length() > 0) {
            empty_chunks_.push_back(DataChunk{0, Length()});
        }
    }

    [[nodiscard]] const T& operator[](int index) const
    {
        assert(index >= 0 && index < Length());
        return storage_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] T& operator[](int index)
    {
        assert(index >= 0 && index < Length());
        return storage_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] const std::vector<T>& Data() const
    {
        return storage_;
    }

    [[nodiscard]] std::vector<T>& Data()
    {
        return storage_;
    }

    [[nodiscard]] std::string ToSummary() const
    {
        std::ostringstream stream;
        stream << "ExNativeArray Length:" << Length()
               << " Count:" << Count()
               << " IsValid:" << (IsValid() ? "true" : "false");
        return stream.str();
    }

private:
    DataChunk GetEmptyChunk(int data_length)
    {
        if (data_length <= 0) {
            return DataChunk::Empty();
        }

        for (std::size_t index = 0; index < empty_chunks_.size(); ++index) {
            DataChunk chunk = empty_chunks_[index];
            if (data_length == chunk.data_length) {
                empty_chunks_.erase(empty_chunks_.begin() + static_cast<std::ptrdiff_t>(index));
                return chunk;
            }
            if (data_length < chunk.data_length) {
                DataChunk used{chunk.start_index, data_length};
                chunk.start_index += data_length;
                chunk.data_length -= data_length;
                empty_chunks_[index] = chunk;
                return used;
            }
        }
        return DataChunk::Empty();
    }

    void AddEmptyChunk(DataChunk chunk)
    {
        if (!chunk.IsValid()) {
            return;
        }

        for (std::size_t index = 0; index < empty_chunks_.size(); ++index) {
            DataChunk& existing = empty_chunks_[index];
            if (existing.EndIndex() == chunk.start_index) {
                existing.data_length += chunk.data_length;
                chunk = existing;
                empty_chunks_.erase(empty_chunks_.begin() + static_cast<std::ptrdiff_t>(index));
                break;
            }
        }

        for (std::size_t index = 0; index < empty_chunks_.size(); ++index) {
            DataChunk& existing = empty_chunks_[index];
            if (existing.start_index == chunk.EndIndex()) {
                chunk.data_length += existing.data_length;
                empty_chunks_.erase(empty_chunks_.begin() + static_cast<std::ptrdiff_t>(index));
                break;
            }
        }

        empty_chunks_.push_back(chunk);
    }

    void RecalculateUseCount()
    {
        int max_used = Length();
        bool changed = true;
        while (changed) {
            changed = false;
            for (const DataChunk& empty : empty_chunks_) {
                if (empty.EndIndex() == max_used) {
                    max_used = empty.start_index;
                    changed = true;
                    break;
                }
            }
        }
        use_count_ = max_used;
    }

    std::vector<T> storage_;
    std::vector<DataChunk> empty_chunks_;
    int use_count_ = 0;
};

}  // namespace hocloth::mc2
