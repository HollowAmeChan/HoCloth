#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"

#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_container.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <utility>

namespace hocloth::mc2 {

namespace {

constexpr std::uint8_t BaseLineFlagIncludeLine = 0x01;

quaternion ApplyNegativeScaleQuaternion(const quaternion& rotation, const float4& negative_scale_quaternion)
{
    return quaternion{
        rotation.w * negative_scale_quaternion.w,
        rotation.x * negative_scale_quaternion.x,
        rotation.y * negative_scale_quaternion.y,
        rotation.z * negative_scale_quaternion.z,
    };
}

}  // namespace

Result VirtualMeshManager::Initialize()
{
    Dispose();
    initialized_ = true;
    return Result::Ok();
}

void VirtualMeshManager::Dispose()
{
    ClearMeshes();
    team_ids_.Dispose();
    attributes_.Dispose();
    uv_.Dispose();
    vertex_bind_pose_positions_.Dispose();
    vertex_bind_pose_rotations_.Dispose();
    vertex_depths_.Dispose();
    vertex_root_indices_.Dispose();
    vertex_local_positions_.Dispose();
    vertex_local_rotations_.Dispose();
    vertex_parent_indices_.Dispose();
    vertex_to_triangles_.Dispose();
    vertex_child_index_array_.Dispose();
    vertex_child_data_array_.Dispose();
    normal_adjustment_rotations_.Dispose();
    triangle_team_ids_.Dispose();
    triangles_.Dispose();
    triangle_normals_.Dispose();
    triangle_tangents_.Dispose();
    edge_team_ids_.Dispose();
    edges_.Dispose();
    edge_flags_.Dispose();
    base_line_flags_.Dispose();
    base_line_team_ids_.Dispose();
    base_line_start_data_indices_.Dispose();
    base_line_data_counts_.Dispose();
    base_line_data_.Dispose();
    local_positions_.Dispose();
    local_normals_.Dispose();
    local_tangents_.Dispose();
    bone_weights_.Dispose();
    skin_bone_transform_indices_.Dispose();
    skin_bone_bind_poses_.Dispose();
    vertex_to_transform_rotations_.Dispose();
    positions_.Dispose();
    rotations_.Dispose();
    mapping_id_array_.Dispose();
    mapping_reference_indices_.Dispose();
    mapping_attributes_.Dispose();
    mapping_local_positions_.Dispose();
    mapping_local_normals_.Dispose();
    mapping_bone_weights_.Dispose();
    mapping_positions_.Dispose();
    mapping_normals_.Dispose();
    initialized_ = false;
}

ManagerStatus VirtualMeshManager::Status() const
{
    std::ostringstream detail;
    detail << "proxy_vertices=" << ProxyVertexCount()
           << " proxy_triangles=" << ProxyTriangleCount()
           << " proxy_edges=" << ProxyEdgeCount()
           << " proxy_baselines=" << ProxyBaseLineCount()
           << " local_positions=" << ProxyLocalPositionCount()
           << " mapping_vertices=" << MappingVertexCount();
    return ManagerStatus{
        "VirtualMeshManager",
        initialized_,
        static_cast<std::uint32_t>(MeshCount()),
        detail.str(),
    };
}

bool VirtualMeshManager::IsValid() const
{
    return initialized_;
}

int VirtualMeshManager::MeshCount() const
{
    int count = 0;
    for (const auto& mesh : meshes_) {
        if (mesh != nullptr) {
            ++count;
        }
    }
    return count;
}

int VirtualMeshManager::RegisterMesh(std::shared_ptr<VirtualMesh> mesh)
{
    if (!initialized_ || mesh == nullptr) {
        return -1;
    }

    if (!free_mesh_ids_.empty()) {
        const int mesh_id = free_mesh_ids_.back();
        free_mesh_ids_.pop_back();
        meshes_[static_cast<std::size_t>(mesh_id)] = std::move(mesh);
        return mesh_id;
    }

    meshes_.push_back(std::move(mesh));
    return static_cast<int>(meshes_.size() - 1);
}

void VirtualMeshManager::ReleaseMesh(int mesh_id)
{
    if (mesh_id < 0 || mesh_id >= static_cast<int>(meshes_.size())) {
        return;
    }
    if (meshes_[static_cast<std::size_t>(mesh_id)] == nullptr) {
        return;
    }
    meshes_[static_cast<std::size_t>(mesh_id)].reset();
    free_mesh_ids_.push_back(mesh_id);
}

void VirtualMeshManager::ClearMeshes()
{
    meshes_.clear();
    free_mesh_ids_.clear();
}

VirtualMesh* VirtualMeshManager::GetMesh(int mesh_id)
{
    if (mesh_id < 0 || mesh_id >= static_cast<int>(meshes_.size())) {
        return nullptr;
    }
    return meshes_[static_cast<std::size_t>(mesh_id)].get();
}

const VirtualMesh* VirtualMeshManager::GetMesh(int mesh_id) const
{
    if (mesh_id < 0 || mesh_id >= static_cast<int>(meshes_.size())) {
        return nullptr;
    }
    return meshes_[static_cast<std::size_t>(mesh_id)].get();
}

void VirtualMeshManager::RegisterProxyMesh(
    int team_id,
    const VirtualMeshContainer& proxy_mesh_container,
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    if (!initialized_ || !team_manager.IsValidTeam(team_id)) {
        return;
    }

    const VirtualMesh* proxy_mesh = proxy_mesh_container.SharedVirtualMesh();
    if (proxy_mesh == nullptr) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    team_data.proxy_transform_chunk = transform_manager.AddTransform(proxy_mesh_container, team_id);
    team_data.center_transform_index =
        proxy_mesh->center_transform_index >= 0
            ? team_data.proxy_transform_chunk.start_index + proxy_mesh->center_transform_index
            : -1;

    const int vertex_count = proxy_mesh->VertexCount();
    if (vertex_count > 0) {
        team_data.proxy_common_chunk = team_ids_.AddRange(vertex_count, static_cast<std::int16_t>(team_id));
        attributes_.AddRange(proxy_mesh->attributes);
        uv_.AddRange(proxy_mesh->uv);
        vertex_bind_pose_positions_.AddRange(proxy_mesh->vertex_bind_pose_positions);
        vertex_bind_pose_rotations_.AddRange(proxy_mesh->vertex_bind_pose_rotations);
        vertex_depths_.AddRange(proxy_mesh->vertex_depths);
        vertex_root_indices_.AddRange(proxy_mesh->vertex_root_indices);
        vertex_local_positions_.AddRange(proxy_mesh->vertex_local_positions);
        vertex_local_rotations_.AddRange(proxy_mesh->vertex_local_rotations);
        vertex_parent_indices_.AddRange(proxy_mesh->vertex_parent_indices);
        if (proxy_mesh->vertex_to_triangles.Count() == vertex_count) {
            vertex_to_triangles_.AddRange(proxy_mesh->vertex_to_triangles);
        } else {
            vertex_to_triangles_.AddRange(vertex_count);
        }
        vertex_child_index_array_.AddRange(proxy_mesh->vertex_child_index_array);
        normal_adjustment_rotations_.AddRange(proxy_mesh->normal_adjustment_rotations);
        positions_.AddRange(vertex_count);
        rotations_.AddRange(vertex_count);
    }

    if (proxy_mesh->vertex_child_data_array.Count() > 0) {
        team_data.proxy_vertex_child_data_chunk =
            vertex_child_data_array_.AddRange(proxy_mesh->vertex_child_data_array);
    }

    if (proxy_mesh->TriangleCount() > 0) {
        team_data.proxy_triangle_chunk =
            triangle_team_ids_.AddRange(proxy_mesh->TriangleCount(), static_cast<std::int16_t>(team_id));
        triangles_.AddRange(proxy_mesh->triangles);
        triangle_normals_.AddRange(proxy_mesh->TriangleCount());
        triangle_tangents_.AddRange(proxy_mesh->TriangleCount());

        if (proxy_mesh->vertex_to_triangles.Count() != vertex_count
            && team_data.proxy_common_chunk.IsValid()) {
            for (int triangle_index = 0; triangle_index < proxy_mesh->TriangleCount(); ++triangle_index) {
                const int3 triangle = proxy_mesh->triangles[triangle_index];
                const std::uint32_t packed_triangle =
                    data::Pack12_20(0, triangle_index);
                const int vertices[3] = {triangle.x, triangle.y, triangle.z};
                for (int vertex_offset = 0; vertex_offset < 3; ++vertex_offset) {
                    const int local_vertex_index = vertices[vertex_offset];
                    const int vertex_index =
                        team_data.proxy_common_chunk.start_index + local_vertex_index;
                    if (local_vertex_index < 0
                        || local_vertex_index >= vertex_count
                        || vertex_index < 0
                        || vertex_index >= vertex_to_triangles_.Length()) {
                        continue;
                    }

                    VirtualMesh::VertexTriangleList triangle_list =
                        vertex_to_triangles_[vertex_index];
                    triangle_list.Set(packed_triangle);
                    vertex_to_triangles_[vertex_index] = triangle_list;
                }
            }
        }
    }

    if (proxy_mesh->edges.Count() > 0) {
        team_data.proxy_edge_chunk =
            edge_team_ids_.AddRange(proxy_mesh->edges.Count(), static_cast<std::int16_t>(team_id));
        edges_.AddRange(proxy_mesh->edges);
        edge_flags_.AddRange(proxy_mesh->edge_flags);
    }

    if (proxy_mesh->base_line_start_data_indices.Count() > 0
        && proxy_mesh->base_line_data_counts.Count()
            == proxy_mesh->base_line_start_data_indices.Count()) {
        if (proxy_mesh->base_line_flags.Count()
            == proxy_mesh->base_line_start_data_indices.Count()) {
            base_line_flags_.AddRange(proxy_mesh->base_line_flags);
        } else {
            base_line_flags_.AddRange(
                proxy_mesh->base_line_start_data_indices.Count(),
                BitFlag8{}
            );
        }
        base_line_team_ids_.AddRange(
            proxy_mesh->base_line_start_data_indices.Count(),
            static_cast<std::int16_t>(team_id)
        );
        team_data.baseline_chunk =
            base_line_start_data_indices_.AddRange(proxy_mesh->base_line_start_data_indices);
        base_line_data_counts_.AddRange(proxy_mesh->base_line_data_counts);
    }

    if (proxy_mesh->base_line_data.Count() > 0) {
        team_data.baseline_data_chunk = base_line_data_.AddRange(proxy_mesh->base_line_data);
    }

    if (proxy_mesh->VertexCount() > 0) {
        team_data.proxy_mesh_chunk = local_positions_.AddRange(proxy_mesh->local_positions);
        local_normals_.AddRange(proxy_mesh->local_normals);
        local_tangents_.AddRange(proxy_mesh->local_tangents);
        bone_weights_.AddRange(proxy_mesh->bone_weights);
    }

    if (proxy_mesh->SkinBoneCount() > 0) {
        team_data.proxy_skin_bone_chunk =
            skin_bone_transform_indices_.AddRange(proxy_mesh->skin_bone_transform_indices);
        skin_bone_bind_poses_.AddRange(proxy_mesh->skin_bone_bind_poses);
    }

    if (proxy_mesh->vertex_to_transform_rotations.Count() > 0) {
        team_data.proxy_bone_chunk =
            vertex_to_transform_rotations_.AddRange(proxy_mesh->vertex_to_transform_rotations);
    }
}

void VirtualMeshManager::ExitProxyMesh(
    int team_id,
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    if (!initialized_ || !team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    transform_manager.RemoveTransform(team_data.proxy_transform_chunk);
    team_ids_.Remove(team_data.proxy_common_chunk);
    attributes_.Remove(team_data.proxy_common_chunk);
    uv_.Remove(team_data.proxy_common_chunk);
    vertex_bind_pose_positions_.Remove(team_data.proxy_common_chunk);
    vertex_bind_pose_rotations_.Remove(team_data.proxy_common_chunk);
    vertex_depths_.Remove(team_data.proxy_common_chunk);
    vertex_root_indices_.Remove(team_data.proxy_common_chunk);
    vertex_local_positions_.Remove(team_data.proxy_common_chunk);
    vertex_local_rotations_.Remove(team_data.proxy_common_chunk);
    vertex_parent_indices_.Remove(team_data.proxy_common_chunk);
    vertex_to_triangles_.Remove(team_data.proxy_common_chunk);
    vertex_child_index_array_.Remove(team_data.proxy_common_chunk);
    normal_adjustment_rotations_.Remove(team_data.proxy_common_chunk);
    positions_.Remove(team_data.proxy_common_chunk);
    rotations_.Remove(team_data.proxy_common_chunk);

    vertex_child_data_array_.Remove(team_data.proxy_vertex_child_data_chunk);
    triangle_team_ids_.Remove(team_data.proxy_triangle_chunk);
    triangles_.Remove(team_data.proxy_triangle_chunk);
    triangle_normals_.Remove(team_data.proxy_triangle_chunk);
    triangle_tangents_.Remove(team_data.proxy_triangle_chunk);
    edge_team_ids_.Remove(team_data.proxy_edge_chunk);
    edges_.Remove(team_data.proxy_edge_chunk);
    edge_flags_.Remove(team_data.proxy_edge_chunk);
    base_line_flags_.Remove(team_data.baseline_chunk);
    base_line_team_ids_.Remove(team_data.baseline_chunk);
    base_line_start_data_indices_.Remove(team_data.baseline_chunk);
    base_line_data_counts_.Remove(team_data.baseline_chunk);
    base_line_data_.Remove(team_data.baseline_data_chunk);
    local_positions_.Remove(team_data.proxy_mesh_chunk);
    local_normals_.Remove(team_data.proxy_mesh_chunk);
    local_tangents_.Remove(team_data.proxy_mesh_chunk);
    bone_weights_.Remove(team_data.proxy_mesh_chunk);
    skin_bone_transform_indices_.Remove(team_data.proxy_skin_bone_chunk);
    skin_bone_bind_poses_.Remove(team_data.proxy_skin_bone_chunk);
    vertex_to_transform_rotations_.Remove(team_data.proxy_bone_chunk);

    team_data.proxy_transform_chunk.Clear();
    team_data.proxy_common_chunk.Clear();
    team_data.proxy_vertex_child_data_chunk.Clear();
    team_data.proxy_triangle_chunk.Clear();
    team_data.proxy_edge_chunk.Clear();
    team_data.proxy_mesh_chunk.Clear();
    team_data.proxy_bone_chunk.Clear();
    team_data.proxy_skin_bone_chunk.Clear();
    team_data.baseline_chunk.Clear();
    team_data.baseline_data_chunk.Clear();
    team_data.center_transform_index = -1;
}

void VirtualMeshManager::UpdateProxyPositionsFromBindPose(const TeamManager& team_manager)
{
    if (!initialized_) {
        return;
    }

    for (int team_id = 0; team_id < team_manager.TeamCount(); ++team_id) {
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess() || !team_data.proxy_common_chunk.IsValid()) {
            continue;
        }

        const int start = team_data.proxy_common_chunk.start_index;
        const int count = team_data.proxy_common_chunk.data_length;
        for (int offset = 0; offset < count; ++offset) {
            const int vertex_index = start + offset;
            if (vertex_index < 0
                || vertex_index >= positions_.Length()
                || vertex_index >= rotations_.Length()
                || vertex_index >= vertex_bind_pose_positions_.Length()
                || vertex_index >= vertex_bind_pose_rotations_.Length()) {
                continue;
            }

            positions_[vertex_index] = vertex_bind_pose_positions_[vertex_index];
            rotations_[vertex_index] = vertex_bind_pose_rotations_[vertex_index];
        }
    }
}

void VirtualMeshManager::PreProxyMeshUpdate(
    const TeamManager& team_manager,
    const TransformManager& transform_manager
)
{
    // Ported from MC2 VirtualMeshManager.PreProxyMeshUpdate()/CalcProxyMeshSkinningJob.
    if (!initialized_) {
        return;
    }

    const TransformData& transform_data = transform_manager.Data();
    for (int team_id = 1; team_id < team_manager.TeamCount(); ++team_id) {
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess() || team_data.IsCullingInvisible()) {
            continue;
        }

        const DataChunk common_chunk = team_data.proxy_common_chunk;
        const DataChunk mesh_chunk = team_data.proxy_mesh_chunk;
        if (!common_chunk.IsValid() || !mesh_chunk.IsValid()) {
            continue;
        }

        for (int local_index = 0; local_index < common_chunk.data_length; ++local_index) {
            const int vertex_index = common_chunk.start_index + local_index;
            const int mesh_index = mesh_chunk.start_index + local_index;
            if (vertex_index < 0
                || vertex_index >= positions_.Length()
                || vertex_index >= rotations_.Length()
                || mesh_index < 0
                || mesh_index >= local_positions_.Length()
                || mesh_index >= local_normals_.Length()
                || mesh_index >= local_tangents_.Length()
                || mesh_index >= bone_weights_.Length()) {
                continue;
            }

            const VirtualMeshBoneWeight& bone_weight = bone_weights_[mesh_index];
            const int weight_count = bone_weight.Count();
            float total_weight = 0.0f;
            float3 world_position{};
            float3 world_normal{};
            float3 world_tangent{};

            for (int weight_index = 0; weight_index < weight_count; ++weight_index) {
                const float weight = bone_weight.weights[static_cast<std::size_t>(weight_index)];
                const int local_bone_index =
                    bone_weight.bone_indices[static_cast<std::size_t>(weight_index)];
                const int skin_index = team_data.proxy_skin_bone_chunk.start_index + local_bone_index;
                if (weight <= 0.0f
                    || local_bone_index < 0
                    || skin_index < 0
                    || skin_index >= skin_bone_bind_poses_.Length()
                    || skin_index >= skin_bone_transform_indices_.Length()) {
                    continue;
                }

                const int transform_index =
                    team_data.proxy_transform_chunk.start_index + skin_bone_transform_indices_[skin_index];
                if (transform_index < 0
                    || transform_index >= transform_data.local_to_world_matrix_array.Length()) {
                    continue;
                }

                const float4x4 bone_pose = skin_bone_bind_poses_[skin_index];
                const float4x4 local_to_world =
                    transform_data.local_to_world_matrix_array[transform_index];
                const float4x4 matrix = Multiply(local_to_world, bone_pose);
                world_position = Add(
                    world_position,
                    Scale(TransformPoint(local_positions_[mesh_index], matrix), weight)
                );
                world_normal = Add(
                    world_normal,
                    Scale(TransformDirection(local_normals_[mesh_index], matrix), weight)
                );
                world_tangent = Add(
                    world_tangent,
                    Scale(TransformDirection(local_tangents_[mesh_index], matrix), weight)
                );
                total_weight += weight;
            }

            if (total_weight <= 1.0e-6f) {
                if (vertex_index < vertex_bind_pose_positions_.Length()) {
                    positions_[vertex_index] = vertex_bind_pose_positions_[vertex_index];
                }
                if (vertex_index < vertex_bind_pose_rotations_.Length()) {
                    rotations_[vertex_index] = vertex_bind_pose_rotations_[vertex_index];
                }
                continue;
            }

            const float inv_weight = 1.0f / total_weight;
            positions_[vertex_index] = Scale(world_position, inv_weight);
            world_normal = Normalize(Scale(world_normal, inv_weight), float3{0.0f, 1.0f, 0.0f});
            world_tangent = Normalize(Scale(world_tangent, inv_weight), float3{1.0f, 0.0f, 0.0f});
            rotations_[vertex_index] = ToRotation(world_normal, world_tangent);
        }
    }
}

