#pragma once

#include "hocloth/cloth/parameters/check_slider_serialize_data.hpp"
#include "hocloth/core/define/system_define.hpp"
#include "hocloth/core/interface/i_data_validate.hpp"
#include "hocloth/utility/math/math_utility.hpp"

namespace hocloth::mc2 {

// Native data port for Scripts/Core/Cloth/CullingSettings.cs.
// Renderer/GameObject references stay at the Blender integration boundary.
struct CullingSettings final : public IDataValidate {
    enum class CameraCullingMode {
        Off = 0,
        Reset = 10,
        Keep = 20,
        AnimatorLinkage = 30,
    };

    enum class CameraCullingMethod {
        AutomaticRenderer = 0,
        ManualRenderer = 10,
    };

    CameraCullingMode camera_culling_mode = CameraCullingMode::AnimatorLinkage;
    CameraCullingMethod camera_culling_method = CameraCullingMethod::AutomaticRenderer;
    CheckSliderSerializeData distance_culling_length{true, 30.0f};
    float distance_culling_fade_ratio = 0.2f;

    void DataValidate() override
    {
        distance_culling_length.DataValidate(0.0f, define::system::DistanceCullingMaxLength);
        distance_culling_fade_ratio = Clamp01(distance_culling_fade_ratio);
    }

    [[nodiscard]] CullingSettings Clone() const
    {
        return *this;
    }
};

}  // namespace hocloth::mc2
