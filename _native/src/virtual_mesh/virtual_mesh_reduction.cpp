#include "virtual_mesh_internal.hpp"

#include "hocloth/reduction/reduction_settings.hpp"
#include "hocloth/reduction/reduction_work_data.hpp"
#include "hocloth/reduction/same_distance_reduction.hpp"
#include "hocloth/reduction/shape_distance_reduction.hpp"
#include "hocloth/reduction/simple_distance_reduction.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace hocloth::mc2 {

void VirtualMesh::Reduction(const ReductionSettings& settings)
{
    // Port target: Scripts/Core/VirtualMesh/Function/VirtualMeshReduction.cs
    result = Result::Ok();
    try {
        ReductionWorkData work_data{this};
        InitReductionWorkData(work_data);
        if (result.Failed()) {
            return;
        }

        const float max_side_length = MaxSideLength(bounding_box);
        if (max_side_length < 1.0e-8f) {
            result = Result::Error(
                ResultCode::Reduction_MaxSideLengthZero,
                "VirtualMesh reduction max side length is zero."
            );
            return;
        }

        const float same_distance =
            max_side_length * Clamp01(define::system::ReductionSameDistance);
        const float simple_distance = max_side_length * Clamp01(settings.simple_distance);
        const float shape_distance = max_side_length * Clamp01(settings.shape_distance);

        {
            SameDistanceReduction same_reduction{name, this, &work_data, same_distance};
            const Result same_result = same_reduction.Reduction();
            if (same_result.Failed()) {
                result = same_result;
                return;
            }
        }

        if (simple_distance > same_distance) {
            const float start_distance = std::min(same_distance * 2.0f, simple_distance);
            SimpleDistanceReduction simple_reduction{
                "SimpleDistanceReduction [" + name + "]",
                this,
                &work_data,
                start_distance,
                simple_distance,
                define::system::ReductionMaxStep,
                define::system::ReductionDontMakeLine,
                define::system::ReductionJoinPositionAdjustment,
            };
            const Result simple_result = simple_reduction.Reduction();
            if (simple_result.Failed()) {
                result = simple_result;
                return;
            }
        }

        if (shape_distance > 0.0f && shape_distance > simple_distance) {
            const float start_distance =
                std::min(std::max(same_distance * 2.0f, simple_distance), shape_distance);
            ShapeDistanceReduction shape_reduction{
                "ShapeReduction [" + name + "]",
                this,
                &work_data,
                start_distance,
                shape_distance,
                define::system::ReductionMaxStep,
                define::system::ReductionDontMakeLine,
                define::system::ReductionJoinPositionAdjustment,
            };
            const Result shape_result = shape_reduction.Reduction();
            if (shape_result.Failed()) {
                result = shape_result;
                return;
            }
        }

        Organization(settings, work_data);
        if (result.Failed()) {
            return;
        }
        OrganizeStoreVirtualMesh(work_data);
        if (result.Failed()) {
            return;
        }
        RefreshDerivedTopology();
        CalcAverageAndMaxVertexDistanceRun();
    } catch (...) {
        result = Result::Error(ResultCode::Reduction_Exception, "VirtualMesh reduction failed.");
    }
}

void VirtualMesh::InitReductionWorkData(ReductionWorkData& work_data)
{
    // Port target: Scripts/Core/VirtualMesh/Function/VirtualMeshReduction.cs
    try {
        const int vertex_count = VertexCount();
        work_data.vmesh = this;
        work_data.vertex_join_indices.assign(static_cast<std::size_t>(vertex_count), -1);
        work_data.vertex_to_vertex_map.clear();

        for (int triangle_index = 0; triangle_index < TriangleCount(); ++triangle_index) {
            const int3 triangle = triangles[triangle_index];
            if (triangle.x < 0
                || triangle.y < 0
                || triangle.z < 0
                || triangle.x >= vertex_count
                || triangle.y >= vertex_count
                || triangle.z >= vertex_count) {
                continue;
            }

            const std::uint16_t x = static_cast<std::uint16_t>(triangle.x);
            const std::uint16_t y = static_cast<std::uint16_t>(triangle.y);
            const std::uint16_t z = static_cast<std::uint16_t>(triangle.z);
            work_data.vertex_to_vertex_map[x].push_back(y);
            work_data.vertex_to_vertex_map[x].push_back(z);
            work_data.vertex_to_vertex_map[y].push_back(x);
            work_data.vertex_to_vertex_map[y].push_back(z);
            work_data.vertex_to_vertex_map[z].push_back(x);
            work_data.vertex_to_vertex_map[z].push_back(y);
        }
    } catch (...) {
        result = Result::Error(ResultCode::Reduction_InitError, "VirtualMesh reduction init failed.");
    }
}

