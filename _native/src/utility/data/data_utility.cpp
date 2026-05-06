#include "hocloth/utility/data/data_utility.hpp"

#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>

namespace hocloth::mc2::data {

int2 PackInt2(int a, int b)
{
    return a < b ? int2{a, b} : int2{b, a};
}

int3 PackInt3(int a, int b, int c)
{
    if (a > b) {
        std::swap(a, b);
    }
    if (b > c) {
        std::swap(b, c);
    }
    if (a > b) {
        std::swap(a, b);
    }
    return int3{a, b, c};
}

std::uint32_t Pack32(int hi, int low)
{
    return (static_cast<std::uint32_t>(hi) << 16)
        | (static_cast<std::uint32_t>(low) & 0xffffu);
}

std::uint32_t Pack32Sort(int a, int b)
{
    return a > b ? Pack32(b, a) : Pack32(a, b);
}

int Unpack32Hi(std::uint32_t pack)
{
    return static_cast<int>((pack >> 16) & 0xffffu);
}

int Unpack32Low(std::uint32_t pack)
{
    return static_cast<int>(pack & 0xffffu);
}

std::uint32_t Pack12_20(int hi, int low)
{
    return (static_cast<std::uint32_t>(hi) << 20)
        | (static_cast<std::uint32_t>(low) & 0xfffffu);
}

int Unpack12_20Hi(std::uint32_t pack)
{
    return static_cast<int>((pack >> 20) & 0xfffu);
}

int Unpack12_20Low(std::uint32_t pack)
{
    return static_cast<int>(pack & 0xfffffu);
}

void Unpack12_20(std::uint32_t pack, int& hi, int& low)
{
    hi = Unpack12_20Hi(pack);
    low = Unpack12_20Low(pack);
}

float EvaluateCurve(const float4x4& curve, float time)
{
    constexpr float interval = 1.0f / 15.0f;
    const float clamped_time = Clamp01(time);
    const int index = std::min(static_cast<int>(clamped_time * 15.0f), 14);
    const float local_time = clamped_time - static_cast<float>(index) * interval;
    const float t = local_time / interval;
    const float a = MC2GetValue(curve, index);
    const float b = MC2GetValue(curve, index + 1);
    return a + (b - a) * t;
}

}  // namespace hocloth::mc2::data
