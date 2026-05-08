#include "virtual_mesh_internal.hpp"

#include "hocloth/cloth/cloth_serialize_data.hpp"
#include "hocloth/cloth/selection_data.hpp"
#include "hocloth/core/define/system_define.hpp"
#include "hocloth/utility/data/multi_data_builder.hpp"
#include "hocloth/utility/grid/grid_map.hpp"
#include "hocloth/utility/native_collection/ex_cost_sorted_list1.hpp"
#include "hocloth/utility/native_collection/ex_cost_sorted_list4.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hocloth::mc2 {

void VirtualMesh::CreateProxyFixedListAndAABB()
{
    // Ported from Magica Cloth 2: ProxyCreateFixedListAndAABB()
    const int vertex_count = VertexCount();
    center_fixed_list.Dispose();
    bounding_box = AABB{};
    local_center_position = float3{};

    if (vertex_count <= 0
        || attributes.Count() < vertex_count
        || local_positions.Count() < vertex_count) {
        return;
    }

    float3 fixed_center{};
    int fixed_count = 0;
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const VertexAttribute attr = attributes[vertex_index];

        Encapsulate(bounding_box, local_positions[vertex_index]);
        if (!attr.IsMove() && IsValidVertexIndex(vertex_index, vertex_count)) {
            bool connected_to_move = false;
            bool has_connection = false;
            if (vertex_to_vertex_index_array.Count() > vertex_index
                && vertex_to_vertex_data_array.Count() > 0) {
                int data_count = 0;
                int data_start = 0;
                data::Unpack12_20(
                    vertex_to_vertex_index_array[vertex_index],
                    data_count,
                    data_start
                );
                has_connection = data_count > 0;
                for (int offset = 0; offset < data_count; ++offset) {
                    const int data_index = data_start + offset;
                    if (data_index < 0 || data_index >= vertex_to_vertex_data_array.Count()) {
                        continue;
                    }
                    const int connected_index = vertex_to_vertex_data_array[data_index];
                    if (connected_index < 0 || connected_index >= attributes.Count()) {
                        continue;
                    }
                    if (attributes[connected_index].IsMove()) {
                        connected_to_move = true;
                        break;
                    }
                }
            }
            if (has_connection && !connected_to_move) {
                continue;
            }
            center_fixed_list.Add(static_cast<std::uint16_t>(vertex_index));
            fixed_center = Add(fixed_center, local_positions[vertex_index]);
            ++fixed_count;
        }
    }
    if (fixed_count > 0) {
        local_center_position = Scale(fixed_center, 1.0f / static_cast<float>(fixed_count));
    }
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

    BuildEdgeToTriangles();
    OptimizeTriangleDirection(
        triangles,
        triangle_normals,
        edge_to_triangles,
        local_positions,
        define::system::SameSurfaceAngle
    );
    BuildEdgeToTriangles();
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
        const float2 uv0 = triangle.x < uv.Count() ? uv[triangle.x] : float2{};
        const float2 uv1 = triangle.y < uv.Count() ? uv[triangle.y] : float2{};
        const float2 uv2 = triangle.z < uv.Count() ? uv[triangle.z] : float2{};
        triangle_normals[static_cast<std::size_t>(triangle_index)] = TriangleNormal(p0, p1, p2);
        triangle_tangents[static_cast<std::size_t>(triangle_index)] =
            TriangleTangent(p0, p1, p2, uv0, uv1, uv2);
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
            final_normal = Add(final_normal, triangle_normals[static_cast<std::size_t>(triangle_index)]);
            final_tangent = Add(final_tangent, triangle_tangents[static_cast<std::size_t>(triangle_index)]);
        }

        if (Length(final_normal) < 0.5f) {
            float max_distance = -1.0f;
            final_normal = float3{};
            for (int index = 0; index < count; ++index) {
                const int candidate_index = static_cast<int>(triangle_list[index]);
                if (candidate_index < 0 || candidate_index >= triangle_count) {
                    continue;
                }
                const float3 candidate_normal = triangle_normals[static_cast<std::size_t>(candidate_index)];
                float3 normal_sum{};
                for (int other_index = 0; other_index < count; ++other_index) {
                    const int triangle_index = static_cast<int>(triangle_list[other_index]);
                    if (triangle_index < 0
                        || triangle_index >= triangle_count
                        || triangle_index == candidate_index) {
                        continue;
                    }
                    const float3 normal = triangle_normals[static_cast<std::size_t>(triangle_index)];
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
                const float3 candidate_tangent = triangle_tangents[static_cast<std::size_t>(candidate_index)];
                float3 tangent_sum{};
                for (int other_index = 0; other_index < count; ++other_index) {
                    const int triangle_index = static_cast<int>(triangle_list[other_index]);
                    if (triangle_index < 0
                        || triangle_index >= triangle_count
                        || triangle_index == candidate_index) {
                        continue;
                    }
                    const float3 tangent = triangle_tangents[static_cast<std::size_t>(triangle_index)];
                    tangent_sum = Add(
                        tangent_sum,
                        Dot(candidate_tangent, tangent) >= 0.0f ? tangent : Scale(tangent, -1.0f)
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

        float3 normal{};
        float3 tangent{};
        for (int index = 0; index < count; ++index) {
            const std::uint32_t packed = triangle_list[index];
            const int flip_flag = data::Unpack12_20Hi(packed);
            const int triangle_index = data::Unpack12_20Low(packed);
            if (triangle_index < 0 || triangle_index >= triangle_count) {
                continue;
            }

            const float normal_sign = (flip_flag & 0x1) == 0 ? 1.0f : -1.0f;
            const float tangent_sign = (flip_flag & 0x2) == 0 ? 1.0f : -1.0f;
            normal = Add(normal, Scale(triangle_normals[static_cast<std::size_t>(triangle_index)], normal_sign));
            tangent = Add(tangent, Scale(triangle_tangents[static_cast<std::size_t>(triangle_index)], tangent_sign));
        }

        if (LengthSquared(normal) > define::system::Epsilon
            && LengthSquared(tangent) > define::system::Epsilon) {
            normal = Normalize(normal);
            const float3 binormal = Normalize(Cross(normal, tangent));
            if (LengthSquared(binormal) > define::system::Epsilon) {
                local_normals[vertex_index] = normal;
                local_tangents[vertex_index] = binormal;
            }
        }
    }
}

void VirtualMesh::BuildVertexToVertexFromTopology()
{
    // Ported from MC2 Proxy_CalcVertexToVertexFromTriangleJob and
    // Proxy_CalcVertexToVertexFromLineJob.
    const int vertex_count = VertexCount();
    vertex_to_vertex_index_array.Dispose();
    vertex_to_vertex_data_array.Dispose();
    if (vertex_count <= 0) {
        return;
    }

    data::MultiDataBuilder<std::uint16_t> vertex_builder(vertex_count, vertex_count * 4);
    std::vector<std::unordered_set<std::uint16_t>> unique_neighbors(static_cast<std::size_t>(vertex_count));
    std::vector<int2> edge_list;
    std::unordered_set<std::uint32_t> edge_keys;

    const auto add_neighbor = [&](int from, int to) {
        if (!IsValidVertexIndex(from, vertex_count) || !IsValidVertexIndex(to, vertex_count)) {
            return;
        }
        const std::uint16_t value = static_cast<std::uint16_t>(to);
        if (!unique_neighbors[static_cast<std::size_t>(from)].insert(value).second) {
            return;
        }
        vertex_builder.Add(from, value);
    };

    for (int triangle_index = 0; triangle_index < TriangleCount(); ++triangle_index) {
        const int3 triangle = triangles[triangle_index];
        if (!IsValidVertexIndex(triangle.x, vertex_count)
            || !IsValidVertexIndex(triangle.y, vertex_count)
            || !IsValidVertexIndex(triangle.z, vertex_count)) {
            continue;
        }

        add_neighbor(triangle.x, triangle.y);
        add_neighbor(triangle.x, triangle.z);
        add_neighbor(triangle.y, triangle.x);
        add_neighbor(triangle.y, triangle.z);
        add_neighbor(triangle.z, triangle.x);
        add_neighbor(triangle.z, triangle.y);

        AddUniqueEdge(edge_list, edge_keys, triangle.x, triangle.y);
        AddUniqueEdge(edge_list, edge_keys, triangle.y, triangle.z);
        AddUniqueEdge(edge_list, edge_keys, triangle.z, triangle.x);
    }

    for (int line_index = 0; line_index < LineCount(); ++line_index) {
        const int2 line = lines[line_index];
        if (!IsValidVertexIndex(line.x, vertex_count)
            || !IsValidVertexIndex(line.y, vertex_count)) {
            continue;
        }

        add_neighbor(line.x, line.y);
        add_neighbor(line.y, line.x);
        AddUniqueEdge(edge_list, edge_keys, line.x, line.y);
    }

    const auto [vertex_data, vertex_indices] = vertex_builder.ToArray();
    vertex_to_vertex_data_array.AddRange(vertex_data);
    vertex_to_vertex_index_array.AddRange(vertex_indices);

    edges.Dispose();
    if (!edge_list.empty()) {
        edges.AddRange(edge_list);
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
            UniqueAdd(edge_to_triangles, data::Pack32(edge.x, edge.y), static_cast<std::uint16_t>(triangle_index));
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

void VirtualMesh::RefreshDerivedTopology()
{
    // Porting guardrail: MC2 rebuilds proxy adjacency from the current topology.
    BuildVertexToTriangles();
    BuildVertexToVertexFromTopology();
    BuildEdgeToTriangles();
    BuildEdgeFlags();
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

void VirtualMesh::ApplySelectionAttribute(
    const SelectionData& selection_data,
    bool clear_fixed_move_flags
)
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
                nearest_attribute = selection_data.attributes[static_cast<std::size_t>(selection_index)];
            }
        }

        VertexAttribute attribute = attributes[vertex_index];
        if (clear_fixed_move_flags) {
            attribute.Set(static_cast<std::uint8_t>(VertexAttribute::FlagFixed | VertexAttribute::FlagMove), false);
        }
        attribute.Set(nearest_attribute, true);
        attributes[vertex_index] = attribute;
    }

    if (!is_bone_cloth || transform_data.flag_array.Count() < VertexCount()) {
        return;
    }
    for (int vertex_index = 0; vertex_index < VertexCount(); ++vertex_index) {
        const VertexAttribute attribute = attributes[vertex_index];
        BitFlag8 flag = transform_data.flag_array[vertex_index];
        if (clear_fixed_move_flags) {
            flag.SetFlag(TransformManager::FlagLocalPosRotWrite, false);
            flag.SetFlag(TransformManager::FlagWorldRotWrite, false);
            flag.SetFlag(TransformManager::FlagRestore, false);
        }
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

void VirtualMesh::ApplyBoneClothDefaultSelection()
{
    // MC2 fallback path for BoneCloth without paint/manual selection:
    // fill all proxy vertices as Move, then mark root transforms as Fixed.
    const int vertex_count = VertexCount();
    if (vertex_count <= 0 || local_positions.Count() < vertex_count) {
        return;
    }

    std::vector<float3> positions;
    positions.reserve(static_cast<std::size_t>(vertex_count));
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        positions.push_back(local_positions[vertex_index]);
    }

    std::vector<int> root_indices;
    root_indices.reserve(static_cast<std::size_t>(vertex_count));
    if (vertex_parent_indices.Count() >= vertex_count) {
        for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
            if (vertex_parent_indices[vertex_index] < 0) {
                root_indices.push_back(vertex_index);
            }
        }
    }
    if (root_indices.empty() && !transform_data.root_id_list.empty()) {
        for (int root_id : transform_data.root_id_list) {
            auto found = std::find(transform_data.id_array.begin(), transform_data.id_array.end(), root_id);
            if (found != transform_data.id_array.end()) {
                root_indices.push_back(static_cast<int>(std::distance(transform_data.id_array.begin(), found)));
            }
        }
    }
    if (root_indices.empty()) {
        root_indices.push_back(0);
    }

    SelectionData selection = SelectionData::CreateBoneClothDefault(positions, root_indices);
    if (attributes.Count() < vertex_count) {
        attributes.Dispose();
        attributes.AddRange(vertex_count, VertexAttribute::Invalid());
    }
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        attributes[vertex_index] = selection.attributes[static_cast<std::size_t>(vertex_index)];
    }

    if (!is_bone_cloth || transform_data.flag_array.Count() < vertex_count) {
        return;
    }
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
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

    if (vertex_count <= 0 || attributes.Count() < vertex_count || local_positions.Count() < vertex_count) {
        return;
    }

    if (vertex_to_vertex_index_array.Count() < vertex_count) {
        BuildVertexToVertexFromTopology();
    }
    if (vertex_to_vertex_index_array.Count() < vertex_count) {
        vertex_parent_indices.AddRange(vertex_count, -1);
        vertex_child_index_array.AddRange(vertex_count, std::uint32_t{});
        return;
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

    struct BaseLineWork {
        int vertex_index = -1;
        float distance = 0.0f;
    };

    data::MultiDataBuilder<std::uint16_t> child_builder(vertex_count, vertex_count);
    std::vector<std::uint8_t> mark(static_cast<std::size_t>(vertex_count), 0);
    std::vector<BaseLineWork> current;
    current.reserve(fixed_vertices.size());
    for (int vertex_index : fixed_vertices) {
        current.push_back(BaseLineWork{vertex_index, 0.0f});
    }

    const auto read_neighbors = [this, vertex_count](int vertex_index, auto&& visitor) {
        if (vertex_index < 0 || vertex_index >= vertex_count || vertex_index >= vertex_to_vertex_index_array.Count()) {
            return;
        }

        int data_count = 0;
        int data_start = 0;
        data::Unpack12_20(vertex_to_vertex_index_array[vertex_index], data_count, data_start);
        for (int offset = 0; offset < data_count; ++offset) {
            const int data_index = data_start + offset;
            if (data_index < 0 || data_index >= vertex_to_vertex_data_array.Count()) {
                continue;
            }
            const int target = vertex_to_vertex_data_array[data_index];
            if (target >= 0 && target < vertex_count) {
                visitor(target);
            }
        }
    };

    while (!current.empty()) {
        for (const BaseLineWork& work : current) {
            const int vertex_index = work.vertex_index;
            if (vertex_index >= 0 && vertex_index < vertex_count) {
                mark[static_cast<std::size_t>(vertex_index)] = 1;
            }
        }

        std::sort(current.begin(), current.end(), [](const BaseLineWork& lhs, const BaseLineWork& rhs) {
            return lhs.distance < rhs.distance;
        });

        for (const BaseLineWork& work : current) {
            const int vertex_index = work.vertex_index;
            if (vertex_index < 0 || vertex_index >= vertex_count || attributes[vertex_index].IsDontMove()) {
                continue;
            }

            ExCostSortedList1 cost{-1.0f, -1};
            const float3 position = local_positions[vertex_index];
            read_neighbors(vertex_index, [&](int target) {
                if (mark[static_cast<std::size_t>(target)] == 0 || target < 0 || target >= vertex_count) {
                    return;
                }

                const float3 target_position = local_positions[target];
                if (attributes[target].IsDontMove()) {
                    cost.Add(Distance(position, target_position), target);
                } else {
                    const int parent_index = vertex_parent_indices[target];
                    if (parent_index < 0 || parent_index >= vertex_count) {
                        return;
                    }
                    const float angle = Angle(
                        Subtract(target_position, position),
                        Subtract(local_positions[parent_index], target_position)
                    );
                    cost.Add(angle, target);
                }
            });

            if (cost.IsValid()) {
                vertex_parent_indices[vertex_index] = cost.Data();
                mark[static_cast<std::size_t>(vertex_index)] = 1;
            }
        }

        for (const BaseLineWork& work : current) {
            const int vertex_index = work.vertex_index;
            if (vertex_index >= 0 && vertex_index < vertex_count) {
                mark[static_cast<std::size_t>(vertex_index)] = 2;
                const int parent_index = vertex_parent_indices[vertex_index];
                if (parent_index >= 0 && parent_index < vertex_count) {
                    child_builder.Add(parent_index, static_cast<std::uint16_t>(vertex_index));
                }
            }
        }

        std::vector<float> best_distance(static_cast<std::size_t>(vertex_count), std::numeric_limits<float>::max());
        std::vector<BaseLineWork> next;
        for (const BaseLineWork& work : current) {
            const int vertex_index = work.vertex_index;
            if (vertex_index < 0 || vertex_index >= vertex_count) {
                continue;
            }
            const float3 position = local_positions[vertex_index];
            read_neighbors(vertex_index, [&](int target) {
                if (target < 0 || target >= vertex_count || attributes[target].IsInvalid()
                    || mark[static_cast<std::size_t>(target)] != 0) {
                    return;
                }

                const float distance = Distance(position, local_positions[target]);
                if (distance < best_distance[static_cast<std::size_t>(target)]) {
                    if (best_distance[static_cast<std::size_t>(target)] == std::numeric_limits<float>::max()) {
                        next.push_back(BaseLineWork{target, distance});
                    } else {
                        for (BaseLineWork& candidate : next) {
                            if (candidate.vertex_index == target) {
                                candidate.distance = distance;
                                break;
                            }
                        }
                    }
                    best_distance[static_cast<std::size_t>(target)] = distance;
                }
            });
        }

        std::sort(next.begin(), next.end(), [](const BaseLineWork& lhs, const BaseLineWork& rhs) {
            return lhs.distance < rhs.distance;
        });
        current = std::move(next);
    }

    const auto [child_data, child_index] = child_builder.ToArray();
    vertex_child_data_array.AddRange(child_data);
    vertex_child_index_array.AddRange(child_index);

    std::vector<BitFlag8> line_flags;
    std::vector<std::uint16_t> start_indices;
    std::vector<std::uint16_t> data_counts;
    std::vector<std::uint16_t> indices;
    line_flags.reserve(fixed_vertices.size());
    start_indices.reserve(fixed_vertices.size());
    data_counts.reserve(fixed_vertices.size());
    indices.reserve(static_cast<std::size_t>(vertex_count));

    std::stack<int> stack;
    for (int fixed_vertex : fixed_vertices) {
        if (fixed_vertex < 0 || fixed_vertex >= vertex_count || child_builder.CountValuesForKey(fixed_vertex) == 0) {
            continue;
        }

        const std::uint16_t start = static_cast<std::uint16_t>(indices.size());
        std::uint16_t count = 0;
        BitFlag8 line_flag;
        stack.push(fixed_vertex);
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

            int data_count = 0;
            int data_start = 0;
            data::Unpack12_20(child_index[static_cast<std::size_t>(vertex_index)], data_count, data_start);
            for (int offset = 0; offset < data_count; ++offset) {
                const int data_index = data_start + offset;
                if (data_index >= 0 && data_index < static_cast<int>(child_data.size())) {
                    stack.push(child_data[static_cast<std::size_t>(data_index)]);
                }
            }
        }

        line_flags.push_back(line_flag);
        start_indices.push_back(start);
        data_counts.push_back(count);
    }

    base_line_flags.AddRange(line_flags);
    base_line_start_data_indices.AddRange(start_indices);
    base_line_data_counts.AddRange(data_counts);
    base_line_data.AddRange(indices);

    CreateBaseLinePose();
    CreateVertexRootAndDepth();
}

void VirtualMesh::BuildTransformBaseLines()
{
    // Ported from Magica Cloth 2: CreateTransformBaseLine()
    const int vertex_count = VertexCount();
    vertex_parent_indices.Dispose();
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

    BuildBaseLinesFromParents();
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
            data::Unpack12_20(child_index[static_cast<std::size_t>(current_root)], packed_count, packed_start);
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
                data::Unpack12_20(child_index[static_cast<std::size_t>(vertex_index)], current_count, current_start);
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

    CreateBaseLinePose();
    CreateVertexRootAndDepth();
}

void VirtualMesh::CreateBaseLinePose()
{
    // Ported from MC2 CreateBaseLinePose() / BaseLine_CalcLocalPositionRotationJob.
    const int vertex_count = VertexCount();
    vertex_local_positions.Dispose();
    vertex_local_rotations.Dispose();
    if (vertex_count <= 0 || local_positions.Count() < vertex_count || vertex_parent_indices.Count() < vertex_count) {
        return;
    }

    vertex_local_positions.AddRange(vertex_count, float3{});
    vertex_local_rotations.AddRange(vertex_count, quaternion{});
    for (int data_index = 0; data_index < base_line_data.Count(); ++data_index) {
        const int vertex_index = base_line_data[data_index];
        if (vertex_index < 0 || vertex_index >= vertex_count) {
            continue;
        }

        const int parent_index = vertex_parent_indices[vertex_index];
        if (parent_index < 0 || parent_index >= vertex_count) {
            vertex_local_positions[vertex_index] = float3{};
            vertex_local_rotations[vertex_index] = quaternion{};
            continue;
        }

        const float3 parent_position = local_positions[parent_index];
        const float3 parent_normal = LocalNormalOrDefault(*this, parent_index);
        const float3 parent_tangent = LocalTangentOrDefault(*this, parent_index);
        const quaternion inverse_parent_rotation = Inverse(ToRotation(parent_normal, parent_tangent));

        const float3 position = local_positions[vertex_index];
        const quaternion rotation = ToRotation(
            LocalNormalOrDefault(*this, vertex_index),
            LocalTangentOrDefault(*this, vertex_index)
        );
        vertex_local_positions[vertex_index] = Rotate(inverse_parent_rotation, Subtract(position, parent_position));
        vertex_local_rotations[vertex_index] = Multiply(inverse_parent_rotation, rotation);
    }
}

void VirtualMesh::CreateVertexRootAndDepth()
{
    // Ported from MC2 CreateVertexRootAndDepth() / BaseLine_CalcMaxBaseLineLengthJob.
    const int vertex_count = VertexCount();
    vertex_root_indices.Dispose();
    vertex_depths.Dispose();
    if (vertex_count <= 0
        || attributes.Count() < vertex_count
        || local_positions.Count() < vertex_count
        || vertex_parent_indices.Count() < vertex_count) {
        return;
    }

    std::vector<float> root_lengths(static_cast<std::size_t>(vertex_count), 0.0f);
    vertex_root_indices.AddRange(vertex_count, -1);
    vertex_depths.AddRange(vertex_count, 0.0f);
    float max_length = 0.0f;
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        int root_index = -1;
        float root_length = 0.0f;
        if (attributes[vertex_index].IsMove()) {
            int current = vertex_index;
            int parent = vertex_parent_indices[current];
            int guard = 0;
            while (parent >= 0 && parent < vertex_count && guard++ < vertex_count) {
                root_length += Distance(local_positions[current], local_positions[parent]);
                root_index = parent;
                if (!attributes[parent].IsMove()) {
                    break;
                }
                current = parent;
                parent = vertex_parent_indices[current];
            }
        }
        vertex_root_indices[vertex_index] = root_index;
        root_lengths[static_cast<std::size_t>(vertex_index)] = root_length;
        max_length = std::max(max_length, root_length);
    }

    if (max_length > define::system::Epsilon) {
        for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
            vertex_depths[vertex_index] = SafeDepth(root_lengths[static_cast<std::size_t>(vertex_index)], max_length);
        }
    }
}

