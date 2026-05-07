#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/reduction/reduction_settings.hpp"
#include "hocloth/reduction/reduction_work_data.hpp"
#include "hocloth/reduction/same_distance_reduction.hpp"
#include "hocloth/reduction/shape_distance_reduction.hpp"
#include "hocloth/reduction/simple_distance_reduction.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/utility/data/multi_data_builder.hpp"
#include "hocloth/utility/grid/grid_map.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/utility/native_collection/ex_cost_sorted_list1.hpp"
#include "hocloth/utility/native_collection/ex_cost_sorted_list4.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace hocloth::mc2 {

namespace {

constexpr std::uint8_t BaseLineFlagIncludeLine = 0x01;

float SafeDepth(float length, float max_length)
{
    return max_length > define::system::Epsilon ? Clamp01(length / max_length) : 0.0f;
}

bool IsValidVertexIndex(int index, int vertex_count)
{
    return index >= 0 && index < vertex_count && index <= std::numeric_limits<std::uint16_t>::max();
}

float3 LocalNormalOrDefault(const VirtualMesh& mesh, int vertex_index)
{
    return vertex_index >= 0 && vertex_index < mesh.local_normals.Count()
        ? mesh.local_normals[vertex_index]
        : float3{0.0f, 1.0f, 0.0f};
}

float3 LocalTangentOrDefault(const VirtualMesh& mesh, int vertex_index)
{
    return vertex_index >= 0 && vertex_index < mesh.local_tangents.Count()
        ? mesh.local_tangents[vertex_index]
        : float3{0.0f, 0.0f, 1.0f};
}

std::uint64_t PackedTriangleKey(const int3& triangle)
{
    const int3 packed = data::PackInt3(triangle);
    return (static_cast<std::uint64_t>(packed.x) << 32)
        | (static_cast<std::uint64_t>(packed.y) << 16)
        | static_cast<std::uint64_t>(packed.z);
}

std::uint64_t PackedEdgeKey(const int2& edge)
{
    const int2 packed = data::PackInt2(edge);
    return data::Pack32(packed.x, packed.y);
}

void UniqueAdd(
    std::unordered_map<std::uint16_t, std::vector<std::uint16_t>>& map,
    std::uint16_t key,
    std::uint16_t value
)
{
    std::vector<std::uint16_t>& values = map[key];
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

void UniqueAdd(
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

float2 SphereMappingUV(const float3& position, const AABB& bounds, int index)
{
    const float3 direction = Normalize(Subtract(position, Center(bounds)));
    constexpr float pi = 3.14159265358979323846f;
    const float u = Clamp((std::atan2(direction.x, direction.z) + pi) / (pi * 2.0f), 0.0f, 1.0f);
    const float v = Clamp((1.0f - direction.y) * 0.5f, 0.0f, 1.0f);
    const float add = static_cast<float>(index) * 0.0001234f;
    return float2{v * 10.0f + add, u * 10.0f + add};
}

template <typename T>
void CopyTransformArray(ExNativeArray<T>& array, const std::vector<int>& indices)
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

void OrganizeReductionTransform(VirtualMesh& mesh, ReductionWorkData& work_data)
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
        work_data.new_skin_bone_transform_indices.push_back(static_cast<int>(old_to_new_indices.size()));
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

}  // namespace

void VirtualMesh::CreateProxyFixedListAndAABB()
{
    // Ported from Magica Cloth 2: ProxyCreateFixedListAndAABB()
    const int vertex_count = VertexCount();
    center_fixed_list.Dispose();
    bounding_box = AABB{};
    average_vertex_distance = 0.0f;
    max_vertex_distance = 0.0f;

    if (vertex_count <= 0
        || attributes.Count() < vertex_count
        || local_positions.Count() < vertex_count) {
        return;
    }

    float total_distance = 0.0f;
    int distance_count = 0;
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const VertexAttribute attr = attributes[vertex_index];
        if (attr.IsInvalid()) {
            continue;
        }

        Encapsulate(bounding_box, local_positions[vertex_index]);
        if (attr.IsFixed() && IsValidVertexIndex(vertex_index, vertex_count)) {
            center_fixed_list.Add(static_cast<std::uint16_t>(vertex_index));
        }
    }

    const int edge_count = edges.Count();
    for (int edge_index = 0; edge_index < edge_count; ++edge_index) {
        const int2 edge = edges[edge_index];
        if (edge.x < 0 || edge.y < 0 || edge.x >= vertex_count || edge.y >= vertex_count) {
            continue;
        }
        if (attributes[edge.x].IsInvalid() || attributes[edge.y].IsInvalid()) {
            continue;
        }

        const float length = Distance(local_positions[edge.x], local_positions[edge.y]);
        total_distance += length;
        max_vertex_distance = std::max(max_vertex_distance, length);
        ++distance_count;
    }

    average_vertex_distance = distance_count > 0
        ? total_distance / static_cast<float>(distance_count)
        : 0.0f;
}

void VirtualMesh::CreateVertexBindPose()
{
    // Ported from Magica Cloth 2: Proxy_CalcVertexBindPoseJob2
    const int vertex_count = VertexCount();
    vertex_bind_pose_positions.Dispose();
    vertex_bind_pose_rotations.Dispose();
    if (vertex_count <= 0 || local_positions.Count() < vertex_count) {
        return;
    }

    vertex_bind_pose_positions.AddRange(vertex_count, float3{});
    vertex_bind_pose_rotations.AddRange(vertex_count, quaternion{});
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const float3 position = local_positions[vertex_index];
        const quaternion rotation = ToRotation(
            LocalNormalOrDefault(*this, vertex_index),
            LocalTangentOrDefault(*this, vertex_index)
        );
        vertex_bind_pose_positions[vertex_index] = float3{-position.x, -position.y, -position.z};
        vertex_bind_pose_rotations[vertex_index] = Inverse(rotation);
    }
}

void VirtualMesh::CreateVertexToTransformRotations()
{
    // Ported from Magica Cloth 2: Proxy_CalcVertexToTransformJob
    const int vertex_count = VertexCount();
    vertex_to_transform_rotations.Dispose();
    if (vertex_count <= 0
        || local_positions.Count() < vertex_count
        || transform_data.rotation_array.Count() < vertex_count) {
        return;
    }

    vertex_to_transform_rotations.AddRange(vertex_count, quaternion{});
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const quaternion transform_local_rotation =
            Multiply(init_inverse_rotation, transform_data.rotation_array[vertex_index]);
        const quaternion vertex_local_rotation = ToRotation(
            LocalNormalOrDefault(*this, vertex_index),
            LocalTangentOrDefault(*this, vertex_index)
        );
        vertex_to_transform_rotations[vertex_index] =
            Multiply(Inverse(vertex_local_rotation), transform_local_rotation);
    }
}

