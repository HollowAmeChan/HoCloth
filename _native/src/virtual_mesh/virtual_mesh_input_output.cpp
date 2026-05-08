#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hocloth::mc2 {

namespace {

void AddUniquePackedEdge(std::unordered_set<std::uint32_t>& edge_set, int a, int b)
{
    if (a < 0 || b < 0 || a == b) {
        return;
    }
    edge_set.insert(data::Pack32Sort(a, b));
}

void AddUniqueEdge(std::vector<int2>& edges, std::unordered_set<std::uint32_t>& edge_set, int a, int b)
{
    if (a < 0 || b < 0 || a == b) {
        return;
    }
    const std::uint32_t key = data::Pack32Sort(a, b);
    if (edge_set.insert(key).second) {
        edges.push_back(data::PackInt2(a, b));
    }
}

std::uint64_t PackedTriangleKey(const int3& triangle)
{
    const int3 packed = data::PackInt3(triangle);
    return (static_cast<std::uint64_t>(packed.x) << 32)
        | (static_cast<std::uint64_t>(packed.y) << 16)
        | static_cast<std::uint64_t>(packed.z);
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

int AddTransformRecord(TransformData& destination, const TransformRecord& record, bool check_duplicate = true)
{
    if (!record.IsValid()) {
        return -1;
    }
    if (check_duplicate && record.id != 0) {
        const auto found = std::find(destination.id_array.begin(), destination.id_array.end(), record.id);
        if (found != destination.id_array.end()) {
            return static_cast<int>(std::distance(destination.id_array.begin(), found));
        }
    }

    const int destination_index = destination.Count();
    destination.flag_array.Add(BitFlag8{TransformManager::FlagRead});
    destination.init_local_position_array.Add(record.local_position);
    destination.init_local_rotation_array.Add(record.local_rotation);
    destination.position_array.Add(record.position);
    destination.rotation_array.Add(record.rotation);
    destination.inverse_rotation_array.Add(Inverse(record.rotation));
    destination.scale_array.Add(record.scale);
    destination.local_position_array.Add(record.local_position);
    destination.local_rotation_array.Add(record.local_rotation);
    destination.local_to_world_matrix_array.Add(record.local_to_world_matrix);
    destination.team_id_array.Add(0);

    destination.EnsureRecordCapacity(destination_index + 1);
    destination.id_array[static_cast<std::size_t>(destination_index)] = record.id;
    destination.parent_id_array[static_cast<std::size_t>(destination_index)] = record.parent_id;
    destination.name_array[static_cast<std::size_t>(destination_index)] = record.name;
    destination.is_dirty = true;
    return destination_index;
}

void ReplaceTransformRecord(TransformData& destination, int index, const TransformRecord& record)
{
    if (!record.IsValid() || index < 0 || index >= destination.Count()) {
        return;
    }

    destination.flag_array[index] = BitFlag8{TransformManager::FlagRead};
    destination.init_local_position_array[index] = record.local_position;
    destination.init_local_rotation_array[index] = record.local_rotation;
    destination.position_array[index] = record.position;
    destination.rotation_array[index] = record.rotation;
    destination.inverse_rotation_array[index] = Inverse(record.rotation);
    destination.scale_array[index] = record.scale;
    destination.local_position_array[index] = record.local_position;
    destination.local_rotation_array[index] = record.local_rotation;
    destination.local_to_world_matrix_array[index] = record.local_to_world_matrix;
    destination.team_id_array[index] = 0;
    destination.EnsureRecordCapacity(index + 1);
    destination.id_array[static_cast<std::size_t>(index)] = record.id;
    destination.parent_id_array[static_cast<std::size_t>(index)] = record.parent_id;
    destination.name_array[static_cast<std::size_t>(index)] = record.name;
    destination.is_dirty = true;
}

template <typename T>
T ExSimpleValueOrDefault(const ExSimpleNativeArray<T>& array, int index, const T& fallback = T{})
{
    return index >= 0 && index < array.Count() ? array[index] : fallback;
}

template <typename T>
T ExNativeValueOrDefault(const ExNativeArray<T>& array, int index, const T& fallback = T{})
{
    return index >= 0 && index < array.Count() ? array[index] : fallback;
}

int AddTransformRecordFromSource(TransformData& destination, const TransformData& source, int source_index)
{
    if (source_index < 0 || source_index >= source.Count()) {
        return -1;
    }

    const int source_id =
        source_index < static_cast<int>(source.id_array.size())
            ? source.id_array[static_cast<std::size_t>(source_index)]
            : 0;
    if (source_id != 0) {
        const auto found = std::find(destination.id_array.begin(), destination.id_array.end(), source_id);
        if (found != destination.id_array.end()) {
            return static_cast<int>(std::distance(destination.id_array.begin(), found));
        }
    }

    const int destination_index = destination.Count();
    destination.flag_array.Add(ExNativeValueOrDefault(source.flag_array, source_index, BitFlag8{}));
    destination.init_local_position_array.Add(
        ExNativeValueOrDefault(source.init_local_position_array, source_index, float3{})
    );
    destination.init_local_rotation_array.Add(
        ExNativeValueOrDefault(source.init_local_rotation_array, source_index, quaternion{})
    );
    destination.position_array.Add(
        ExNativeValueOrDefault(source.position_array, source_index, float3{})
    );
    destination.rotation_array.Add(
        ExNativeValueOrDefault(source.rotation_array, source_index, quaternion{})
    );
    destination.inverse_rotation_array.Add(
        ExNativeValueOrDefault(source.inverse_rotation_array, source_index, quaternion{})
    );
    destination.scale_array.Add(
        ExNativeValueOrDefault(source.scale_array, source_index, float3{1.0f, 1.0f, 1.0f})
    );
    destination.local_position_array.Add(
        ExNativeValueOrDefault(source.local_position_array, source_index, float3{})
    );
    destination.local_rotation_array.Add(
        ExNativeValueOrDefault(source.local_rotation_array, source_index, quaternion{})
    );
    destination.local_to_world_matrix_array.Add(
        ExNativeValueOrDefault(source.local_to_world_matrix_array, source_index, float4x4{})
    );
    destination.team_id_array.Add(
        ExNativeValueOrDefault<std::int16_t>(source.team_id_array, source_index, 0)
    );

    destination.EnsureRecordCapacity(destination_index + 1);
    if (destination_index < static_cast<int>(destination.id_array.size())) {
        destination.id_array[static_cast<std::size_t>(destination_index)] = source_id;
    }
    if (destination_index < static_cast<int>(destination.parent_id_array.size())
        && source_index < static_cast<int>(source.parent_id_array.size())) {
        destination.parent_id_array[static_cast<std::size_t>(destination_index)] =
            source.parent_id_array[static_cast<std::size_t>(source_index)];
    }
    if (destination_index < static_cast<int>(destination.name_array.size())
        && source_index < static_cast<int>(source.name_array.size())) {
        destination.name_array[static_cast<std::size_t>(destination_index)] =
            source.name_array[static_cast<std::size_t>(source_index)];
    }
    destination.is_dirty = true;
    return destination_index;
}

void ImportRenderSetupTransforms(VirtualMesh& mesh, const RenderSetupData& render_setup)
{
    const int transform_count = render_setup.TransformCount();
    mesh.transform_data.Dispose();
    mesh.transform_data.Initialize(transform_count);
    mesh.transform_data.flag_array.AddRange(transform_count);
    mesh.transform_data.init_local_position_array.AddRange(transform_count);
    mesh.transform_data.init_local_rotation_array.AddRange(transform_count);
    mesh.transform_data.position_array.AddRange(transform_count);
    mesh.transform_data.rotation_array.AddRange(transform_count);
    mesh.transform_data.inverse_rotation_array.AddRange(transform_count);
    mesh.transform_data.scale_array.AddRange(transform_count);
    mesh.transform_data.local_position_array.AddRange(transform_count);
    mesh.transform_data.local_rotation_array.AddRange(transform_count);
    mesh.transform_data.local_to_world_matrix_array.AddRange(transform_count);
    mesh.transform_data.team_id_array.AddRange(transform_count);
    mesh.transform_data.root_id_list = render_setup.root_transform_ids;

    for (int index = 0; index < transform_count; ++index) {
        const TransformRecord record = render_setup.GetTransformRecordFromIndex(index);
        mesh.transform_data.flag_array[index] =
            BitFlag8{TransformManager::FlagRead | TransformManager::FlagEnable};
        mesh.transform_data.init_local_position_array[index] = record.local_position;
        mesh.transform_data.init_local_rotation_array[index] = record.local_rotation;
        mesh.transform_data.position_array[index] = record.position;
        mesh.transform_data.rotation_array[index] = record.rotation;
        mesh.transform_data.inverse_rotation_array[index] =
            index < static_cast<int>(render_setup.transform_inverse_rotations.size())
                ? render_setup.transform_inverse_rotations[static_cast<std::size_t>(index)]
                : Inverse(record.rotation);
        mesh.transform_data.scale_array[index] = record.scale;
        mesh.transform_data.local_position_array[index] = record.local_position;
        mesh.transform_data.local_rotation_array[index] = record.local_rotation;
        mesh.transform_data.local_to_world_matrix_array[index] = record.local_to_world_matrix;
        mesh.transform_data.team_id_array[index] = 0;
        mesh.transform_data.id_array[static_cast<std::size_t>(index)] = record.id;
        mesh.transform_data.parent_id_array[static_cast<std::size_t>(index)] = record.parent_id;
        mesh.transform_data.name_array[static_cast<std::size_t>(index)] = record.name;
    }
    mesh.transform_data.is_dirty = true;
}

}  // namespace

void VirtualMesh::ImportFromRenderSetup(const RenderSetupData& render_setup)
{
    // Ported from Magica Cloth 2: VirtualMesh.ImportFrom(RenderSetupData).
    Dispose();
    name = render_setup.name;
    if (render_setup.IsFailed()) {
        result = Result::Error(
            ResultCode::VirtualMesh_InvalidSetup,
            "VirtualMesh render setup is failed."
        );
        return;
    }

    switch (render_setup.setup_type) {
    case RenderSetupData::SetupType::MeshCloth:
        mesh_type = MeshType::NormalMesh;
        is_bone_cloth = false;
        ImportMeshType(render_setup);
        if (!result.Succeeded()) {
            return;
        }
        break;
    case RenderSetupData::SetupType::BoneCloth:
    case RenderSetupData::SetupType::BoneSpring:
        mesh_type = MeshType::NormalBoneMesh;
        is_bone_cloth = true;
        ImportBoneType(render_setup);
        if (!result.Succeeded()) {
            return;
        }
        break;
    default:
        result = Result::Error(
            ResultCode::RenderSetup_InvalidType,
            "Unknown RenderSetup type."
        );
        return;
    }

    bounding_box = AABB{};
    for (int index = 0; index < local_positions.Count(); ++index) {
        Encapsulate(bounding_box, local_positions[index]);
    }
    if (TriangleCount() > 0 && uv.Count() == VertexCount()) {
        for (int index = 0; index < VertexCount(); ++index) {
            uv[index] = SphereMappingUV(local_positions[index], bounding_box, index);
        }
    }
    CalcAverageAndMaxVertexDistanceRun();
    result = Result::Ok();
}

void VirtualMesh::BuildBoneConnection(const RenderSetupData& render_setup)
{
    // Ported from MC2 VirtualMeshInputOutput.cs BoneConnectionMode construction.
    const RenderSetupData::BoneConnectionMode connection_mode = render_setup.bone_connection_mode;
    const bool setup_as_bone_spring =
        render_setup.setup_type == RenderSetupData::SetupType::BoneSpring;
    const int vertex_count = VertexCount();
    lines.Dispose();
    edges.Dispose();
    triangles.Dispose();
    edge_flags.Dispose();
    edge_to_triangles.clear();

    if (vertex_count <= 0
        || local_positions.Count() < vertex_count
        || transform_data.id_array.size() < static_cast<std::size_t>(vertex_count)
        || transform_data.parent_id_array.size() < static_cast<std::size_t>(vertex_count)) {
        return;
    }

    std::unordered_map<int, int> id_to_index;
    id_to_index.reserve(static_cast<std::size_t>(vertex_count));
    std::vector<int> root_ids;
    root_ids.reserve(transform_data.root_id_list.size());
    std::unordered_set<int> root_seen;
    for (int root_id : transform_data.root_id_list) {
        if (root_id != 0 && root_seen.insert(root_id).second) {
            root_ids.push_back(root_id);
        }
    }
    for (int index = 0; index < vertex_count; ++index) {
        const int transform_id = transform_data.id_array[static_cast<std::size_t>(index)];
        if (!id_to_index.contains(transform_id)) {
            id_to_index.emplace(transform_id, index);
        }
        if (transform_data.parent_id_array[static_cast<std::size_t>(index)] == 0
            && transform_id != 0
            && root_seen.insert(transform_id).second) {
            root_ids.push_back(transform_id);
        }
    }
    if (root_ids.empty() && !transform_data.id_array.empty()) {
        root_ids.push_back(transform_data.id_array.front());
    }
    root_ids.erase(
        std::remove_if(
            root_ids.begin(),
            root_ids.end(),
            [&id_to_index](int root_id) {
                return id_to_index.find(root_id) == id_to_index.end();
            }
        ),
        root_ids.end()
    );
    if (root_ids.empty()) {
        return;
    }
    const int original_root_count = static_cast<int>(root_ids.size());

    std::vector<int2> line_list;
    std::unordered_set<std::uint32_t> line_keys;
    auto add_line = [&](int a, int b) {
        AddUniqueEdge(line_list, line_keys, a, b);
    };
    auto apply_bone_spring_collision_attributes = [&]() {
        if (!setup_as_bone_spring || attributes.Count() < vertex_count) {
            return;
        }
        attributes.Fill(0, vertex_count, VertexAttribute::DisableCollision());
        for (int collision_index : render_setup.collision_bone_indices) {
            if (collision_index >= 0 && collision_index < vertex_count) {
                attributes[collision_index] = VertexAttribute::Invalid();
            }
        }
    };

    if (connection_mode == RenderSetupData::BoneConnectionMode::Line) {
        for (int index = 0; index < vertex_count; ++index) {
            const int parent_index = render_setup.GetParentTransformIndex(index, true);
            if (parent_index >= 0 && parent_index < vertex_count) {
                add_line(parent_index, index);
            }
        }

        apply_bone_spring_collision_attributes();
        if (!line_list.empty()) {
            lines.AddRange(line_list);
            edges.AddRange(line_list);
            edge_flags.AddRange(static_cast<int>(line_list.size()), BitFlag8{});
            BuildEdgeToTriangles();
        }
        return;
    }

    bool loop_connection =
        connection_mode == RenderSetupData::BoneConnectionMode::SequentialLoopMesh;
    const bool sequential_connection =
        connection_mode == RenderSetupData::BoneConnectionMode::SequentialLoopMesh
        || connection_mode == RenderSetupData::BoneConnectionMode::SequentialNonLoopMesh;

    std::vector<int> ordered_root_ids = root_ids;
    if (connection_mode == RenderSetupData::BoneConnectionMode::AutomaticMesh
        && ordered_root_ids.size() > 1) {
        std::vector<int> remaining_root_ids = ordered_root_ids;
        ordered_root_ids.clear();
        ordered_root_ids.push_back(remaining_root_ids.front());
        float last_distance = 0.0f;
        while (!remaining_root_ids.empty()) {
            const int root_id = ordered_root_ids.back();
            remaining_root_ids.erase(
                std::remove(remaining_root_ids.begin(), remaining_root_ids.end(), root_id),
                remaining_root_ids.end()
            );
            if (remaining_root_ids.empty()) {
                break;
            }

            const auto found_root = id_to_index.find(root_id);
            if (found_root == id_to_index.end()) {
                break;
            }
            const int root_index = found_root->second;
            const float3 position = local_positions[root_index];
            float min_distance = std::numeric_limits<float>::max();
            int min_id = 0;
            for (int candidate_id : remaining_root_ids) {
                const auto found_candidate = id_to_index.find(candidate_id);
                if (found_candidate == id_to_index.end()) {
                    continue;
                }
                const float distance = Distance(position, local_positions[found_candidate->second]);
                if (distance < min_distance) {
                    min_distance = distance;
                    min_id = candidate_id;
                }
            }
            if (min_id == 0) {
                break;
            }
            if (last_distance == 0.0f || min_distance < last_distance * 1.5f) {
                ordered_root_ids.push_back(min_id);
                last_distance = last_distance == 0.0f
                    ? min_distance
                    : (last_distance + min_distance) * 0.5f;
            } else {
                std::reverse(ordered_root_ids.begin(), ordered_root_ids.end());
                last_distance = 0.0f;
            }
        }

        if (ordered_root_ids.size() >= 3 && last_distance > 0.0f) {
            const int first_root = ordered_root_ids.front();
            const int last_root = ordered_root_ids.back();
            const auto found_first = id_to_index.find(first_root);
            const auto found_last = id_to_index.find(last_root);
            if (found_first != id_to_index.end() && found_last != id_to_index.end()) {
                const float distance = Distance(
                    local_positions[found_first->second],
                    local_positions[found_last->second]
                );
                if (distance < last_distance * 1.5f) {
                    loop_connection = true;
                }
            }
        }
    }

    std::unordered_map<int, std::vector<int>> children_by_parent_id;
    children_by_parent_id.reserve(static_cast<std::size_t>(vertex_count));
    const bool has_child_id_list =
        static_cast<int>(render_setup.transform_child_ids.size()) >= vertex_count;
    for (int index = 0; index < vertex_count; ++index) {
        const int transform_id = transform_data.id_array[static_cast<std::size_t>(index)];
        if (has_child_id_list) {
            const std::vector<int>& child_ids =
                render_setup.transform_child_ids[static_cast<std::size_t>(index)];
            for (int child_id : child_ids) {
                if (id_to_index.contains(child_id)) {
                    children_by_parent_id[transform_id].push_back(child_id);
                }
            }
        } else {
            const int parent_id = transform_data.parent_id_array[static_cast<std::size_t>(index)];
            if (id_to_index.contains(parent_id)) {
                children_by_parent_id[parent_id].push_back(transform_id);
            }
        }
    }

    std::vector<std::vector<int>> link_list(static_cast<std::size_t>(vertex_count));
    std::vector<int> vertex_level(static_cast<std::size_t>(vertex_count), 0);
    std::vector<int> vertex_root_index(static_cast<std::size_t>(vertex_count), 0);
    std::map<int, std::vector<int>> vertices_by_level;
    std::unordered_set<std::uint32_t> main_edge_set;
    std::vector<std::uint8_t> visited(static_cast<std::size_t>(vertex_count), 0);

    for (std::size_t root_order = 0;
         root_order < ordered_root_ids.size()
         && root_order < static_cast<std::size_t>(original_root_count);
         ++root_order) {
        const int root_id = ordered_root_ids[root_order];
        if (!id_to_index.contains(root_id)) {
            continue;
        }

        std::stack<int> id_stack;
        std::stack<int> level_stack;
        id_stack.push(root_id);
        level_stack.push(0);
        while (!id_stack.empty()) {
            const int id = id_stack.top();
            id_stack.pop();
            const int level = level_stack.top();
            level_stack.pop();
            const auto found_index = id_to_index.find(id);
            if (found_index == id_to_index.end()) {
                continue;
            }

            const int vertex_index = found_index->second;
            if (visited[static_cast<std::size_t>(vertex_index)] != 0) {
                continue;
            }
            visited[static_cast<std::size_t>(vertex_index)] = 1;
            vertex_level[static_cast<std::size_t>(vertex_index)] = level;
            vertex_root_index[static_cast<std::size_t>(vertex_index)] =
                static_cast<int>(root_order);
            vertices_by_level[level].push_back(vertex_index);

            const int parent_id = transform_data.parent_id_array[static_cast<std::size_t>(vertex_index)];
            const auto found_parent = id_to_index.find(parent_id);
            if (found_parent != id_to_index.end()) {
                link_list[static_cast<std::size_t>(vertex_index)].push_back(found_parent->second);
                AddUniquePackedEdge(main_edge_set, vertex_index, found_parent->second);
            }

            const auto found_children = children_by_parent_id.find(id);
            if (found_children == children_by_parent_id.end()) {
                continue;
            }
            for (int child_id : found_children->second) {
                const auto found_child = id_to_index.find(child_id);
                if (found_child == id_to_index.end()) {
                    continue;
                }
                id_stack.push(child_id);
                level_stack.push(level + 1);
                link_list[static_cast<std::size_t>(vertex_index)].push_back(found_child->second);
                AddUniquePackedEdge(main_edge_set, vertex_index, found_child->second);
            }
        }
    }

    const int first_root_index = 0;
    const int last_root_index = original_root_count - 1;
    const std::uint32_t start_end_root_pack = data::Pack32Sort(first_root_index, last_root_index);
    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const int level = vertex_level[static_cast<std::size_t>(vertex_index)];
        const auto found_level = vertices_by_level.find(level);
        if (found_level == vertices_by_level.end()) {
            continue;
        }

        std::vector<int>& link = link_list[static_cast<std::size_t>(vertex_index)];
        const int root_index = vertex_root_index[static_cast<std::size_t>(vertex_index)];
        const float3 position = local_positions[vertex_index];
        float first_distance = std::numeric_limits<float>::max();
        int first_index = -1;

        for (int candidate : found_level->second) {
            if (candidate == vertex_index) {
                continue;
            }
            const int candidate_root = vertex_root_index[static_cast<std::size_t>(candidate)];
            const bool first_last =
                start_end_root_pack == data::Pack32Sort(root_index, candidate_root)
                && start_end_root_pack > 0;
            if (!loop_connection && first_last) {
                continue;
            }
            if (sequential_connection && !(loop_connection && first_last)
                && std::abs(root_index - candidate_root) > 1) {
                continue;
            }

            const float distance = Distance(position, local_positions[candidate]);
            if (distance < first_distance) {
                first_distance = distance;
                first_index = candidate;
            }
        }

        if (first_index < 0) {
            continue;
        }

        link.push_back(first_index);
        first_distance = sequential_connection
            ? std::numeric_limits<float>::max()
            : first_distance * 1.5f;
        for (int candidate : found_level->second) {
            if (candidate == vertex_index || candidate == first_index) {
                continue;
            }
            const int candidate_root = vertex_root_index[static_cast<std::size_t>(candidate)];
            const bool first_last =
                start_end_root_pack == data::Pack32Sort(root_index, candidate_root)
                && start_end_root_pack > 0;
            if (!loop_connection && first_last) {
                continue;
            }
            if (sequential_connection && !(loop_connection && first_last)
                && std::abs(root_index - candidate_root) > 1) {
                continue;
            }
            if (Distance(position, local_positions[candidate]) <= first_distance) {
                link.push_back(candidate);
            }
        }
    }

