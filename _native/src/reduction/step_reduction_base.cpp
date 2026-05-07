#include "hocloth/reduction/step_reduction_base.hpp"

#include "hocloth/reduction/reduction_work_data.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>
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
    remove_pair_list_.clear();
    result_array_.clear();
}

Result StepReductionBase::Reduction()
{
    result_ = Result::Ok();
    if (vmesh_ == nullptr || work_data_ == nullptr) {
        result_ = Result::Error(ExceptionCode(), "StepReductionBase missing mesh or work data.");
        return result_;
    }

    try {
        StepInitialize();
        InitStep();
        while (now_step_index_ < max_step_) {
            ReductionStep();
            ++now_step_index_;
            if (IsEndStep()) {
                break;
            }
            NextStep();
        }

        if (now_step_index_ < max_step_) {
            ReductionStep();
            ++now_step_index_;
        }

        UpdateReductionResult();
        int remove_vertex_count = 0;
        for (int index = 0; index < now_step_index_ && index < static_cast<int>(result_array_.size()); ++index) {
            remove_vertex_count += result_array_[static_cast<std::size_t>(index)];
        }
        work_data_->remove_vertex_count += remove_vertex_count;
        return result_;
    } catch (...) {
        result_ = Result::Error(ExceptionCode(), "Step reduction failed.");
        return result_;
    }
}

void StepReductionBase::StepInitialize()
{
    const int vertex_count = vmesh_ != nullptr ? vmesh_->VertexCount() : 0;
    join_edge_list_.clear();
    join_edge_list_.reserve(static_cast<std::size_t>(std::max(vertex_count / 4, 1)));
    remove_pair_list_.clear();
    remove_pair_list_.reserve(static_cast<std::size_t>(std::max(vertex_count, 1)));
    result_array_.assign(static_cast<std::size_t>(std::max(max_step_, 0)), 0);
}

void StepReductionBase::CustomReductionStep()
{
}

ResultCode StepReductionBase::ExceptionCode() const
{
    return ResultCode::Reduction_Exception;
}

int StepReductionBase::CountLinks(int vertex_index) const
{
    return work_data_ != nullptr ? work_data_->LinkCount(vertex_index) : 0;
}

bool StepReductionBase::CheckJoin2(int vertex_index, int target_vertex_index) const
{
    if (work_data_ == nullptr
        || vertex_index < 0
        || target_vertex_index < 0
        || vertex_index > std::numeric_limits<std::uint16_t>::max()
        || target_vertex_index > std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }

    std::vector<std::uint16_t> join_links =
        work_data_->GatherMergedLinks(vertex_index, target_vertex_index);

    if (join_links.empty()) {
        return false;
    }

    if (!dont_make_line_) {
        return true;
    }

    std::vector<std::uint16_t> stack{join_links.front()};
    while (!stack.empty()) {
        const std::uint16_t index = stack.back();
        stack.pop_back();

        const auto found = std::find(join_links.begin(), join_links.end(), index);
        if (found == join_links.end()) {
            continue;
        }
        join_links.erase(found);

        const auto found_link = work_data_->vertex_to_vertex_map.find(index);
        if (found_link == work_data_->vertex_to_vertex_map.end()) {
            continue;
        }
        for (std::uint16_t neighbor : found_link->second) {
            if (std::find(join_links.begin(), join_links.end(), neighbor) != join_links.end()) {
                stack.push_back(neighbor);
            }
        }
    }

    return join_links.empty();
}

void StepReductionBase::InitStep()
{
    now_step_index_ = 0;
    now_merge_length_ = start_merge_length_;
    now_step_scale_ = 2.0f;
}

bool StepReductionBase::IsEndStep() const
{
    return now_merge_length_ == end_merge_length_;
}

void StepReductionBase::NextStep()
{
    now_step_scale_ = std::max(now_step_scale_ * 0.93f, 1.1f);
    now_merge_length_ = std::min(now_merge_length_ * now_step_scale_, end_merge_length_);
}

void StepReductionBase::ReductionStep()
{
    PreReductionStep();
    CustomReductionStep();
    PostReductionStep();
}

void StepReductionBase::PreReductionStep()
{
    join_edge_list_.clear();
}

void StepReductionBase::PostReductionStep()
{
    SortJoinEdge();
    DetermineJoinEdge();
    RunJoinEdge();
    UpdateJoinAndLink();
}

void StepReductionBase::SortJoinEdge()
{
    std::sort(join_edge_list_.begin(), join_edge_list_.end());
}

