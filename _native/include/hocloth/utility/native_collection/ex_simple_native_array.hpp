#pragma once

#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/native_array_extensions.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/NativeCollection/ExSimpleNativeArray.cs
template <typename T>
class ExSimpleNativeArray {
public:
    ExSimpleNativeArray() = default;

    explicit ExSimpleNativeArray(int data_length, bool area_only = false)
    {
        ResizeStorage(data_length);
        if (!area_only) {
            count_ = data_length;
        }
    }

    explicit ExSimpleNativeArray(const std::vector<T>& data_array)
        : storage_(data_array)
        , count_(static_cast<int>(data_array.size()))
    {
    }

    [[nodiscard]] bool IsValid() const
    {
        return !storage_.empty();
    }

    [[nodiscard]] int Count() const
    {
        return count_;
    }

    [[nodiscard]] int Length() const
    {
        return static_cast<int>(storage_.size());
    }

    void Dispose()
    {
        storage_.clear();
        count_ = 0;
    }

    void Clear()
    {
        count_ = 0;
    }

    void SetCount(int new_count)
    {
        assert(new_count >= 0 && new_count <= Length());
        count_ = new_count;
    }

    void AddCapacity(int capacity)
    {
        Expand(capacity, true);
    }

    DataChunk AddRange(int data_length)
    {
        const int start = count_;
        Expand(data_length);
        count_ += data_length;
        return DataChunk{start, data_length};
    }

    DataChunk AddRange(int data_length, const T& fill_data)
    {
        const DataChunk chunk = AddRange(data_length);
        Fill(chunk.start_index, chunk.data_length, fill_data);
        return chunk;
    }

    DataChunk AddRange(const std::vector<T>& values)
    {
        const DataChunk chunk = AddRange(static_cast<int>(values.size()));
        std::copy(values.begin(), values.end(), storage_.begin() + chunk.start_index);
        return chunk;
    }

    DataChunk AddRange(const std::vector<T>& values, int count)
    {
        assert(count >= 0);
        assert(count <= static_cast<int>(values.size()));
        const DataChunk chunk = AddRange(count);
        std::copy(values.begin(), values.begin() + count, storage_.begin() + chunk.start_index);
        return chunk;
    }

    DataChunk AddRange(const ExSimpleNativeArray<T>& values)
    {
        const DataChunk chunk = AddRange(values.Count());
        std::copy(
            values.storage_.begin(),
            values.storage_.begin() + values.Count(),
            storage_.begin() + chunk.start_index
        );
        return chunk;
    }

    int Add(const T& value)
    {
        if (Length() == 0) {
            Expand(16);
        } else if (count_ == Length()) {
            Expand(Length());
        }
        storage_[static_cast<std::size_t>(count_)] = value;
        return count_++;
    }

    [[nodiscard]] std::vector<T> ToArray() const
    {
        return storage_;
    }

    void CopyTo(std::vector<T>& values) const
    {
        values = storage_;
    }

    void CopyFrom(const std::vector<T>& values)
    {
        assert(static_cast<int>(values.size()) <= Length());
        std::copy(values.begin(), values.end(), storage_.begin());
        count_ = static_cast<int>(values.size());
    }

    template <typename U>
    void CopyFromTypeChange(const std::vector<U>& values)
    {
        const std::vector<std::uint8_t> bytes = native_array_extensions::ToRawBytes(values);
        const std::vector<T> converted = native_array_extensions::FromRawBytes<T>(bytes);
        assert(static_cast<int>(converted.size()) <= Length());
        std::copy(converted.begin(), converted.end(), storage_.begin());
        count_ = static_cast<int>(converted.size());
    }

    void Fill(int start_index, int data_length, const T& fill_data = T{})
    {
        assert(start_index >= 0);
        assert(data_length >= 0);
        assert(start_index + data_length <= Length());
        std::fill(
            storage_.begin() + start_index,
            storage_.begin() + start_index + data_length,
            fill_data
        );
    }

    void RemoveRange(DataChunk chunk)
    {
        if (!chunk.IsValid() || chunk.start_index >= count_) {
            return;
        }
        const int end = std::min(chunk.EndIndex(), count_);
        const int remove_count = end - chunk.start_index;
        if (remove_count <= 0) {
            return;
        }
        std::move(
            storage_.begin() + end,
            storage_.begin() + count_,
            storage_.begin() + chunk.start_index
        );
        count_ -= remove_count;
    }

    [[nodiscard]] std::string ToSummary() const
    {
        std::ostringstream stream;
        stream << "ExSimpleNativeArray Length:" << Length()
               << " Count:" << Count()
               << " IsValid:" << (IsValid() ? "true" : "false");
        return stream.str();
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << ToSummary() << '\n';
        stream << "---- Datas[~100] ----\n";
        for (int index = 0; index < Length() && index < 100; ++index) {
            stream << storage_[static_cast<std::size_t>(index)] << '\n';
        }
        return stream.str();
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

    [[nodiscard]] const std::vector<T>& GetNativeArray() const
    {
        return storage_;
    }

    [[nodiscard]] std::vector<T>& GetNativeArray()
    {
        return storage_;
    }

    struct SerializationData {
        int count = 0;
        int length = 0;
        std::vector<std::uint8_t> array_bytes;
    };

    [[nodiscard]] SerializationData Serialize() const
    {
        SerializationData data;
        data.count = count_;
        data.length = Length();
        data.array_bytes = native_array_extensions::ToRawBytes(storage_);
        return data;
    }

    bool Deserialize(const SerializationData& data)
    {
        storage_ = native_array_extensions::FromRawBytes<T>(data.array_bytes);
        if (data.length > static_cast<int>(storage_.size())) {
            storage_.resize(static_cast<std::size_t>(data.length));
        }
        count_ = std::min(data.count, Length());
        return true;
    }

private:
    void ResizeStorage(int length)
    {
        assert(length >= 0);
        storage_.resize(static_cast<std::size_t>(length));
    }

    void Expand(int data_length, bool force = false)
    {
        assert(data_length >= 0);
        const int new_length = force ? Length() + data_length : count_ + data_length;
        if (Length() == 0) {
            ResizeStorage(data_length);
        } else if (new_length > Length()) {
            ResizeStorage(new_length);
        }
    }

    std::vector<T> storage_;
    int count_ = 0;
};

}  // namespace hocloth::mc2
