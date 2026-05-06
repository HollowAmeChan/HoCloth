#include "hocloth/manager/simulation/time_manager.hpp"

#include <algorithm>
#include <cmath>

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

    const float frequency_ratio =
        static_cast<float>(define::system::DefaultSimulationFrequency)
        / static_cast<float>(simulation_frequency_);
    simulation_power_ = float4{
        frequency_ratio,
        frequency_ratio > 1.0f ? std::pow(frequency_ratio, 0.5f) : frequency_ratio,
        frequency_ratio > 1.0f ? std::pow(frequency_ratio, 0.3f) : frequency_ratio,
        std::pow(frequency_ratio, 1.8f),
    };
}

int TimeManager::SimulationFrequency() const
{
    return simulation_frequency_;
}

int TimeManager::MaxSimulationCountPerFrame() const
{
    return max_simulation_count_per_frame_;
}

float TimeManager::SimulationDeltaTime() const
{
    return simulation_delta_time_;
}

float TimeManager::MaxDeltaTime() const
{
    return max_delta_time_;
}

float4 TimeManager::SimulationPower() const
{
    return simulation_power_;
}

}  // namespace hocloth::mc2
