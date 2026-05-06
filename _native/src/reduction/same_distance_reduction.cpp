#include "hocloth/reduction/same_distance_reduction.hpp"

#include "hocloth/reduction/reduction_work_data.hpp"
#include "hocloth/utility/grid/grid_map.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hocloth::mc2 {

namespace {

using VertexLinkMap = std::unordered_map<std::uint16_t, std::vector<std::uint16_t>>;

void UniqueAdd(VertexLinkMap& map, std::uint16_t key, std::uint16_t value)
{
    std::vector<std::uint16_t>& values = map[key];
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void RemoveValue(VertexLinkMap& map, std::uint16_t key, std::uint16_t value)
{
    const auto found = map.find(key);
    if (found == map.end()) {
        return;
    }
    std::vector<std::uint16_t>& values = found->second;
    values.erase(std::remove(values.begin(), values.end(), value), values.end());
}

int ResolveJoinRoot(const std::vector<int>& join_indices, int index)
{
    if (index < 0 || index >= static_cast<int>(join_indices.size())) {
        return index;
    }

    int guard = 0;
    while (join_indices[static_cast<std::size_t>(index)] >= 0
        && guard < static_cast<int>(join_indices.size())) {
        index = join_indices[static_cast<std::size_t>(index)];
        ++guard;
    }
    return index;
}

void UpdateJoinAndLink(ReductionWorkData& work_data, int vertex_count)
{
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        int& join = work_data.vertex_join_indices[static_cast<std::size_t>(vertex_index)];
        if (join >= 0) {
            join = ResolveJoinRoot(work_data.vertex_join_indices, join);
        }
    }

    VertexLinkMap new_map;
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        if (work_data.vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0) {
            continue;
        }

        const auto found = work_data.vertex_to_vertex_map.find(static_cast<std::uint16_t>(vertex_index));
        if (found == work_data.vertex_to_vertex_map.end()) {
            continue;
        }

        for (std::uint16_t old_link : found->second) {
            int link = static_cast<int>(old_link);
            if (link < 0 || link >= vertex_count) {
                continue;
            }
            link = ResolveJoinRoot(work_data.vertex_join_indices, link);
            if (link == vertex_index
                || link < 0
                || link > std::numeric_limits<std::uint16_t>::max()) {
                continue;
            }
            UniqueAdd(new_map, static_cast<std::uint16_t>(vertex_index), static_cast<std::uint16_t>(link));
        }
    }

    work_data.vertex_to_vertex_map = std::move(new_map);
}

void FinalizeLiveVertices(VirtualMesh& mesh, const ReductionWorkData& work_data)
{
    const int vertex_count = mesh.VertexCount();
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        if (work_data.vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0) {
            continue;
        }
        if (vertex_index < mesh.local_normals.Count()) {
            mesh.local_normals[vertex_index] =
                Normalize(mesh.local_normals[vertex_index], float3{0.0f, 1.0f, 0.0f});
        }
        if (vertex_index < mesh.bone_weights.Count()) {
            mesh.bone_weights[vertex_index].AdjustWeight();
        }
    }
}

}  // namespace

SameDistanceReduction::SameDistanceReduction(
    std::string name,
    VirtualMesh* mesh,
    ReductionWorkData* working_data,
    float merge_length
)
    : name_(std::move(name))
    , vmesh_(mesh)
    , work_data_(working_data)
    , result_(Result::Ok())
    , merge_length_(std::max(merge_length, 1.0e-9f))
{
}

void SameDistanceReduction::Dispose()
{
}