void VirtualMesh::CreateCustomSkinning(
    const CustomSkinningSettings& settings,
    const std::vector<TransformRecord>& custom_skinning_bone_records
)
{
    (void)settings;
    // Ported from MC2 Proxy_CalcCustomSkinningWeightsJobV2.
    if (CustomSkinningBoneCount() == 0
        || VertexCount() <= 0
        || attributes.Count() < VertexCount()
        || local_positions.Count() < VertexCount()
        || bone_weights.Count() < VertexCount()) {
        return;
    }

    struct SkinningBoneInfo {
        int child_transform_index = -1;
        float3 child_position{};
    };
    std::vector<SkinningBoneInfo> bone_infos;
    bone_infos.reserve(custom_skinning_bone_indices.size());
    for (std::size_t index = 0; index < custom_skinning_bone_indices.size(); ++index) {
        const int skin_bone_index = custom_skinning_bone_indices[index];
        if (skin_bone_index < 0 || index >= custom_skinning_bone_records.size()) {
            continue;
        }
        const TransformRecord& record = custom_skinning_bone_records[index];
        if (!record.IsValid()) {
            continue;
        }
        bone_infos.push_back(SkinningBoneInfo{skin_bone_index, record.local_position});
    }
    if (bone_infos.empty()) {
        return;
    }

    for (int vertex_index = 0; vertex_index < VertexCount(); ++vertex_index) {
        if (attributes[vertex_index].IsDontMove()) {
            continue;
        }

        ExCostSortedList4 cost_list{-1.0f};
        const float3 position = local_positions[vertex_index];
        for (const SkinningBoneInfo& info : bone_infos) {
            const float distance = Distance(position, info.child_position);
            const int current_index = cost_list.IndexOf(info.child_transform_index);
            if (current_index >= 0) {
                if (distance < cost_list.costs[current_index]) {
                    cost_list.RemoveItem(info.child_transform_index);
                    cost_list.Add(distance, info.child_transform_index);
                }
            } else {
                cost_list.Add(distance, info.child_transform_index);
            }
        }

        int count = cost_list.Count();
        if (count <= 0) {
            continue;
        }
        const float min_distance = cost_list.MinCost() * define::system::CustomSkinningDistanceReduction;
        for (int index = 0; index < count; ++index) {
            cost_list.costs[index] = std::pow(
                std::max(cost_list.costs[index] - min_distance, 0.0f),
                define::system::CustomSkinningDistancePow
            );
        }

        if (cost_list.MinCost() < define::system::Epsilon) {
            cost_list.costs = float4{1.0f, 0.0f, 0.0f, 0.0f};
            cost_list.data = int4{cost_list.data[0], 0, 0, 0};
        } else {
            float inverse_sum = 0.0f;
            count = cost_list.Count();
            for (int index = 0; index < count; ++index) {
                inverse_sum += 1.0f / cost_list.costs[index];
            }
            if (inverse_sum <= define::system::Epsilon) {
                continue;
            }
            for (int index = 0; index < count; ++index) {
                cost_list.costs[index] = (1.0f / cost_list.costs[index]) / inverse_sum;
            }

            constexpr float InvalidWeight = 0.001f;
            float weight_sum = 0.0f;
            for (int index = 0; index < 4; ++index) {
                if (index >= count || cost_list.costs[index] < InvalidWeight) {
                    cost_list.costs[index] = 0.0f;
                    cost_list.data[index] = 0;
                } else {
                    weight_sum += cost_list.costs[index];
                }
            }
            if (weight_sum <= define::system::Epsilon) {
                continue;
            }
            for (int index = 0; index < 4; ++index) {
                cost_list.costs[index] /= weight_sum;
            }
        }

        bone_weights[vertex_index] = VirtualMeshBoneWeight(cost_list.data, cost_list.costs);
    }
}