void VirtualMesh::BuildVertexToTriangles()
{
    // Ported from MC2 Proxy_CalcVertexToTriangleJob + Proxy_OrganizeVertexToTrianglsJob.
    const int vertex_count = VertexCount();
    const int triangle_count = TriangleCount();
    vertex_to_triangles.Dispose();
    if (vertex_count <= 0 || triangle_count <= 0) {
        return;
    }

    vertex_to_triangles.AddRange(vertex_count, VertexTriangleList{});

    std::vector<float3> triangle_normals(static_cast<std::size_t>(triangle_count));
    std::vector<float3> triangle_tangents(static_cast<std::size_t>(triangle_count));
    for (int triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
        const int3 triangle = triangles[triangle_index];
        if (!IsValidVertexIndex(triangle.x, vertex_count)
            || !IsValidVertexIndex(triangle.y, vertex_count)
            || !IsValidVertexIndex(triangle.z, vertex_count)) {
            continue;
        }

        const float3 p0 = local_positions[triangle.x];
        const float3 p1 = local_positions[triangle.y];
        const float3 p2 = local_positions[triangle.z];
        triangle_normals[static_cast<std::size_t>(triangle_index)] = TriangleNormal(p0, p1, p2);
        const float2 uv0 = triangle.x < uv.Count() ? uv[triangle.x] : float2{};
        const float2 uv1 = triangle.y < uv.Count() ? uv[triangle.y] : float2{};
        const float2 uv2 = triangle.z < uv.Count() ? uv[triangle.z] : float2{};
        triangle_tangents[static_cast<std::size_t>(triangle_index)] =
            TriangleTangent(p0, p1, p2, uv0, uv1, uv2);

        vertex_to_triangles[triangle.x].Set(static_cast<std::uint32_t>(triangle_index));
        vertex_to_triangles[triangle.y].Set(static_cast<std::uint32_t>(triangle_index));
        vertex_to_triangles[triangle.z].Set(static_cast<std::uint32_t>(triangle_index));
    }

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        VertexTriangleList triangle_list = vertex_to_triangles[vertex_index];
        const int count = triangle_list.Length();
        if (count <= 0) {
            continue;
        }

        if (vertex_index < attributes.Count()) {
            VertexAttribute attribute = attributes[vertex_index];
            attribute.Set(VertexAttribute::FlagTriangle, true);
            attributes[vertex_index] = attribute;
        }

        float3 final_normal{};
        float3 final_tangent{};
        for (int index = 0; index < count; ++index) {
            const int triangle_index = static_cast<int>(triangle_list[index]);
            if (triangle_index < 0 || triangle_index >= triangle_count) {
                continue;
            }
            final_normal = Add(
                final_normal,
                triangle_normals[static_cast<std::size_t>(triangle_index)]
            );
            final_tangent = Add(
                final_tangent,
                triangle_tangents[static_cast<std::size_t>(triangle_index)]
            );
        }

        if (Length(final_normal) < 0.5f) {
            float max_distance = -1.0f;
            final_normal = float3{};
            for (int index = 0; index < count; ++index) {
                const int candidate_index = static_cast<int>(triangle_list[index]);
                if (candidate_index < 0 || candidate_index >= triangle_count) {
                    continue;
                }
                const float3 candidate_normal =
                    triangle_normals[static_cast<std::size_t>(candidate_index)];
                float3 normal_sum{};
                for (int other_index = 0; other_index < count; ++other_index) {
                    const int triangle_index = static_cast<int>(triangle_list[other_index]);
                    if (triangle_index < 0
                        || triangle_index >= triangle_count
                        || triangle_index == candidate_index) {
                        continue;
                    }
                    const float3 normal =
                        triangle_normals[static_cast<std::size_t>(triangle_index)];
                    normal_sum = Add(
                        normal_sum,
                        Dot(candidate_normal, normal) >= 0.0f ? normal : Scale(normal, -1.0f)
                    );
                }
                const float distance = LengthSquared(normal_sum);
                if (distance > max_distance) {
                    max_distance = distance;
                    final_normal = candidate_normal;
                }
            }
        } else {
            final_normal = Normalize(final_normal);
        }

        if (Length(final_tangent) < 0.5f) {
            float max_distance = -1.0f;
            final_tangent = float3{};
            for (int index = 0; index < count; ++index) {
                const int candidate_index = static_cast<int>(triangle_list[index]);
                if (candidate_index < 0 || candidate_index >= triangle_count) {
                    continue;
                }
                const float3 candidate_tangent =
                    triangle_tangents[static_cast<std::size_t>(candidate_index)];
                float3 tangent_sum{};
                for (int other_index = 0; other_index < count; ++other_index) {
                    const int triangle_index = static_cast<int>(triangle_list[other_index]);
                    if (triangle_index < 0
                        || triangle_index >= triangle_count
                        || triangle_index == candidate_index) {
                        continue;
                    }
                    const float3 tangent =
                        triangle_tangents[static_cast<std::size_t>(triangle_index)];
                    tangent_sum = Add(
                        tangent_sum,
                        Dot(candidate_tangent, tangent) >= 0.0f
                            ? tangent
                            : Scale(tangent, -1.0f)
                    );
                }
                const float distance = LengthSquared(tangent_sum);
                if (distance > max_distance) {
                    max_distance = distance;
                    final_tangent = candidate_tangent;
                }
            }
        } else {
            final_tangent = Normalize(final_tangent);
        }

        for (int index = 0; index < count; ++index) {
            const int triangle_index = static_cast<int>(triangle_list[index]);
            if (triangle_index < 0 || triangle_index >= triangle_count) {
                continue;
            }

            int flip_flag = 0;
            if (Dot(final_normal, triangle_normals[static_cast<std::size_t>(triangle_index)]) < 0.0f) {
                flip_flag |= 0x1;
            }
            if (Dot(final_tangent, triangle_tangents[static_cast<std::size_t>(triangle_index)]) < 0.0f) {
                flip_flag |= 0x2;
            }
            triangle_list[index] = data::Pack12_20(flip_flag, triangle_index);
        }
        vertex_to_triangles[vertex_index] = triangle_list;
    }
}

void VirtualMesh::BuildEdgeToTriangles()
{
    // Ported from MC2 Proxy_CalcEdgeToTriangleJob.
    edge_to_triangles.clear();
    const int vertex_count = VertexCount();
    const int triangle_count = TriangleCount();
    if (vertex_count <= 0 || triangle_count <= 0) {
        return;
    }

    edge_to_triangles.reserve(static_cast<std::size_t>(triangle_count * 3));
    for (int triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
        const int3 triangle = triangles[triangle_index];
        if (!IsValidVertexIndex(triangle.x, vertex_count)
            || !IsValidVertexIndex(triangle.y, vertex_count)
            || !IsValidVertexIndex(triangle.z, vertex_count)) {
            continue;
        }

        const int2 edges_to_add[3] = {
            data::PackInt2(triangle.x, triangle.y),
            data::PackInt2(triangle.y, triangle.z),
            data::PackInt2(triangle.z, triangle.x),
        };
        for (const int2& edge : edges_to_add) {
            UniqueAdd(
                edge_to_triangles,
                data::Pack32(edge.x, edge.y),
                static_cast<std::uint16_t>(triangle_index)
            );
        }
    }
}

void VirtualMesh::BuildEdgeFlags()
{
    // Ported from MC2 Proxy_CreateEdgeFlagJob.
    edge_flags.Dispose();
    const int edge_count = edges.Count();
    if (edge_count <= 0) {
        return;
    }

    if (edge_to_triangles.empty() && TriangleCount() > 0) {
        BuildEdgeToTriangles();
    }

    edge_flags.AddRange(edge_count, BitFlag8{});
    for (int edge_index = 0; edge_index < edge_count; ++edge_index) {
        const int2 edge = data::PackInt2(edges[edge_index]);
        const auto found = edge_to_triangles.find(data::Pack32(edge.x, edge.y));
        if (found == edge_to_triangles.end()) {
            continue;
        }

        BitFlag8 flag;
        if (found->second.size() <= 1) {
            flag.SetFlag(EdgeFlagCut, true);
        }
        edge_flags[edge_index] = flag;
    }
}

void VirtualMesh::ConvertInvalidToFixed()
{
    // Ported from MC2 Proxy_ConvertInvalidToFixedJob.
    const int vertex_count = VertexCount();
    if (vertex_count <= 0
        || attributes.Count() < vertex_count
        || vertex_to_vertex_index_array.Count() < vertex_count) {
        return;
    }

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        VertexAttribute attribute = attributes[vertex_index];
        if (!attribute.IsInvalid()) {
            continue;
        }

        int data_count = 0;
        int data_start = 0;
        data::Unpack12_20(vertex_to_vertex_index_array[vertex_index], data_count, data_start);
        for (int offset = 0; offset < data_count; ++offset) {
            const int data_index = data_start + offset;
            if (data_index < 0 || data_index >= vertex_to_vertex_data_array.Count()) {
                continue;
            }
            const int connected_vertex = vertex_to_vertex_data_array[data_index];
            if (connected_vertex < 0 || connected_vertex >= attributes.Count()) {
                continue;
            }
            if (!attributes[connected_vertex].IsMove()) {
                continue;
            }

            attribute.Set(VertexAttribute::FlagInvalidMotion, true);
            attribute.Set(VertexAttribute::FlagFixed, true);
            attributes[vertex_index] = attribute;
            break;
        }
    }
}

