#pragma once

#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"

#include <cstdint>

namespace hocloth::mc2::data {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Data/DataUtility.cs
[[nodiscard]] int2 PackInt2(int a, int b);
[[nodiscard]] int2 PackInt2(const int2& value);
[[nodiscard]] int3 PackInt3(int a, int b, int c);
[[nodiscard]] int3 PackInt3(const int3& value);
[[nodiscard]] int4 PackInt4(int a, int b, int c, int d);
[[nodiscard]] int4 PackInt4(const int4& value);
[[nodiscard]] std::uint32_t Pack32(int hi, int low);
[[nodiscard]] std::uint32_t Pack32(int x, int y, int z, int w);
[[nodiscard]] std::uint64_t Pack32(const int4& value);
[[nodiscard]] std::uint32_t Pack32Sort(int a, int b);
[[nodiscard]] int Unpack32Hi(std::uint32_t pack);
[[nodiscard]] int Unpack32Low(std::uint32_t pack);
[[nodiscard]] int4 Unpack32(std::uint32_t pack);
[[nodiscard]] std::uint32_t Pack10_22(int hi, int low);
[[nodiscard]] int Unpack10_22Hi(std::uint32_t pack);
[[nodiscard]] int Unpack10_22Low(std::uint32_t pack);
void Unpack10_22(std::uint32_t pack, int& hi, int& low);
[[nodiscard]] std::uint64_t Pack64(int x, int y, int z, int w);
[[nodiscard]] std::uint64_t Pack64(const int4& value);
[[nodiscard]] int4 Unpack64(std::uint64_t pack);
[[nodiscard]] int Unpack64X(std::uint64_t pack);
[[nodiscard]] int Unpack64Y(std::uint64_t pack);
[[nodiscard]] int Unpack64Z(std::uint64_t pack);
[[nodiscard]] int Unpack64W(std::uint64_t pack);
[[nodiscard]] std::uint32_t Pack12_20(int hi, int low);
[[nodiscard]] int Unpack12_20Hi(std::uint32_t pack);
[[nodiscard]] int Unpack12_20Low(std::uint32_t pack);
void Unpack12_20(std::uint32_t pack, int& hi, int& low);
[[nodiscard]] int RemainingData(const int3& data, const int2& used);
[[nodiscard]] float EvaluateCurve(const float4x4& curve, float time);
[[nodiscard]] ColliderManager::ColliderType GetColliderType(BitFlag8 flag);
[[nodiscard]] BitFlag8 SetColliderType(BitFlag8 flag, ColliderManager::ColliderType type);

}  // namespace hocloth::mc2::data
