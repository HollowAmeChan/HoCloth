#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace hocloth::mc2 {

namespace {

bool IsValidVertexIndexForWork(int index, int vertex_count)
{
    return index >= 0 && index < vertex_count && index <= std::numeric_limits<std::uint16_t>::max();
}

void UniqueAddEdgeTriangle(
    VirtualMesh::EdgeToTrianglesMap& map,
    std::uint32_t key,
    std::uint16_t value
)
{
    auto& values = map[key];
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

}  // namespace

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
    // Ported from MC2 VirtualMeshWork.IntersectRayMesh().
    std::vector<VirtualMeshRaycastHit> hits;
    hits.reserve(100);

    const float3 local_ray_position = InverseTransformPoint(ray_position, init_world_to_local);
    const float3 local_ray_direction = InverseTransformDirection(ray_direction, init_world_to_local);
    const float3 ray_end_position = Add(ray_position, Scale(ray_direction, 1000.0f));
    const float3 local_ray_end_position = InverseTransformPoint(ray_end_position, init_world_to_local);
    const float local_point_radius =
        init_scale.x != 0.0f ? std::max(point_radius / init_scale.x, 0.0f) : std::max(point_radius, 0.0f);

    const int vertex_count = VertexCount();
    for (int triangle_index = 0; triangle_index < TriangleCount(); ++triangle_index) {
        const int3 triangle = triangles[triangle_index];
        if (!IsValidVertexIndexForWork(triangle.x, vertex_count)
            || !IsValidVertexIndexForWork(triangle.y, vertex_count)
            || !IsValidVertexIndexForWork(triangle.z, vertex_count)) {
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
            if (!IsValidVertexIndexForWork(triangle.x, vertex_count)
                || !IsValidVertexIndexForWork(triangle.y, vertex_count)
                || !IsValidVertexIndexForWork(triangle.z, vertex_count)) {
                continue;
            }
            const int2 triangle_edges[3] = {
                data::PackInt2(triangle.x, triangle.y),
                data::PackInt2(triangle.y, triangle.z),
                data::PackInt2(triangle.z, triangle.x),
            };
            for (const int2& edge : triangle_edges) {
                UniqueAddEdgeTriangle(
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
        if (!IsValidVertexIndexForWork(edge.x, vertex_count)
            || !IsValidVertexIndexForWork(edge.y, vertex_count)) {
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

}  // namespace hocloth::mc2
