#pragma once

#include "hocloth/utility/math/math_types.hpp"

#include <cstdint>

namespace hocloth::mc2::data {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Data/DataUtility.cs
[[nodiscard]] int2 PackInt2(int a, int b);
[[nodiscard]] int3 PackInt3(int a, int b, int c);
[[nodiscard]] std::uint32_t Pack32(int hi, int low);
[[nodiscard]] std::uint32_t Pack32Sort(int a, int b);
[[nodiscard]] int Unpack32Hi(std::uint32_t pack);
[[nodiscard]] int Unpack32Low(std::uint32_t pack);
[[nodiscard]] std::uint32_t Pack12_20(int hi, int low);
[[nodiscard]] int Unpack12_20Hi(std::uint32_t pack);
[[nodiscard]] int Unpack12_20Low(std::uint32_t pack);
void Unpack12_20(std::uint32_t pack, int& hi, int& low);
[[nodiscard]] float EvaluateCurve(const float4x4& curve, float time);

}  // namespace hocloth::mc2::data