    std::vector<int2> edge_list;
    std::vector<int2> triangle_edge_list;
    std::vector<int3> triangle_list;
    std::unordered_set<std::uint32_t> edge_set;
    std::unordered_set<std::uint32_t> triangle_edge_set;
    std::unordered_set<std::uint64_t> triangle_set;

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const std::vector<int>& link = link_list[static_cast<std::size_t>(vertex_index)];
        if (link.empty()) {
            continue;
        }
        if (link.size() == 1) {
            AddUniqueEdge(edge_list, edge_set, vertex_index, link.front());
            continue;
        }

        for (int linked : link) {
            AddUniqueEdge(edge_list, edge_set, vertex_index, linked);
        }

        const int root_index = vertex_root_index[static_cast<std::size_t>(vertex_index)];
        const float3 position = local_positions[vertex_index];
        for (std::size_t j = 0; j + 1 < link.size(); ++j) {
            const int v1_index = link[j];
            const float3 v1 = Subtract(local_positions[v1_index], position);
            for (std::size_t k = j + 1; k < link.size(); ++k) {
                const int v2_index = link[k];
                const float3 v2 = Subtract(local_positions[v2_index], position);
                if (LengthSquared(v1) < 1.0e-6f || LengthSquared(v2) < 1.0e-6f) {
                    continue;
                }
                const float angle_degrees = Angle(v1, v2) * 57.29577951308232f;
                if (angle_degrees >= define::system::ProxyMeshBoneClothTriangleAngle) {
                    continue;
                }

                const int root1 = vertex_root_index[static_cast<std::size_t>(v1_index)];
                const int root2 = vertex_root_index[static_cast<std::size_t>(v2_index)];
                if (root1 != root_index && root2 != root_index && root1 != root2) {
                    continue;
                }

                int main_edge_count = 0;
                main_edge_count += main_edge_set.contains(data::Pack32Sort(vertex_index, v1_index)) ? 1 : 0;
                main_edge_count += main_edge_set.contains(data::Pack32Sort(vertex_index, v2_index)) ? 1 : 0;
                main_edge_count += main_edge_set.contains(data::Pack32Sort(v1_index, v2_index)) ? 1 : 0;
                if (main_edge_count == 0) {
                    continue;
                }

                const int3 triangle = data::PackInt3(vertex_index, v1_index, v2_index);
                const std::uint64_t triangle_key = PackedTriangleKey(triangle);
                if (!triangle_set.insert(triangle_key).second) {
                    continue;
                }
                triangle_list.push_back(triangle);
                const int2 edge0 = data::PackInt2(vertex_index, v1_index);
                const int2 edge1 = data::PackInt2(vertex_index, v2_index);
                const std::uint32_t edge_key0 = data::Pack32(edge0.x, edge0.y);
                const std::uint32_t edge_key1 = data::Pack32(edge1.x, edge1.y);
                if (triangle_edge_set.insert(edge_key0).second) {
                    triangle_edge_list.push_back(edge0);
                }
                if (triangle_edge_set.insert(edge_key1).second) {
                    triangle_edge_list.push_back(edge1);
                }
            }
        }
    }

    if (!triangle_list.empty()) {
        triangles.AddRange(triangle_list);
    }
    for (const int2& edge : triangle_edge_list) {
        edge_set.erase(data::Pack32(edge.x, edge.y));
    }
    std::vector<int2> remaining_line_list;
    remaining_line_list.reserve(edge_list.size());
    for (const int2& edge : edge_list) {
        if (edge_set.contains(data::Pack32(edge.x, edge.y))) {
            remaining_line_list.push_back(edge);
        }
    }
    if (!remaining_line_list.empty()) {
        lines.AddRange(remaining_line_list);
    }
    if (!edge_list.empty()) {
        edges.AddRange(edge_list);
        edge_flags.AddRange(static_cast<int>(edge_list.size()), BitFlag8{});
    }
    BuildEdgeToTriangles();
}

