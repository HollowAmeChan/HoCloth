#pragma once

#include <string>

namespace hocloth::mc2 {

// Ported from Magica Cloth 2: Scripts/Core/Utility/NativeCollection/DataChunk.cs
struct DataChunk {
    int start_index = 0;
    int data_length = 0;

    DataChunk() = default;
    constexpr DataChunk(int start_index_, int data_length_)
        : start_index(start_index_)
        , data_length(data_length_)
    {
    }

    explicit constexpr DataChunk(int start_index_)
        : start_index(start_index_)
        , data_length(1)
    {
    }

    [[nodiscard]] constexpr bool IsValid() const
    {
        return data_length > 0;
    }

    void Clear()
    {
        start_index = 0;
        data_length = 0;
    }

    [[nodiscard]] constexpr int EndIndex() const
    {
        return start_index + data_length;
    }

    [[nodiscard]] constexpr bool Contains(int index) const
    {
        return index >= start_index && index < EndIndex();
    }

    [[nodiscard]] std::string ToString() const;

    [[nodiscard]] static constexpr DataChunk Empty()
    {
        return DataChunk{};
    }
};

}  // namespace hocloth::mc2