void VirtualMeshManager::PostProxyMeshUpdate(
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    // Ported from MC2 VirtualMeshManager.PostProxyMeshUpdate()/WriteTransformDataJob.
    if (!initialized_) {
        return;
    }

    TransformData& transform_data = transform_manager.Data();
    for (int team_id = 1; team_id < team_manager.TeamCount(); ++team_id) {
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess()
            || team_data.IsCullingInvisible()
            || team_data.proxy_mesh_type != VirtualMesh::MeshType::ProxyBoneMesh
            || !team_data.proxy_common_chunk.IsValid()
            || !team_data.proxy_transform_chunk.IsValid()) {
            continue;
        }

        const ClothParameters& parameters = team_manager.GetParameters(team_id);
        if (team_data.proxy_triangle_chunk.IsValid()
            && team_data.proxy_triangle_chunk.data_length > 0) {
            const int vertex_start = team_data.proxy_common_chunk.start_index;
            for (int triangle_offset = 0; triangle_offset < team_data.proxy_triangle_chunk.data_length; ++triangle_offset) {
                const int triangle_index = team_data.proxy_triangle_chunk.start_index + triangle_offset;
                if (triangle_index < 0
                    || triangle_index >= triangles_.Length()
                    || triangle_index >= triangle_normals_.Length()
                    || triangle_index >= triangle_tangents_.Length()
                    || triangle_index >= triangle_team_ids_.Length()
                    || triangle_team_ids_[triangle_index] != team_id) {
                    continue;
                }

                const int3 triangle = triangles_[triangle_index];
                const int v0 = vertex_start + triangle.x;
                const int v1 = vertex_start + triangle.y;
                const int v2 = vertex_start + triangle.z;
                if (v0 < 0
                    || v1 < 0
                    || v2 < 0
                    || v0 >= positions_.Length()
                    || v1 >= positions_.Length()
                    || v2 >= positions_.Length()
                    || v0 >= uv_.Length()
                    || v1 >= uv_.Length()
                    || v2 >= uv_.Length()) {
                    continue;
                }

                triangle_normals_[triangle_index] =
                    Scale(
                        TriangleNormal(positions_[v0], positions_[v1], positions_[v2]),
                        team_data.negative_scale_triangle_sign.x
                    );
                triangle_tangents_[triangle_index] =
                    Scale(
                        TriangleTangent(
                            positions_[v0],
                            positions_[v1],
                            positions_[v2],
                            uv_[v0],
                            uv_[v1],
                            uv_[v2]
                        ),
                        team_data.negative_scale_triangle_sign.y
                    );
            }

            for (int local_index = 0; local_index < team_data.proxy_common_chunk.data_length; ++local_index) {
                const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
                if (vertex_index < 0
                    || vertex_index >= vertex_to_triangles_.Length()
                    || vertex_index >= rotations_.Length()
                    || vertex_index >= normal_adjustment_rotations_.Length()) {
                    continue;
                }

                const VirtualMesh::VertexTriangleList& triangle_list =
                    vertex_to_triangles_[vertex_index];
                if (triangle_list.Length() <= 0) {
                    continue;
                }

                float3 normal_sum{};
                float3 tangent_sum{};
                for (int triangle_list_index = 0;
                     triangle_list_index < triangle_list.Length();
                     ++triangle_list_index) {
                    const std::uint32_t packed = triangle_list[triangle_list_index];
                    const int flip_flag = data::Unpack12_20Hi(packed);
                    const int local_triangle_index = data::Unpack12_20Low(packed);
                    const int triangle_index =
                        team_data.proxy_triangle_chunk.start_index + local_triangle_index;
                    if (triangle_index < 0
                        || triangle_index >= triangle_normals_.Length()
                        || triangle_index >= triangle_tangents_.Length()) {
                        continue;
                    }

                    normal_sum = Add(
                        normal_sum,
                        Scale(
                            triangle_normals_[triangle_index],
                            (flip_flag & 0x1) == 0 ? 1.0f : -1.0f
                        )
                    );
                    tangent_sum = Add(
                        tangent_sum,
                        Scale(
                            triangle_tangents_[triangle_index],
                            (flip_flag & 0x2) == 0 ? 1.0f : -1.0f
                        )
                    );
                }

                if (Length(normal_sum) <= 1.0e-6f || Length(tangent_sum) <= 1.0e-6f) {
                    continue;
                }

                const float3 normal = Normalize(normal_sum);
                const float3 tangent = Normalize(tangent_sum);
                const float dot = Dot(normal, tangent);
                if (dot >= 1.0f || dot <= -1.0f) {
                    continue;
                }

                const float3 binormal = Normalize(Cross(normal, tangent));
                quaternion rotation = LookRotation(binormal, normal);
                rotation = Normalize(Multiply(
                    rotation,
                    ApplyNegativeScaleQuaternion(
                        normal_adjustment_rotations_[vertex_index],
                        team_data.negative_scale_quaternion_value
                    )
                ));
                rotations_[vertex_index] = rotation;
            }
        }

        if (team_data.baseline_chunk.IsValid() && team_data.baseline_data_chunk.IsValid()) {
            const int vertex_start = team_data.proxy_common_chunk.start_index;
            const int baseline_data_start = team_data.baseline_data_chunk.start_index;
            const int child_data_start = team_data.proxy_vertex_child_data_chunk.start_index;
            const float average_rate = Clamp01(parameters.rotational_interpolation);
            const float root_interpolation = Clamp01(parameters.root_rotation);

            for (int baseline_offset = 0; baseline_offset < team_data.baseline_chunk.data_length; ++baseline_offset) {
                const int baseline_index = team_data.baseline_chunk.start_index + baseline_offset;
                if (baseline_index < 0
                    || baseline_index >= base_line_flags_.Length()
                    || baseline_index >= base_line_start_data_indices_.Length()
                    || baseline_index >= base_line_data_counts_.Length()
                    || baseline_index >= base_line_team_ids_.Length()
                    || !base_line_flags_[baseline_index].IsSet(BaseLineFlagIncludeLine)
                    || base_line_team_ids_[baseline_index] != team_id) {
                    continue;
                }

                int data_index = baseline_data_start + base_line_start_data_indices_[baseline_index];
                const int data_count = base_line_data_counts_[baseline_index];
                for (int index = 0; index < data_count; ++index, ++data_index) {
                    if (data_index < 0 || data_index >= base_line_data_.Length()) {
                        continue;
                    }

                    const int vertex_index = vertex_start + base_line_data_[data_index];
                    if (vertex_index < 0
                        || vertex_index >= positions_.Length()
                        || vertex_index >= rotations_.Length()
                        || vertex_index >= attributes_.Length()
                        || vertex_index >= vertex_child_index_array_.Length()) {
                        continue;
                    }

                    const float3 parent_position = positions_[vertex_index];
                    quaternion parent_rotation = rotations_[vertex_index];
                    const VertexAttribute parent_attribute = attributes_[vertex_index];

                    int child_count = 0;
                    int child_start = 0;
                    data::Unpack12_20(
                        vertex_child_index_array_[vertex_index],
                        child_count,
                        child_start
                    );
                    if (child_count <= 0) {
                        continue;
                    }

                    int move_count = 0;
                    float3 rest_child_vector_sum{};
                    float3 current_child_vector_sum{};
                    for (int child_offset = 0; child_offset < child_count; ++child_offset) {
                        const int child_data_index = child_data_start + child_start + child_offset;
                        if (child_data_index < 0 || child_data_index >= vertex_child_data_array_.Length()) {
                            continue;
                        }

                        const int child_vertex_index =
                            vertex_start + vertex_child_data_array_[child_data_index];
                        if (child_vertex_index < 0
                            || child_vertex_index >= positions_.Length()
                            || child_vertex_index >= rotations_.Length()
                            || child_vertex_index >= attributes_.Length()
                            || child_vertex_index >= vertex_local_positions_.Length()
                            || child_vertex_index >= vertex_local_rotations_.Length()) {
                            continue;
                        }

                        const VertexAttribute child_attribute = attributes_[child_vertex_index];
                        const float3 child_local_position = vertex_local_positions_[child_vertex_index];
                        const float3 scaled_child_local_position{
                            child_local_position.x * team_data.negative_scale_direction.x,
                            child_local_position.y * team_data.negative_scale_direction.y,
                            child_local_position.z * team_data.negative_scale_direction.z,
                        };
                        const float3 rest_vector = Rotate(parent_rotation, scaled_child_local_position);
                        rest_child_vector_sum = Add(rest_child_vector_sum, rest_vector);

                        if (child_attribute.IsMove()) {
                            const float3 current_vector =
                                Subtract(positions_[child_vertex_index], parent_position);
                            current_child_vector_sum = Add(current_child_vector_sum, current_vector);
                            const quaternion delta_rotation = FromToRotation(rest_vector, current_vector);
                            const quaternion child_local_rotation = ApplyNegativeScaleQuaternion(
                                vertex_local_rotations_[child_vertex_index],
                                team_data.negative_scale_quaternion_value
                            );
                            rotations_[child_vertex_index] = Normalize(Multiply(
                                delta_rotation,
                                Multiply(parent_rotation, child_local_rotation)
                            ));
                            ++move_count;
                        } else {
                            current_child_vector_sum =
                                Add(current_child_vector_sum, rest_vector);
                        }
                    }

                    if (move_count <= 0) {
                        continue;
                    }

                    const float interpolation =
                        parent_attribute.IsMove() ? average_rate : root_interpolation;
                    const quaternion delta_rotation = FromToRotation(
                        rest_child_vector_sum,
                        current_child_vector_sum,
                        interpolation
                    );
                    rotations_[vertex_index] =
                        Normalize(Multiply(delta_rotation, parent_rotation));
                }
            }
        }

        for (int local_index = 0; local_index < team_data.proxy_common_chunk.data_length; ++local_index) {
            const int vertex_index = team_data.proxy_common_chunk.start_index + local_index;
            const int transform_index = team_data.proxy_transform_chunk.start_index + local_index;
            if (vertex_index < 0
                || vertex_index >= positions_.Length()
                || vertex_index >= rotations_.Length()
                || transform_index < 0
                || transform_index >= transform_data.position_array.Length()
                || transform_index >= transform_data.rotation_array.Length()
                || transform_index >= transform_data.scale_array.Length()
                || transform_index >= transform_data.local_position_array.Length()
                || transform_index >= transform_data.local_rotation_array.Length()
                || transform_index >= transform_data.local_to_world_matrix_array.Length()) {
                continue;
            }

            quaternion world_rotation = rotations_[vertex_index];
            const int vertex_to_transform_index = team_data.proxy_bone_chunk.start_index + local_index;
            if (team_data.proxy_bone_chunk.IsValid()
                && vertex_to_transform_index >= 0
                && vertex_to_transform_index < vertex_to_transform_rotations_.Length()) {
                world_rotation = Normalize(
                    Multiply(world_rotation, vertex_to_transform_rotations_[vertex_to_transform_index])
                );
            }

            transform_data.position_array[transform_index] = positions_[vertex_index];
            transform_data.rotation_array[transform_index] = world_rotation;
            transform_data.inverse_rotation_array[transform_index] = Inverse(world_rotation);
            transform_data.local_to_world_matrix_array[transform_index] =
                TRS(positions_[vertex_index], world_rotation, transform_data.scale_array[transform_index]);

            const int parent_local_index =
                vertex_index < vertex_parent_indices_.Length() ? vertex_parent_indices_[vertex_index] : -1;
            const int parent_transform_index =
                parent_local_index >= 0
                    ? team_data.proxy_transform_chunk.start_index + parent_local_index
                    : -1;
            if (parent_transform_index >= 0
                && parent_transform_index < transform_data.position_array.Length()
                && parent_transform_index < transform_data.rotation_array.Length()
                && parent_transform_index < transform_data.scale_array.Length()) {
                const quaternion inverse_parent_rotation =
                    Inverse(transform_data.rotation_array[parent_transform_index]);
                transform_data.local_position_array[transform_index] =
                    Rotate(
                        inverse_parent_rotation,
                        Subtract(
                            transform_data.position_array[transform_index],
                            transform_data.position_array[parent_transform_index]
                        )
                    );
                const quaternion local_rotation =
                    Multiply(inverse_parent_rotation, transform_data.rotation_array[transform_index]);
                transform_data.local_rotation_array[transform_index] = Normalize(local_rotation);
            } else {
                transform_data.local_position_array[transform_index] =
                    transform_data.position_array[transform_index];
                transform_data.local_rotation_array[transform_index] =
                    transform_data.rotation_array[transform_index];
            }
        }
    }

    transform_manager.SetDirty(true);
}

