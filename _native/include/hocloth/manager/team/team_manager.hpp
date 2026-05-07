#pragma once

#include "hocloth/cloth/cloth_parameters.hpp"
#include "hocloth/cloth/constraints/inertia_constraint_data.hpp"
#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/i_manager.hpp"
#include "hocloth/manager/team/team_wind_data.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"
#include "hocloth/utility/native_collection/ex_simple_native_array.hpp"
#include "hocloth/utility/native_collection/fixed_list.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hocloth::mc2 {

class TransformManager;
class VirtualMeshManager;
class WindManager;

// Port target for Magica Cloth 2: Scripts/Core/Manager/Team/TeamManager.cs
class TeamManager final : public IManager {
public:
    static constexpr int FlagValid = 0;
    static constexpr int FlagEnable = 1;
    static constexpr int FlagReset = 2;
    static constexpr int FlagTimeReset = 3;
    static constexpr int FlagSyncSuspend = 4;
    static constexpr int FlagRunning = 5;
    static constexpr int FlagSynchronization = 6;
    static constexpr int FlagStepRunning = 7;
    static constexpr int FlagExit = 8;
    static constexpr int FlagKeepTeleport = 9;
    static constexpr int FlagInertiaShift = 10;
    static constexpr int FlagCameraCullingInvisible = 11;
    static constexpr int FlagCameraCullingKeep = 12;
    static constexpr int FlagSpring = 13;
    static constexpr int FlagSkipWriting = 14;
    static constexpr int FlagAnchor = 15;
    static constexpr int FlagAnchorReset = 16;
    static constexpr int FlagNegativeScale = 17;
    static constexpr int FlagNegativeScaleTeleport = 18;
    static constexpr int FlagDistanceCullingInvisible = 19;
    static constexpr int FlagRestoreTransformOnlyOnce = 20;

    static constexpr int FlagSelfPointPrimitive = 32;
    static constexpr int FlagSelfEdgePrimitive = 33;
    static constexpr int FlagSelfTrianglePrimitive = 34;
    static constexpr int FlagSelfEdgeEdge = 35;
    static constexpr int FlagSyncEdgeEdge = 36;
    static constexpr int FlagPSyncEdgeEdge = 37;
    static constexpr int FlagSelfPointTriangle = 38;
    static constexpr int FlagSyncPointTriangle = 39;
    static constexpr int FlagPSyncPointTriangle = 40;
    static constexpr int FlagSelfTrianglePoint = 41;
    static constexpr int FlagSyncTrianglePoint = 42;
    static constexpr int FlagPSyncTrianglePoint = 43;
    static constexpr int FlagSelfEdgeTriangleIntersect = 44;
    static constexpr int FlagSyncEdgeTriangleIntersect = 45;
    static constexpr int FlagPSyncEdgeTriangleIntersect = 46;
    static constexpr int FlagSelfTriangleEdgeIntersect = 47;
    static constexpr int FlagSyncTriangleEdgeIntersect = 48;
    static constexpr int FlagPSyncTriangleEdgeIntersect = 49;

    struct TeamSyncParentList {
        static constexpr int Capacity = 7;
        FixedList<int, Capacity> values;

        [[nodiscard]] int Length() const;
        [[nodiscard]] bool IsFull() const;
        [[nodiscard]] bool Contains(int team_id) const;
        [[nodiscard]] int operator[](int index) const;
        bool Add(int team_id);
        bool RemoveSwapBack(int team_id);
        void Clear();
    };

    struct TeamMappingList {
        static constexpr int Capacity = 31;
        FixedList<short, Capacity> values;

        [[nodiscard]] int Length() const;
        [[nodiscard]] bool IsFull() const;
        [[nodiscard]] bool Contains(short mapping_index) const;
        [[nodiscard]] short operator[](int index) const;
        bool Add(short mapping_index);
        bool RemoveSwapBack(short mapping_index);
        void Clear();
    };

    struct MappingData {
        int team_id = 0;
        int center_transform_index = -1;
        DataChunk mapping_common_chunk;
        float4x4 to_proxy_matrix{};
        quaternion to_proxy_rotation{};
        bool same_space = false;
        float4x4 to_mapping_matrix{};
        quaternion to_mapping_rotation{};
        float scale_ratio = 1.0f;

        [[nodiscard]] bool IsValid() const;
        [[nodiscard]] int VertexCount() const;
    };

    struct TeamData {
        BitFlag64 flag;

