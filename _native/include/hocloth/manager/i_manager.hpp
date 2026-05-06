#pragma once

#include "hocloth/manager/manager_status.hpp"
#include "hocloth/utility/result_code/result_code.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/IManager.cs
class IManager {
public:
    virtual ~IManager() = default;

    virtual Result Initialize() = 0;
    virtual void Dispose() = 0;
    [[nodiscard]] virtual ManagerStatus Status() const = 0;
};

}  // namespace hocloth::mc2