void VirtualMesh::Organization(const ReductionSettings& settings, ReductionWorkData& work_data)
{
    try {
        OrganizationInit(settings, work_data);
        OrganizationCreateRemapData(work_data);
        OrganizationCreateBasicData(work_data);
        OrganizationCreateLineTriangle(work_data);
    } catch (...) {
        result = Result::Error(ResultCode::Reduction_OrganizationError, "VirtualMesh reduction organization failed.");
    }
}

void VirtualMesh::OrganizationInit(const ReductionSettings&, ReductionWorkData& work_data)
{
    work_data.old_vertex_count = VertexCount();
    if (work_data.vertex_join_indices.size() < static_cast<std::size_t>(work_data.old_vertex_count)) {
        work_data.vertex_join_indices.resize(static_cast<std::size_t>(work_data.old_vertex_count), -1);
    }
    work_data.remove_vertex_count = 0;
    for (int index = 0; index < work_data.old_vertex_count; ++index) {
        if (work_data.vertex_join_indices[static_cast<std::size_t>(index)] >= 0) {
            ++work_data.remove_vertex_count;
        }
    }
    work_data.new_vertex_count = work_data.old_vertex_count - work_data.remove_vertex_count;

    work_data.vertex_remap_indices.assign(static_cast<std::size_t>(work_data.old_vertex_count), -1);
    work_data.old_vertex_to_new_vertex_indices.assign(
        static_cast<std::size_t>(work_data.old_vertex_count),
        -1
    );
    work_data.use_skin_bone_map.clear();
    work_data.new_vertex_to_vertex_map.clear();
    work_data.edge_set.clear();
    work_data.triangle_set.clear();
    work_data.new_line_list.clear();
    work_data.new_triangle_list.clear();
    work_data.new_skin_bone_count = 0;
    work_data.new_skin_bone_transform_indices.clear();
    work_data.new_skin_bone_bind_pose_list.clear();

    const int new_vertex_count = std::max(work_data.new_vertex_count, 0);
    work_data.new_attributes.Dispose();
    work_data.new_local_positions.Dispose();
    work_data.new_local_normals.Dispose();
    work_data.new_local_tangents.Dispose();
    work_data.new_uv.Dispose();
    work_data.new_bone_weights.Dispose();
    work_data.new_attributes = ExSimpleNativeArray<VertexAttribute>(new_vertex_count);
    work_data.new_local_positions = ExSimpleNativeArray<float3>(new_vertex_count);
    work_data.new_local_normals = ExSimpleNativeArray<float3>(new_vertex_count);
    work_data.new_local_tangents = ExSimpleNativeArray<float3>(new_vertex_count);
    work_data.new_uv = ExSimpleNativeArray<float2>(new_vertex_count);
    work_data.new_bone_weights = ExSimpleNativeArray<VirtualMeshBoneWeight>(new_vertex_count);
}

void VirtualMesh::OrganizationCreateRemapData(ReductionWorkData& work_data)
{
    int remap_index = 0;
    for (int vertex_index = 0; vertex_index < work_data.old_vertex_count; ++vertex_index) {
        const int join = work_data.vertex_join_indices[static_cast<std::size_t>(vertex_index)];
        if (join < 0) {
            work_data.vertex_remap_indices[static_cast<std::size_t>(vertex_index)] = remap_index;
            work_data.old_vertex_to_new_vertex_indices[static_cast<std::size_t>(vertex_index)] = remap_index;
            ++remap_index;
        }
    }

    for (int vertex_index = 0; vertex_index < work_data.old_vertex_count; ++vertex_index) {
        const int join = work_data.vertex_join_indices[static_cast<std::size_t>(vertex_index)];
        if (join < 0) {
            continue;
        }
        work_data.vertex_remap_indices[static_cast<std::size_t>(vertex_index)] =
            join >= 0 && join < work_data.old_vertex_count
                ? work_data.vertex_remap_indices[static_cast<std::size_t>(join)]
                : -1;
        work_data.old_vertex_to_new_vertex_indices[static_cast<std::size_t>(vertex_index)] =
            work_data.vertex_remap_indices[static_cast<std::size_t>(vertex_index)];
    }

    for (int vertex_index = 0; vertex_index < work_data.old_vertex_count; ++vertex_index) {
        const int join = work_data.vertex_join_indices[static_cast<std::size_t>(vertex_index)];
        if (join >= 0 || vertex_index >= bone_weights.Count()) {
            continue;
        }

        const VirtualMeshBoneWeight bone_weight = bone_weights[vertex_index];
        for (int weight_index = 0; weight_index < 4; ++weight_index) {
            if (bone_weight.weights[static_cast<std::size_t>(weight_index)] <= 0.0f) {
                continue;
            }
            const int old_bone_index = bone_weight.bone_indices[static_cast<std::size_t>(weight_index)];
            if (old_bone_index < 0
                || work_data.use_skin_bone_map.find(old_bone_index)
                    != work_data.use_skin_bone_map.end()) {
                continue;
            }

            const int new_bone_index = static_cast<int>(work_data.use_skin_bone_map.size());
            work_data.use_skin_bone_map[old_bone_index] = new_bone_index;
            if (old_bone_index < skin_bone_bind_poses.Count()) {
                work_data.new_skin_bone_bind_pose_list.push_back(skin_bone_bind_poses[old_bone_index]);
            }
        }
    }
    work_data.new_skin_bone_count = static_cast<int>(work_data.use_skin_bone_map.size());
}

