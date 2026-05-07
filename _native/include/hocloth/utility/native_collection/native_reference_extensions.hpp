#pragma once

#include <atomic>

namespace hocloth::mc2::native_reference_extensions {

// Port target for Magica Cloth 2:
// Scripts/Core/Utility/NativeCollection/NativeReferenceExtensions.cs
[[nodiscard]] inline int MC2InterlockedStartIndex(std::atomic<int>& counter, int data_count)
{
    return counter.fetch_add(data_count);
}

[[nodiscard]] inline int MC2InterlockedStartIndex(int& counter, int data_count)
{
    const int start = counter;
    counter += data_count;
    return start;
}

}  // namespace hocloth::mc2::native_reference_extensions
