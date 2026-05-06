#include "hocloth/reduction/shape_distance_reduction.hpp"

#include "hocloth/reduction/reduction_work_data.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace hocloth::mc2 {

void ShapeDistanceReduction::CustomReductionStep()
{
    if (vmesh_ == nullptr || work_data_ == nullptr) {
        return;
    }

    const int vertex_count = vmesh_->VertexCount();
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        if (vertex_index >= vmesh_->local_positions.Count()
            || vertex_index >= static_cast<int>(work_data_->vertex_join_indices.size())
            || work_data_->vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0
            || vertex_index > std::numeric_limits<std::uint16_t>::max()) {
            continue;
        }

        const int link_count_int = CountLinks(vertex_index);
        if (link_count_int == 0) {
            continue;
        }

        const float3 position = vmesh_->local_positions[vertex_index];
        const float link_count = static_cast<float>(std::max(link_count_int - 1, 1));
        float min_cost = std::numeric_limits<float>::max();
        int min_vertex_index = -1;

        const auto found =
            work_data_->vertex_to_vertex_map.find(static_cast<std::uint16_t>(vertex_index));
        if (found == work_data_->vertex_to_vertex_map.end()) {
            continue;
        }

        for (std::uint16_t target_value : found->second) {
            const int target_index = static_cast<int>(target_value);
            if (target_index < 0
                || target_index >= vertex_count
                || target_index >= vmesh_->local_positions.Count()) {
                continue;
            }

            const float distance =
                Distance(position, vmesh_->local_positions[target_index]);
            if (distance > now_merge_length_) {
                continue;
            }
            if (!CheckJoin2(vertex_index, target_index)) {
                continue;
            }

            const float target_link_count =
                static_cast<float>(std::max(CountLinks(target_index) - 1, 1));
            const float cost = distance * (1.0f + (link_count + target_link_count) / 2.0f);
            if (cost < min_cost) {
                min_cost = cost;
                min_vertex_index = target_index;
            }
        }

        if (min_vertex_index >= 0) {
            join_edge_list_.push_back(
                JoinEdge{int2{vertex_index, min_vertex_index}, min_cost}
            );
        }
    }
}

ResultCode ShapeDistanceReduction::ExceptionCode() const
{
    return ResultCode::Reduction_ShapeDistanceException;
}

}  // namespace hocloth::mc2