void VirtualMesh::OrganizationCreateBasicData(ReductionWorkData& work_data)
{
    const int old_vertex_count = work_data.old_vertex_count;
    for (int vertex_index = 0; vertex_index < old_vertex_count; ++vertex_index) {
        const int join = work_data.vertex_join_indices[static_cast<std::size_t>(vertex_index)];
        if (join >= 0) {
            continue;
        }

        const int new_index = work_data.vertex_remap_indices[static_cast<std::size_t>(vertex_index)];
        if (new_index < 0 || new_index >= work_data.new_vertex_count) {
            continue;
        }

        if (vertex_index < attributes.Count()) {
            work_data.new_attributes[new_index] = attributes[vertex_index];
        }
        if (vertex_index < local_positions.Count()) {
            work_data.new_local_positions[new_index] = local_positions[vertex_index];
            work_data.new_uv[new_index] = SphereMappingUV(local_positions[vertex_index], bounding_box, new_index);
        }
        if (vertex_index < local_normals.Count()) {
            work_data.new_local_normals[new_index] = local_normals[vertex_index];
        }
        if (vertex_index < local_tangents.Count()) {
            work_data.new_local_tangents[new_index] = local_tangents[vertex_index];
        }
        if (vertex_index < bone_weights.Count()) {
            VirtualMeshBoneWeight bone_weight = bone_weights[vertex_index];
            for (int weight_index = 0; weight_index < 4; ++weight_index) {
                if (bone_weight.weights[static_cast<std::size_t>(weight_index)] > 0.0f) {
                    const int old_bone_index = bone_weight.bone_indices[static_cast<std::size_t>(weight_index)];
                    const auto found = work_data.use_skin_bone_map.find(old_bone_index);
                    bone_weight.bone_indices[static_cast<std::size_t>(weight_index)] =
                        found != work_data.use_skin_bone_map.end() ? found->second : 0;
                } else {
                    bone_weight.bone_indices[static_cast<std::size_t>(weight_index)] = 0;
                }
            }
            work_data.new_bone_weights[new_index] = bone_weight;
        }
    }

    for (int vertex_index = 0; vertex_index < old_vertex_count; ++vertex_index) {
        const int join = work_data.vertex_join_indices[static_cast<std::size_t>(vertex_index)];
        if (join >= 0) {
            continue;
        }
        const int new_index = work_data.vertex_remap_indices[static_cast<std::size_t>(vertex_index)];
        if (new_index < 0 || new_index > std::numeric_limits<std::uint16_t>::max()) {
            continue;
        }

        const auto found = work_data.vertex_to_vertex_map.find(static_cast<std::uint16_t>(vertex_index));
        if (found == work_data.vertex_to_vertex_map.end()) {
            continue;
        }
        for (std::uint16_t old_link_index : found->second) {
            const int old_link = static_cast<int>(old_link_index);
            if (old_link < 0 || old_link >= old_vertex_count) {
                continue;
            }
            const int new_link_index = work_data.vertex_remap_indices[static_cast<std::size_t>(old_link)];
            if (new_link_index < 0
                || new_link_index == new_index
                || new_link_index > std::numeric_limits<std::uint16_t>::max()) {
                continue;
            }
            UniqueAdd(
                work_data.new_vertex_to_vertex_map,
                static_cast<std::uint16_t>(new_index),
                static_cast<std::uint16_t>(new_link_index)
            );
        }
    }
}