void VirtualMesh::ProxyNormalAdjustment(
    const ClothSerializeData& serialize_data,
    const TransformRecord& normal_adjustment_transform_record
)
{
    // Ported from Magica Cloth 2: ProxyNormalAdjustment().
    const int vertex_count = VertexCount();
    normal_adjustment_rotations.Dispose();
    normal_adjustment_rotations.AddRange(vertex_count, quaternion{});
    if (vertex_count <= 0) {
        return;
    }

    const auto mode = serialize_data.normal_alignment_setting.alignment_mode;
    if (mode == NormalAlignmentSettings::AlignmentMode::None) {
        return;
    }

    float3 center{};
    if (mode == NormalAlignmentSettings::AlignmentMode::BoundingBoxCenter) {
        center = Center(bounding_box);
    } else if (mode == NormalAlignmentSettings::AlignmentMode::Transform) {
        if (!normal_adjustment_transform_record.IsValid()) {
            return;
        }
        center = TransformPoint(normal_adjustment_transform_record.position, init_world_to_local);
    } else {
        return;
    }

    if (local_positions.Count() < vertex_count
        || local_normals.Count() < vertex_count
        || local_tangents.Count() < vertex_count
        || vertex_parent_indices.Count() < vertex_count
        || vertex_child_index_array.Count() < vertex_count) {
        return;
    }

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const float3 local_position = local_positions[vertex_index];
        float3 radiation = Subtract(local_position, center);
        if (Length(radiation) < define::system::Epsilon) {
            continue;
        }
        radiation = Normalize(radiation);

        const quaternion local_rotation = ToRotation(local_normals[vertex_index], local_tangents[vertex_index]);
        quaternion adjusted_rotation = local_rotation;

        float3 child_vector{};
        int child_count = 0;
        int child_start = 0;
        data::Unpack12_20(vertex_child_index_array[vertex_index], child_count, child_start);
        if (child_count > 0) {
            for (int index = 0; index < child_count; ++index) {
                const int child_index = vertex_child_data_array[child_start + index];
                if (child_index >= 0 && child_index < vertex_count) {
                    child_vector = Add(child_vector, Subtract(local_positions[child_index], local_position));
                }
            }

            if (LengthSquared(child_vector) > define::system::Epsilon) {
                const float3 tangent = Normalize(child_vector);
                float3 normal = Cross(tangent, radiation);
                normal = Cross(normal, tangent);
                if (LengthSquared(normal) > define::system::Epsilon) {
                    normal = Normalize(normal);
                    local_normals[vertex_index] = normal;
                    local_tangents[vertex_index] = tangent;
                    adjusted_rotation = ToRotation(normal, tangent);
                }
            }
        } else {
            const int parent_index = vertex_parent_indices[vertex_index];
            if (parent_index >= 0 && parent_index < vertex_count) {
                const float3 parent_position = local_positions[parent_index];
                const float3 tangent = Normalize(Subtract(local_position, parent_position));
                float3 normal = Cross(tangent, radiation);
                normal = Cross(normal, tangent);
                if (LengthSquared(normal) > define::system::Epsilon) {
                    normal = Normalize(normal);
                    local_normals[vertex_index] = normal;
                    local_tangents[vertex_index] = tangent;
                    adjusted_rotation = ToRotation(normal, tangent);
                }
            }
        }

        normal_adjustment_rotations[vertex_index] = Multiply(Inverse(local_rotation), adjusted_rotation);
    }
}

