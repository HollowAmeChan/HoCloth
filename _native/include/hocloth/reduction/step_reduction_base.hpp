#pragma once

#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/result_code/result_code.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

class ReductionWorkData;
class VirtualMesh;

// Skeleton/data port for Scripts/Core/Reduction/StepReductionBase.cs.
class StepReductionBase {
public:
    struct JoinEdge {
        int2 vertex_pair{};
        float cost = 0.0f;

        [[nodiscard]] bool Contains(const int2& pair) const
        {
            return vertex_pair.x == pair.x || vertex_pair.x == pair.y
                || vertex_pair.y == pair.x || vertex_pair.y == pair.y;
        }

        [[nodiscard]] int CompareTo(const JoinEdge& other) const
        {
            if (cost == other.cost) {
                return 0;
            }
            return cost < other.cost ? -1 : 1;
        }

        friend bool operator<(const JoinEdge& lhs, const JoinEdge& rhs)
        {
            return lhs.cost < rhs.cost;
        }
    };

    StepReductionBase() = default;
    StepReductionBase(
        std::string name,
        VirtualMesh* mesh,
        ReductionWorkData* working_data,
        float start_merge_length,
        float end_merge_length,
        int max_step,
        bool dont_make_line,
        float join_position_adjustment
    );
    virtual ~StepReductionBase() = default;

    virtual void Dispose();
    virtual Result Reduction();

    [[nodiscard]] const Result& GetResult() const
    {
        return result_;
    }

protected:
    virtual void StepInitialize();
    virtual void CustomReductionStep();
    virtual ResultCode ExceptionCode() const;

    [[nodiscard]] int CountLinks(int vertex_index) const;
    [[nodiscard]] bool CheckJoin2(int vertex_index, int target_vertex_index) const;

    std::string name_;
    VirtualMesh* vmesh_ = nullptr;
    ReductionWorkData* work_data_ = nullptr;
    Result result_;
    float start_merge_length_ = 0.0f;
    float end_merge_length_ = 0.0f;
    int max_step_ = 0;
    bool dont_make_line_ = false;
    float join_position_adjustment_ = 0.0f;
    int now_step_index_ = 0;
    float now_merge_length_ = 0.0f;
    float now_step_scale_ = 0.0f;
    std::vector<JoinEdge> join_edge_list_;
    std::vector<int2> remove_pair_list_;
    std::vector<int> result_array_;

private:
    void InitStep();
    [[nodiscard]] bool IsEndStep() const;
    void NextStep();
    void ReductionStep();
    void PreReductionStep();
    void PostReductionStep();
    void SortJoinEdge();
    void DetermineJoinEdge();
    void RunJoinEdge();
    void UpdateJoinAndLink();
    void UpdateReductionResult();
};

}  // namespace hocloth::mc2