void VirtualMesh::ApplySelectionAttribute(const SelectionData& selection_data)
{
    // Ported from MC2 Proxy_ApplySelectionJob + Proxy_BoneClothApplayTransformFlagJob.
    if (!selection_data.IsValid() || VertexCount() <= 0 || attributes.Count() < VertexCount()) {
        return;
    }

    float search_radius = std::max(average_vertex_distance, selection_data.max_connection_distance);
    search_radius = std::max(search_radius, define::system::MinimumGridSize);
    const float grid_size = search_radius * 1.5f;

    GridMap<int> grid_map(selection_data.Count());
    auto& map = grid_map.GetMap();
    for (int selection_index = 0; selection_index < selection_data.Count(); ++selection_index) {
        GridMap<int>::AddGrid(
            selection_data.positions[static_cast<std::size_t>(selection_index)],
            selection_index,
            map,
            grid_size
        );
    }

    for (int vertex_index = 0; vertex_index < VertexCount(); ++vertex_index) {
        if (vertex_index >= local_positions.Count()) {
            break;
        }

        const float3 position = local_positions[vertex_index];
        float min_distance = std::numeric_limits<float>::max();
        VertexAttribute nearest_attribute = VertexAttribute::Invalid();
        for (const int3& grid : GridMap<int>::GetArea(position, search_radius, grid_size)) {
            const auto found = map.find(grid);
            if (found == map.end()) {
                continue;
            }
            for (int selection_index : found->second) {
                if (selection_index < 0 || selection_index >= selection_data.Count()) {
                    continue;
                }
                const float distance = Distance(
                    position,
                    selection_data.positions[static_cast<std::size_t>(selection_index)]
                );
                if (distance > search_radius || distance > min_distance) {
                    continue;
                }
                min_distance = distance;
                nearest_attribute =
                    selection_data.attributes[static_cast<std::size_t>(selection_index)];
            }
        }

        VertexAttribute attribute = attributes[vertex_index];
        attribute.Set(nearest_attribute, true);
        attributes[vertex_index] = attribute;
    }

    if (!is_bone_cloth || transform_data.flag_array.Count() < VertexCount()) {
        return;
    }
    for (int vertex_index = 0; vertex_index < VertexCount(); ++vertex_index) {
        const VertexAttribute attribute = attributes[vertex_index];
        BitFlag8 flag = transform_data.flag_array[vertex_index];
        if (attribute.IsMove()) {
            flag.SetFlag(TransformManager::FlagLocalPosRotWrite, true);
        } else if (attribute.IsFixed()) {
            flag.SetFlag(TransformManager::FlagWorldRotWrite, true);
        }
        if (!attribute.IsInvalid()) {
            flag.SetFlag(TransformManager::FlagRestore, true);
        }
        transform_data.flag_array[vertex_index] = flag;
    }
}

void VirtualMesh::BuildMeshBaseLinesFromEdges()
{
    // Ported from Magica Cloth 2: Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs
    const int vertex_count = VertexCount();
    vertex_parent_indices.Dispose();
    center_fixed_list.Dispose();
    vertex_child_index_array.Dispose();
    vertex_child_data_array.Dispose();
    base_line_flags.Dispose();
    base_line_start_data_indices.Dispose();
    base_line_data_counts.Dispose();
    base_line_data.Dispose();
    vertex_local_positions.Dispose();
    vertex_local_rotations.Dispose();
    vertex_root_indices.Dispose();
    vertex_depths.Dispose();

    if (vertex_count <= 0
        || attributes.Count() < vertex_count
        || local_positions.Count() < vertex_count) {
        return;
    }

    std::vector<std::vector<int>> adjacency(static_cast<std::size_t>(vertex_count));
    const int edge_count = edges.Count();
    for (int edge_index = 0; edge_index < edge_count; ++edge_index) {
        const int2 edge = edges[edge_index];
        if (edge.x < 0 || edge.y < 0 || edge.x >= vertex_count || edge.y >= vertex_count) {
            continue;
        }
        adjacency[static_cast<std::size_t>(edge.x)].push_back(edge.y);
        adjacency[static_cast<std::size_t>(edge.y)].push_back(edge.x);
    }

    std::vector<int> fixed_vertices;
    fixed_vertices.reserve(static_cast<std::size_t>(vertex_count));
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        if (attributes[vertex_index].IsFixed()) {
            fixed_vertices.push_back(vertex_index);
        }
    }
    if (fixed_vertices.empty()) {
        vertex_parent_indices.AddRange(vertex_count, -1);
        vertex_child_index_array.AddRange(vertex_count, std::uint32_t{});
        return;
    }

    std::vector<std::uint16_t> fixed_vertex_indices;
    fixed_vertex_indices.reserve(fixed_vertices.size());
    for (int vertex_index : fixed_vertices) {
        if (IsValidVertexIndex(vertex_index, vertex_count)) {
            fixed_vertex_indices.push_back(static_cast<std::uint16_t>(vertex_index));
        }
    }
    center_fixed_list.AddRange(fixed_vertex_indices);
    vertex_parent_indices.AddRange(vertex_count, -1);
    std::vector<std::uint8_t> mark(static_cast<std::size_t>(vertex_count), 0);
    std::vector<int> current;
    current.reserve(fixed_vertices.size());
    for (int vertex_index : fixed_vertices) {
        current.push_back(vertex_index);
        mark[static_cast<std::size_t>(vertex_index)] = 1;
    }

    while (!current.empty()) {
        std::sort(
            current.begin(),
            current.end(),
            [this](int a, int b) {
                return local_positions[a].x < local_positions[b].x;
            }
        );

        for (int vertex_index : current) {
            if (vertex_index < 0 || vertex_index >= vertex_count) {
                continue;
            }
            if (attributes[vertex_index].IsDontMove()) {
                continue;
            }

            ExCostSortedList1 cost{-1.0f, -1};
            const float3 position = local_positions[vertex_index];
            for (int target : adjacency[static_cast<std::size_t>(vertex_index)]) {
                if (target < 0 || target >= vertex_count || mark[static_cast<std::size_t>(target)] == 0) {
                    continue;
                }

                const float3 target_position = local_positions[target];
                if (attributes[target].IsDontMove()) {
                    cost.Add(Distance(position, target_position), target);
                } else {
                    const int parent_index = vertex_parent_indices[target];
                    if (parent_index < 0 || parent_index >= vertex_count) {
                        continue;
                    }
                    const float angle = Angle(
                        Subtract(target_position, position),
                        Subtract(local_positions[parent_index], target_position)
                    );
                    cost.Add(angle, target);
                }
            }

            if (cost.IsValid()) {
                vertex_parent_indices[vertex_index] = cost.Data();
                mark[static_cast<std::size_t>(vertex_index)] = 1;
            }
        }

        for (int vertex_index : current) {
            if (vertex_index >= 0 && vertex_index < vertex_count) {
                mark[static_cast<std::size_t>(vertex_index)] = 2;
            }
        }

        std::vector<float> best_distance(static_cast<std::size_t>(vertex_count), std::numeric_limits<float>::max());
        std::vector<int> next;
        for (int vertex_index : current) {
            if (vertex_index < 0 || vertex_index >= vertex_count) {
                continue;
            }
            const float3 position = local_positions[vertex_index];
            for (int target : adjacency[static_cast<std::size_t>(vertex_index)]) {
                if (target < 0
                    || target >= vertex_count
                    || attributes[target].IsInvalid()
                    || mark[static_cast<std::size_t>(target)] != 0) {
                    continue;
                }

                const float distance = Distance(position, local_positions[target]);
                if (distance < best_distance[static_cast<std::size_t>(target)]) {
                    if (best_distance[static_cast<std::size_t>(target)]
                        == std::numeric_limits<float>::max()) {
                        next.push_back(target);
                    }
                    best_distance[static_cast<std::size_t>(target)] = distance;
                }
            }
        }

        std::sort(
            next.begin(),
            next.end(),
            [&best_distance](int a, int b) {
                return best_distance[static_cast<std::size_t>(a)]
                    < best_distance[static_cast<std::size_t>(b)];
            }
        );
        current = std::move(next);
    }

    BuildBaseLinesFromParents();
}

