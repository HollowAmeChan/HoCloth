#pragma once

#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/reduction/reduction_work_data.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hocloth::mc2 {

inline constexpr std::uint8_t BaseLineFlagIncludeLine = 0x01;

inline float SafeDepth(float length, float max_length)
{
    return max_length > define::system::Epsilon ? Clamp01(length / max_length) : 0.0f;
}

inline bool IsValidVertexIndex(int index, int vertex_count)
{
    return index >= 0 && index < vertex_count && index <= std::numeric_limits<std::uint16_t>::max();
}

inline float3 LocalNormalOrDefault(const VirtualMesh& mesh, int vertex_index)
{
    return vertex_index >= 0 && vertex_index < mesh.local_normals.Count()
        ? mesh.local_normals[vertex_index]
        : float3{0.0f, 1.0f, 0.0f};
}

inline float3 LocalTangentOrDefault(const VirtualMesh& mesh, int vertex_index)
{
    return vertex_index >= 0 && vertex_index < mesh.local_tangents.Count()
        ? mesh.local_tangents[vertex_index]
        : float3{0.0f, 0.0f, 1.0f};
}

inline void AddUniquePackedEdge(std::unordered_set<std::uint32_t>& edge_set, int a, int b)
{
    if (a < 0 || b < 0 || a == b) {
        return;
    }
    edge_set.insert(data::Pack32Sort(a, b));
}

inline void AddUniqueEdge(std::vector<int2>& edges, std::unordered_set<std::uint32_t>& edge_set, int a, int b)
{
    if (a < 0 || b < 0 || a == b) {
        return;
    }
    const std::uint32_t key = data::Pack32Sort(a, b);
    if (edge_set.insert(key).second) {
        edges.push_back(data::PackInt2(a, b));
    }
}

inline std::uint64_t PackedTriangleKey(const int3& triangle)
{
    const int3 packed = data::PackInt3(triangle);
    return (static_cast<std::uint64_t>(packed.x) << 32)
        | (static_cast<std::uint64_t>(packed.y) << 16)
        | static_cast<std::uint64_t>(packed.z);
}

inline std::uint64_t PackedEdgeKey(const int2& edge)
{
    const int2 packed = data::PackInt2(edge);
    return data::Pack32(packed.x, packed.y);
}

inline void UniqueAdd(
    std::unordered_map<std::uint16_t, std::vector<std::uint16_t>>& map,
    std::uint16_t key,
    std::uint16_t value
)
{
    ReductionWorkData::AddUniqueLink(map, key, value);
}

