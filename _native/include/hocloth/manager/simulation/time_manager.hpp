#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/i_manager.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Simulation/TimeManager.cs
class TimeManager final : public IManager {
public:
    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    void FrameUpdate(int requested_frequency);

    [[nodiscard]] int SimulationFrequency() const;
    [[nodiscard]] float SimulationDeltaTime() const;
    [[nodiscard]] float MaxDeltaTime() const;

private:
    bool initialized_ = false;
    int simulation_frequency_ = define::system::DefaultSimulationFrequency;
    int max_simulation_count_per_frame_ = define::system::DefaultMaxSimulationCountPerFrame;
    float simulation_delta_time_ = 1.0f / static_cast<float>(define::system::DefaultSimulationFrequency);
    float max_delta_time_ =
        (1.0f / static_cast<float>(define::system::DefaultSimulationFrequency))
        * static_cast<float>(define::system::DefaultMaxSimulationCountPerFrame);
};

}  // namespace hocloth::mc2
