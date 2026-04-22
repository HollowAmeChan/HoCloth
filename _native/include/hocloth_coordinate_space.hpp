#pragma once

#include <cmath>

namespace hocloth {

// Blender uses a Z-up right-handed basis. The current native placeholder
// solver stores data in a Y-up-style internal basis so the runtime API can
// remain stable while the custom XPBD backend evolves.
inline void blender_to_solver_vector(const float in[3], float out[3]) {
    out[0] = in[0];
    out[1] = in[2];
    out[2] = -in[1];
}


inline void solver_to_blender_vector(const float in[3], float out[3]) {
    out[0] = in[0];
    out[1] = -in[2];
    out[2] = in[1];
}


inline void quaternion_to_matrix3(const float q[4], float out[9]) {
    const float w = q[0];
    const float x = q[1];
    const float y = q[2];
    const float z = q[3];

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    out[0] = 1.0f - 2.0f * (yy + zz);
    out[1] = 2.0f * (xy - wz);
    out[2] = 2.0f * (xz + wy);

    out[3] = 2.0f * (xy + wz);
    out[4] = 1.0f - 2.0f * (xx + zz);
    out[5] = 2.0f * (yz - wx);

    out[6] = 2.0f * (xz - wy);
    out[7] = 2.0f * (yz + wx);
    out[8] = 1.0f - 2.0f * (xx + yy);
}


inline void matrix3_multiply(const float a[9], const float b[9], float out[9]) {
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out[row * 3 + col] =
                a[row * 3 + 0] * b[0 * 3 + col] +
                a[row * 3 + 1] * b[1 * 3 + col] +
                a[row * 3 + 2] * b[2 * 3 + col];
        }
    }
}


inline void matrix3_to_quaternion(const float m[9], float out[4]) {
    const float trace = m[0] + m[4] + m[8];
    if (trace > 0.0f) {
        const float s = 0.5f / std::sqrt(trace + 1.0f);
        out[0] = 0.25f / s;
        out[1] = (m[7] - m[5]) * s;
        out[2] = (m[2] - m[6]) * s;
        out[3] = (m[3] - m[1]) * s;
        return;
    }

    if (m[0] > m[4] && m[0] > m[8]) {
        const float s = 2.0f * std::sqrt(1.0f + m[0] - m[4] - m[8]);
        out[0] = (m[7] - m[5]) / s;
        out[1] = 0.25f * s;
        out[2] = (m[1] + m[3]) / s;
        out[3] = (m[2] + m[6]) / s;
        return;
    }

    if (m[4] > m[8]) {
        const float s = 2.0f * std::sqrt(1.0f + m[4] - m[0] - m[8]);
        out[0] = (m[2] - m[6]) / s;
        out[1] = (m[1] + m[3]) / s;
        out[2] = 0.25f * s;
        out[3] = (m[5] + m[7]) / s;
        return;
    }

    const float s = 2.0f * std::sqrt(1.0f + m[8] - m[0] - m[4]);
    out[0] = (m[3] - m[1]) / s;
    out[1] = (m[2] + m[6]) / s;
    out[2] = (m[5] + m[7]) / s;
    out[3] = 0.25f * s;
}


inline void normalize_quaternion(float io[4]) {
    const float len = std::sqrt(io[0] * io[0] + io[1] * io[1] + io[2] * io[2] + io[3] * io[3]);
    if (len <= 0.0f) {
        io[0] = 1.0f;
        io[1] = 0.0f;
        io[2] = 0.0f;
        io[3] = 0.0f;
        return;
    }

    io[0] /= len;
    io[1] /= len;
    io[2] /= len;
    io[3] /= len;
}


inline void blender_to_solver_quaternion(const float in[4], float out[4]) {
    static constexpr float basis[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, -1.0f, 0.0f,
    };
    static constexpr float basis_inv[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f,
        0.0f, 1.0f, 0.0f,
    };

    float rotation[9];
    float temp[9];
    float converted[9];
    quaternion_to_matrix3(in, rotation);
    matrix3_multiply(basis, rotation, temp);
    matrix3_multiply(temp, basis_inv, converted);
    matrix3_to_quaternion(converted, out);
    normalize_quaternion(out);
}


inline void solver_to_blender_quaternion(const float in[4], float out[4]) {
    static constexpr float basis[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, -1.0f, 0.0f,
    };
    static constexpr float basis_inv[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f,
        0.0f, 1.0f, 0.0f,
    };

    float rotation[9];
    float temp[9];
    float converted[9];
    quaternion_to_matrix3(in, rotation);
    matrix3_multiply(basis_inv, rotation, temp);
    matrix3_multiply(temp, basis, converted);
    matrix3_to_quaternion(converted, out);
    normalize_quaternion(out);
}

}  // namespace hocloth
