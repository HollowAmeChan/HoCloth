#pragma once

#include <cstdint>

namespace hocloth::mc2 {

class BitFlag8 {
public:
    BitFlag8() = default;
    explicit BitFlag8(std::uint8_t value)
        : value_(value)
    {
    }

    explicit BitFlag8(int value)
        : value_(static_cast<std::uint8_t>(value))
    {
    }

    [[nodiscard]] bool IsSet(std::uint8_t flag) const
    {
        return (value_ & flag) != 0;
    }

    void SetFlag(std::uint8_t flag, bool enabled)
    {
        if (enabled) {
            value_ = static_cast<std::uint8_t>(value_ | flag);
        } else {
            value_ = static_cast<std::uint8_t>(value_ & ~flag);
        }
    }

    void Clear()
    {
        value_ = 0;
    }

    [[nodiscard]] std::uint8_t Value() const
    {
        return value_;
    }

private:
    std::uint8_t value_ = 0;
};

// Port target for Magica Cloth 2: Unity.Collections.BitField64 usage in TeamManager.cs
class BitFlag64 {
public:
    [[nodiscard]] bool IsSet(int bit) const
    {
        return (value_ & (std::uint64_t{1} << bit)) != 0;
    }

    [[nodiscard]] bool TestAny(int start_bit, int count) const
    {
        if (count <= 0) {
            return false;
        }
        const std::uint64_t mask = count >= 64
            ? ~std::uint64_t{0}
            : ((std::uint64_t{1} << count) - 1) << start_bit;
        return (value_ & mask) != 0;
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
