#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/i_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Simulation/TimeManager.cs
class TimeManager final : public IManager {
public:
    enum class UpdateLocation {
        AfterLateUpdate = 0,
        BeforeLateUpdate = 1,
    };

    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    void FrameUpdate(int requested_frequency);
    void FrameUpdate();
    void SetSimulationFrequency(int frequency);
    void SetMaxSimulationCountPerFrame(int max_count);
    void SetGlobalTimeScale(float time_scale);
    void SetUpdateLocation(UpdateLocation update_location);

    [[nodiscard]] int SimulationFrequency() const;
    [[nodiscard]] int MaxSimulationCountPerFrame() const;
    [[nodiscard]] float GlobalTimeScale() const;
    [[nodiscard]] UpdateLocation CurrentUpdateLocation() const;
    [[nodiscard]] float SimulationDeltaTime() const;
    [[nodiscard]] float MaxDeltaTime() const;
    [[nodiscard]] float4 SimulationPower() const;

private:
    bool initialized_ = false;
    int simulation_frequency_ = define::system::DefaultSimulationFrequency;
    int max_simulation_count_per_frame_ = define::system::DefaultMaxSimulationCountPerFrame;
    float global_time_scale_ = 1.0f;
    UpdateLocation update_location_ = UpdateLocation::AfterLateUpdate;
    float simulation_delta_time_ = 1.0f / static_cast<float>(define::system::DefaultSimulationFrequency);
    float max_delta_time_ =
        (1.0f / static_cast<float>(define::system::DefaultSimulationFrequency))
        * static_cast<float>(define::system::DefaultMaxSimulationCountPerFrame);
    float4 simulation_power_{1.0f, 1.0f, 1.0f, 1.0f};
};

}  // namespace hocloth::mc2
