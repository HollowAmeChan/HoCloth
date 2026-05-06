#include "hocloth/reduction/simple_distance_reduction.hpp"

#include "hocloth/reduction/reduction_work_data.hpp"
#include "hocloth/utility/grid/grid_map.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <algorithm>

namespace hocloth::mc2 {

void SimpleDistanceReduction::CustomReductionStep()
{
    if (vmesh_ == nullptr || work_data_ == nullptr) {
        return;
    }

    const int vertex_count = vmesh_->VertexCount();
    const float grid_size = now_merge_length_ * 2.0f;
    GridMap<int> grid_map(vertex_count);

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        if (vertex_index >= vmesh_->local_positions.Count()
            || vertex_index >= static_cast<int>(work_data_->vertex_join_indices.size())
            || work_data_->vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0) {
            continue;
        }

        GridMap<int>::AddGrid(
            vmesh_->local_positions[vertex_index],
            vertex_index,
            grid_map.GetMap(),
            grid_size
        );
    }

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        if (vertex_index >= vmesh_->local_positions.Count()
            || vertex_index >= static_cast<int>(work_data_->vertex_join_indices.size())
            || work_data_->vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0) {
            continue;
        }

        const float3 position = vmesh_->local_positions[vertex_index];
        const float link_count = static_cast<float>(std::max(CountLinks(vertex_index) - 1, 1));
        for (const int3& grid : GridMap<int>::GetArea(position, now_merge_length_, grid_size)) {
            const auto found_grid = grid_map.GetMap().find(grid);
            if (found_grid == grid_map.GetMap().end()) {
                continue;
            }

            for (int target_index : found_grid->second) {
                if (target_index == vertex_index
                    || target_index < 0
                    || target_index >= vertex_count
                    || target_index >= vmesh_->local_positions.Count()) {
                    continue;
                }

                const float distance =
                    Distance(position, vmesh_->local_positions[target_index]);
                if (distance > now_merge_length_) {
                    continue;
                }

                const float target_link_count =
                    static_cast<float>(std::max(CountLinks(target_index) - 1, 1));
                const float cost = distance * (1.0f + (link_count + target_link_count) / 2.0f);
                join_edge_list_.push_back(JoinEdge{int2{vertex_index, target_index}, cost});
            }
        }
    }
}

ResultCode SimpleDistanceReduction::ExceptionCode() const
{
    return ResultCode::Reduction_SimpleDistanceException;
}

}  // namespace hocloth::mc2
