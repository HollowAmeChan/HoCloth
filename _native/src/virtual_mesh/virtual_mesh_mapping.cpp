#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/utility/native_collection/ex_cost_sorted_list1.hpp"
#include "hocloth/utility/native_collection/ex_cost_sorted_list4.hpp"

#include <cmath>
#include <queue>
#include <unordered_set>
#include <vector>

namespace hocloth::mc2 {

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
    if (!merge_chunk.IsValid() || proxy_mesh.join_indices.Count() <= 0) {
        return;
    }

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const int join_index = merge_chunk.start_index + vertex_index;
        if (join_index < 0 || join_index >= proxy_mesh.join_indices.Count()) {
            attributes[vertex_index] = VertexAttribute::Invalid();
            continue;
        }

        const int proxy_vertex_index = proxy_mesh.join_indices[join_index];
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

}  // namespace hocloth::mc2
