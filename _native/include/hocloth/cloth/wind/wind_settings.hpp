#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/core/interface/i_data_validate.hpp"
#include "hocloth/core/interface/i_valid.hpp"
#include "hocloth/utility/math/math_utility.hpp"

namespace hocloth::mc2 {

// Native port of Magica Cloth 2: Scripts/Core/Cloth/Wind/WindSettings.cs.
struct WindSettings final : public IValid, public IDataValidate {
    float influence = 1.0f;
    float frequency = 1.0f;
    float turbulence = 1.0f;
    float blend = 0.7f;
    float synchronization = 0.7f;
    float depth_weight = 0.0f;
    float moving_wind = 0.0f;

    [[nodiscard]] bool IsValid() const override
    {
        return influence > define::system::Epsilon;
    }

    void DataValidate() override
    {
        influence = Clamp(influence, 0.0f, 2.0f);
        frequency = Clamp(frequency, 0.0f, 2.0f);
        turbulence = Clamp(turbulence, 0.0f, 2.0f);
        blend = Clamp01(blend);
        synchronization = Clamp01(synchronization);
        depth_weight = Clamp01(depth_weight);
        moving_wind = Clamp(moving_wind, 0.0f, 10.0f);
    }

    [[nodiscard]] WindSettings Clone() const
    {
        return *this;
    }
};

}  // namespace hocloth::mc2
