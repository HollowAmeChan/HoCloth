#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/time_manager.hpp"

#include <algorithm>

namespace hocloth::mc2 {

enum class InitializationLocation {
    Start = 0,
    Awake = 1,
};

// Port target for Magica Cloth 2: Scripts/Core/Manager/MagicaSettings.cs
struct MagicaSettings {
    enum class RefreshMode {
        OnAwake = 0,
        EveryFrame = 1,
    };

    RefreshMode refresh_mode = RefreshMode::OnAwake;
    int simulation_frequency = define::system::DefaultSimulationFrequency;
    int max_simulation_count_per_frame = define::system::DefaultMaxSimulationCountPerFrame;
    InitializationLocation initialization_location = InitializationLocation::Start;
    TimeManager::UpdateLocation update_location = TimeManager::UpdateLocation::AfterLateUpdate;

    void DataValidate()
    {
        simulation_frequency = std::clamp(
            simulation_frequency,
            define::system::SimulationFrequencyLow,
            define::system::SimulationFrequencyHigh
        );
        max_simulation_count_per_frame = std::clamp(
            max_simulation_count_per_frame,
            define::system::MaxSimulationCountPerFrameLow,
            define::system::MaxSimulationCountPerFrameHigh
        );
    }
};

}  // namespace hocloth::mc2
