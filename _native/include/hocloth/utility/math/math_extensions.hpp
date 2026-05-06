#pragma once

#include "hocloth/utility/math/math_types.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Math/MathExtensions.cs
[[nodiscard]] float MC2GetValue(const float4x4& matrix, int index);
void MC2SetValue(float4x4& matrix, int index, float value);
[[nodiscard]] float MC2EvaluateCurve(const float4x4& matrix, float time);
[[nodiscard]] float MC2EvaluateCurveClamp01(const float4x4& matrix, float time);

}  // namespace hocloth::mc2
