#pragma once

#include "hocloth/manager/i_manager.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Simulation/WindManager.cs
class WindManager final : public IManager {
public:
    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

private:
    bool initialized_ = false;
};

}  // namespace hocloth::mc2
