#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/i_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/ex_simple_native_array.hpp"

#include <vector>

namespace hocloth::mc2 {

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

    struct TeamData {
        BitFlag64 flag;

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

        int sync_team_id = 0;
        int sync_center_transform_index = -1;
        float animation_pose_ratio = 1.0f;
        float velocity_weight = 1.0f;
        float distance_weight = 1.0f;
        float blend_weight = 1.0f;

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
        [[nodiscard]] bool IsCullingInvisible() const;
        [[nodiscard]] bool IsSpring() const;
        [[nodiscard]] int ParticleCount() const;
        [[nodiscard]] int ColliderCount() const;
        [[nodiscard]] int BaseLineCount() const;
        [[nodiscard]] int TriangleCount() const;
        [[nodiscard]] int EdgeCount() const;
    };

    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] int TeamCount() const;
    [[nodiscard]] bool IsValidTeam(int team_id) const;
    [[nodiscard]] const TeamData& GetTeamData(int team_id) const;
    [[nodiscard]] TeamData& GetTeamData(int team_id);

    [[nodiscard]] int CreateTeam(bool enabled = true, bool spring = false);
    void ReleaseTeam(int team_id);
    void ClearTeams();

private:
    bool initialized_ = false;
    ExSimpleNativeArray<TeamData> team_data_array_;
    std::vector<int> free_team_ids_;
};

}  // namespace hocloth::mc2