void VirtualMesh::ImportMeshType(const RenderSetupData& render_setup)
{
    // Ported from MC2 ImportMeshType() for native/serialized mesh data.
    const int vertex_count = render_setup.vertex_count > 0
        ? render_setup.vertex_count
        : static_cast<int>(render_setup.local_positions.size());
    if (vertex_count <= 0
        || static_cast<int>(render_setup.local_positions.size()) < vertex_count) {
        result = Result::Error(
            ResultCode::VirtualMesh_InvalidSetup,
            "Mesh RenderSetup has no local vertex positions."
        );
        return;
    }

    ImportRenderSetupTransforms(*this, render_setup);
    center_transform_index = render_setup.render_transform_index;
    init_local_to_world = render_setup.init_render_local_to_world;
    init_world_to_local = render_setup.init_render_world_to_local;
    init_rotation = render_setup.init_render_rotation;
    init_inverse_rotation = Inverse(init_rotation);
    init_scale = render_setup.init_render_scale;
    skin_root_index = render_setup.skin_root_bone_index >= 0
        ? render_setup.skin_root_bone_index
        : center_transform_index;

    const int skin_bone_count = render_setup.skin_bone_count > 0
        ? render_setup.skin_bone_count
        : std::max(0, std::min(render_setup.TransformCount(), static_cast<int>(render_setup.bind_pose_list.size())));
    skin_bone_transform_indices.Dispose();
    skin_bone_bind_poses.Dispose();
    if (skin_bone_count > 0) {
        skin_bone_transform_indices.AddRange(skin_bone_count);
        skin_bone_bind_poses.AddRange(skin_bone_count);
        for (int index = 0; index < skin_bone_count; ++index) {
            skin_bone_transform_indices[index] = index;
            skin_bone_bind_poses[index] =
                index < static_cast<int>(render_setup.bind_pose_list.size())
                    ? render_setup.bind_pose_list[static_cast<std::size_t>(index)]
                    : float4x4{};
        }
    } else if (center_transform_index >= 0) {
        skin_bone_transform_indices.Add(center_transform_index);
        skin_bone_bind_poses.Add(init_world_to_local);
    }

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
    edge_to_triangles.clear();

    reference_indices.AddRange(vertex_count);
    attributes.AddRange(vertex_count, VertexAttribute::Invalid());
    local_positions.AddRange(vertex_count);
    local_normals.AddRange(vertex_count);
    local_tangents.AddRange(vertex_count);
    uv.AddRange(vertex_count);
    bone_weights.AddRange(vertex_count, VirtualMeshBoneWeight{});

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        reference_indices[vertex_index] = vertex_index;
        local_positions[vertex_index] = render_setup.local_positions[static_cast<std::size_t>(vertex_index)];
        local_normals[vertex_index] =
            vertex_index < static_cast<int>(render_setup.local_normals.size())
                ? Normalize(render_setup.local_normals[static_cast<std::size_t>(vertex_index)], float3{0.0f, 1.0f, 0.0f})
                : float3{0.0f, 1.0f, 0.0f};
        if (vertex_index < static_cast<int>(render_setup.local_tangents.size())) {
            local_tangents[vertex_index] =
                Normalize(render_setup.local_tangents[static_cast<std::size_t>(vertex_index)], float3{0.0f, 0.0f, 1.0f});
        } else {
            const float3 normal = local_normals[vertex_index];
            const float3 tangent_seed = Dot(normal, float3{0.0f, 1.0f, 0.0f}) < 0.9f
                ? float3{0.0f, 1.0f, 0.0f}
                : float3{1.0f, 0.0f, 0.0f};
            local_tangents[vertex_index] = Normalize(Cross(normal, tangent_seed), float3{0.0f, 0.0f, 1.0f});
        }
        uv[vertex_index] =
            vertex_index < static_cast<int>(render_setup.uv.size())
                ? render_setup.uv[static_cast<std::size_t>(vertex_index)]
                : float2{};
        if (vertex_index < static_cast<int>(render_setup.mesh_bone_weights.size())) {
            bone_weights[vertex_index] = render_setup.mesh_bone_weights[static_cast<std::size_t>(vertex_index)];
        } else {
            bone_weights[vertex_index] =
                VirtualMeshBoneWeight(int4{0, 0, 0, 0}, float4{1.0f, 0.0f, 0.0f, 0.0f});
        }
    }

    if (!render_setup.triangles.empty()) {
        triangles.AddRange(render_setup.triangles);
    }
    if (render_setup.has_bone_weight && skin_bone_transform_indices.Count() > 0) {
        for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
            VirtualMeshBoneWeight& bone_weight = bone_weights[vertex_index];
            const int count = bone_weight.Count();
            float3 world_position{};
            float3 world_normal{};
            float3 world_tangent{};
            for (int index = 0; index < count; ++index) {
                const float weight = bone_weight.weights[static_cast<std::size_t>(index)];
                const int bone_index = bone_weight.bone_indices[static_cast<std::size_t>(index)];
                if (weight <= define::system::Epsilon
                    || bone_index < 0
                    || bone_index >= skin_bone_transform_indices.Count()
                    || bone_index >= skin_bone_bind_poses.Count()) {
                    continue;
                }

                float3 position = TransformPoint(local_positions[vertex_index], skin_bone_bind_poses[bone_index]);
                float3 normal = TransformVector(local_normals[vertex_index], skin_bone_bind_poses[bone_index]);
                float3 tangent = TransformVector(local_tangents[vertex_index], skin_bone_bind_poses[bone_index]);
                const int transform_index = skin_bone_transform_indices[bone_index];
                if (transform_index >= 0 && transform_index < transform_data.Count()) {
                    TransformPositionNormalTangent(
                        transform_data.position_array[transform_index],
                        transform_data.rotation_array[transform_index],
                        transform_data.scale_array[transform_index],
                        position,
                        normal,
                        tangent
                    );
                }
                world_position = Add(world_position, Scale(position, weight));
                world_normal = Add(world_normal, Scale(normal, weight));
                world_tangent = Add(world_tangent, Scale(tangent, weight));
            }
            if (count > 0) {
                local_positions[vertex_index] = TransformPoint(world_position, init_world_to_local);
                local_normals[vertex_index] = TransformDirection(world_normal, init_world_to_local);
                local_tangents[vertex_index] = TransformDirection(world_tangent, init_world_to_local);
            }
        }
    }

    result = Result::Ok();
}

