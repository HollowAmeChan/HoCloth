#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/utility/data/multi_data_builder.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/utility/native_collection/ex_cost_sorted_list1.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stack>
#include <unordered_map>
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

void VirtualMesh::Dispose()
{
    reference_indices.Dispose();
    attributes.Dispose();
    local_positions.Dispose();
    local_normals.Dispose();
    local_tangents.Dispose();
    uv.Dispose();
    bone_weights.Dispose();
    triangles.Dispose();
    lines.Dispose();
    edges.Dispose();
    edge_flags.Dispose();
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
    merge_chunk.Clear();
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
