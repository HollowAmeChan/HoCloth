#pragma once

#include "hocloth/cloth/wind/wind_settings.hpp"
#include "hocloth/core/define/system_define.hpp"
#include "hocloth/core/interface/i_valid.hpp"

namespace hocloth::mc2 {

enum class ClothType {
    Unknown = 0,
    MeshCloth = 1,
    BoneCloth = 2,
    BoneSpring = 3,
};

// Native port of Magica Cloth 2: Scripts/Core/Cloth/Wind/WindParams.cs.
struct WindParams final : public IValid {
    float influence = 0.0f;
    float frequency = 0.0f;
    float turbulence = 0.0f;
    float blend = 0.0f;
    float synchronization = 0.0f;
    float depth_weight = 0.0f;
    float moving_wind = 0.0f;

    void Convert(const WindSettings& settings, ClothType cloth_type = ClothType::Unknown)
    {
        (void)cloth_type;
        influence = settings.influence;
        frequency = settings.frequency;
        turbulence = settings.turbulence;
        blend = settings.blend;
        synchronization = settings.synchronization;
        depth_weight = settings.depth_weight;
        moving_wind = settings.moving_wind;
    }

    [[nodiscard]] bool IsValid() const override
    {
        return influence > define::system::Epsilon;
    }
};

}  // namespace hocloth::mc2
