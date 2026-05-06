#pragma once

#include "hocloth/utility/native_collection/bit_flag.hpp"

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace hocloth::mc2::native_array_extensions {

// Port target for Magica Cloth 2:
// Scripts/Core/Utility/NativeCollection/NativeArrayExtensions.cs
template <typename T>
[[nodiscard]] std::vector<std::uint8_t> ToRawBytes(const std::vector<T>& values)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (values.empty()) {
        return {};
    }
    std::vector<std::uint8_t> bytes(sizeof(T) * values.size());
    std::memcpy(bytes.data(), values.data(), bytes.size());
    return bytes;
}

template <typename T>
[[nodiscard]] std::vector<T> FromRawBytes(const std::vector<std::uint8_t>& bytes)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (bytes.empty() || bytes.size() < sizeof(T)) {
        return {};
    }
    const std::size_t count = bytes.size() / sizeof(T);
    std::vector<T> values(count);
    std::memcpy(values.data(), bytes.data(), count * sizeof(T));
    return values;
}

[[nodiscard]] inline std::vector<BitFlag8> FromRawBitFlag8Bytes(
    const std::vector<std::uint8_t>& bytes
)
{
    std::vector<BitFlag8> values;
    values.reserve(bytes.size());
    for (std::uint8_t value : bytes) {
        values.emplace_back(value);
    }
    return values;
}

[[nodiscard]] inline std::vector<std::uint8_t> ToRawBitFlag8Bytes(
    const std::vector<BitFlag8>& values
)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size());
    for (const BitFlag8& value : values) {
        bytes.push_back(value.Value());
    }
    return bytes;
}

}  // namespace hocloth::mc2::native_array_extensions
