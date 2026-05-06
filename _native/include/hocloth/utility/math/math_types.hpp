#pragma once

#include <limits>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Math/*.cs
struct float3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] float operator[](int index) const
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }

    [[nodiscard]] float& operator[](int index)
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }
};

struct float2 {
    float x = 0.0f;
    float y = 0.0f;

    [[nodiscard]] float operator[](int index) const
    {
        return index == 0 ? x : y;
    }

    [[nodiscard]] float& operator[](int index)
    {
        return index == 0 ? x : y;
    }
};

struct float4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct quaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct float4x4 {
    float4 c0{1.0f, 0.0f, 0.0f, 0.0f};
    float4 c1{0.0f, 1.0f, 0.0f, 0.0f};
    float4 c2{0.0f, 0.0f, 1.0f, 0.0f};
    float4 c3{0.0f, 0.0f, 0.0f, 1.0f};
};

struct int2 {
    int x = 0;
    int y = 0;

    [[nodiscard]] int operator[](int index) const
    {
        return index == 0 ? x : y;
    }

    [[nodiscard]] int& operator[](int index)
    {
        return index == 0 ? x : y;
    }
};

struct int3 {
    int x = 0;
    int y = 0;
    int z = 0;

    [[nodiscard]] int operator[](int index) const
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }

    [[nodiscard]] int& operator[](int index)
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }
};

struct int4 {
    int x = 0;
    int y = 0;
    int z = 0;
    int w = 0;

    [[nodiscard]] int operator[](int index) const
    {
        switch (index) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            return w;
        }
    }
};

struct AABB {
    float3 min{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    float3 max{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };
};

}  // namespace hocloth::mc2