DataChunk VirtualMeshManager::RegisterMappingMesh(
    int team_id,
    VirtualMeshContainer& mapping_mesh_container,
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    if (!initialized_ || !team_manager.IsValidTeam(team_id)) {
        return DataChunk::Empty();
    }

    VirtualMesh* mapping_mesh = mapping_mesh_container.SharedVirtualMesh();
    if (mapping_mesh == nullptr || !mapping_mesh->IsMapping()) {
        return DataChunk::Empty();
    }

    TeamManager::MappingData mapping_data;
    mapping_data.team_id = team_id;

    BitFlag8 transform_flag{TransformManager::FlagRead};
    transform_flag.SetFlag(TransformManager::FlagEnable, true);
    const DataChunk center_transform_chunk = transform_manager.AddTransform(
        mapping_mesh_container.GetCenterTransformRecord(),
        transform_flag,
        team_id
    );
    mapping_data.center_transform_index =
        center_transform_chunk.IsValid() ? center_transform_chunk.start_index : -1;

    mapping_data.to_proxy_matrix = mapping_mesh->to_proxy_matrix;
    mapping_data.to_proxy_rotation = mapping_mesh->to_proxy_rotation;

    const int mapping_index = team_manager.RegisterMappingData(team_id, mapping_data);
    if (mapping_index < 0) {
        if (center_transform_chunk.IsValid()) {
            transform_manager.RemoveTransform(center_transform_chunk);
        }
        return DataChunk::Empty();
    }

    const int vertex_count = mapping_mesh->VertexCount();
    if (vertex_count > 0) {
        mapping_data.mapping_common_chunk =
            mapping_id_array_.AddRange(vertex_count, static_cast<short>(mapping_index + 1));
        mapping_reference_indices_.AddRange(mapping_mesh->reference_indices);
        mapping_attributes_.AddRange(mapping_mesh->attributes);
        mapping_local_positions_.AddRange(mapping_mesh->local_positions);
        mapping_local_normals_.AddRange(mapping_mesh->local_normals);
        mapping_bone_weights_.AddRange(mapping_mesh->bone_weights);
        mapping_positions_.AddRange(vertex_count);
        mapping_normals_.AddRange(vertex_count);
    }

    team_manager.MappingDataArray()[mapping_index] = mapping_data;
    mapping_mesh->mapping_id = mapping_index;
    return mapping_data.mapping_common_chunk;
}

