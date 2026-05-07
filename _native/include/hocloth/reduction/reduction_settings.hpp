#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/core/interface/i_data_validate.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Reduction/ReductionSettings.cs
struct ReductionSettings final : public IDataValidate {
    float simple_distance = 0.0f;
    float shape_distance = 0.0f;

    [[nodiscard]] bool IsEnabled() const
    {
        return define::system::ReductionEnable;
    }

    [[nodiscard]] float GetMaxConnectionDistance() const
    {
        return std::max(
            std::max(define::system::ReductionSameDistance, simple_distance),
            shape_distance
        );
    }

    [[nodiscard]] ReductionSettings Clone() const
    {
        return *this;
    }

    void DataValidate() override
    {
        simple_distance = Clamp(simple_distance, 0.0f, 0.2f);
        shape_distance = Clamp(shape_distance, 0.0f, 0.2f);
    }

    [[nodiscard]] int GetHashCode() const
    {
        return FloatHash(simple_distance) + FloatHash(shape_distance);
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "ReductionSettings. sameDist:" << define::system::ReductionSameDistance
               << ", simpleDist:" << simple_distance
               << ", shapeDist:" << shape_distance
               << " maxStep:" << define::system::ReductionMaxStep;
        return stream.str();
    }

private:
    [[nodiscard]] static int FloatHash(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return static_cast<int>(bits);
    }
};

}  // namespace hocloth::mc2