void VirtualMesh::OrganizationCreateLineTriangle(ReductionWorkData& work_data)
{
    for (int vertex_index = 0; vertex_index < work_data.new_vertex_count; ++vertex_index) {
        const auto found = work_data.new_vertex_to_vertex_map.find(static_cast<std::uint16_t>(vertex_index));
        if (found == work_data.new_vertex_to_vertex_map.end()) {
            continue;
        }
        for (std::uint16_t link_index : found->second) {
            if (link_index == vertex_index) {
                continue;
            }
            work_data.edge_set.insert(PackedEdgeKey(int2{vertex_index, static_cast<int>(link_index)}));
        }
    }

    for (std::uint64_t edge_key : work_data.edge_set) {
        const int edge_x = data::Unpack32Hi(static_cast<std::uint32_t>(edge_key));
        const int edge_y = data::Unpack32Low(static_cast<std::uint32_t>(edge_key));
        const auto found = work_data.new_vertex_to_vertex_map.find(static_cast<std::uint16_t>(edge_x));
        int triangle_count = 0;
        if (found != work_data.new_vertex_to_vertex_map.end()) {
            for (std::uint16_t vertex_index : found->second) {
                if (vertex_index == edge_x || vertex_index == edge_y) {
                    continue;
                }
                const auto found_y =
                    work_data.new_vertex_to_vertex_map.find(static_cast<std::uint16_t>(edge_y));
                const bool contains_y =
                    found_y != work_data.new_vertex_to_vertex_map.end()
                    && std::find(found_y->second.begin(), found_y->second.end(), vertex_index)
                        != found_y->second.end();
                if (!contains_y) {
                    continue;
                }

                const int3 triangle = data::PackInt3(edge_x, edge_y, static_cast<int>(vertex_index));
                work_data.triangle_set.insert(PackedTriangleKey(triangle));
                ++triangle_count;
            }
        }
        if (triangle_count == 0) {
            work_data.new_line_list.push_back(data::PackInt2(edge_x, edge_y));
        }
    }

    for (std::uint64_t triangle_key : work_data.triangle_set) {
        const int x = static_cast<int>((triangle_key >> 32) & 0xffffull);
        const int y = static_cast<int>((triangle_key >> 16) & 0xffffull);
        const int z = static_cast<int>(triangle_key & 0xffffull);
        work_data.new_triangle_list.push_back(int3{x, y, z});
    }
}

void VirtualMesh::OrganizeStoreVirtualMesh(ReductionWorkData& work_data)
{
    try {
        const int vertex_count = work_data.new_vertex_count;
        reference_indices.Dispose();
        reference_indices = ExSimpleNativeArray<int>(vertex_count);
        for (int index = 0; index < vertex_count; ++index) {
            reference_indices[index] = index;
        }

        attributes.Dispose();
        attributes = std::move(work_data.new_attributes);
        local_positions.Dispose();
        local_positions = std::move(work_data.new_local_positions);
        local_normals.Dispose();
        local_normals = std::move(work_data.new_local_normals);
        local_tangents.Dispose();
        local_tangents = std::move(work_data.new_local_tangents);
        uv.Dispose();
        uv = std::move(work_data.new_uv);
        bone_weights.Dispose();
        bone_weights = std::move(work_data.new_bone_weights);

        lines.Dispose();
        lines.AddRange(work_data.new_line_list);
        edges.Dispose();
        std::vector<int2> new_edges;
        new_edges.reserve(work_data.edge_set.size());
        for (std::uint64_t edge_key : work_data.edge_set) {
            new_edges.push_back(int2{
                data::Unpack32Hi(static_cast<std::uint32_t>(edge_key)),
                data::Unpack32Low(static_cast<std::uint32_t>(edge_key)),
            });
        }
        edges.AddRange(new_edges);
        triangles.Dispose();
        triangles.AddRange(work_data.new_triangle_list);

        OrganizeReductionTransform(*this, work_data);

        skin_bone_transform_indices.Dispose();
        skin_bone_transform_indices.AddRange(work_data.new_skin_bone_transform_indices);
        skin_bone_bind_poses.Dispose();
        skin_bone_bind_poses.AddRange(work_data.new_skin_bone_bind_pose_list);
        join_indices.Dispose();
        join_indices.AddRange(work_data.vertex_remap_indices);
    } catch (...) {
        result = Result::Error(ResultCode::Reduction_StoreVirtualMeshError, "VirtualMesh reduction store failed.");
    }
}

}  // namespace hocloth::mc2