void VirtualMesh::BuildTransformBaseLines()
{
    // Ported from Magica Cloth 2: CreateTransformBaseLine()
    const int vertex_count = VertexCount();
    vertex_parent_indices.Dispose();
    center_fixed_list.Dispose();
    vertex_child_index_array.Dispose();
    vertex_child_data_array.Dispose();
    base_line_flags.Dispose();
    base_line_start_data_indices.Dispose();
    base_line_data_counts.Dispose();
    base_line_data.Dispose();
    vertex_local_positions.Dispose();
    vertex_local_rotations.Dispose();
    vertex_root_indices.Dispose();
    vertex_depths.Dispose();

    if (vertex_count <= 0
        || attributes.Count() < vertex_count
        || local_positions.Count() < vertex_count
        || transform_data.id_array.size() < static_cast<std::size_t>(vertex_count)
        || transform_data.parent_id_array.size() < static_cast<std::size_t>(vertex_count)) {
        return;
    }

    std::unordered_map<int, int> id_to_index;
    id_to_index.reserve(static_cast<std::size_t>(vertex_count));
    for (int index = 0; index < vertex_count; ++index) {
        id_to_index[transform_data.id_array[static_cast<std::size_t>(index)]] = index;
    }

    vertex_parent_indices.AddRange(vertex_count, -1);
    for (int index = 0; index < vertex_count; ++index) {
        const int parent_id = transform_data.parent_id_array[static_cast<std::size_t>(index)];
        const auto found = id_to_index.find(parent_id);
        vertex_parent_indices[index] = found != id_to_index.end() ? found->second : -1;
    }

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        if (attributes[vertex_index].IsFixed() && IsValidVertexIndex(vertex_index, vertex_count)) {
            center_fixed_list.Add(static_cast<std::uint16_t>(vertex_index));
        }
    }

    BuildBaseLinesFromParents();
}

void VirtualMesh::CalcAverageAndMaxVertexDistanceRun()
{
    // Port target: Scripts/Core/VirtualMesh/Function/VirtualMeshWork.cs
    average_vertex_distance = 0.0f;
    max_vertex_distance = 0.0f;

    const int vertex_count = VertexCount();
    if (vertex_count <= 0 || local_positions.Count() < vertex_count) {
        return;
    }

    float sum_squared_length = 0.0f;
    float max_squared_length = 0.0f;
    int count = 0;

    const int triangle_count = TriangleCount();
    if (triangle_count > 0) {
        const int step = std::max(triangle_count / 100, 1);
        for (int triangle_index = 0; triangle_index < triangle_count; triangle_index += step) {
            const int3 triangle = triangles[triangle_index];
            if (triangle.x < 0
                || triangle.y < 0
                || triangle.z < 0
                || triangle.x >= vertex_count
                || triangle.y >= vertex_count
                || triangle.z >= vertex_count) {
                continue;
            }
            const float3 p0 = local_positions[triangle.x];
            const float3 p1 = local_positions[triangle.y];
            const float3 p2 = local_positions[triangle.z];
            const float squared_lengths[3] = {
                LengthSquared(Subtract(p0, p1)),
                LengthSquared(Subtract(p1, p2)),
                LengthSquared(Subtract(p2, p0)),
            };
            for (float squared_length : squared_lengths) {
                sum_squared_length += squared_length;
                max_squared_length = std::max(max_squared_length, squared_length);
                ++count;
            }
        }
    }

    const int line_count = LineCount();
    if (line_count > 0) {
        const int step = std::max(line_count / 100, 1);
        for (int line_index = 0; line_index < line_count; line_index += step) {
            const int2 line = lines[line_index];
            if (line.x < 0
                || line.y < 0
                || line.x >= vertex_count
                || line.y >= vertex_count) {
                continue;
            }
            const float squared_length =
                LengthSquared(Subtract(local_positions[line.x], local_positions[line.y]));
            sum_squared_length += squared_length;
            max_squared_length = std::max(max_squared_length, squared_length);
            ++count;
        }
    }

    if (count > 0) {
        average_vertex_distance =
            std::sqrt(sum_squared_length / static_cast<float>(count));
        max_vertex_distance = std::sqrt(max_squared_length);
    }
}

GridMap<int> VirtualMesh::CreateVertexIndexGridMapRun(float grid_size) const
{
    // Ported from MC2 VirtualMeshWork.CreateVertexIndexGridMapRun().
    GridMap<int> grid_map(VertexCount());
    auto& map = grid_map.GetMap();
    if (grid_size <= define::system::Epsilon) {
        return grid_map;
    }

    const int vertex_count = VertexCount();
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        if (vertex_index >= local_positions.Count()) {
            break;
        }
        GridMap<int>::AddGrid(local_positions[vertex_index], vertex_index, map, grid_size);
    }
    return grid_map;
}

VirtualMeshRaycastHit VirtualMesh::IntersectRayMesh(
    const float3& ray_position,
    const float3& ray_direction,
    bool double_side,
    float point_radius
) const
{
    // Ported from MC2 VirtualMeshWork.IntersectRayMesh(). Native callers pass local mesh-space rays.
    std::vector<VirtualMeshRaycastHit> hits;
    hits.reserve(100);

    const float3 local_ray_position = ray_position;
    const float3 local_ray_direction = Normalize(ray_direction, float3{0.0f, 0.0f, 1.0f});
    const float3 local_ray_end_position =
        Add(local_ray_position, Scale(local_ray_direction, 1000.0f));
    const float local_point_radius = std::max(point_radius, 0.0f);

    const int vertex_count = VertexCount();
    for (int triangle_index = 0; triangle_index < TriangleCount(); ++triangle_index) {
        const int3 triangle = triangles[triangle_index];
        if (!IsValidVertexIndex(triangle.x, vertex_count)
            || !IsValidVertexIndex(triangle.y, vertex_count)
            || !IsValidVertexIndex(triangle.z, vertex_count)) {
            continue;
        }

        const float3 p0 = local_positions[triangle.x];
        const float3 p1 = local_positions[triangle.y];
        const float3 p2 = local_positions[triangle.z];
        float3 sphere_center{};
        float sphere_radius = 0.0f;
        GetTriangleSphere(p0, p1, p2, sphere_center, sphere_radius);

        float sphere_t = 0.0f;
        float3 sphere_hit{};
        if (!IntersectRaySphere(
                local_ray_position,
                local_ray_direction,
                sphere_center,
                sphere_radius,
                sphere_t,
                sphere_hit
            )) {
            continue;
        }

        float u = 0.0f;
        float v = 0.0f;
        float w = 0.0f;
        float t = 0.0f;
        if (!IntersectSegmentTriangle(
                local_ray_position,
                local_ray_end_position,
                p0,
                p1,
                p2,
                double_side,
                u,
                v,
                w,
                t
            )) {
            continue;
        }

        VirtualMeshRaycastHit hit;
        hit.type = VirtualMeshPrimitive::Triangle;
        hit.index = triangle_index;
        hit.position = Lerp(local_ray_position, local_ray_end_position, t);
        hit.normal = TriangleNormal(p0, p1, p2);
        hit.distance = t;
        hits.push_back(hit);
    }

    const EdgeToTrianglesMap* edge_to_triangles_lookup = &edge_to_triangles;
    EdgeToTrianglesMap built_edge_to_triangles;
    if (edge_to_triangles_lookup->empty() && TriangleCount() > 0) {
        const int triangle_count = TriangleCount();
        built_edge_to_triangles.reserve(static_cast<std::size_t>(triangle_count * 3));
        for (int triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
            const int3 triangle = triangles[triangle_index];
            if (!IsValidVertexIndex(triangle.x, vertex_count)
                || !IsValidVertexIndex(triangle.y, vertex_count)
                || !IsValidVertexIndex(triangle.z, vertex_count)) {
                continue;
            }
            const int2 triangle_edges[3] = {
                data::PackInt2(triangle.x, triangle.y),
                data::PackInt2(triangle.y, triangle.z),
                data::PackInt2(triangle.z, triangle.x),
            };
            for (const int2& edge : triangle_edges) {
                UniqueAdd(
                    built_edge_to_triangles,
                    data::Pack32(edge.x, edge.y),
                    static_cast<std::uint16_t>(triangle_index)
                );
            }
        }
        edge_to_triangles_lookup = &built_edge_to_triangles;
    }

    for (int edge_index = 0; edge_index < edges.Count(); ++edge_index) {
        const int2 edge = edges[edge_index];
        if (!IsValidVertexIndex(edge.x, vertex_count)
            || !IsValidVertexIndex(edge.y, vertex_count)) {
            continue;
        }
        const int2 packed_edge = data::PackInt2(edge);
        if (edge_to_triangles_lookup->find(data::Pack32(packed_edge.x, packed_edge.y))
            != edge_to_triangles_lookup->end()) {
            continue;
        }

        const float3 p0 = local_positions[edge.x];
        const float3 p1 = local_positions[edge.y];
        float s = 0.0f;
        float t = 0.0f;
        float3 c1{};
        float3 c2{};
        const float distance_sq = ClosestPtSegmentSegment(
            p0,
            p1,
            local_ray_position,
            local_ray_end_position,
            s,
            t,
            c1,
            c2
        );
        if (std::sqrt(distance_sq) > local_point_radius) {
            continue;
        }

        VirtualMeshRaycastHit hit;
        hit.type = VirtualMeshPrimitive::Edge;
        hit.index = edge_index;
        hit.position = c2;
        hit.normal = Scale(local_ray_direction, -1.0f);
        hit.distance = t;
        hits.push_back(hit);
    }

    if (hits.empty()) {
        return VirtualMeshRaycastHit{};
    }
    return *std::min_element(
        hits.begin(),
        hits.end(),
        [](const VirtualMeshRaycastHit& lhs, const VirtualMeshRaycastHit& rhs) {
            return lhs.distance < rhs.distance;
        }
    );
}

