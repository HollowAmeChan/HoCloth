#include "hocloth/manager/simulation/time_manager.hpp"

#include <algorithm>

namespace hocloth::mc2 {

Result TimeManager::Initialize()
{
    initialized_ = true;
    simulation_frequency_ = define::system::DefaultSimulationFrequency;
    max_simulation_count_per_frame_ = define::system::DefaultMaxSimulationCountPerFrame;
    FrameUpdate(simulation_frequency_);
    return Result::Ok();
}

void TimeManager::Dispose()
{
    initialized_ = false;
}

ManagerStatus TimeManager::Status() const
{
    return ManagerStatus{"TimeManager", initialized_, 1};
}

void TimeManager::FrameUpdate(int requested_frequency)
{
    simulation_frequency_ = std::clamp(
        requested_frequency,
        define::system::SimulationFrequencyLow,
        define::system::SimulationFrequencyHigh
    );
    max_simulation_count_per_frame_ = std::clamp(
        max_simulation_count_per_frame_,
        define::system::MaxSimulationCountPerFrameLow,
        define::system::MaxSimulationCountPerFrameHigh
    );
    simulation_delta_time_ = 1.0f / static_cast<float>(simulation_frequency_);
    max_delta_time_ = simulation_delta_time_ * static_cast<float>(max_simulation_count_per_frame_);
}

int TimeManager::SimulationFrequency() const
{
    return simulation_frequency_;
}

float TimeManager::SimulationDeltaTime() const
{
    return simulation_delta_time_;
}

float TimeManager::MaxDeltaTime() const
{
    return max_delta_time_;
}

}  // namespace hocloth::mc2
