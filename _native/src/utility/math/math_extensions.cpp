#include "hocloth/utility/math/math_extensions.hpp"

#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>

namespace hocloth::mc2 {

namespace {

float& ColumnElement(float4& column, int row)
{
    switch (row) {
    case 0:
        return column.x;
    case 1:
        return column.y;
    case 2:
        return column.z;
    default:
        return column.w;
    }
}

const float& ColumnElement(const float4& column, int row)
{
    switch (row) {
    case 0:
        return column.x;
    case 1:
        return column.y;
    case 2:
        return column.z;
    default:
        return column.w;
    }
}

float4& Column(float4x4& matrix, int column_index)
{
    switch (column_index) {
    case 0:
        return matrix.c0;
    case 1:
        return matrix.c1;
    case 2:
        return matrix.c2;
    default:
        return matrix.c3;
    }
}

const float4& Column(const float4x4& matrix, int column_index)
{
    switch (column_index) {
    case 0:
        return matrix.c0;
    case 1:
        return matrix.c1;
    case 2:
        return matrix.c2;
    default:
        return matrix.c3;
    }
}

}  // namespace

float MC2GetValue(const float4x4& matrix, int index)
{
    const int clamped = std::clamp(index, 0, 15);
    return ColumnElement(Column(matrix, clamped / 4), clamped % 4);
}

void MC2SetValue(float4x4& matrix, int index, float value)
{
    const int clamped = std::clamp(index, 0, 15);
    ColumnElement(Column(matrix, clamped / 4), clamped % 4) = value;
}

float MC2EvaluateCurve(const float4x4& matrix, float time)
{
    return data::EvaluateCurve(matrix, time);
}

float MC2EvaluateCurveClamp01(const float4x4& matrix, float time)
{
    return Clamp01(MC2EvaluateCurve(matrix, time));
}

}  // namespace hocloth::mc2
