#pragma once

#include "hocloth/utility/math/math_utility.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Cloth/CheckSliderSerializeData.cs
struct CheckSliderSerializeData {
    float value = 0.0f;
    bool use = false;

    CheckSliderSerializeData() = default;
    CheckSliderSerializeData(bool use_, float value_)
        : value(value_)
        , use(use_)
    {
    }

    [[nodiscard]] float GetValue(float unused_value) const
    {
        return use ? value : unused_value;
    }

    void SetValue(bool use_, float value_)
    {
        use = use_;
        value = value_;
    }

    void DataValidate(float min_value, float max_value)
    {
        value = Clamp(value, min_value, max_value);
    }

    [[nodiscard]] CheckSliderSerializeData Clone() const
    {
        return *this;
    }
};

}  // namespace hocloth::mc2
