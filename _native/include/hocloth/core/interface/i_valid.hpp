#pragma once

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Interface/IValid.cs
class IValid {
public:
    virtual ~IValid() = default;

    [[nodiscard]] virtual bool IsValid() const = 0;
};

}  // namespace hocloth::mc2
