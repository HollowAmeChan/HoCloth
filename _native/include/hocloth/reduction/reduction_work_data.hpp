#pragma once

#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/ex_simple_native_array.hpp"
#include "hocloth/virtual_mesh/vertex_attribute.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_bone_weight.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace hocloth::mc2 {

class VirtualMesh;

// Native data-layer port for Scripts/Core/Reduction/ReductionWorkData.cs.
struct ReductionWorkData {
    VirtualMesh* vmesh = nullptr;

    std::vector<int> vertex_join_indices;
    std::unordered_map<std::uint16_t, std::vector<std::uint16_t>> vertex_to_vertex_map;

    std::vector<int> vertex_remap_indices;
    std::vector<int> old_vertex_to_new_vertex_indices;
    std::unordered_map<int, int> use_skin_bone_map;
    std::unordered_map<std::uint16_t, std::vector<std::uint16_t>> new_vertex_to_vertex_map;
    std::unordered_set<std::uint64_t> edge_set;
    std::unordered_set<std::uint64_t> triangle_set;

    int old_vertex_count = 0;
    int new_vertex_count = 0;
    int remove_vertex_count = 0;

    ExSimpleNativeArray<VertexAttribute> new_attributes;
    ExSimpleNativeArray<float3> new_local_positions;
    ExSimpleNativeArray<float3> new_local_normals;
    ExSimpleNativeArray<float3> new_local_tangents;
    ExSimpleNativeArray<float2> new_uv;
    ExSimpleNativeArray<VirtualMeshBoneWeight> new_bone_weights;

    int new_skin_bone_count = 0;
    std::vector<int> new_skin_bone_transform_indices;
    std::vector<float4x4> new_skin_bone_bind_pose_list;

    std::vector<int2> new_line_list;
    std::vector<int3> new_triangle_list;

    ReductionWorkData() = default;
    explicit ReductionWorkData(VirtualMesh* mesh)
        : vmesh(mesh)
    {
    }

    [[nodiscard]] int VertexCount() const
    {
        return !vertex_join_indices.empty()
            ? static_cast<int>(vertex_join_indices.size())
            : old_vertex_count;
    }

    [[nodiscard]] int LiveVertexCount() const
    {
        int count = 0;
        for (int join : vertex_join_indices) {
            if (join < 0) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] int RemovedVertexCount() const
    {
        int count = 0;
        for (int join : vertex_join_indices) {
            if (join >= 0) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] int LinkCount(int vertex_index) const
    {
        if (!CanStoreU16(vertex_index)) {
            return 0;
        }
        const auto found = vertex_to_vertex_map.find(static_cast<std::uint16_t>(vertex_index));
        return found != vertex_to_vertex_map.end() ? static_cast<int>(found->second.size()) : 0;
    }

    [[nodiscard]] bool HasVertexLink(int vertex_index, int link_index) const
    {
        if (!CanStoreU16(vertex_index) || !CanStoreU16(link_index)) {
            return false;
        }
        const auto found = vertex_to_vertex_map.find(static_cast<std::uint16_t>(vertex_index));
        if (found == vertex_to_vertex_map.end()) {
            return false;
        }
        const auto value = static_cast<std::uint16_t>(link_index);
        return std::find(found->second.begin(), found->second.end(), value) != found->second.end();
    }

    bool AddUniqueVertexLink(int vertex_index, int link_index)
    {
        if (!CanStoreU16(vertex_index) || !CanStoreU16(link_index) || vertex_index == link_index) {
            return false;
        }
        return AddUniqueLink(
            vertex_to_vertex_map,
            static_cast<std::uint16_t>(vertex_index),
            static_cast<std::uint16_t>(link_index)
        );
    }

    bool RemoveVertexLink(int vertex_index, int link_index)
    {
        if (!CanStoreU16(vertex_index) || !CanStoreU16(link_index)) {
            return false;
        }
        return RemoveLink(
            vertex_to_vertex_map,
            static_cast<std::uint16_t>(vertex_index),
            static_cast<std::uint16_t>(link_index)
        );
    }

    void ReplaceVertexLinks(int vertex_index, std::vector<std::uint16_t> links)
    {
        if (!CanStoreU16(vertex_index)) {
            return;
        }
        vertex_to_vertex_map[static_cast<std::uint16_t>(vertex_index)] = std::move(links);
    }

    void RemoveVertexLinks(int vertex_index)
    {
        if (CanStoreU16(vertex_index)) {
            vertex_to_vertex_map.erase(static_cast<std::uint16_t>(vertex_index));
        }
    }

    [[nodiscard]] std::vector<std::uint16_t> GatherMergedLinks(int vertex_index, int target_vertex_index) const
    {
        std::vector<std::uint16_t> links;
        GatherLinksForMerge(links, vertex_index, target_vertex_index, vertex_index);
        GatherLinksForMerge(links, target_vertex_index, vertex_index, target_vertex_index);
        return links;
    }

    [[nodiscard]] int ResolveJoinRoot(int index) const
    {
        return ResolveJoinRoot(vertex_join_indices, index);
    }

    void RefreshJoinAndLinks(int vertex_count)
    {
        if (vertex_count <= 0 || vertex_join_indices.empty()) {
            vertex_to_vertex_map.clear();
            return;
        }

        vertex_count = std::min(vertex_count, static_cast<int>(vertex_join_indices.size()));
        for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
            int& join = vertex_join_indices[static_cast<std::size_t>(vertex_index)];
            if (join >= 0) {
                join = ResolveJoinRoot(join);
            }
        }

        std::unordered_map<std::uint16_t, std::vector<std::uint16_t>> new_map;
        for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
            if (vertex_join_indices[static_cast<std::size_t>(vertex_index)] >= 0
                || !CanStoreU16(vertex_index)) {
                continue;
            }

            const auto found = vertex_to_vertex_map.find(static_cast<std::uint16_t>(vertex_index));
            if (found == vertex_to_vertex_map.end()) {
                continue;
            }

            for (std::uint16_t old_link : found->second) {
                int link = static_cast<int>(old_link);
                if (link < 0 || link >= vertex_count) {
                    continue;
                }
                link = ResolveJoinRoot(link);
                if (link == vertex_index || !CanStoreU16(link)) {
                    continue;
                }
                AddUniqueLink(
                    new_map,
                    static_cast<std::uint16_t>(vertex_index),
                    static_cast<std::uint16_t>(link)
                );
            }
        }
        vertex_to_vertex_map = std::move(new_map);
    }