void VirtualMeshManager::ExitMappingMesh(
    int team_id,
    int mapping_index,
    TeamManager& team_manager,
    TransformManager& transform_manager
)
{
    if (!initialized_ || !team_manager.IsValidTeam(team_id) || mapping_index < 0
        || mapping_index >= team_manager.MappingDataArray().Length()) {
        return;
    }

    const TeamManager::MappingData mapping_data = team_manager.MappingDataArray()[mapping_index];
    if (!mapping_data.IsValid() || mapping_data.team_id != team_id) {
        return;
    }

    if (mapping_data.center_transform_index >= 0) {
        transform_manager.RemoveTransform(DataChunk{mapping_data.center_transform_index, 1});
    }

    mapping_id_array_.RemoveAndFill(mapping_data.mapping_common_chunk, 0);
    mapping_reference_indices_.Remove(mapping_data.mapping_common_chunk);
    mapping_attributes_.Remove(mapping_data.mapping_common_chunk);
    mapping_local_positions_.Remove(mapping_data.mapping_common_chunk);
    mapping_local_normals_.Remove(mapping_data.mapping_common_chunk);
    mapping_bone_weights_.Remove(mapping_data.mapping_common_chunk);
    mapping_positions_.Remove(mapping_data.mapping_common_chunk);
    mapping_normals_.Remove(mapping_data.mapping_common_chunk);

    team_manager.RemoveMappingData(team_id, mapping_index);
}

