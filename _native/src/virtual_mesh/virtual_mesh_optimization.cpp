#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hocloth::mc2 {

namespace {

std::uint64_t PackedTriangleKeyForOptimization(const int3& triangle)
{
    const int3 packed = data::PackInt3(triangle);
    return (static_cast<std::uint64_t>(packed.x) << 32)
        | (static_cast<std::uint64_t>(packed.y) << 16)
        | static_cast<std::uint64_t>(packed.z);
}

}  // namespace

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
                    remove_triangle_set.insert(PackedTriangleKeyForOptimization(triangle_a));
                    remove_triangle_set.insert(PackedTriangleKeyForOptimization(triangle_b));
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
        if (remove_triangle_set.find(PackedTriangleKeyForOptimization(triangle)) != remove_triangle_set.end()) {
            continue;
        }
        new_triangles.push_back(triangle);
    }

    triangles.Dispose();
    triangles.AddRange(new_triangles);
}

}  // namespace hocloth::mc2