void StepReductionBase::DetermineJoinEdge()
{
    std::unordered_set<int> complete_vertices;
    remove_pair_list_.clear();
    int count = 0;
    for (const JoinEdge& join_edge : join_edge_list_) {
        const int2 pair = join_edge.vertex_pair;
        if (complete_vertices.find(pair.x) != complete_vertices.end()
            || complete_vertices.find(pair.y) != complete_vertices.end()) {
            continue;
        }

        remove_pair_list_.push_back(pair);
        complete_vertices.insert(pair.x);
        complete_vertices.insert(pair.y);
        ++count;
    }

    if (now_step_index_ >= 0 && now_step_index_ < static_cast<int>(result_array_.size())) {
        result_array_[static_cast<std::size_t>(now_step_index_)] = count;
    }
}

void StepReductionBase::RunJoinEdge()
{
    if (vmesh_ == nullptr || work_data_ == nullptr) {
        return;
    }

    for (const int2& pair : remove_pair_list_) {
        const int vertex_index1 = pair.x;
        const int vertex_index2 = pair.y;
        if (vertex_index1 < 0
            || vertex_index2 < 0
            || vertex_index1 >= vmesh_->VertexCount()
            || vertex_index2 >= vmesh_->VertexCount()
            || vertex_index1 >= vmesh_->local_positions.Count()
            || vertex_index2 >= vmesh_->local_positions.Count()
            || vertex_index1 >= static_cast<int>(work_data_->vertex_join_indices.size())
            || vertex_index2 >= static_cast<int>(work_data_->vertex_join_indices.size())) {
            continue;
        }

        const float3 position1 = vmesh_->local_positions[vertex_index1];
        const float3 position2 = vmesh_->local_positions[vertex_index2];
        const float3 normal1 =
            vertex_index1 < vmesh_->local_normals.Count()
                ? vmesh_->local_normals[vertex_index1]
                : float3{0.0f, 1.0f, 0.0f};
        const float3 normal2 =
            vertex_index2 < vmesh_->local_normals.Count()
                ? vmesh_->local_normals[vertex_index2]
                : float3{0.0f, 1.0f, 0.0f};

        work_data_->vertex_join_indices[static_cast<std::size_t>(vertex_index1)] = vertex_index2;

        const float link_count1 = static_cast<float>(std::max(CountLinks(vertex_index1) - 1, 1));
        const float link_count2 = static_cast<float>(std::max(CountLinks(vertex_index2) - 1, 1));
        float ratio = link_count2 / (link_count1 + link_count2);
        ratio = ratio + (0.5f - ratio) * join_position_adjustment_;

        vmesh_->local_positions[vertex_index2] = Lerp(position2, position1, ratio);
        if (vertex_index2 < vmesh_->local_normals.Count()) {
            vmesh_->local_normals[vertex_index2] = Add(normal1, normal2);
        }

        work_data_->ReplaceVertexLinks(
            vertex_index2,
            work_data_->GatherMergedLinks(vertex_index1, vertex_index2)
        );

        if (vertex_index2 < vmesh_->bone_weights.Count() && vertex_index1 < vmesh_->bone_weights.Count()) {
            vmesh_->bone_weights[vertex_index2].AddWeight(vmesh_->bone_weights[vertex_index1]);
        }
        if (vertex_index2 < vmesh_->attributes.Count() && vertex_index1 < vmesh_->attributes.Count()) {
            vmesh_->attributes[vertex_index2] = VertexAttribute::JoinAttribute(
                vmesh_->attributes[vertex_index1],
                vmesh_->attributes[vertex_index2]
            );
            vmesh_->attributes[vertex_index1] = VertexAttribute::Invalid();
        }
    }
}

void StepReductionBase::UpdateJoinAndLink()
{
    if (work_data_ == nullptr || vmesh_ == nullptr) {
        return;
    }

    work_data_->RefreshJoinAndLinks(vmesh_->VertexCount());
}

void StepReductionBase::UpdateReductionResult()
{
    if (vmesh_ == nullptr || work_data_ == nullptr) {
        return;
    }

    for (int vertex_index = 0; vertex_index < vmesh_->VertexCount(); ++vertex_index) {
        if (work_data_->vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0) {
            continue;
        }
        if (vertex_index < vmesh_->local_normals.Count()) {
            vmesh_->local_normals[vertex_index] =
                Normalize(vmesh_->local_normals[vertex_index], float3{0.0f, 1.0f, 0.0f});
        }
        if (vertex_index < vmesh_->bone_weights.Count()) {
            vmesh_->bone_weights[vertex_index].AdjustWeight();
        }
    }
}

}  // namespace hocloth::mc2