int VirtualMeshManager::ProxyVertexCount() const
{
    return team_ids_.Count();
}

int VirtualMeshManager::ProxyTriangleCount() const
{
    return triangles_.Count();
}

int VirtualMeshManager::ProxyEdgeCount() const
{
    return edges_.Count();
}

int VirtualMeshManager::ProxyBaseLineCount() const
{
    return base_line_flags_.Count();
}

int VirtualMeshManager::ProxyLocalPositionCount() const
{
    return local_positions_.Count();
}

int VirtualMeshManager::MappingVertexCount() const
{
    return mapping_id_array_.Count();
}

const ExNativeArray<VertexAttribute>& VirtualMeshManager::Attributes() const
{
    return attributes_;
}

const ExNativeArray<quaternion>& VirtualMeshManager::VertexBindPoseRotations() const
{
    return vertex_bind_pose_rotations_;
}

const ExNativeArray<float>& VirtualMeshManager::VertexDepths() const
{
    return vertex_depths_;
}

const ExNativeArray<int>& VirtualMeshManager::VertexRootIndices() const
{
    return vertex_root_indices_;
}

const ExNativeArray<int>& VirtualMeshManager::VertexParentIndices() const
{
    return vertex_parent_indices_;
}

const ExNativeArray<float3>& VirtualMeshManager::VertexLocalPositions() const
{
    return vertex_local_positions_;
}