        ClothUpdateMode update_mode = ClothUpdateMode::Normal;
        float frame_delta_time = 0.0f;
        float time = 0.0f;
        float old_time = 0.0f;
        float now_update_time = 0.0f;
        float old_update_time = 0.0f;
        float frame_update_time = 0.0f;
        float frame_old_time = 0.0f;
        float time_scale = 1.0f;
        float now_time_scale = 1.0f;
        int update_count = 0;
        int skip_count = 0;
        float frame_interpolation = 0.0f;

        float gravity_ratio = 1.0f;
        float gravity_dot = 1.0f;
        int center_transform_index = -1;
        int anchor_transform_id = 0;

        float3 init_scale{1.0f, 1.0f, 1.0f};
        float scale_ratio = 1.0f;
        float negative_scale_sign = 1.0f;
        float3 negative_scale_direction{1.0f, 1.0f, 1.0f};
        float3 negative_scale_change{1.0f, 1.0f, 1.0f};
        float2 negative_scale_triangle_sign{1.0f, 1.0f};
        float4 negative_scale_quaternion_value{1.0f, 1.0f, 1.0f, 1.0f};

        int sync_team_id = 0;
        TeamSyncParentList sync_parent_team_ids;
        int sync_center_transform_index = -1;
        float animation_pose_ratio = 1.0f;
        float velocity_weight = 1.0f;
        float distance_weight = 1.0f;
        float blend_weight = 1.0f;
        ClothForceMode force_mode = ClothForceMode::None;
        float3 impact_force{};

        VirtualMesh::MeshType proxy_mesh_type = VirtualMesh::MeshType::NormalMesh;
        DataChunk proxy_transform_chunk;
        DataChunk proxy_common_chunk;
        DataChunk proxy_vertex_child_data_chunk;
        DataChunk proxy_triangle_chunk;
        DataChunk proxy_edge_chunk;
        DataChunk proxy_mesh_chunk;
        DataChunk proxy_bone_chunk;
        DataChunk proxy_skin_bone_chunk;
        DataChunk baseline_chunk;
        DataChunk baseline_data_chunk;
        DataChunk fixed_data_chunk;
        DataChunk particle_chunk;
        DataChunk collider_chunk;
        DataChunk collider_transform_chunk;
        int collider_count = 0;
        DataChunk distance_start_chunk;
        DataChunk distance_data_chunk;
        DataChunk bending_pair_chunk;
        DataChunk bending_write_index_chunk;
        DataChunk bending_buffer_chunk;
        DataChunk self_point_chunk;
        DataChunk self_edge_chunk;
        DataChunk self_triangle_chunk;

