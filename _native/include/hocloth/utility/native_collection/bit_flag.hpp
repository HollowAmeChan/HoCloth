#pragma once

#include <cstdint>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Unity.Collections.BitField64 usage in TeamManager.cs
class BitFlag64 {
public:
    [[nodiscard]] bool IsSet(int bit) const
    {
        return (value_ & (std::uint64_t{1} << bit)) != 0;
    }

    void Set(int bit, bool enabled)
    {
        const std::uint64_t mask = std::uint64_t{1} << bit;
        if (enabled) {
            value_ |= mask;
        } else {
            value_ &= ~mask;
        }
    }

    void Clear()
    {
        value_ = 0;
    }

    [[nodiscard]] std::uint64_t Value() const
    {
        return value_;
    }

private:
    std::uint64_t value_ = 0;
};

}  // namespace hocloth::mc2