void VirtualMesh::ImportBoneType(const RenderSetupData& render_setup)
{
    // Ported from MC2 ImportBoneType(). The native form expects RenderSetupData to
    // already contain backend transform records in render-local/world pose space.
    const int transform_count = render_setup.TransformCount();
    const int vertex_count =
        render_setup.render_transform_index >= 0
            ? std::min(render_setup.render_transform_index, transform_count)
            : std::max(0, transform_count - 1);
    if (vertex_count <= 0 || transform_count <= 0) {
        result = Result::Error(
            ResultCode::VirtualMesh_InvalidSetup,
            "Bone RenderSetup has no bone transforms."
        );
        return;
    }

    center_transform_index = render_setup.render_transform_index >= 0
        ? render_setup.render_transform_index
        : transform_count - 1;
    init_local_to_world = render_setup.init_render_local_to_world;
    init_world_to_local = render_setup.init_render_world_to_local;
    init_rotation = render_setup.init_render_rotation;
    init_inverse_rotation = Inverse(init_rotation);
    init_scale = render_setup.init_render_scale;
    skin_root_index = render_setup.skin_root_bone_index;

    transform_data.Dispose();
    transform_data.Initialize(transform_count);
    transform_data.flag_array.AddRange(transform_count);
    transform_data.init_local_position_array.AddRange(transform_count);
    transform_data.init_local_rotation_array.AddRange(transform_count);
    transform_data.position_array.AddRange(transform_count);
    transform_data.rotation_array.AddRange(transform_count);
    transform_data.inverse_rotation_array.AddRange(transform_count);
    transform_data.scale_array.AddRange(transform_count);
    transform_data.local_position_array.AddRange(transform_count);
    transform_data.local_rotation_array.AddRange(transform_count);
    transform_data.local_to_world_matrix_array.AddRange(transform_count);
    transform_data.team_id_array.AddRange(transform_count);
    transform_data.root_id_list = render_setup.root_transform_ids;
    for (int index = 0; index < transform_count; ++index) {
        const int id = index < static_cast<int>(render_setup.transform_ids.size())
            ? render_setup.transform_ids[static_cast<std::size_t>(index)]
            : index + 1;
        const int parent_id = index < static_cast<int>(render_setup.transform_parent_ids.size())
            ? render_setup.transform_parent_ids[static_cast<std::size_t>(index)]
            : 0;
        transform_data.id_array[static_cast<std::size_t>(index)] = id;
        transform_data.parent_id_array[static_cast<std::size_t>(index)] = parent_id;
        transform_data.name_array[static_cast<std::size_t>(index)] =
            index < static_cast<int>(render_setup.transform_names.size())
                ? render_setup.transform_names[static_cast<std::size_t>(index)]
                : std::string{};

        BitFlag8 flag{
            TransformManager::FlagRead
            | TransformManager::FlagEnable
        };
        if (index < vertex_count) {
            flag.SetFlag(TransformManager::FlagRestore, true);
        }
        transform_data.flag_array[index] = flag;

        const float3 world_position =
            index < static_cast<int>(render_setup.transform_positions.size())
                ? render_setup.transform_positions[static_cast<std::size_t>(index)]
                : float3{};
        const quaternion world_rotation =
            index < static_cast<int>(render_setup.transform_rotations.size())
                ? render_setup.transform_rotations[static_cast<std::size_t>(index)]
                : quaternion{};
        const float3 world_scale =
            index < static_cast<int>(render_setup.transform_scales.size())
                ? render_setup.transform_scales[static_cast<std::size_t>(index)]
                : float3{1.0f, 1.0f, 1.0f};
        const float3 local_position =
            index < static_cast<int>(render_setup.transform_local_positions.size())
                ? render_setup.transform_local_positions[static_cast<std::size_t>(index)]
                : world_position;
        const quaternion local_rotation =
            index < static_cast<int>(render_setup.transform_local_rotations.size())
                ? render_setup.transform_local_rotations[static_cast<std::size_t>(index)]
                : world_rotation;
        const quaternion inverse_rotation =
            index < static_cast<int>(render_setup.transform_inverse_rotations.size())
                ? render_setup.transform_inverse_rotations[static_cast<std::size_t>(index)]
                : Inverse(world_rotation);

        transform_data.init_local_position_array[index] = local_position;
        transform_data.init_local_rotation_array[index] = local_rotation;
        transform_data.position_array[index] = world_position;
        transform_data.rotation_array[index] = world_rotation;
        transform_data.inverse_rotation_array[index] = inverse_rotation;
        transform_data.scale_array[index] = world_scale;
        transform_data.local_position_array[index] = local_position;
        transform_data.local_rotation_array[index] = local_rotation;
        transform_data.local_to_world_matrix_array[index] =
            index < static_cast<int>(render_setup.transform_local_to_world_matrices.size())
                ? render_setup.transform_local_to_world_matrices[static_cast<std::size_t>(index)]
                : TRS(world_position, world_rotation, world_scale);
        transform_data.team_id_array[index] = 0;
    }
    if (transform_data.root_id_list.empty()) {
        for (int index = 0; index < vertex_count; ++index) {
            if (transform_data.parent_id_array[static_cast<std::size_t>(index)] == 0) {
                transform_data.root_id_list.push_back(transform_data.id_array[static_cast<std::size_t>(index)]);
            }
        }
    }

    reference_indices.AddRange(vertex_count, 0);
    attributes.AddRange(vertex_count, VertexAttribute::Invalid());
    local_positions.AddRange(vertex_count, float3{});
    local_normals.AddRange(vertex_count, float3{0.0f, 1.0f, 0.0f});
    local_tangents.AddRange(vertex_count, float3{0.0f, 0.0f, 1.0f});
    uv.AddRange(vertex_count, float2{});
    bone_weights.AddRange(vertex_count, VirtualMeshBoneWeight{});
    skin_bone_transform_indices.AddRange(vertex_count, 0);
    skin_bone_bind_poses.AddRange(vertex_count, float4x4{});

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        reference_indices[vertex_index] = vertex_index;
        skin_bone_transform_indices[vertex_index] = vertex_index;

        const float3 world_position = transform_data.position_array[vertex_index];
        const quaternion world_rotation = transform_data.rotation_array[vertex_index];
        const float3 world_scale = transform_data.scale_array[vertex_index];
        const float4x4 local_to_world = transform_data.local_to_world_matrix_array[vertex_index];

        const float3 local_position = InverseTransformPoint(world_position, init_world_to_local);
        float3 local_normal = InverseTransformDirection(
            Rotate(world_rotation, float3{0.0f, 1.0f, 0.0f}),
            init_world_to_local
        );
        float3 local_tangent = InverseTransformDirection(
            Rotate(world_rotation, float3{0.0f, 0.0f, 1.0f}),
            init_world_to_local
        );
        if (Length(local_normal) <= define::system::Epsilon) {
            local_normal = float3{0.0f, 1.0f, 0.0f};
        }
        if (Length(local_tangent) <= define::system::Epsilon) {
            local_tangent = float3{0.0f, 0.0f, 1.0f};
        }

        local_positions[vertex_index] = local_position;
        local_normals[vertex_index] = Normalize(local_normal);
        local_tangents[vertex_index] = Normalize(local_tangent);
        bone_weights[vertex_index] = VirtualMeshBoneWeight{
            int4{vertex_index, -1, -1, -1},
            float4{1.0f, 0.0f, 0.0f, 0.0f}
        };
        skin_bone_bind_poses[vertex_index] = Multiply(InverseAffine(local_to_world), init_local_to_world);
        (void)world_scale;
    }

    BuildBoneConnection(render_setup);
    if (render_setup.setup_type == RenderSetupData::SetupType::BoneCloth) {
        ApplyBoneClothDefaultSelection();
    }
    result = Result::Ok();
}

