#include "hocloth/reduction/step_reduction_base.hpp"

#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <utility>

namespace hocloth::mc2 {

StepReductionBase::StepReductionBase(
    std::string name,
    VirtualMesh* mesh,
    ReductionWorkData* working_data,
    float start_merge_length,
    float end_merge_length,
    int max_step,
    bool dont_make_line,
    float join_position_adjustment
)
    : name_(std::move(name))
    , vmesh_(mesh)
    , work_data_(working_data)
    , result_(Result::Ok())
    , start_merge_length_(std::max(start_merge_length, 1.0e-9f))
    , end_merge_length_(std::max(end_merge_length, 1.0e-9f))
    , max_step_(std::min(max_step, 100))
    , dont_make_line_(dont_make_line)
    , join_position_adjustment_(join_position_adjustment)
{
}

void StepReductionBase::Dispose()
{
    join_edge_list_.clear();
}

}  // namespace hocloth::mc2
