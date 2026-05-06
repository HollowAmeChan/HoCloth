#include "hocloth/manager/transform/transform_record.hpp"

#include "hocloth/utility/math/math_utility.hpp"

namespace hocloth::mc2 {

float3 TransformRecord::InverseTransformDirection(const float3& direction) const
{
    return Rotate(Inverse(rotation), direction);
}

}  // namespace hocloth::mc2