void VirtualMesh::AddMesh(VirtualMesh& source_mesh)
{
    // Port target: Scripts/Core/VirtualMesh/Function/VirtualMeshInputOutput.cs AddMesh().
    if (!IsValid() || !source_mesh.IsValid()) {
        result = Result::Error(ResultCode::VirtualMesh_InvalidSetup, "VirtualMesh AddMesh source is invalid.");
        return;
    }

    const int skin_bone_start = SkinBoneCount();
    const int vertex_start = VertexCount();
    const int triangle_start = TriangleCount();
    const int line_start = LineCount();
    const int source_vertex_count = source_mesh.VertexCount();
    const int source_skin_bone_count = source_mesh.SkinBoneCount();

    const float4x4 to_mesh = source_mesh.CenterTransformTo(*this);

    std::vector<int> source_skin_to_destination(
        static_cast<std::size_t>(std::max(source_skin_bone_count, 0)),
        -1
    );
    skin_bone_transform_indices.AddRange(source_skin_bone_count);
    for (int bone_index = 0; bone_index < source_skin_bone_count; ++bone_index) {
        const int source_transform_index = source_mesh.skin_bone_transform_indices[bone_index];
        const int destination_transform_index =
            AddTransformRecordFromSource(transform_data, source_mesh.transform_data, source_transform_index);
        source_skin_to_destination[static_cast<std::size_t>(bone_index)] =
            destination_transform_index;
        skin_bone_transform_indices[skin_bone_start + bone_index] =
            destination_transform_index;
    }

    source_mesh.merge_chunk = DataChunk{vertex_start, source_vertex_count};

    attributes.AddRange(source_vertex_count);
    local_positions.AddRange(source_vertex_count);
    local_normals.AddRange(source_vertex_count);
    local_tangents.AddRange(source_vertex_count);
    uv.AddRange(source_vertex_count);
    bone_weights.AddRange(source_vertex_count);

    for (int source_index = 0; source_index < source_vertex_count; ++source_index) {
        const int destination_index = vertex_start + source_index;
        local_positions[destination_index] = TransformPoint(
            ExSimpleValueOrDefault(source_mesh.local_positions, source_index, float3{}),
            to_mesh
        );
        local_normals[destination_index] = TransformDirection(
            ExSimpleValueOrDefault(source_mesh.local_normals, source_index, float3{0.0f, 1.0f, 0.0f}),
            to_mesh
        );
        local_tangents[destination_index] = TransformDirection(
            ExSimpleValueOrDefault(source_mesh.local_tangents, source_index, float3{0.0f, 0.0f, 1.0f}),
            to_mesh
        );
        uv[destination_index] = ExSimpleValueOrDefault(source_mesh.uv, source_index, float2{});
        attributes[destination_index] =
            ExSimpleValueOrDefault(source_mesh.attributes, source_index, VertexAttribute::Invalid());

        VirtualMeshBoneWeight bone_weight =
            ExSimpleValueOrDefault(source_mesh.bone_weights, source_index, VirtualMeshBoneWeight{});
        for (int weight_index = 0; weight_index < 4; ++weight_index) {
            const int source_bone_index =
                bone_weight.bone_indices[static_cast<std::size_t>(weight_index)];
            if (bone_weight.weights[static_cast<std::size_t>(weight_index)] <= define::system::Epsilon
                || source_bone_index < 0
                || source_bone_index >= source_skin_bone_count) {
                bone_weight.bone_indices[static_cast<std::size_t>(weight_index)] = 0;
                continue;
            }

            int destination_bone_index = skin_bone_start + source_bone_index;
            const int destination_transform_index =
                source_skin_to_destination[static_cast<std::size_t>(source_bone_index)];
            for (int test_index = 0; test_index < destination_bone_index; ++test_index) {
                if (test_index < skin_bone_transform_indices.Count()
                    && skin_bone_transform_indices[test_index] == destination_transform_index) {
                    destination_bone_index = test_index;
                    break;
                }
            }
            bone_weight.bone_indices[static_cast<std::size_t>(weight_index)] = destination_bone_index;
        }
        bone_weights[destination_index] = bone_weight;
    }

    skin_bone_bind_poses.AddRange(source_skin_bone_count);
    for (int bone_index = 0; bone_index < source_skin_bone_count; ++bone_index) {
        const int source_transform_index = source_mesh.skin_bone_transform_indices[bone_index];
        const float3 position =
            ExNativeValueOrDefault(source_mesh.transform_data.position_array, source_transform_index, float3{});
        const quaternion rotation =
            ExNativeValueOrDefault(source_mesh.transform_data.rotation_array, source_transform_index, quaternion{});
        const float3 scale =
            ExNativeValueOrDefault(source_mesh.transform_data.scale_array, source_transform_index, float3{1.0f, 1.0f, 1.0f});
        skin_bone_bind_poses[skin_bone_start + bone_index] =
            Multiply(InverseAffine(TRS(position, rotation, scale)), init_local_to_world);
    }

    triangles.AddRange(source_mesh.TriangleCount());
    for (int triangle_index = 0; triangle_index < source_mesh.TriangleCount(); ++triangle_index) {
        const int3 triangle = source_mesh.triangles[triangle_index];
        triangles[triangle_start + triangle_index] =
            int3{triangle.x + vertex_start, triangle.y + vertex_start, triangle.z + vertex_start};
    }

    lines.AddRange(source_mesh.LineCount());
    for (int line_index = 0; line_index < source_mesh.LineCount(); ++line_index) {
        const int2 line = source_mesh.lines[line_index];
        lines[line_start + line_index] =
            int2{line.x + vertex_start, line.y + vertex_start};
    }

    AABB source_bounds = source_mesh.bounding_box;
    Transform(source_bounds, to_mesh);
    if (vertex_start <= 0 && source_vertex_count > 0) {
        bounding_box = source_bounds;
    } else {
        Encapsulate(bounding_box, source_bounds.min);
        Encapsulate(bounding_box, source_bounds.max);
    }

    const float source_scale_length = Length(source_mesh.init_scale);
    const float destination_scale_length = std::max(Length(init_scale), define::system::Epsilon);
    const float scale_ratio = source_scale_length / destination_scale_length;
    average_vertex_distance =
        std::max(average_vertex_distance, source_mesh.average_vertex_distance * scale_ratio);
    max_vertex_distance =
        std::max(max_vertex_distance, source_mesh.max_vertex_distance * scale_ratio);
    result = Result::Ok();
}

