#pragma once

#include "hocloth/cloth/cloth_parameters.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

namespace hocloth::mc2 {

// Native-side port of Scripts/Core/Cloth/CurveSerializeData.cs.
// Unity AnimationCurve is represented by the MC2 16-sample float4x4 curve data already used by jobs.
struct CurveSerializeData {
    float value = 0.0f;
    bool use_curve = false;
    float4x4 curve = ConstantCurve(1.0f);

    CurveSerializeData() = default;
    explicit CurveSerializeData(float value_)
        : value(value_)
    {
    }

    CurveSerializeData(float value_, float curve_start, float curve_end, bool use_curve_ = true)
        : value(value_)
        , use_curve(use_curve_)
    {
        SetLinearCurve(curve_start, curve_end);
    }

    CurveSerializeData(float value_, const float4x4& curve_)
        : value(value_)
        , use_curve(true)
        , curve(curve_)
    {
    }

    void SetValue(float value_)
    {
        value = value_;
        use_curve = false;
    }

    void SetValue(float value_, float curve_start, float curve_end, bool use_curve_ = true)
    {
        value = value_;
        use_curve = use_curve_;
        SetLinearCurve(curve_start, curve_end);
    }

    void SetValue(float value_, const float4x4& curve_)
    {
        value = value_;
        use_curve = true;
        curve = curve_;
    }

    void DataValidate(float min_value, float max_value)
    {
        value = Clamp(value, min_value, max_value);
    }

    [[nodiscard]] float Evaluate(float time) const
    {
        return use_curve ? MC2EvaluateCurve(curve, time) * value : value;
    }

    [[nodiscard]] float4x4 ConvertFloatArray() const
    {
        if (!use_curve) {
            return ConstantCurve(value);
        }

        float4x4 result = curve;
        for (int index = 0; index < 16; ++index) {
            MC2SetValue(result, index, MC2GetValue(result, index) * value);
        }
        return result;
    }

    [[nodiscard]] CurveSerializeData Clone() const
    {
        return *this;
    }

private:
    void SetLinearCurve(float curve_start, float curve_end)
    {
        const float start = Clamp01(curve_start);
        const float end = Clamp01(curve_end);
        for (int index = 0; index < 16; ++index) {
            const float t = static_cast<float>(index) / 15.0f;
            MC2SetValue(curve, index, start + (end - start) * t);
        }
    }
};

}  // namespace hocloth::mc2