void VirtualMesh::ConvertProxyMesh(
    const ClothSerializeData& serialize_data,
    const TransformRecord& cloth_transform_record,
    const std::vector<TransformRecord>& custom_skinning_bone_records,
    const TransformRecord& normal_adjustment_transform_record
)
{
    // Ported from MC2 VirtualMesh.ConvertProxyMesh(...). Import/reduction are handled
    // before this native finalization stage, matching MC2's build pipeline ownership.
    if (!IsValid() || VertexCount() <= 0) {
        return;
    }

    const bool use_custom_skinning =
        serialize_data.custom_skinning_setting.enable
        && serialize_data.cloth_type != ClothType::BoneSpring;
    std::vector<TransformRecord> custom_skinning_records = custom_skinning_bone_records;
    if (use_custom_skinning) {
        SetCustomSkinningBones(cloth_transform_record, custom_skinning_records);
    } else {
        custom_skinning_bone_indices.clear();
    }

    RefreshDerivedTopology();
    CreateProxyFixedListAndAABB();
    if (is_bone_cloth) {
        BuildTransformBaseLines();
    } else {
        BuildMeshBaseLinesFromEdges();
    }

    ProxyNormalAdjustment(serialize_data, normal_adjustment_transform_record);

    if (is_bone_cloth) {
        CreateVertexToTransformRotations();
    }
    CreateVertexBindPose();

    center_world_position = TransformPoint(local_center_position, init_local_to_world);
    center_world_rotation = init_rotation;
    center_world_scale = init_scale;
    if (use_custom_skinning) {
        CreateCustomSkinning(serialize_data.custom_skinning_setting, custom_skinning_records);
    }
    mesh_type = is_bone_cloth ? MeshType::ProxyBoneMesh : MeshType::ProxyMesh;
    result = Result::Ok();
}

}  // namespace hocloth::mc2