Result SameDistanceReduction::Reduction()
{
    result_ = Result::Ok();
    if (vmesh_ == nullptr || work_data_ == nullptr) {
        result_ = Result::Error(
            ResultCode::Reduction_SameDistanceException,
            "SameDistanceReduction missing mesh or work data."
        );
        return result_;
    }

    try {
        const int vertex_count = vmesh_->VertexCount();
        if (work_data_->vertex_join_indices.size() < static_cast<std::size_t>(vertex_count)) {
            work_data_->vertex_join_indices.resize(static_cast<std::size_t>(vertex_count), -1);
        }

        GridMap<int> grid_map(vertex_count);
        const float grid_size = merge_length_ * 2.0f;
        for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
            if (work_data_->vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0
                || vertex_index >= vmesh_->local_positions.Count()) {
                continue;
            }
            GridMap<int>::AddGrid(
                vmesh_->local_positions[vertex_index],
                vertex_index,
                grid_map.GetMap(),
                grid_size
            );
        }

        std::unordered_map<std::uint16_t, std::vector<std::uint16_t>> join_pair_map;
        for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
            if (work_data_->vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0
                || vertex_index >= vmesh_->local_positions.Count()) {
                continue;
            }

            const float3 position = vmesh_->local_positions[vertex_index];
            for (const int3& grid : GridMap<int>::GetArea(position, merge_length_, grid_size)) {
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
                    if (Distance(position, vmesh_->local_positions[target_index]) > merge_length_) {
                        continue;
                    }
                    join_pair_map[static_cast<std::uint16_t>(vertex_index)].push_back(
                        static_cast<std::uint16_t>(target_index)
                    );
                }
            }
        }

        int remove_count = 0;
        for (int live_index = 0; live_index < vertex_count; ++live_index) {
            if (work_data_->vertex_join_indices[static_cast<std::size_t>(live_index)] >= 0) {
                continue;
            }

            const auto found = join_pair_map.find(static_cast<std::uint16_t>(live_index));
            if (found == join_pair_map.end()) {
                continue;
            }

            for (std::uint16_t dead_value : found->second) {
                const int dead_index = static_cast<int>(dead_value);
                if (dead_index == live_index
                    || dead_index < 0
                    || dead_index >= vertex_count
                    || work_data_->vertex_join_indices[static_cast<std::size_t>(dead_index)] >= 0) {
                    continue;
                }

                work_data_->vertex_join_indices[static_cast<std::size_t>(dead_index)] = live_index;
                ++remove_count;
                RemoveValue(
                    work_data_->vertex_to_vertex_map,
                    static_cast<std::uint16_t>(live_index),
                    static_cast<std::uint16_t>(dead_index)
                );

                std::vector<std::uint16_t> dead_links;
                const auto found_dead =
                    work_data_->vertex_to_vertex_map.find(static_cast<std::uint16_t>(dead_index));
                if (found_dead != work_data_->vertex_to_vertex_map.end()) {
                    dead_links = found_dead->second;
                }

                for (std::uint16_t link_value : dead_links) {
                    const int link_index = static_cast<int>(link_value);
                    if (link_index < 0
                        || link_index >= vertex_count
                        || link_index == live_index
                        || link_index == dead_index
                        || work_data_->vertex_join_indices[static_cast<std::size_t>(link_index)] >= 0) {
                        continue;
                    }

                    RemoveValue(
                        work_data_->vertex_to_vertex_map,
                        static_cast<std::uint16_t>(link_index),
                        static_cast<std::uint16_t>(dead_index)
                    );
                    UniqueAdd(
                        work_data_->vertex_to_vertex_map,
                        static_cast<std::uint16_t>(live_index),
                        static_cast<std::uint16_t>(link_index)
                    );
                    UniqueAdd(
                        work_data_->vertex_to_vertex_map,
                        static_cast<std::uint16_t>(link_index),
                        static_cast<std::uint16_t>(live_index)
                    );
                }

                if (live_index < vmesh_->bone_weights.Count() && dead_index < vmesh_->bone_weights.Count()) {
                    vmesh_->bone_weights[live_index].AddWeight(vmesh_->bone_weights[dead_index]);
                }
                if (live_index < vmesh_->attributes.Count() && dead_index < vmesh_->attributes.Count()) {
                    vmesh_->attributes[live_index] = VertexAttribute::JoinAttribute(
                        vmesh_->attributes[dead_index],
                        vmesh_->attributes[live_index]
                    );
                    vmesh_->attributes[dead_index] = VertexAttribute::Invalid();
                }
            }
        }

        UpdateJoinAndLink(*work_data_, vertex_count);
        FinalizeLiveVertices(*vmesh_, *work_data_);
        work_data_->remove_vertex_count += remove_count;
        return result_;
    } catch (...) {
        result_ = Result::Error(
            ResultCode::Reduction_SameDistanceException,
            "SameDistanceReduction failed."
        );
        return result_;
    }
}

}  // namespace hocloth::mc2