void VirtualMesh::Optimization()
{
    // Port target: Scripts/Core/VirtualMesh/Function/VirtualMeshOptimization.cs
    try {
        RemoveDuplicateTriangles();
    } catch (...) {
        result = Result::Error(ResultCode::Optimize_Exception, "VirtualMesh optimization failed.");
    }
}

void VirtualMesh::RemoveDuplicateTriangles()
{
    if (TriangleCount() < 2 || local_positions.Count() < VertexCount()) {
        return;
    }

    std::unordered_map<std::uint32_t, std::vector<int>> edge_to_triangles;
    edge_to_triangles.reserve(static_cast<std::size_t>(TriangleCount() * 3));
    for (int triangle_index = 0; triangle_index < TriangleCount(); ++triangle_index) {
        const int3 triangle = triangles[triangle_index];
        const int2 triangle_edges[3] = {
            data::PackInt2(triangle.x, triangle.y),
            data::PackInt2(triangle.y, triangle.z),
            data::PackInt2(triangle.z, triangle.x),
        };
        for (const int2& edge : triangle_edges) {
            edge_to_triangles[data::Pack32(edge.x, edge.y)].push_back(triangle_index);
        }
    }

    std::unordered_set<std::uint64_t> used_quad_set;
    std::unordered_set<std::uint64_t> remove_triangle_set;
    for (const auto& key_value : edge_to_triangles) {
        const std::vector<int>& triangle_indices = key_value.second;
        if (triangle_indices.size() < 2) {
            continue;
        }

        const int edge_x = data::Unpack32Hi(key_value.first);
        const int edge_y = data::Unpack32Low(key_value.first);
        if (edge_x < 0
            || edge_y < 0
            || edge_x >= VertexCount()
            || edge_y >= VertexCount()) {
            continue;
        }
        const int2 edge{edge_x, edge_y};
        const float3 px = local_positions[edge.x];
        const float3 py = local_positions[edge.y];

        for (std::size_t i = 0; i + 1 < triangle_indices.size(); ++i) {
            const int3 triangle_a = triangles[triangle_indices[i]];
            const int z = data::RemainingData(triangle_a, edge);
            if (z < 0 || z >= VertexCount()) {
                continue;
            }
            const float3 pz = local_positions[z];

            for (std::size_t j = i + 1; j < triangle_indices.size(); ++j) {
                const int3 triangle_b = triangles[triangle_indices[j]];
                const int w = data::RemainingData(triangle_b, edge);
                if (w < 0 || w >= VertexCount()) {
                    continue;
                }
                const float3 pw = local_positions[w];

                const float angle_degrees = Abs(TriangleAngle(px, py, pz, pw)) * 57.29577951308232f;
                if (angle_degrees > define::system::ProxyMeshTrianglePairAngle) {
                    continue;
                }

                float s = 0.0f;
                float t = 0.0f;
                ClosestPtSegmentSegment2(px, py, pz, pw, s, t);
                if (s == 0.0f || s == 1.0f || t == 0.0f || t == 1.0f) {
                    continue;
                }

                const std::uint64_t quad_key =
                    data::Pack64(data::PackInt4(edge.x, edge.y, z, w));
                if (used_quad_set.find(quad_key) != used_quad_set.end()) {
                    remove_triangle_set.insert(PackedTriangleKey(triangle_a));
                    remove_triangle_set.insert(PackedTriangleKey(triangle_b));
                } else {
                    used_quad_set.insert(quad_key);
                }
            }
        }
    }

    if (remove_triangle_set.empty()) {
        return;
    }

    std::vector<int3> new_triangles;
    new_triangles.reserve(static_cast<std::size_t>(TriangleCount()));
    for (int triangle_index = 0; triangle_index < TriangleCount(); ++triangle_index) {
        const int3 triangle = triangles[triangle_index];
        if (remove_triangle_set.find(PackedTriangleKey(triangle)) != remove_triangle_set.end()) {
            continue;
        }
        new_triangles.push_back(triangle);
    }

    triangles.Dispose();
    triangles.AddRange(new_triangles);
}

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
    } catch (...) {
        result = Result::Error(ResultCode::Reduction_StoreVirtualMeshError, "VirtualMesh reduction store failed.");
    }
}

void VirtualMesh::Dispose()
{
    reference_indices.Dispose();
    attributes.Dispose();
    local_positions.Dispose();
    local_normals.Dispose();
    local_tangents.Dispose();
    uv.Dispose();
    vertex_to_triangles.Dispose();
    vertex_to_vertex_index_array.Dispose();
    vertex_to_vertex_data_array.Dispose();
    bone_weights.Dispose();
    triangles.Dispose();
    lines.Dispose();
    edges.Dispose();
    edge_flags.Dispose();
    edge_to_triangles.clear();
    skin_bone_transform_indices.Dispose();
    skin_bone_bind_poses.Dispose();
    transform_data.Dispose();
    vertex_child_index_array.Dispose();
    vertex_child_data_array.Dispose();
    vertex_bind_pose_positions.Dispose();
    vertex_bind_pose_rotations.Dispose();
    vertex_depths.Dispose();
    vertex_root_indices.Dispose();
    vertex_parent_indices.Dispose();
    center_fixed_list.Dispose();
    vertex_local_positions.Dispose();
    vertex_local_rotations.Dispose();
    base_line_flags.Dispose();
    base_line_start_data_indices.Dispose();
    base_line_data_counts.Dispose();
    base_line_data.Dispose();
    normal_adjustment_rotations.Dispose();
    vertex_to_transform_rotations.Dispose();
    custom_skinning_bone_indices.clear();
    local_center_position = float3{};
    center_world_position = float3{};
    center_world_rotation = quaternion{};
    center_world_scale = float3{1.0f, 1.0f, 1.0f};
    result = Result::Ok();
    is_managed = false;
    mesh_type = MeshType::NormalMesh;
    is_bone_cloth = false;
    center_transform_index = -1;
    init_local_to_world = float4x4{};
    init_world_to_local = float4x4{};
    init_rotation = quaternion{};
    init_inverse_rotation = quaternion{};
    init_scale = float3{1.0f, 1.0f, 1.0f};
    skin_root_index = -1;
    bounding_box = AABB{};
    average_vertex_distance = 0.0f;
    max_vertex_distance = 0.0f;
    merge_chunk.Clear();
    to_proxy_matrix = float4x4{};
    to_proxy_rotation = quaternion{};
    mapping_id = -1;
}