        [[nodiscard]] bool IsValid() const;
        [[nodiscard]] bool IsEnabled() const;
        [[nodiscard]] bool IsProcess() const;
        [[nodiscard]] bool IsFixedUpdate() const;
        [[nodiscard]] bool IsUnscaled() const;
        [[nodiscard]] bool IsReset() const;
        [[nodiscard]] bool IsKeepReset() const;
        [[nodiscard]] bool IsInertiaShift() const;
        [[nodiscard]] bool IsStepRunning() const;
        [[nodiscard]] bool IsCameraCullingInvisible() const;
        [[nodiscard]] bool IsCameraCullingKeep() const;
        [[nodiscard]] bool IsDistanceCullingInvisible() const;
        [[nodiscard]] bool IsCullingInvisible() const;
        [[nodiscard]] bool IsRunning() const;
        [[nodiscard]] bool IsNegativeScale() const;
        [[nodiscard]] bool IsNegativeScaleTeleport() const;
        [[nodiscard]] bool IsSpring() const;
        [[nodiscard]] int ParticleCount() const;
        [[nodiscard]] int ColliderCount() const;
        [[nodiscard]] int BaseLineCount() const;
        [[nodiscard]] int TriangleCount() const;
        [[nodiscard]] int EdgeCount() const;
        [[nodiscard]] float InitScale() const;
    };

    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] int TeamCount() const;
    [[nodiscard]] int TrueTeamCount() const;
    [[nodiscard]] int ActiveTeamCount() const;
    [[nodiscard]] bool ContainsTeamData(int team_id) const;
    [[nodiscard]] bool IsValidTeam(int team_id) const;
    [[nodiscard]] bool IsEnable(int team_id) const;
    [[nodiscard]] bool IsProcess(int team_id) const;
    [[nodiscard]] const TeamData& GetTeamData(int team_id) const;
    [[nodiscard]] TeamData& GetTeamData(int team_id);
    [[nodiscard]] const ClothParameters& GetParameters(int team_id) const;
    [[nodiscard]] ClothParameters& GetParameters(int team_id);
    void SetParameters(int team_id, const ClothParameters& parameters);
    [[nodiscard]] const InertiaCenterData& GetCenterData(int team_id) const;
    [[nodiscard]] InertiaCenterData& GetCenterData(int team_id);
    void SetCenterData(int team_id, const InertiaCenterData& center_data);
    [[nodiscard]] int CenterDataCount() const;
    [[nodiscard]] const TeamWindData& GetTeamWindData(int team_id) const;
    [[nodiscard]] TeamWindData& GetTeamWindData(int team_id);
    [[nodiscard]] int MappingCount() const;
    [[nodiscard]] const ExNativeArray<MappingData>& MappingDataArray() const;
    [[nodiscard]] ExNativeArray<MappingData>& MappingDataArray();
    [[nodiscard]] const TeamMappingList& GetTeamMapping(int team_id) const;
    [[nodiscard]] TeamMappingList& GetTeamMapping(int team_id);

    [[nodiscard]] int CreateTeam(bool enabled = true, bool spring = false);
    [[nodiscard]] int CreateTeam(const ClothParameters& parameters, bool enabled = true, bool spring = false);
    void ReleaseTeam(int team_id);
    void ClearTeams();
    void SetEnable(int team_id, bool enabled);
    void SetSkipWriting(int team_id, bool skip_writing);
    void SetReset(int team_id, bool reset);
    void SetTimeReset(int team_id, bool reset);
    void SetAnimationPoseRatio(int team_id, float ratio);
    void SetUpdateMode(int team_id, ClothUpdateMode update_mode);
    void SetTimeScale(int team_id, float time_scale);
    void SetSyncSuspend(int team_id, bool suspend);
    void SetCameraCullingInvisible(int team_id, bool invisible, bool keep = false);
    void SetDistanceCullingInvisible(int team_id, bool invisible, float distance_weight = 1.0f);
    void SetAnchor(
        int team_id,
        int anchor_transform_id,
        const float3& anchor_position,
        const quaternion& anchor_rotation
    );
    void AddForce(int team_id, ClothForceMode force_mode, const float3& force);
    void ClearForce(int team_id);
    [[nodiscard]] bool RestoreTransformOnlyOnce(int team_id) const;
    void ClearRestoreTransformOnlyOnce(int team_id);
    [[nodiscard]] int EdgeColliderCollisionCount() const;
    [[nodiscard]] int AlwaysTeamUpdate(
        float frame_delta_time,
        float fixed_delta_time,
        float unscaled_delta_time,
        float global_time_scale,
        float simulation_delta_time,
        int max_simulation_count_per_frame
    );
    void SimulationStepTeamUpdate(int update_index, float simulation_delta_time);
    void UpdateCenterAndInertia(
        float simulation_delta_time,
        const TransformManager& transform_manager,
        const VirtualMeshManager& virtual_mesh_manager,
        const WindManager& wind_manager,
        const ExNativeArray<std::uint16_t>& fixed_array
    );
    void PostTeamUpdate();
    bool SetSyncTeam(int team_id, int sync_team_id);
    bool AddSyncParent(int sync_team_id, int parent_team_id);
    bool RemoveSyncParent(int sync_team_id, int parent_team_id);
    [[nodiscard]] int RegisterMappingData(int team_id, const MappingData& mapping_data);
    void RemoveMappingData(int team_id, int mapping_index);
    void ClearTeamMappings(int team_id);
    void SyncTeamTimeAndParameters(int team_id, int sync_team_id);

private:
    bool initialized_ = false;
    ExSimpleNativeArray<TeamData> team_data_array_;
    ExSimpleNativeArray<TeamMappingList> team_mapping_index_array_;
    ExNativeArray<MappingData> mapping_data_array_;
    ExSimpleNativeArray<ClothParameters> parameter_array_;
    ExSimpleNativeArray<InertiaCenterData> center_data_array_;
    ExSimpleNativeArray<TeamWindData> team_wind_array_;
    std::vector<int> free_team_ids_;
    int edge_collider_collision_count_ = 0;

    void UpdateTeamWind(
        int team_id,
        const ClothParameters& parameters,
        const float3& center_world_position,
        const WindManager& wind_manager
    );
};

}  // namespace hocloth::mc2