void VirtualMesh::SetTransform(
    const TransformRecord& center_record,
    const TransformRecord* skin_root_record
)
{
    // Port target: VirtualMeshInputOutput.SetTransform(TransformRecord, TransformRecord).
    SetCenterTransform(center_record);
    if (skin_root_record != nullptr && skin_root_record->IsValid()) {
        SetSkinRoot(*skin_root_record);
    } else {
        skin_root_index = center_transform_index;
    }

    init_local_to_world = center_record.local_to_world_matrix;
    init_world_to_local = center_record.world_to_local_matrix;
    init_rotation = center_record.rotation;
    init_inverse_rotation = Inverse(init_rotation);
    init_scale = center_record.scale;
}

void VirtualMesh::SetCenterTransform(const TransformRecord& record)
{
    if (!record.IsValid()) {
        return;
    }
    if (center_transform_index >= 0) {
        ReplaceTransformRecord(transform_data, center_transform_index, record);
    } else {
        center_transform_index = AddTransformRecord(transform_data, record);
    }
}

void VirtualMesh::SetSkinRoot(const TransformRecord& record)
{
    if (!record.IsValid()) {
        return;
    }
    if (skin_root_index >= 0) {
        ReplaceTransformRecord(transform_data, skin_root_index, record);
    } else {
        skin_root_index = AddTransformRecord(transform_data, record);
    }
}