void VirtualMesh::BuildBaseLinesFromParents()
{
    // Ported from Magica Cloth 2: Scripts/Core/VirtualMesh/Function/VirtualMeshProxy.cs
    const int vertex_count = VertexCount();
    base_line_flags.Dispose();
    base_line_start_data_indices.Dispose();
    base_line_data_counts.Dispose();
    base_line_data.Dispose();
    vertex_child_index_array.Dispose();
    vertex_child_data_array.Dispose();
    vertex_local_positions.Dispose();
    vertex_local_rotations.Dispose();
    vertex_root_indices.Dispose();
    vertex_depths.Dispose();

    if (vertex_count <= 0
        || attributes.Count() < vertex_count
        || local_positions.Count() < vertex_count
        || vertex_parent_indices.Count() < vertex_count) {
        return;
    }

    data::MultiDataBuilder<std::uint16_t> child_builder(vertex_count, vertex_count * 2);
    std::vector<int> roots;
    roots.reserve(static_cast<std::size_t>(vertex_count));
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const int parent_index = vertex_parent_indices[vertex_index];
        if (parent_index >= 0 && parent_index < vertex_count) {
            child_builder.Add(parent_index, static_cast<std::uint16_t>(vertex_index));
        } else {
            roots.push_back(vertex_index);
        }
    }

    std::vector<BitFlag8> line_flags;
    std::vector<std::uint16_t> start_indices;
    std::vector<std::uint16_t> data_counts;
    std::vector<std::uint16_t> indices;
    line_flags.reserve(roots.size());
    start_indices.reserve(roots.size());
    data_counts.reserve(roots.size());
    indices.reserve(static_cast<std::size_t>(vertex_count));

    std::stack<int> root_stack;
    std::stack<int> stack;
    for (int root_index : roots) {
        if (root_index < 0 || root_index >= vertex_count) {
            continue;
        }

        root_stack.push(root_index);
        while (!root_stack.empty()) {
            const int current_root = root_stack.top();
            root_stack.pop();
            const VertexAttribute root_attr = attributes[current_root];
            if (!root_attr.IsDontMove()) {
                continue;
            }

            bool has_move_child = false;
            const int child_count = child_builder.CountValuesForKey(current_root);
            if (child_count <= 0) {
                continue;
            }

            const auto [child_data, child_index] = child_builder.ToArray();
            int packed_count = 0;
            int packed_start = 0;
            data::Unpack12_20(
                child_index[static_cast<std::size_t>(current_root)],
                packed_count,
                packed_start
            );
            for (int index = 0; index < packed_count; ++index) {
                const int child = child_data[static_cast<std::size_t>(packed_start + index)];
                if (child >= 0 && child < vertex_count && attributes[child].IsMove()) {
                    has_move_child = true;
                    break;
                }
            }

            if (!has_move_child) {
                for (int index = 0; index < packed_count; ++index) {
                    const int child = child_data[static_cast<std::size_t>(packed_start + index)];
                    if (child >= 0 && child < vertex_count && attributes[child].IsDontMove()) {
                        root_stack.push(child);
                    }
                }
                continue;
            }

            const std::uint16_t start = static_cast<std::uint16_t>(indices.size());
            std::uint16_t count = 0;
            BitFlag8 line_flag;
            stack.push(current_root);
            while (!stack.empty()) {
                const int vertex_index = stack.top();
                stack.pop();
                if (vertex_index < 0 || vertex_index >= vertex_count) {
                    continue;
                }

                indices.push_back(static_cast<std::uint16_t>(vertex_index));
                ++count;
                if (!attributes[vertex_index].IsSet(VertexAttribute::FlagTriangle)) {
                    line_flag.SetFlag(BaseLineFlagIncludeLine, true);
                }

                int current_count = 0;
                int current_start = 0;
                data::Unpack12_20(
                    child_index[static_cast<std::size_t>(vertex_index)],
                    current_count,
                    current_start
                );
                for (int index = 0; index < current_count; ++index) {
                    const int child = child_data[static_cast<std::size_t>(current_start + index)];
                    if (child >= 0 && child < vertex_count && attributes[child].IsMove()) {
                        stack.push(child);
                    }
                }
            }

            line_flags.push_back(line_flag);
            start_indices.push_back(start);
            data_counts.push_back(count);
        }
    }

    if (!line_flags.empty()) {
        base_line_flags.AddRange(line_flags);
        base_line_start_data_indices.AddRange(start_indices);
        base_line_data_counts.AddRange(data_counts);
        base_line_data.AddRange(indices);
    }

    const auto [child_data, child_index] = child_builder.ToArray();
    vertex_child_data_array.AddRange(child_data);
    vertex_child_index_array.AddRange(child_index);

    vertex_local_positions.AddRange(vertex_count, float3{});
    vertex_local_rotations.AddRange(vertex_count, quaternion{});
    for (int data_index = 0; data_index < base_line_data.Count(); ++data_index) {
        const int vertex_index = base_line_data[data_index];
        const int parent_index = vertex_parent_indices[vertex_index];
        if (parent_index < 0 || parent_index >= vertex_count) {
            vertex_local_positions[vertex_index] = float3{};
            vertex_local_rotations[vertex_index] = quaternion{};
            continue;
        }

        const float3 parent_position = local_positions[parent_index];
        const float3 parent_normal =
            parent_index < local_normals.Count() ? local_normals[parent_index] : float3{0.0f, 1.0f, 0.0f};
        const float3 parent_tangent =
            parent_index < local_tangents.Count() ? local_tangents[parent_index] : float3{0.0f, 0.0f, 1.0f};
        const quaternion parent_rotation = ToRotation(parent_normal, parent_tangent);
        const quaternion inverse_parent_rotation = Inverse(parent_rotation);

        const float3 position = local_positions[vertex_index];
        const float3 normal =
            vertex_index < local_normals.Count() ? local_normals[vertex_index] : float3{0.0f, 1.0f, 0.0f};
        const float3 tangent =
            vertex_index < local_tangents.Count() ? local_tangents[vertex_index] : float3{0.0f, 0.0f, 1.0f};
        const quaternion rotation = ToRotation(normal, tangent);

        vertex_local_positions[vertex_index] =
            Rotate(inverse_parent_rotation, Subtract(position, parent_position));
        vertex_local_rotations[vertex_index] =
            Multiply(inverse_parent_rotation, rotation);
    }

    std::vector<float> root_lengths(static_cast<std::size_t>(vertex_count), 0.0f);
    vertex_root_indices.AddRange(vertex_count, -1);
    vertex_depths.AddRange(vertex_count, 0.0f);
    float max_length = 0.0f;
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        int root_index = -1;
        float root_length = 0.0f;
        int current = vertex_index;
        int guard = 0;
        while (current >= 0 && current < vertex_count && guard++ < vertex_count) {
            const int parent = vertex_parent_indices[current];
            if (parent < 0 || parent >= vertex_count) {
                break;
            }
            root_index = parent;
            root_length += Distance(local_positions[current], local_positions[parent]);
            if (attributes[parent].IsDontMove()) {
                break;
            }
            current = parent;
        }
        vertex_root_indices[vertex_index] = root_index;
        root_lengths[static_cast<std::size_t>(vertex_index)] = root_length;
        max_length = std::max(max_length, root_length);
    }
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        vertex_depths[vertex_index] =
            SafeDepth(root_lengths[static_cast<std::size_t>(vertex_index)], max_length);
    }
}

bool VirtualMesh::CompareSpace(const VirtualMesh& target) const
{
    constexpr float eps = 1.0e-5f;
    return Distance(local_center_position, target.local_center_position) <= eps
        && std::abs(init_scale.x - target.init_scale.x) <= eps
        && std::abs(init_scale.y - target.init_scale.y) <= eps
        && std::abs(init_scale.z - target.init_scale.z) <= eps;
}

float4x4 VirtualMesh::CenterTransformTo(const VirtualMesh& target) const
{
    // Ported from Magica Cloth 2: Scripts/Core/VirtualMesh/Function/VirtualMeshInputOutput.cs
    if (CompareSpace(target)) {
        return float4x4{};
    }
    return Multiply(target.init_world_to_local, init_local_to_world);
}

