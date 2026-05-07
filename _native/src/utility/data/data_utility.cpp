#include "hocloth/utility/data/data_utility.hpp"

#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>

namespace hocloth::mc2::data {

int2 PackInt2(int a, int b)
{
    return a < b ? int2{a, b} : int2{b, a};
}

int2 PackInt2(const int2& value)
{
    return PackInt2(value.x, value.y);
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

int3 PackInt3(const int3& value)
{
    return PackInt3(value.x, value.y, value.z);
}

int4 PackInt4(int a, int b, int c, int d)
{
    if (a > d) {
        std::swap(a, d);
    }
    if (b > c) {
        std::swap(b, c);
    }
    if (a > b) {
        std::swap(a, b);
    }
    if (c > d) {
        std::swap(c, d);
    }
    if (b > c) {
        std::swap(b, c);
    }
    return int4{a, b, c, d};
}

int4 PackInt4(const int4& value)
{
    return PackInt4(value.x, value.y, value.z, value.w);
}

std::uint32_t Pack32(int hi, int low)
{
    return (static_cast<std::uint32_t>(hi) << 16)
        | (static_cast<std::uint32_t>(low) & 0xffffu);
}

std::uint32_t Pack32(int x, int y, int z, int w)
{
    return ((static_cast<std::uint32_t>(x) & 0xffu) << 24)
        | ((static_cast<std::uint32_t>(y) & 0xffu) << 16)
        | ((static_cast<std::uint32_t>(z) & 0xffu) << 8)
        | (static_cast<std::uint32_t>(w) & 0xffu);
}

std::uint64_t Pack32(const int4& value)
{
    return Pack64(value);
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

int4 Unpack32(std::uint32_t pack)
{
    return int4{
        static_cast<int>((pack >> 24) & 0xffu),
        static_cast<int>((pack >> 16) & 0xffu),
        static_cast<int>((pack >> 8) & 0xffu),
        static_cast<int>(pack & 0xffu),
    };
}

std::uint32_t Pack10_22(int hi, int low)
{
    return (static_cast<std::uint32_t>(hi) << 22)
        | (static_cast<std::uint32_t>(low) & 0x3fffffu);
}

int Unpack10_22Hi(std::uint32_t pack)
{
    return static_cast<int>((pack >> 22) & 0x3ffu);
}

int Unpack10_22Low(std::uint32_t pack)
{
    return static_cast<int>(pack & 0x3fffffu);
}

void Unpack10_22(std::uint32_t pack, int& hi, int& low)
{
    hi = Unpack10_22Hi(pack);
    low = Unpack10_22Low(pack);
}

std::uint64_t Pack64(int x, int y, int z, int w)
{
    return ((static_cast<std::uint64_t>(x) & 0xffffull) << 48)
        | ((static_cast<std::uint64_t>(y) & 0xffffull) << 32)
        | ((static_cast<std::uint64_t>(z) & 0xffffull) << 16)
        | (static_cast<std::uint64_t>(w) & 0xffffull);
}

std::uint64_t Pack64(const int4& value)
{
    return Pack64(value.x, value.y, value.z, value.w);
}

int4 Unpack64(std::uint64_t pack)
{
    return int4{
        static_cast<int>((pack >> 48) & 0xffffull),
        static_cast<int>((pack >> 32) & 0xffffull),
        static_cast<int>((pack >> 16) & 0xffffull),
        static_cast<int>(pack & 0xffffull),
    };
}

int Unpack64X(std::uint64_t pack)
{
    return static_cast<int>((pack >> 48) & 0xffffull);
}

int Unpack64Y(std::uint64_t pack)
{
    return static_cast<int>((pack >> 32) & 0xffffull);
}

int Unpack64Z(std::uint64_t pack)
{
    return static_cast<int>((pack >> 16) & 0xffffull);
}

int Unpack64W(std::uint64_t pack)
{
    return static_cast<int>(pack & 0xffffull);
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

int RemainingData(const int3& data, const int2& used)
{
    if (data.x != used.x && data.x != used.y) {
        return data.x;
    }
    if (data.y != used.x && data.y != used.y) {
        return data.y;
    }
    return data.z;
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