void VirtualMesh::SetCustomSkinningBones(
    const TransformRecord& cloth_transform_record,
    std::vector<TransformRecord>& custom_skinning_bone_records
)
{
    // Ported from Magica Cloth 2: VirtualMeshInputOutput.SetCustomSkinningBones(...).
    custom_skinning_bone_indices.clear();
    if (custom_skinning_bone_records.empty()) {
        return;
    }

    custom_skinning_bone_indices.reserve(custom_skinning_bone_records.size());
    for (TransformRecord& record : custom_skinning_bone_records) {
        if (!record.IsValid()) {
            custom_skinning_bone_indices.push_back(-1);
            continue;
        }

        if (cloth_transform_record.IsValid()) {
            record.local_position =
                TransformPoint(record.position, cloth_transform_record.world_to_local_matrix);
        }

        int transform_index = -1;
        const auto found = std::find(
            transform_data.id_array.begin(),
            transform_data.id_array.end(),
            record.id
        );
        if (found != transform_data.id_array.end()) {
            transform_index = static_cast<int>(std::distance(transform_data.id_array.begin(), found));
        }
        if (transform_index < 0 && record.id == cloth_transform_record.id) {
            transform_index = center_transform_index;
        }

        const DataChunk transform_chunk = transform_data.flag_array.Add(BitFlag8{});
        const int added_transform_index = transform_chunk.IsValid()
            ? transform_chunk.start_index
            : transform_index;
        if (transform_chunk.IsValid()) {
            transform_data.init_local_position_array.Add(record.local_position);
            transform_data.init_local_rotation_array.Add(record.local_rotation);
            transform_data.position_array.Add(record.position);
            transform_data.rotation_array.Add(record.rotation);
            transform_data.inverse_rotation_array.Add(Inverse(record.rotation));
            transform_data.scale_array.Add(record.scale);
            transform_data.local_position_array.Add(record.local_position);
            transform_data.local_rotation_array.Add(record.local_rotation);
            transform_data.local_to_world_matrix_array.Add(record.local_to_world_matrix);
            transform_data.team_id_array.Add(0);
            transform_data.EnsureRecordCapacity(added_transform_index + 1);
            transform_data.id_array[static_cast<std::size_t>(added_transform_index)] = record.id;
            transform_data.parent_id_array[static_cast<std::size_t>(added_transform_index)] =
                record.parent_id;
            transform_data.name_array[static_cast<std::size_t>(added_transform_index)] = record.name;
            transform_data.is_dirty = true;
        }

        const int skin_bone_index = skin_bone_transform_indices.Count();
        skin_bone_transform_indices.Add(added_transform_index);
        skin_bone_bind_poses.Add(Multiply(record.world_to_local_matrix, init_local_to_world));
        custom_skinning_bone_indices.push_back(skin_bone_index);
    }
}

bool VirtualMesh::CompareSpace(const VirtualMesh& target) const
{
    // MC2 compares initialization matrices directly. Center/scale-only checks
    // can wrongly skip mapping transforms when rotation or translation differs.
    return CompareMatrix(init_local_to_world, target.init_local_to_world);
}

float4x4 VirtualMesh::CenterTransformTo(const VirtualMesh& target) const
{
    // Ported from Magica Cloth 2: VirtualMeshInputOutput.CenterTransformTo().
    if (CompareSpace(target)) {
        return float4x4{};
    }
    return Transform(init_local_to_world, target.init_world_to_local);
}

}  // namespace hocloth::mc2
