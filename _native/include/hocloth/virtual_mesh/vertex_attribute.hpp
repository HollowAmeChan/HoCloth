#pragma once

#include <cstdint>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VertexAttribute.cs
class VertexAttribute {
public:
    static constexpr std::uint8_t FlagFixed = 0x01;
    static constexpr std::uint8_t FlagMove = 0x02;
    static constexpr std::uint8_t FlagInvalidMotion = 0x08;
    static constexpr std::uint8_t FlagDisableCollision = 0x10;
    static constexpr std::uint8_t FlagTriangle = 0x80;

    static constexpr VertexAttribute Invalid()
    {
        return VertexAttribute{};
    }

    static constexpr VertexAttribute Fixed()
    {
        return VertexAttribute{FlagFixed};
    }

    static constexpr VertexAttribute Move()
    {
        return VertexAttribute{FlagMove};
    }

    static constexpr VertexAttribute DisableCollision()
    {
        return VertexAttribute{FlagDisableCollision};
    }

    VertexAttribute() = default;
    constexpr explicit VertexAttribute(std::uint8_t value)
        : value_(value)
    {
    }

    [[nodiscard]] constexpr bool IsSet(std::uint8_t flag) const
    {
        return (value_ & flag) != 0;
    }

    void Clear()
    {
        value_ = 0;
    }

    void Set(std::uint8_t flag, bool enabled)
    {
        if (enabled) {
            value_ = static_cast<std::uint8_t>(value_ | flag);
        } else {
            value_ = static_cast<std::uint8_t>(value_ & ~flag);
        }
    }

    void Set(VertexAttribute attribute, bool enabled)
    {
        Set(attribute.Value(), enabled);
    }

    [[nodiscard]] constexpr bool IsInvalid() const
    {
        return !IsSet(static_cast<std::uint8_t>(FlagFixed | FlagMove));
    }

    [[nodiscard]] constexpr bool IsFixed() const
    {
        return IsSet(FlagFixed);
    }

    [[nodiscard]] constexpr bool IsMove() const
    {
        return IsSet(FlagMove);
    }

    [[nodiscard]] constexpr bool IsDontMove() const
    {
        return !IsSet(FlagMove);
    }

    [[nodiscard]] constexpr bool IsMotion() const
    {
        return !IsSet(FlagInvalidMotion);
    }

    [[nodiscard]] constexpr bool IsDisableCollision() const
    {
        return IsSet(FlagDisableCollision);
    }

    [[nodiscard]] constexpr std::uint8_t Value() const
    {
        return value_;
    }

    [[nodiscard]] static constexpr VertexAttribute JoinAttribute(VertexAttribute a, VertexAttribute b)
    {
        return a.Value() < b.Value() ? a : b;
    }

    friend constexpr bool operator==(VertexAttribute a, VertexAttribute b)
    {
        return a.Value() == b.Value();
    }

    friend constexpr bool operator!=(VertexAttribute a, VertexAttribute b)
    {
        return !(a == b);
    }

private:
    std::uint8_t value_ = 0;
};

}  // namespace hocloth::mc2