    void ClearOrganizationData()
    {
        vertex_remap_indices.clear();
        old_vertex_to_new_vertex_indices.clear();
        use_skin_bone_map.clear();
        new_vertex_to_vertex_map.clear();
        edge_set.clear();
        triangle_set.clear();
        new_attributes.Dispose();
        new_local_positions.Dispose();
        new_local_normals.Dispose();
        new_local_tangents.Dispose();
        new_uv.Dispose();
        new_bone_weights.Dispose();
        new_skin_bone_count = 0;
        new_skin_bone_transform_indices.clear();
        new_skin_bone_bind_pose_list.clear();
        new_line_list.clear();
        new_triangle_list.clear();
        old_vertex_count = 0;
        new_vertex_count = 0;
        remove_vertex_count = 0;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "ReductionWorkData("
               << "vertexCount:" << VertexCount()
               << ", live:" << LiveVertexCount()
               << ", removed:" << RemovedVertexCount()
               << ", linkKeys:" << vertex_to_vertex_map.size()
               << ", newVertexCount:" << new_vertex_count
               << ", newSkinBoneCount:" << new_skin_bone_count
               << ", newLines:" << new_line_list.size()
               << ", newTriangles:" << new_triangle_list.size()
               << ")";
        return stream.str();
    }

    void Dispose()
    {
        vertex_join_indices.clear();
        vertex_to_vertex_map.clear();
        ClearOrganizationData();
    }

    [[nodiscard]] static bool CanStoreU16(int value)
    {
        return value >= 0 && value <= std::numeric_limits<std::uint16_t>::max();
    }

    static bool AddUniqueLink(
        std::unordered_map<std::uint16_t, std::vector<std::uint16_t>>& map,
        std::uint16_t key,
        std::uint16_t value
    )
    {
        std::vector<std::uint16_t>& values = map[key];
        if (std::find(values.begin(), values.end(), value) != values.end()) {
            return false;
        }
        values.push_back(value);
        return true;
    }

    static bool RemoveLink(
        std::unordered_map<std::uint16_t, std::vector<std::uint16_t>>& map,
        std::uint16_t key,
        std::uint16_t value
    )
    {
        const auto found = map.find(key);
        if (found == map.end()) {
            return false;
        }
        std::vector<std::uint16_t>& values = found->second;
        const auto old_size = values.size();
        values.erase(std::remove(values.begin(), values.end(), value), values.end());
        const bool removed = values.size() != old_size;
        if (values.empty()) {
            map.erase(found);
        }
        return removed;
    }

    [[nodiscard]] static int ResolveJoinRoot(const std::vector<int>& join_indices, int index)
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

private:
    void GatherLinksForMerge(
        std::vector<std::uint16_t>& links,
        int source_index,
        int reject_a,
        int reject_b
    ) const
    {
        if (!CanStoreU16(source_index)) {
            return;
        }
        const auto found = vertex_to_vertex_map.find(static_cast<std::uint16_t>(source_index));
        if (found == vertex_to_vertex_map.end()) {
            return;
        }
        for (std::uint16_t link : found->second) {
            if (link == reject_a || link == reject_b) {
                continue;
            }
            if (std::find(links.begin(), links.end(), link) == links.end()) {
                links.push_back(link);
            }
        }
    }
};

}  // namespace hocloth::mc2
