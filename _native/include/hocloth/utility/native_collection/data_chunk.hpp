#pragma once

#include <string>

namespace hocloth::mc2 {

// Ported from Magica Cloth 2: Scripts/Core/Utility/NativeCollection/DataChunk.cs
struct DataChunk {
    int start_index = 0;
    int data_length = 0;

    [[nodiscard]] bool IsValid() const;
    void Clear();
    [[nodiscard]] int EndIndex() const;
    [[nodiscard]] bool Contains(int index) const;
    [[nodiscard]] std::string ToString() const;

    static DataChunk Empty();
};

}  // namespace hocloth::mc2
