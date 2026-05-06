#pragma once

#include "hocloth/utility/result_code/result_code.hpp"

#include <string>

namespace hocloth::mc2 {

class VirtualMesh;
struct ReductionWorkData;

// Port target for Magica Cloth 2: Scripts/Core/Reduction/SameDistanceReduction.cs
class SameDistanceReduction {
public:
    SameDistanceReduction() = default;
    SameDistanceReduction(
        std::string name,
        VirtualMesh* mesh,
        ReductionWorkData* working_data,
        float merge_length
    );

    void Dispose();
    Result Reduction();

    [[nodiscard]] const Result& GetResult() const
    {
        return result_;
    }

private:
    std::string name_;
    VirtualMesh* vmesh_ = nullptr;
    ReductionWorkData* work_data_ = nullptr;
    Result result_ = Result::Ok();
    float merge_length_ = 1.0e-9f;
};

}  // namespace hocloth::mc2