const ExNativeArray<quaternion>& VirtualMeshManager::VertexLocalRotations() const
{
    return vertex_local_rotations_;
}

const ExNativeArray<std::int16_t>& VirtualMeshManager::TriangleTeamIds() const
{
    return triangle_team_ids_;
}

const ExNativeArray<int3>& VirtualMeshManager::Triangles() const
{
    return triangles_;
}

const ExNativeArray<std::int16_t>& VirtualMeshManager::EdgeTeamIds() const
{
    return edge_team_ids_;
}

const ExNativeArray<int2>& VirtualMeshManager::Edges() const
{
    return edges_;
}

const ExNativeArray<BitFlag8>& VirtualMeshManager::EdgeFlags() const
{
    return edge_flags_;
}

const ExNativeArray<BitFlag8>& VirtualMeshManager::BaseLineFlags() const
{
    return base_line_flags_;
}

const ExNativeArray<std::int16_t>& VirtualMeshManager::BaseLineTeamIds() const
{
    return base_line_team_ids_;
}

const ExNativeArray<std::uint16_t>& VirtualMeshManager::BaseLineStartDataIndices() const
{
    return base_line_start_data_indices_;
}

const ExNativeArray<std::uint16_t>& VirtualMeshManager::BaseLineDataCounts() const
{
    return base_line_data_counts_;
}

