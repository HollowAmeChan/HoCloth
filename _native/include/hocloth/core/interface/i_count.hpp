#pragma once

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Interface/ICount.cs
class ICount {
public:
    virtual ~ICount() = default;

    [[nodiscard]] virtual int Count() const = 0;
};

}  // namespace hocloth::mc2
