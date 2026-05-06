#pragma once

#include "hocloth/reduction/step_reduction_base.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Reduction/SimpleDistanceReduction.cs
class SimpleDistanceReduction final : public StepReductionBase {
public:
    using StepReductionBase::StepReductionBase;

protected:
    void CustomReductionStep() override;
    [[nodiscard]] ResultCode ExceptionCode() const override;
};

}  // namespace hocloth::mc2