float4 VirtualMesh::CalcMappingVertexWeights(float4 distances)
{
    // Ported from Magica Cloth 2: Scripts/Core/VirtualMesh/Function/VirtualMeshMapping.cs
    distances.x = std::max(distances.x, 0.0f);
    distances.y = std::max(distances.y, 0.0f);
    distances.z = std::max(distances.z, 0.0f);
    distances.w = std::max(distances.w, 0.0f);

    constexpr float pow_value = 4.0f;
    distances.x = std::pow(distances.x, pow_value);
    distances.y = std::pow(distances.y, pow_value);
    distances.z = std::pow(distances.z, pow_value);
    distances.w = std::pow(distances.w, pow_value);

    const float minimum = distances.x;
    distances.x = distances.x > 0.0f ? minimum / distances.x : 0.0f;
    distances.y = distances.y > 0.0f ? minimum / distances.y : 0.0f;
    distances.z = distances.z > 0.0f ? minimum / distances.z : 0.0f;
    distances.w = distances.w > 0.0f ? minimum / distances.w : 0.0f;

    float sum = distances.x + distances.y + distances.z + distances.w;
    if (sum <= 0.0f) {
        return float4{1.0f, 0.0f, 0.0f, 0.0f};
    }
    distances.x /= sum;
    distances.y /= sum;
    distances.z /= sum;
    distances.w /= sum;

    constexpr float remove_weight = 0.01f;
    if (distances.w < remove_weight) {
        distances.w = 0.0f;
    }
    if (distances.z < remove_weight) {
        distances.z = 0.0f;
    }
    if (distances.y < remove_weight) {
        distances.y = 0.0f;
    }

    sum = distances.x + distances.y + distances.z + distances.w;
    if (sum > 0.0f) {
        distances.x /= sum;
        distances.y /= sum;
        distances.z /= sum;
        distances.w /= sum;
    }
    return distances;
}

void VirtualMesh::DirectMapping(
    VirtualMesh& proxy_mesh,
    const float4x4& to_proxy,
    std::vector<MappingWorkData>& mapping_work_data
)
{
    // Ported from MC2 Mapping_DirectConnectionVertexDataJob.
    const int vertex_count = VertexCount();
    if (!merge_chunk.IsValid() || proxy_mesh.reference_indices.Count() <= 0) {
        return;
    }

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const int join_index = merge_chunk.start_index + vertex_index;
        if (join_index < 0 || join_index >= proxy_mesh.reference_indices.Count()) {
            attributes[vertex_index] = VertexAttribute::Invalid();
            continue;
        }

        const int proxy_vertex_index = proxy_mesh.reference_indices[join_index];
        if (proxy_vertex_index < 0 || proxy_vertex_index >= proxy_mesh.VertexCount()) {
            attributes[vertex_index] = VertexAttribute::Invalid();
            continue;
        }

        const VertexAttribute proxy_attr = proxy_mesh.attributes[proxy_vertex_index];
        if (proxy_attr.IsInvalid()) {
            attributes[vertex_index] = VertexAttribute::Invalid();
            continue;
        }

        const float3 position = TransformPoint(local_positions[vertex_index], to_proxy);
        const float3 proxy_position = proxy_mesh.local_positions[proxy_vertex_index];
        mapping_work_data[static_cast<std::size_t>(vertex_index)] = MappingWorkData{
            position,
            vertex_index,
            proxy_vertex_index,
            Distance(position, proxy_position),
        };
        attributes[vertex_index] = proxy_attr;
    }
}

void VirtualMesh::CalcDirectMappingWeights(
    VirtualMesh& proxy_mesh,
    const std::vector<MappingWorkData>& mapping_work_data,
    float weight_length
)
{
    // Ported from MC2 Mapping_CalcDirectWeightJob.
    weight_length = std::max(weight_length, define::system::Epsilon);
    for (int vertex_index = 0; vertex_index < VertexCount(); ++vertex_index) {
        if (attributes[vertex_index].IsInvalid()) {
            continue;
        }

        const MappingWorkData& work_data = mapping_work_data[static_cast<std::size_t>(vertex_index)];
        const int proxy_vertex_index = work_data.proxy_vertex_index;
        if (proxy_vertex_index < 0 || proxy_vertex_index >= proxy_mesh.VertexCount()) {
            continue;
        }

        ExCostSortedList4 weights{-1.0f};
        std::queue<int> queue;
        std::unordered_set<int> used;
        queue.push(proxy_vertex_index);
        while (!queue.empty()) {
            const int current = queue.front();
            queue.pop();
            if (current < 0 || current >= proxy_mesh.VertexCount() || used.contains(current)) {
                continue;
            }
            used.insert(current);

            float distance = Distance(work_data.position, proxy_mesh.local_positions[current]);
            if (distance > weight_length) {
                continue;
            }

            constexpr float weight_pow = 3.0f;
            float weight = Clamp01(1.0f - distance / weight_length);
            weight = std::pow(weight, weight_pow);
            weights.Add(1.0f - weight, current);

            int data_count = 0;
            int data_start = 0;
            if (current < proxy_mesh.vertex_to_vertex_index_array.Count()) {
                data::Unpack12_20(proxy_mesh.vertex_to_vertex_index_array[current], data_count, data_start);
            }
            for (int offset = 0; offset < data_count; ++offset) {
                const int data_index = data_start + offset;
                if (data_index < 0 || data_index >= proxy_mesh.vertex_to_vertex_data_array.Count()) {
                    continue;
                }
                const int neighbor = proxy_mesh.vertex_to_vertex_data_array[data_index];
                if (used.contains(neighbor)) {
                    continue;
                }
                distance = Distance(work_data.position, proxy_mesh.local_positions[neighbor]);
                if (distance <= weight_length) {
                    queue.push(neighbor);
                }
            }
        }

        if (weights.Count() == 0) {
            weights.Add(1.0f, proxy_vertex_index);
        }

        const int weight_count = weights.Count();
        float4 weight_values{};
        int4 indices{};
        for (int index = 0; index < 4; ++index) {
            if (index < weight_count) {
                weight_values[index] = 1.0f - weights.costs[index];
                indices[index] = weights.data[index];
            }
        }

        const float total = weight_values.x + weight_values.y + weight_values.z + weight_values.w;
        if (total == 0.0f && weight_count > 0) {
            const float uniform = 1.0f / static_cast<float>(weight_count);
            for (int index = 0; index < weight_count; ++index) {
                weight_values[index] = uniform;
            }
        } else if (total > 0.0f) {
            weight_values.x = Clamp01(weight_values.x / total);
            weight_values.y = Clamp01(weight_values.y / total);
            weight_values.z = Clamp01(weight_values.z / total);
            weight_values.w = Clamp01(weight_values.w / total);
        }
        bone_weights[vertex_index] = VirtualMeshBoneWeight(indices, weight_values);
    }
}

void VirtualMesh::SearchMapping(
    VirtualMesh& proxy_mesh,
    const float4x4& to_proxy,
    std::vector<MappingWorkData>& mapping_work_data
)
{
    // Ported from MC2 Mapping_CalcConnectionVertexDataJob.
    float average_distance = TransformLength(average_vertex_distance, to_proxy);
    average_distance = std::max(average_distance, define::system::MinimumGridSize);
    const float search_radius = average_distance * 2.5f;
    const float grid_size = average_distance * 1.5f;

    GridMap<int> grid_map(proxy_mesh.VertexCount());
    auto& map = grid_map.GetMap();
    for (int proxy_index = 0; proxy_index < proxy_mesh.VertexCount(); ++proxy_index) {
        GridMap<int>::AddGrid(proxy_mesh.local_positions[proxy_index], proxy_index, map, grid_size);
    }

    const auto& transform_ids = transform_data.id_array;
    const auto& proxy_transform_ids = proxy_mesh.transform_data.id_array;
    for (int vertex_index = 0; vertex_index < VertexCount(); ++vertex_index) {
        const float3 position = TransformPoint(local_positions[vertex_index], to_proxy);
        const VirtualMeshBoneWeight& bone_weight = bone_weights[vertex_index];
        const int bone_index = bone_weight.bone_indices[0];
        const int bone_id = bone_index >= 0 && bone_index < static_cast<int>(transform_ids.size())
            ? transform_ids[static_cast<std::size_t>(bone_index)]
            : 0;

        ExCostSortedList1 near_vertex{-1.0f};
        ExCostSortedList1 weighted_vertex{-1.0f};
        for (const int3& grid : GridMap<int>::GetArea(position, search_radius, grid_size)) {
            const auto found = map.find(grid);
            if (found == map.end()) {
                continue;
            }
            for (int proxy_index : found->second) {
                const float distance = Distance(position, proxy_mesh.local_positions[proxy_index]);
                if (distance > search_radius) {
                    continue;
                }
                near_vertex.Add(distance, proxy_index);

                const VirtualMeshBoneWeight& proxy_weight = proxy_mesh.bone_weights[proxy_index];
                bool has_bone = false;
                const int count = proxy_weight.Count();
                for (int index = 0; index < count && !has_bone; ++index) {
                    const int proxy_bone_index = proxy_weight.bone_indices[static_cast<std::size_t>(index)];
                    const int proxy_bone_id =
                        proxy_bone_index >= 0 && proxy_bone_index < static_cast<int>(proxy_transform_ids.size())
                            ? proxy_transform_ids[static_cast<std::size_t>(proxy_bone_index)]
                            : 0;
                    has_bone = proxy_bone_id != 0 && proxy_bone_id == bone_id;
                }
                if (has_bone) {
                    weighted_vertex.Add(distance, proxy_index);
                }
            }
        }

        ExCostSortedList1 connection_vertex = near_vertex;
        if (weighted_vertex.IsValid()
            && (!near_vertex.IsValid() || weighted_vertex.Cost() < near_vertex.Cost() * 3.0f)) {
            connection_vertex = weighted_vertex;
        }
        if (!connection_vertex.IsValid()) {
            attributes[vertex_index] = VertexAttribute::Invalid();
            continue;
        }

        const VertexAttribute connection_attr = proxy_mesh.attributes[connection_vertex.Data()];
        if (connection_attr.IsInvalid()) {
            attributes[vertex_index] = VertexAttribute::Invalid();
            continue;
        }

        mapping_work_data[static_cast<std::size_t>(vertex_index)] = MappingWorkData{
            position,
            vertex_index,
            connection_vertex.Data(),
            connection_vertex.Cost(),
        };
        attributes[vertex_index] = connection_attr;
    }
}