const ExNativeArray<std::uint16_t>& VirtualMeshManager::BaseLineData() const
{
    return base_line_data_;
}

const ExNativeArray<float3>& VirtualMeshManager::Positions() const
{
    return positions_;
}

const ExNativeArray<quaternion>& VirtualMeshManager::Rotations() const
{
    return rotations_;
}

ExNativeArray<float3>& VirtualMeshManager::Positions()
{
    return positions_;
}

ExNativeArray<quaternion>& VirtualMeshManager::Rotations()
{
    return rotations_;
}

const ExNativeArray<short>& VirtualMeshManager::MappingIds() const
{
    return mapping_id_array_;
}

const ExNativeArray<int>& VirtualMeshManager::MappingReferenceIndices() const
{
    return mapping_reference_indices_;
}

const ExNativeArray<VertexAttribute>& VirtualMeshManager::MappingAttributes() const
{
    return mapping_attributes_;
}

const ExNativeArray<float3>& VirtualMeshManager::MappingLocalPositions() const
{
    return mapping_local_positions_;
}

const ExNativeArray<float3>& VirtualMeshManager::MappingLocalNormals() const
{
    return mapping_local_normals_;
}

const ExNativeArray<VirtualMeshBoneWeight>& VirtualMeshManager::MappingBoneWeights() const
{
    return mapping_bone_weights_;
}

const ExNativeArray<float3>& VirtualMeshManager::MappingPositions() const
{
    return mapping_positions_;
}

const ExNativeArray<float3>& VirtualMeshManager::MappingNormals() const
{
    return mapping_normals_;
}

ExNativeArray<float3>& VirtualMeshManager::MappingPositions()
{
    return mapping_positions_;
}

ExNativeArray<float3>& VirtualMeshManager::MappingNormals()
{
    return mapping_normals_;
}

}  // namespace hocloth::mc2