inline void UniqueAdd(
    VirtualMesh::EdgeToTrianglesMap& map,
    std::uint32_t key,
    std::uint16_t value
)
{
    std::vector<std::uint16_t>& values = map[key];
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

inline float2 SphereMappingUV(const float3& position, const AABB& bounds, int index)
{
    const float3 direction = Normalize(Subtract(position, Center(bounds)));
    constexpr float pi = 3.14159265358979323846f;
    const float u = Clamp((std::atan2(direction.x, direction.z) + pi) / (pi * 2.0f), 0.0f, 1.0f);
    const float v = Clamp((1.0f - direction.y) * 0.5f, 0.0f, 1.0f);
    const float add = static_cast<float>(index) * 0.0001234f;
    return float2{v * 10.0f + add, u * 10.0f + add};
}

inline int2 TriangleEdgeAt(const int3& triangle, int edge_index)
{
    switch (edge_index) {
    case 0:
        return data::PackInt2(triangle.x, triangle.y);
    case 1:
        return data::PackInt2(triangle.y, triangle.z);
    default:
        return data::PackInt2(triangle.z, triangle.x);
    }
}

inline float CalcTwoTriangleAngle(
    const ExSimpleNativeArray<float3>& local_positions,
    const int3& tri1,
    const int3& tri2,
    const int2& edge
)
{
    const int vertex1 = data::RemainingData(tri1, edge);
    const int vertex2 = data::RemainingData(tri2, edge);
    if (edge.x < 0
        || edge.y < 0
        || vertex1 < 0
        || vertex2 < 0
        || edge.x >= local_positions.Count()
        || edge.y >= local_positions.Count()
        || vertex1 >= local_positions.Count()
        || vertex2 >= local_positions.Count()) {
        return 180.0f;
    }

    const float3 edge_vector =
        Subtract(local_positions[edge.y], local_positions[edge.x]);
    const float3 tri1_vector =
        Subtract(local_positions[vertex1], local_positions[edge.x]);
    const float3 tri2_vector =
        Subtract(local_positions[vertex2], local_positions[edge.x]);
    const float3 normal0 = Cross(edge_vector, tri1_vector);
    const float3 normal1 = Cross(tri2_vector, edge_vector);
    return Angle(normal0, normal1) * 57.29577951308232f;
}

inline bool CheckTwoTriangleOpen(
    const ExSimpleNativeArray<float3>& local_positions,
    const int3& tri2,
    const int2& edge,
    const float3& tri1_normal
)
{
    const int vertex = data::RemainingData(tri2, edge);
    if (edge.x < 0
        || vertex < 0
        || edge.x >= local_positions.Count()
        || vertex >= local_positions.Count()) {
        return true;
    }
    const float3 direction =
        Normalize(Subtract(local_positions[vertex], local_positions[edge.x]));
    return Dot(tri1_normal, direction) <= 0.0f;
}

inline void OptimizeTriangleDirection(
    ExSimpleNativeArray<int3>& triangles,
    std::vector<float3>& triangle_normals,
    VirtualMesh::EdgeToTrianglesMap& edge_to_triangles,
    const ExSimpleNativeArray<float3>& local_positions,
    float same_surface_angle
)
{
    const int triangle_count = triangles.Count();
    if (triangle_count <= 0 || triangle_normals.size() < static_cast<std::size_t>(triangle_count)) {
        return;
    }
    if (edge_to_triangles.empty()) {
        return;
    }

    int start_index = 0;
    std::unordered_set<int> used_triangles;
    std::queue<int> triangle_queue;
    std::vector<int> layer;
    while (start_index < triangle_count) {
        if (used_triangles.contains(start_index)) {
            ++start_index;
            continue;
        }

        used_triangles.insert(start_index);
        triangle_queue.push(start_index);
        layer.clear();
        int open_count = 0;
        int close_count = 0;

        while (!triangle_queue.empty()) {
            const int triangle_index = triangle_queue.front();
            triangle_queue.pop();
            if (triangle_index < 0 || triangle_index >= triangle_count) {
                continue;
            }

            const float3 triangle_normal =
                triangle_normals[static_cast<std::size_t>(triangle_index)];
            const int3 triangle = triangles[triangle_index];
            layer.push_back(triangle_index);

            for (int edge_index = 0; edge_index < 3; ++edge_index) {
                const int2 edge = TriangleEdgeAt(triangle, edge_index);
                const auto found = edge_to_triangles.find(data::Pack32(edge.x, edge.y));
                if (found == edge_to_triangles.end()) {
                    continue;
                }

                for (std::uint16_t data : found->second) {
                    const int other_index = static_cast<int>(data);
                    if (other_index < 0
                        || other_index >= triangle_count
                        || used_triangles.contains(other_index)) {
                        continue;
                    }

                    int3 other_triangle = triangles[other_index];
                    const float3 other_normal =
                        triangle_normals[static_cast<std::size_t>(other_index)];
                    const float angle = CalcTwoTriangleAngle(
                        local_positions,
                        triangle,
                        other_triangle,
                        edge
                    );
                    if (angle > same_surface_angle) {
                        continue;
                    }

                    if (Dot(triangle_normal, other_normal) < 0.0f) {
                        other_triangle = FlipTriangle(other_triangle);
                        triangles[other_index] = other_triangle;
                        triangle_normals[static_cast<std::size_t>(other_index)] =
                            Scale(other_normal, -1.0f);
                    }

                    if (CheckTwoTriangleOpen(local_positions, other_triangle, edge, triangle_normal)) {
                        ++open_count;
                    } else {
                        ++close_count;
                    }

                    used_triangles.insert(other_index);
                    triangle_queue.push(other_index);
                }
            }
        }

        if (close_count > open_count) {
            for (int triangle_index : layer) {
                if (triangle_index < 0 || triangle_index >= triangle_count) {
                    continue;
                }
                triangles[triangle_index] = FlipTriangle(triangles[triangle_index]);
                triangle_normals[static_cast<std::size_t>(triangle_index)] =
                    Scale(triangle_normals[static_cast<std::size_t>(triangle_index)], -1.0f);
            }
        }
    }
}

template <typename T>
inline void CopyTransformArray(ExNativeArray<T>& array, const std::vector<int>& indices)
{
    const std::vector<T> old_data = array.Data();
    array.Dispose();
    if (indices.empty()) {
        return;
    }
    array = ExNativeArray<T>(static_cast<int>(indices.size()));
    array.AddRange(static_cast<int>(indices.size()));
    for (std::size_t index = 0; index < indices.size(); ++index) {
        const int old_index = indices[index];
        array[static_cast<int>(index)] =
            old_index >= 0 && old_index < static_cast<int>(old_data.size())
                ? old_data[static_cast<std::size_t>(old_index)]
                : T{};
    }
}

inline void OrganizeReductionTransform(VirtualMesh& mesh, ReductionWorkData& work_data)
{
    std::vector<int> old_to_new_indices;
    old_to_new_indices.reserve(static_cast<std::size_t>(work_data.new_skin_bone_count + 2));

    std::vector<std::pair<int, int>> skin_bone_remap(
        work_data.use_skin_bone_map.begin(),
        work_data.use_skin_bone_map.end()
    );
    std::sort(
        skin_bone_remap.begin(),
        skin_bone_remap.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        }
    );

    work_data.new_skin_bone_transform_indices.clear();
    work_data.new_skin_bone_transform_indices.reserve(skin_bone_remap.size());
    for (const auto& old_to_new : skin_bone_remap) {
        const int old_skin_bone_index = old_to_new.first;
        const int transform_index =
            old_skin_bone_index >= 0 && old_skin_bone_index < mesh.skin_bone_transform_indices.Count()
                ? mesh.skin_bone_transform_indices[old_skin_bone_index]
                : -1;
        work_data.new_skin_bone_transform_indices.push_back(old_to_new.second);
        old_to_new_indices.push_back(transform_index);
    }

    const int new_skin_root_index = static_cast<int>(old_to_new_indices.size());
    old_to_new_indices.push_back(mesh.skin_root_index);
    const int new_center_transform_index = static_cast<int>(old_to_new_indices.size());
    old_to_new_indices.push_back(mesh.center_transform_index);

    TransformData& transform_data = mesh.transform_data;
    const std::vector<int> old_ids = transform_data.id_array;
    const std::vector<int> old_parent_ids = transform_data.parent_id_array;
    const std::vector<std::string> old_names = transform_data.name_array;
    CopyTransformArray(transform_data.flag_array, old_to_new_indices);
    CopyTransformArray(transform_data.init_local_position_array, old_to_new_indices);
    CopyTransformArray(transform_data.init_local_rotation_array, old_to_new_indices);
    CopyTransformArray(transform_data.position_array, old_to_new_indices);
    CopyTransformArray(transform_data.rotation_array, old_to_new_indices);
    CopyTransformArray(transform_data.inverse_rotation_array, old_to_new_indices);
    CopyTransformArray(transform_data.scale_array, old_to_new_indices);
    CopyTransformArray(transform_data.local_position_array, old_to_new_indices);
    CopyTransformArray(transform_data.local_rotation_array, old_to_new_indices);
    CopyTransformArray(transform_data.local_to_world_matrix_array, old_to_new_indices);
    CopyTransformArray(transform_data.team_id_array, old_to_new_indices);

    transform_data.id_array.assign(old_to_new_indices.size(), 0);
    transform_data.parent_id_array.assign(old_to_new_indices.size(), 0);
    transform_data.name_array.assign(old_to_new_indices.size(), std::string{});
    for (std::size_t index = 0; index < old_to_new_indices.size(); ++index) {
        const int old_index = old_to_new_indices[index];
        if (old_index < 0) {
            continue;
        }
        const std::size_t source_index = static_cast<std::size_t>(old_index);
        if (source_index < old_ids.size()) {
            transform_data.id_array[index] = old_ids[source_index];
        }
        if (source_index < old_parent_ids.size()) {
            transform_data.parent_id_array[index] = old_parent_ids[source_index];
        }
        if (source_index < old_names.size()) {
            transform_data.name_array[index] = old_names[source_index];
        }
    }

    mesh.skin_root_index = new_skin_root_index;
    mesh.center_transform_index = new_center_transform_index;
    transform_data.is_dirty = true;
}

template <typename T>
inline T ExSimpleValueOrDefault(const ExSimpleNativeArray<T>& array, int index, const T& fallback = T{})
{
    return index >= 0 && index < array.Count() ? array[index] : fallback;
}

template <typename T>
inline T ExNativeValueOrDefault(const ExNativeArray<T>& array, int index, const T& fallback = T{})
{
    return index >= 0 && index < array.Count() ? array[index] : fallback;
}

}  // namespace hocloth::mc2