void VirtualMesh::CalcSearchMappingWeights(
    VirtualMesh& proxy_mesh,
    const std::vector<MappingWorkData>& mapping_work_data
)
{
    // Ported from MC2 Mapping_CalcWeightJob.
    for (int vertex_index = 0; vertex_index < VertexCount(); ++vertex_index) {
        if (attributes[vertex_index].IsInvalid()) {
            continue;
        }

        const MappingWorkData& work_data = mapping_work_data[static_cast<std::size_t>(vertex_index)];
        const int proxy_index = work_data.proxy_vertex_index;
        if (proxy_index < 0 || proxy_index >= proxy_mesh.VertexCount()) {
            attributes[vertex_index] = VertexAttribute::Invalid();
            continue;
        }

        float3 position = work_data.position;
        const float3 proxy_position = proxy_mesh.local_positions[proxy_index];
        const float3 proxy_normal = proxy_index < proxy_mesh.local_normals.Count()
            ? proxy_mesh.local_normals[proxy_index]
            : float3{0.0f, 1.0f, 0.0f};
        const float3 vector = Subtract(position, proxy_position);
        position = Subtract(position, Project(vector, proxy_normal));
        const float vertex_distance = Distance(position, proxy_position);
        const float weight_radius = vertex_distance * 4.0f;

        ExCostSortedList4 vertex_distances{-1.0f};
        vertex_distances.Add(vertex_distance, proxy_index);
        int count = 0;
        int start = 0;
        if (proxy_index < proxy_mesh.vertex_to_vertex_index_array.Count()) {
            data::Unpack12_20(proxy_mesh.vertex_to_vertex_index_array[proxy_index], count, start);
        }
        for (int offset = 0; offset < count; ++offset) {
            const int data_index = start + offset;
            if (data_index < 0 || data_index >= proxy_mesh.vertex_to_vertex_data_array.Count()) {
                continue;
            }
            const int neighbor = proxy_mesh.vertex_to_vertex_data_array[data_index];
            if (vertex_distances.Contains(neighbor)) {
                continue;
            }

            float distance = Distance(position, proxy_mesh.local_positions[neighbor]);
            if (distance <= weight_radius) {
                vertex_distances.Add(distance, neighbor);
            }

            int count2 = 0;
            int start2 = 0;
            if (neighbor < proxy_mesh.vertex_to_vertex_index_array.Count()) {
                data::Unpack12_20(proxy_mesh.vertex_to_vertex_index_array[neighbor], count2, start2);
            }
            for (int offset2 = 0; offset2 < count2; ++offset2) {
                const int data_index2 = start2 + offset2;
                if (data_index2 < 0 || data_index2 >= proxy_mesh.vertex_to_vertex_data_array.Count()) {
                    continue;
                }
                const int neighbor2 = proxy_mesh.vertex_to_vertex_data_array[data_index2];
                if (neighbor2 == proxy_index
                    || neighbor2 == neighbor
                    || vertex_distances.Contains(neighbor2)) {
                    continue;
                }

                distance = Distance(position, proxy_mesh.local_positions[neighbor2]);
                if (distance <= weight_radius) {
                    vertex_distances.Add(distance, neighbor2);
                }
            }
        }

        const float4 weights = CalcMappingVertexWeights(vertex_distances.costs);
        bone_weights[vertex_index] = VirtualMeshBoneWeight(vertex_distances.data, weights);

        float fixed_value = 0.0f;
        float move_value = 0.0f;
        const VirtualMeshBoneWeight& weight = bone_weights[vertex_index];
        const int weight_count = weight.Count();
        for (int index = 0; index < weight_count; ++index) {
            const int weighted_proxy_index = weight.bone_indices[static_cast<std::size_t>(index)];
            if (weighted_proxy_index < 0 || weighted_proxy_index >= proxy_mesh.attributes.Count()) {
                continue;
            }
            const VertexAttribute attr = proxy_mesh.attributes[weighted_proxy_index];
            if (attr.IsMove()) {
                move_value += weight.weights[static_cast<std::size_t>(index)];
            } else if (attr.IsFixed()) {
                fixed_value += weight.weights[static_cast<std::size_t>(index)];
            }
        }
        attributes[vertex_index] =
            move_value > fixed_value ? VertexAttribute::Move() : VertexAttribute::Fixed();
    }
}

void VirtualMesh::Mapping(VirtualMesh& proxy_mesh)
{
    // Ported from Magica Cloth 2: Scripts/Core/VirtualMesh/Function/VirtualMeshMapping.cs
    if (!IsValid() || !proxy_mesh.IsValid()) {
        result = Result::Error(
            ResultCode::MappingMesh_ProxyError,
            "VirtualMesh mapping proxy error."
        );
        return;
    }

    const float4x4 to_proxy = CenterTransformTo(proxy_mesh);
    std::vector<MappingWorkData> mapping_work_data(static_cast<std::size_t>(VertexCount()));
    if (merge_chunk.IsValid()) {
        DirectMapping(proxy_mesh, to_proxy, mapping_work_data);
        CalcDirectMappingWeights(
            proxy_mesh,
            mapping_work_data,
            proxy_mesh.average_vertex_distance * 1.5f
        );
    } else {
        SearchMapping(proxy_mesh, to_proxy, mapping_work_data);
        CalcSearchMappingWeights(proxy_mesh, mapping_work_data);
    }

    to_proxy_matrix = to_proxy;
    to_proxy_rotation = Multiply(proxy_mesh.init_inverse_rotation, init_rotation);
    mesh_type = MeshType::Mapping;
    result = Result::Ok();
}

bool VirtualMesh::IsValid() const
{
    return result.Succeeded();
}

int VirtualMesh::VertexCount() const
{
    return local_positions.Count();
}

int VirtualMesh::TriangleCount() const
{
    return triangles.Count();
}

int VirtualMesh::LineCount() const
{
    return lines.Count();
}

int VirtualMesh::SkinBoneCount() const
{
    return skin_bone_transform_indices.Count();
}

int VirtualMesh::TransformCount() const
{
    return transform_data.Count();
}

int VirtualMesh::CustomSkinningBoneCount() const
{
    return static_cast<int>(custom_skinning_bone_indices.size());
}

int VirtualMesh::CenterFixedPointCount() const
{
    return center_fixed_list.Count();
}

int VirtualMesh::BaseLineCount() const
{
    return base_line_start_data_indices.Count();
}

bool VirtualMesh::IsProxy() const
{
    return mesh_type == MeshType::ProxyMesh || mesh_type == MeshType::ProxyBoneMesh;
}

bool VirtualMesh::IsMapping() const
{
    return mesh_type == MeshType::Mapping;
}

}  // namespace hocloth::mc2
