#include "hocloth/manager/team/team_manager.hpp"

#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/manager/simulation/wind_manager.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace hocloth::mc2 {

namespace {

TeamManager::TeamData MakeEmptyTeamData()
{
    TeamManager::TeamData data;
    data.flag.Clear();
    return data;
}

float SignNonZero(float value)
{
    return value < 0.0f ? -1.0f : 1.0f;
}

float3 ComponentSign(const float3& value)
{
    return float3{SignNonZero(value.x), SignNonZero(value.y), SignNonZero(value.z)};
}

float4 NegativeScaleQuaternionValue(const float3& scale_direction)
{
    return float4{-scale_direction.x, -scale_direction.y, -scale_direction.z, 1.0f};
}

quaternion RotationFromNormalTangent(const quaternion& rotation)
{
    float3 normal;
    float3 tangent;
    ToNormalTangent(rotation, normal, tangent);
    return ToRotation(Scale(normal, -1.0f), Scale(tangent, -1.0f));
}

}  // namespace

int TeamManager::TeamSyncParentList::Length() const
{
    return length;
}

bool TeamManager::TeamSyncParentList::IsFull() const
{
    return length >= Capacity;
}

bool TeamManager::TeamSyncParentList::Contains(int team_id) const
{
    for (int index = 0; index < length; ++index) {
        if (values[static_cast<std::size_t>(index)] == team_id) {
            return true;
        }
    }
    return false;
}

int TeamManager::TeamSyncParentList::operator[](int index) const
{
    if (index < 0 || index >= length) {
        return 0;
    }
    return values[static_cast<std::size_t>(index)];
}

bool TeamManager::TeamSyncParentList::Add(int team_id)
{
    if (team_id <= 0 || Contains(team_id) || IsFull()) {
        return false;
    }
    values[static_cast<std::size_t>(length)] = team_id;
    ++length;
    return true;
}

bool TeamManager::TeamSyncParentList::RemoveSwapBack(int team_id)
{
    for (int index = 0; index < length; ++index) {
        if (values[static_cast<std::size_t>(index)] != team_id) {
            continue;
        }
        --length;
        values[static_cast<std::size_t>(index)] = values[static_cast<std::size_t>(length)];
        values[static_cast<std::size_t>(length)] = 0;
        return true;
    }
    return false;
}

void TeamManager::TeamSyncParentList::Clear()
{
    values.fill(0);
    length = 0;
}

int TeamManager::TeamMappingList::Length() const
{
    return length;
}

bool TeamManager::TeamMappingList::IsFull() const
{
    return length >= Capacity;
}

bool TeamManager::TeamMappingList::Contains(short mapping_index) const
{
    for (int index = 0; index < length; ++index) {
        if (values[static_cast<std::size_t>(index)] == mapping_index) {
            return true;
        }
    }
    return false;
}

short TeamManager::TeamMappingList::operator[](int index) const
{
    if (index < 0 || index >= length) {
        return -1;
    }
    return values[static_cast<std::size_t>(index)];
}

bool TeamManager::TeamMappingList::Add(short mapping_index)
{
    if (mapping_index < 0 || Contains(mapping_index) || IsFull()) {
        return false;
    }
    values[static_cast<std::size_t>(length)] = mapping_index;
    ++length;
    return true;
}

bool TeamManager::TeamMappingList::RemoveSwapBack(short mapping_index)
{
    for (int index = 0; index < length; ++index) {
        if (values[static_cast<std::size_t>(index)] != mapping_index) {
            continue;
        }
        --length;
        values[static_cast<std::size_t>(index)] = values[static_cast<std::size_t>(length)];
        values[static_cast<std::size_t>(length)] = 0;
        return true;
    }
    return false;
}

void TeamManager::TeamMappingList::Clear()
{
    values.fill(0);
    length = 0;
}

bool TeamManager::MappingData::IsValid() const
{
    return team_id > 0;
}

int TeamManager::MappingData::VertexCount() const
{
    return mapping_common_chunk.data_length;
}

bool TeamManager::TeamData::IsValid() const
{
    return flag.IsSet(TeamManager::FlagValid);
}

bool TeamManager::TeamData::IsEnabled() const
{
    return flag.IsSet(TeamManager::FlagEnable);
}

bool TeamManager::TeamData::IsCullingInvisible() const
{
    return IsCameraCullingInvisible() || IsDistanceCullingInvisible();
}

bool TeamManager::TeamData::IsProcess() const
{
    return IsEnabled() && !flag.IsSet(TeamManager::FlagSyncSuspend) && !IsCullingInvisible();
}

bool TeamManager::TeamData::IsFixedUpdate() const
{
    return update_mode == ClothUpdateMode::UnityPhysics;
}

bool TeamManager::TeamData::IsUnscaled() const
{
    return update_mode == ClothUpdateMode::Unscaled;
}

bool TeamManager::TeamData::IsReset() const
{
    return flag.IsSet(TeamManager::FlagReset);
}

bool TeamManager::TeamData::IsKeepReset() const
{
    return flag.IsSet(TeamManager::FlagKeepTeleport);
}

bool TeamManager::TeamData::IsInertiaShift() const
{
    return flag.IsSet(TeamManager::FlagInertiaShift);
}

bool TeamManager::TeamData::IsRunning() const
{
    return flag.IsSet(TeamManager::FlagRunning);
}

bool TeamManager::TeamData::IsStepRunning() const
{
    return flag.IsSet(TeamManager::FlagStepRunning);
}

bool TeamManager::TeamData::IsCameraCullingInvisible() const
{
    return flag.IsSet(TeamManager::FlagCameraCullingInvisible);
}

bool TeamManager::TeamData::IsCameraCullingKeep() const
{
    return flag.IsSet(TeamManager::FlagCameraCullingKeep);
}

bool TeamManager::TeamData::IsDistanceCullingInvisible() const
{
    return flag.IsSet(TeamManager::FlagDistanceCullingInvisible);
}

bool TeamManager::TeamData::IsNegativeScale() const
{
    return flag.IsSet(TeamManager::FlagNegativeScale);
}

bool TeamManager::TeamData::IsNegativeScaleTeleport() const
{
    return flag.IsSet(TeamManager::FlagNegativeScaleTeleport);
}

bool TeamManager::TeamData::IsSpring() const
{
    return flag.IsSet(TeamManager::FlagSpring);
}

int TeamManager::TeamData::ParticleCount() const
{
    return particle_chunk.data_length;
}

int TeamManager::TeamData::ColliderCount() const
{
    return collider_count;
}

int TeamManager::TeamData::BaseLineCount() const
{
    return baseline_chunk.data_length;
}

int TeamManager::TeamData::TriangleCount() const
{
    return proxy_triangle_chunk.data_length;
}

int TeamManager::TeamData::EdgeCount() const
{
    return proxy_edge_chunk.data_length;
}

float TeamManager::TeamData::InitScale() const
{
    return init_scale.x;
}

Result TeamManager::Initialize()
{
    initialized_ = true;
    team_data_array_.Dispose();
    team_mapping_index_array_.Dispose();
    mapping_data_array_.Dispose();
    parameter_array_.Dispose();
    center_data_array_.Dispose();
    team_wind_array_.Dispose();
    team_data_array_ = ExSimpleNativeArray<TeamData>(1, false);
    team_mapping_index_array_ = ExSimpleNativeArray<TeamMappingList>(1, false);
    mapping_data_array_ = ExNativeArray<MappingData>(32);
    parameter_array_ = ExSimpleNativeArray<ClothParameters>(1, false);
    center_data_array_ = ExSimpleNativeArray<InertiaCenterData>(1, false);
    team_wind_array_ = ExSimpleNativeArray<TeamWindData>(1, false);
    team_data_array_[0] = MakeEmptyTeamData();
    team_mapping_index_array_[0] = TeamMappingList{};
    parameter_array_[0] = ClothParameters{};
    center_data_array_[0] = InertiaCenterData{};
    team_wind_array_[0] = TeamWindData{};
    free_team_ids_.clear();
    return Result::Ok();
}

void TeamManager::Dispose()
{
    team_data_array_.Dispose();
    team_mapping_index_array_.Dispose();
    mapping_data_array_.Dispose();
    parameter_array_.Dispose();
    center_data_array_.Dispose();
    team_wind_array_.Dispose();
    free_team_ids_.clear();
    initialized_ = false;
}

ManagerStatus TeamManager::Status() const
{
    int valid_count = 0;
    for (int index = 1; index < team_data_array_.Count(); ++index) {
        if (team_data_array_[index].IsValid()) {
            ++valid_count;
        }
    }
    return ManagerStatus{
        "TeamManager",
        initialized_,
        static_cast<std::uint32_t>(valid_count),
        "center_data=" + std::to_string(center_data_array_.Count())
            + " wind_data=" + std::to_string(team_wind_array_.Count())
            + " mapping=" + std::to_string(MappingCount()),
    };
}

int TeamManager::TeamCount() const
{
    return team_data_array_.Count();
}

int TeamManager::TrueTeamCount() const
{
    int count = 0;
    for (int team_id = 1; team_id < team_data_array_.Count(); ++team_id) {
        if (team_data_array_[team_id].IsValid()) {
            ++count;
        }
    }
    return count;
}

int TeamManager::ActiveTeamCount() const
{
    int count = 0;
    for (int team_id = 1; team_id < team_data_array_.Count(); ++team_id) {
        if (team_data_array_[team_id].IsValid() && team_data_array_[team_id].IsEnabled()) {
            ++count;
        }
    }
    return count;
}

bool TeamManager::ContainsTeamData(int team_id) const
{
    return IsValidTeam(team_id);
}

bool TeamManager::IsValidTeam(int team_id) const
{
    return team_id > 0
        && team_id < team_data_array_.Count()
        && team_data_array_[team_id].IsValid();
}

bool TeamManager::IsEnable(int team_id) const
{
    return IsValidTeam(team_id) && team_data_array_[team_id].IsEnabled();
}

bool TeamManager::IsProcess(int team_id) const
{
    return IsValidTeam(team_id) && team_data_array_[team_id].IsProcess();
}

const TeamManager::TeamData& TeamManager::GetTeamData(int team_id) const
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return team_data_array_[team_id];
}

TeamManager::TeamData& TeamManager::GetTeamData(int team_id)
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return team_data_array_[team_id];
}

const ClothParameters& TeamManager::GetParameters(int team_id) const
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return parameter_array_[team_id];
}

ClothParameters& TeamManager::GetParameters(int team_id)
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return parameter_array_[team_id];
}

void TeamManager::SetParameters(int team_id, const ClothParameters& parameters)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    parameter_array_[team_id] = parameters;
}

const InertiaCenterData& TeamManager::GetCenterData(int team_id) const
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return center_data_array_[team_id];
}

InertiaCenterData& TeamManager::GetCenterData(int team_id)
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return center_data_array_[team_id];
}

void TeamManager::SetCenterData(int team_id, const InertiaCenterData& center_data)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    center_data_array_[team_id] = center_data;
}

int TeamManager::CenterDataCount() const
{
    return center_data_array_.Count();
}

const TeamWindData& TeamManager::GetTeamWindData(int team_id) const
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return team_wind_array_[team_id];
}

TeamWindData& TeamManager::GetTeamWindData(int team_id)
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return team_wind_array_[team_id];
}

int TeamManager::MappingCount() const
{
    return mapping_data_array_.Count();
}

const ExNativeArray<TeamManager::MappingData>& TeamManager::MappingDataArray() const
{
    return mapping_data_array_;
}

ExNativeArray<TeamManager::MappingData>& TeamManager::MappingDataArray()
{
    return mapping_data_array_;
}

const TeamManager::TeamMappingList& TeamManager::GetTeamMapping(int team_id) const
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return team_mapping_index_array_[team_id];
}

TeamManager::TeamMappingList& TeamManager::GetTeamMapping(int team_id)
{
    if (!IsValidTeam(team_id)) {
        throw std::runtime_error("Invalid MC2 team id.");
    }
    return team_mapping_index_array_[team_id];
}

int TeamManager::CreateTeam(bool enabled, bool spring)
{
    return CreateTeam(ClothParameters{}, enabled, spring);
}

int TeamManager::CreateTeam(const ClothParameters& parameters, bool enabled, bool spring)
{
    if (!initialized_) {
        Initialize();
    }

    int team_id = 0;
    if (!free_team_ids_.empty()) {
        team_id = free_team_ids_.back();
        free_team_ids_.pop_back();
    } else {
        if (team_data_array_.Count() >= define::system::MaximumTeamCount) {
            throw std::runtime_error("MC2 maximum team count exceeded.");
        }
        team_id = team_data_array_.Add(MakeEmptyTeamData());
        team_mapping_index_array_.Add(TeamMappingList{});
        parameter_array_.Add(ClothParameters{});
        center_data_array_.Add(InertiaCenterData{});
        team_wind_array_.Add(TeamWindData{});
    }

    TeamData data = MakeEmptyTeamData();
    data.flag.Set(FlagValid, true);
    data.flag.Set(FlagEnable, enabled);
    data.flag.Set(FlagReset, true);
    data.flag.Set(FlagTimeReset, true);
    data.flag.Set(FlagSpring, spring);
    data.update_mode = ClothUpdateMode::Normal;
    data.time_scale = 1.0f;
    data.now_time_scale = 1.0f;
    data.velocity_weight = 1.0f;
    data.distance_weight = 1.0f;
    data.blend_weight = 1.0f;
    data.init_scale = float3{1.0f, 1.0f, 1.0f};
    data.scale_ratio = 1.0f;
    data.negative_scale_sign = 1.0f;
    data.negative_scale_direction = float3{1.0f, 1.0f, 1.0f};
    data.negative_scale_change = float3{1.0f, 1.0f, 1.0f};
    data.negative_scale_triangle_sign = float2{1.0f, 1.0f};
    data.negative_scale_quaternion_value = float4{1.0f, 1.0f, 1.0f, 1.0f};
    data.sync_parent_team_ids.Clear();
    data.proxy_mesh_type = VirtualMesh::MeshType::NormalMesh;
    team_data_array_[team_id] = data;
    team_mapping_index_array_[team_id] = TeamMappingList{};
    parameter_array_[team_id] = parameters;
    center_data_array_[team_id] = InertiaCenterData{};
    team_wind_array_[team_id] = TeamWindData{};
    return team_id;
}

void TeamManager::SetEnable(int team_id, bool enabled)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    TeamData& team_data = team_data_array_[team_id];
    team_data.flag.Set(FlagEnable, enabled);
    team_data.flag.Set(FlagReset, enabled);
    if (!enabled) {
        team_data.flag.Set(FlagRestoreTransformOnlyOnce, true);
        team_data.flag.Set(FlagRunning, false);
        team_data.flag.Set(FlagStepRunning, false);
        team_data.update_count = 0;
        team_data.skip_count = 0;
    }
}

void TeamManager::SetSkipWriting(int team_id, bool skip_writing)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].flag.Set(FlagSkipWriting, skip_writing);
    }
}

void TeamManager::SetReset(int team_id, bool reset)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].flag.Set(FlagReset, reset);
    }
}

void TeamManager::SetTimeReset(int team_id, bool reset)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].flag.Set(FlagTimeReset, reset);
    }
}

void TeamManager::SetAnimationPoseRatio(int team_id, float ratio)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].animation_pose_ratio = Clamp01(ratio);
    }
}

void TeamManager::SetUpdateMode(int team_id, ClothUpdateMode update_mode)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].update_mode = update_mode;
    }
}

void TeamManager::SetTimeScale(int team_id, float time_scale)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].time_scale = Clamp01(time_scale);
    }
}

void TeamManager::SetSyncSuspend(int team_id, bool suspend)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].flag.Set(FlagSyncSuspend, suspend);
    }
}

void TeamManager::SetCameraCullingInvisible(int team_id, bool invisible, bool keep)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    TeamData& team_data = team_data_array_[team_id];
    const bool changed = team_data.flag.IsSet(FlagCameraCullingInvisible) != invisible;
    team_data.flag.Set(FlagCameraCullingInvisible, invisible);
    team_data.flag.Set(FlagCameraCullingKeep, invisible && keep);
    if (changed && invisible && !keep) {
        team_data.flag.Set(FlagReset, true);
    }
}

void TeamManager::SetDistanceCullingInvisible(int team_id, bool invisible, float distance_weight)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    TeamData& team_data = team_data_array_[team_id];
    const bool changed = team_data.flag.IsSet(FlagDistanceCullingInvisible) != invisible;
    team_data.flag.Set(FlagDistanceCullingInvisible, invisible);
    team_data.distance_weight = Clamp01(distance_weight);
    if (changed) {
        team_data.flag.Set(FlagReset, true);
        team_data.flag.Set(FlagCameraCullingKeep, false);
    }
}

void TeamManager::SetAnchor(
    int team_id,
    int anchor_transform_id,
    const float3& anchor_position,
    const quaternion& anchor_rotation
)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    TeamData& team_data = team_data_array_[team_id];
    team_data.flag.Set(FlagAnchor, anchor_transform_id != 0);
    team_data.flag.Set(FlagAnchorReset, anchor_transform_id != team_data.anchor_transform_id);
    team_data.anchor_transform_id = anchor_transform_id;

    InertiaCenterData& center_data = center_data_array_[team_id];
    center_data.anchor_position = anchor_position;
    center_data.anchor_rotation = anchor_rotation;
}

void TeamManager::AddForce(int team_id, ClothForceMode force_mode, const float3& force)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    TeamData& team_data = team_data_array_[team_id];
    if (team_data.force_mode == ClothForceMode::None || team_data.force_mode == force_mode) {
        team_data.force_mode = force_mode;
        team_data.impact_force = Add(team_data.impact_force, force);
        return;
    }

    team_data.force_mode = force_mode;
    team_data.impact_force = force;
}

void TeamManager::ClearForce(int team_id)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].force_mode = ClothForceMode::None;
        team_data_array_[team_id].impact_force = float3{};
    }
}

bool TeamManager::RestoreTransformOnlyOnce(int team_id) const
{
    return IsValidTeam(team_id)
        && team_data_array_[team_id].flag.IsSet(FlagRestoreTransformOnlyOnce);
}

void TeamManager::ClearRestoreTransformOnlyOnce(int team_id)
{
    if (IsValidTeam(team_id)) {
        team_data_array_[team_id].flag.Set(FlagRestoreTransformOnlyOnce, false);
    }
}

int TeamManager::EdgeColliderCollisionCount() const
{
    return edge_collider_collision_count_;
}

void TeamManager::ReleaseTeam(int team_id)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    const int sync_team_id = team_data_array_[team_id].sync_team_id;
    if (IsValidTeam(sync_team_id)) {
        team_data_array_[sync_team_id].sync_parent_team_ids.RemoveSwapBack(team_id);
    }
    for (int index = 1; index < team_data_array_.Count(); ++index) {
        if (index == team_id || !team_data_array_[index].IsValid()) {
            continue;
        }
        team_data_array_[index].sync_parent_team_ids.RemoveSwapBack(team_id);
    }
    ClearTeamMappings(team_id);
    team_data_array_[team_id] = MakeEmptyTeamData();
    team_mapping_index_array_[team_id] = TeamMappingList{};
    parameter_array_[team_id] = ClothParameters{};
    center_data_array_[team_id] = InertiaCenterData{};
    team_wind_array_[team_id] = TeamWindData{};
    free_team_ids_.push_back(team_id);
}

void TeamManager::ClearTeams()
{
    if (team_data_array_.Count() > 0) {
        team_data_array_.SetCount(1);
        parameter_array_.SetCount(1);
        center_data_array_.SetCount(1);
        team_wind_array_.SetCount(1);
        team_mapping_index_array_.SetCount(1);
        mapping_data_array_.Clear();
        team_data_array_[0] = MakeEmptyTeamData();
        team_mapping_index_array_[0] = TeamMappingList{};
        parameter_array_[0] = ClothParameters{};
        center_data_array_[0] = InertiaCenterData{};
        team_wind_array_[0] = TeamWindData{};
    }
    free_team_ids_.clear();
}

int TeamManager::AlwaysTeamUpdate(
    float frame_delta_time,
    float fixed_delta_time,
    float unscaled_delta_time,
    float global_time_scale,
    float simulation_delta_time,
    int max_simulation_count_per_frame
)
{
    if (!initialized_ || simulation_delta_time <= define::system::Epsilon) {
        return 0;
    }

    int max_update_count = 0;
    edge_collider_collision_count_ = 0;
    const int max_count = std::max(0, max_simulation_count_per_frame);
    for (int team_id = 1; team_id < team_data_array_.Count(); ++team_id) {
        TeamData& team_data = team_data_array_[team_id];
        if (!team_data.IsProcess()) {
            team_data.update_count = 0;
            team_data.skip_count = 0;
            team_data.flag.Set(FlagRunning, false);
            continue;
        }

        const ClothParameters& parameters = parameter_array_[team_id];
        if (parameters.collider_collision_constraint.mode == ColliderCollisionMode::Edge) {
            edge_collider_collision_count_ += team_data.EdgeCount();
        }

        if (team_data.flag.IsSet(FlagTimeReset)) {
            team_data.time = 0.0f;
            team_data.old_time = 0.0f;
            team_data.now_update_time = 0.0f;
            team_data.old_update_time = 0.0f;
            team_data.frame_update_time = 0.0f;
            team_data.frame_old_time = 0.0f;
        }

        float team_frame_delta_time = frame_delta_time;
        if (team_data.IsFixedUpdate()) {
            team_frame_delta_time = fixed_delta_time;
        } else if (team_data.IsUnscaled()) {
            team_frame_delta_time = unscaled_delta_time;
        }
        team_frame_delta_time = std::max(team_frame_delta_time, 0.0f);
        team_data.frame_delta_time = team_frame_delta_time;

        float time_scale = team_data.time_scale * (team_data.IsUnscaled() ? 1.0f : global_time_scale);
        if (team_data.flag.IsSet(FlagSyncSuspend)) {
            time_scale = 0.0f;
        }
        time_scale = Clamp01(time_scale);
        team_data.now_time_scale = time_scale;

        const float add_time = team_frame_delta_time * time_scale;
        float time = team_data.time + add_time;
        const float interval = time - team_data.now_update_time;
        const int scheduled_update_count =
            interval > 0.0f ? static_cast<int>(interval / simulation_delta_time) : 0;

        team_data.update_count = std::min(scheduled_update_count, max_count);
        team_data.skip_count = scheduled_update_count - team_data.update_count;
        if (team_data.skip_count > 0) {
            time -= simulation_delta_time * static_cast<float>(team_data.skip_count);
        }

        if (team_data.update_count > 0 && add_time == 0.0f) {
            team_data.update_count = 0;
            team_data.skip_count = 0;
            team_data.now_update_time = time - simulation_delta_time + 0.0001f;
        }

        if (team_data.update_count > 0) {
            team_data.frame_old_time = team_data.frame_update_time;
            team_data.frame_update_time = time;
            team_data.old_update_time = team_data.now_update_time;
        }
        team_data.old_time = team_data.time;
        team_data.time = time;
        team_data.flag.Set(FlagRunning, team_data.update_count > 0);

        max_update_count = std::max(max_update_count, team_data.update_count);
    }
    return max_update_count;
}

bool TeamManager::SetSyncTeam(int team_id, int sync_team_id)
{
    if (!IsValidTeam(team_id) || team_id == sync_team_id) {
        return false;
    }
    if (sync_team_id != 0 && !IsValidTeam(sync_team_id)) {
        return false;
    }

    TeamData& team_data = team_data_array_[team_id];
    const int old_sync_team_id = team_data.sync_team_id;
    if (old_sync_team_id == sync_team_id) {
        return true;
    }

    if (IsValidTeam(old_sync_team_id)) {
        team_data_array_[old_sync_team_id].sync_parent_team_ids.RemoveSwapBack(team_id);
    }

    team_data.sync_team_id = sync_team_id;
    team_data.sync_center_transform_index = 0;
    team_data.flag.Set(FlagSynchronization, sync_team_id != 0);

    if (sync_team_id != 0) {
        team_data_array_[sync_team_id].sync_parent_team_ids.Add(team_id);
        team_data.flag.Set(FlagTimeReset, false);
    }
    return true;
}

bool TeamManager::AddSyncParent(int sync_team_id, int parent_team_id)
{
    if (!IsValidTeam(sync_team_id) || !IsValidTeam(parent_team_id) || sync_team_id == parent_team_id) {
        return false;
    }
    return team_data_array_[sync_team_id].sync_parent_team_ids.Add(parent_team_id);
}

bool TeamManager::RemoveSyncParent(int sync_team_id, int parent_team_id)
{
    if (!IsValidTeam(sync_team_id)) {
        return false;
    }
    return team_data_array_[sync_team_id].sync_parent_team_ids.RemoveSwapBack(parent_team_id);
}

int TeamManager::RegisterMappingData(int team_id, const MappingData& mapping_data)
{
    if (!IsValidTeam(team_id)) {
        return -1;
    }

    TeamMappingList& mapping_list = team_mapping_index_array_[team_id];
    if (mapping_list.IsFull()) {
        return -1;
    }

    MappingData data = mapping_data;
    data.team_id = team_id;
    const DataChunk chunk = mapping_data_array_.Add(data);
    const int mapping_index = chunk.start_index;
    if (mapping_index < 0 || mapping_index > 32767) {
        mapping_data_array_.Remove(chunk);
        return -1;
    }

    mapping_list.Add(static_cast<short>(mapping_index));
    return mapping_index;
}

void TeamManager::RemoveMappingData(int team_id, int mapping_index)
{
    if (!IsValidTeam(team_id) || mapping_index < 0 || mapping_index >= mapping_data_array_.Length()) {
        return;
    }

    TeamMappingList& mapping_list = team_mapping_index_array_[team_id];
    mapping_list.RemoveSwapBack(static_cast<short>(mapping_index));
    mapping_data_array_.RemoveAndFill(DataChunk{mapping_index, 1}, MappingData{});
}

void TeamManager::ClearTeamMappings(int team_id)
{
    if (!IsValidTeam(team_id)) {
        return;
    }

    TeamMappingList& mapping_list = team_mapping_index_array_[team_id];
    while (mapping_list.Length() > 0) {
        const int mapping_index = mapping_list[0];
        mapping_list.RemoveSwapBack(static_cast<short>(mapping_index));
        if (mapping_index >= 0 && mapping_index < mapping_data_array_.Length()) {
            mapping_data_array_.RemoveAndFill(DataChunk{mapping_index, 1}, MappingData{});
        }
    }
}

void TeamManager::SyncTeamTimeAndParameters(int team_id, int sync_team_id)
{
    if (!IsValidTeam(team_id) || !IsValidTeam(sync_team_id)) {
        return;
    }

    TeamData& team_data = team_data_array_[team_id];
    const TeamData& sync_team_data = team_data_array_[sync_team_id];
    team_data.update_mode = sync_team_data.update_mode;
    team_data.time = sync_team_data.time;
    team_data.old_time = sync_team_data.old_time;
    team_data.now_update_time = sync_team_data.now_update_time;
    team_data.old_update_time = sync_team_data.old_update_time;
    team_data.frame_update_time = sync_team_data.frame_update_time;
    team_data.frame_old_time = sync_team_data.frame_old_time;
    team_data.time_scale = sync_team_data.time_scale;
    team_data.update_count = sync_team_data.update_count;
    team_data.frame_interpolation = sync_team_data.frame_interpolation;
    team_data.skip_count = sync_team_data.skip_count;
    team_data.sync_center_transform_index = sync_team_data.center_transform_index;

    ClothParameters& parameters = parameter_array_[team_id];
    const ClothParameters& sync_parameters = parameter_array_[sync_team_id];
    parameters.inertia_constraint.anchor_inertia =
        sync_parameters.inertia_constraint.anchor_inertia;
    parameters.inertia_constraint.world_inertia =
        sync_parameters.inertia_constraint.world_inertia;
    parameters.inertia_constraint.movement_inertia_smoothing =
        sync_parameters.inertia_constraint.movement_inertia_smoothing;
    parameters.inertia_constraint.movement_speed_limit =
        sync_parameters.inertia_constraint.movement_speed_limit;
    parameters.inertia_constraint.rotation_speed_limit =
        sync_parameters.inertia_constraint.rotation_speed_limit;
    parameters.inertia_constraint.teleport_mode =
        sync_parameters.inertia_constraint.teleport_mode;
    parameters.inertia_constraint.teleport_distance =
        sync_parameters.inertia_constraint.teleport_distance;
    parameters.inertia_constraint.teleport_rotation =
        sync_parameters.inertia_constraint.teleport_rotation;
}

void TeamManager::SimulationStepTeamUpdate(int update_index, float simulation_delta_time)
{
    if (!initialized_ || simulation_delta_time <= define::system::Epsilon) {
        return;
    }

    for (int team_id = 1; team_id < team_data_array_.Count(); ++team_id) {
        TeamData& team_data = team_data_array_[team_id];
        if (!team_data.IsProcess()) {
            continue;
        }

        const bool run_step = update_index < team_data.update_count;
        team_data.flag.Set(FlagStepRunning, run_step);
        if (!run_step) {
            continue;
        }

        const ClothParameters& parameters = parameter_array_[team_id];
        InertiaCenterData& center_data = center_data_array_[team_id];

        team_data.now_update_time += simulation_delta_time;
        const float frame_duration = team_data.time - team_data.frame_old_time;
        team_data.frame_interpolation =
            std::abs(frame_duration) > define::system::Epsilon
                ? Clamp01((team_data.now_update_time - team_data.frame_old_time) / frame_duration)
                : 1.0f;

        center_data.old_world_position = center_data.now_world_position;
        center_data.old_world_rotation = center_data.now_world_rotation;
        center_data.now_world_position = Lerp(
            center_data.old_frame_world_position,
            center_data.frame_world_position,
            team_data.frame_interpolation
        );
        center_data.now_world_rotation = Normalize(Slerp(
            center_data.old_frame_world_rotation,
            center_data.frame_world_rotation,
            team_data.frame_interpolation
        ));
        const float3 world_scale = Lerp(
            center_data.old_frame_world_scale,
            center_data.frame_world_scale,
            team_data.frame_interpolation
        );

        center_data.step_vector =
            Subtract(center_data.now_world_position, center_data.old_world_position);
        center_data.step_rotation =
            FromToRotation(center_data.old_world_rotation, center_data.now_world_rotation);
        const float step_angle =
            Angle(center_data.old_world_rotation, center_data.now_world_rotation);

        float local_movement_inertia = 1.0f - parameters.inertia_constraint.local_inertia;
        float local_rotation_inertia = 1.0f - parameters.inertia_constraint.local_inertia;

        const float3 local_vector =
            Scale(center_data.step_vector, 1.0f - local_movement_inertia);
        const float local_movement_speed = Length(local_vector) / simulation_delta_time;
        if (local_movement_speed > parameters.inertia_constraint.local_movement_speed_limit
            && parameters.inertia_constraint.local_movement_speed_limit >= 0.0f) {
            const float t =
                parameters.inertia_constraint.local_movement_speed_limit / local_movement_speed;
            local_movement_inertia = 1.0f + (local_movement_inertia - 1.0f) * t;
        }

        constexpr float radians_to_degrees = 57.29577951308232f;
        const float local_angle = step_angle * (1.0f - local_rotation_inertia);
        const float local_angle_speed = local_angle * radians_to_degrees / simulation_delta_time;
        if (local_angle_speed > parameters.inertia_constraint.local_rotation_speed_limit
            && parameters.inertia_constraint.local_rotation_speed_limit >= 0.0f) {
            const float t =
                parameters.inertia_constraint.local_rotation_speed_limit / local_angle_speed;
            local_rotation_inertia = 1.0f + (local_rotation_inertia - 1.0f) * t;
        }

        center_data.step_move_inertia_ratio = local_movement_inertia;
        center_data.step_rotation_inertia_ratio = local_rotation_inertia;
        center_data.inertia_vector = Scale(center_data.step_vector, local_movement_inertia);
        center_data.inertia_rotation =
            Slerp(quaternion{}, center_data.step_rotation, local_rotation_inertia);

        center_data.angular_velocity = step_angle / simulation_delta_time;
        if (center_data.angular_velocity > define::system::Epsilon) {
            float ignored_angle = 0.0f;
            ToAngleAxis(center_data.step_rotation, ignored_angle, center_data.rotation_axis);
        } else {
            center_data.rotation_axis = float3{};
        }

        const float init_scale_length = std::max(Length(team_data.init_scale), define::system::Epsilon);
        team_data.scale_ratio = std::max(Length(world_scale) / init_scale_length, 1.0e-6f);

        float gravity_dot = 1.0f;
        if (LengthSquared(parameters.world_gravity_direction) > define::system::Epsilon) {
            float3 init_local_gravity_direction = center_data.init_local_gravity_direction;
            init_local_gravity_direction.y *= team_data.negative_scale_direction.y;
            const float3 world_falloff_direction =
                Rotate(center_data.now_world_rotation, init_local_gravity_direction);
            gravity_dot = Clamp01(
                Dot(world_falloff_direction, parameters.world_gravity_direction) * 0.5f + 0.5f
            );
        }
        team_data.gravity_dot = gravity_dot;

        float gravity_ratio = 1.0f;
        if (parameters.gravity > 1.0e-6f && parameters.gravity_falloff > 1.0e-6f) {
            gravity_ratio = (1.0f - parameters.gravity_falloff)
                + (1.0f - (1.0f - parameters.gravity_falloff)) * Clamp01(1.0f - gravity_dot);
        }
        team_data.gravity_ratio = gravity_ratio;

        if (team_data.velocity_weight < 1.0f) {
            const float add_weight =
                parameters.stabilization_time_after_reset > 1.0e-6f
                    ? simulation_delta_time / parameters.stabilization_time_after_reset
                    : 1.0f;
            team_data.velocity_weight = Clamp01(team_data.velocity_weight + add_weight);
        }
        team_data.blend_weight = Clamp01(
            team_data.velocity_weight * parameters.blend_weight * team_data.distance_weight
        );

    }
}

void TeamManager::UpdateCenterAndInertia(
    float simulation_delta_time,
    const TransformManager& transform_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    const WindManager& wind_manager,
    const ExNativeArray<std::uint16_t>& fixed_array
)
{
    if (!initialized_) {
        return;
    }

    constexpr float radians_to_degrees = 57.29577951308232f;
    const auto& transform_data = transform_manager.Data();
    const auto& proxy_positions = virtual_mesh_manager.Positions();
    const auto& proxy_rotations = virtual_mesh_manager.Rotations();
    const auto& vertex_bind_pose_rotations = virtual_mesh_manager.VertexBindPoseRotations();

    for (int team_id = 1; team_id < team_data_array_.Count(); ++team_id) {
        TeamData& team_data = team_data_array_[team_id];
        if (!team_data.IsProcess()) {
            continue;
        }

        ClothParameters& parameters = parameter_array_[team_id];
        if (team_data.sync_team_id != 0 && team_data.flag.IsSet(FlagSynchronization)) {
            SyncTeamTimeAndParameters(team_id, team_data.sync_team_id);
        }

        InertiaCenterData& center_data = center_data_array_[team_id];
        const int center_transform_index =
            team_data.sync_team_id != 0 && team_data.flag.IsSet(FlagSynchronization)
                ? team_data.sync_center_transform_index
                : center_data.center_transform_index;
        if (center_transform_index < 0
            || center_transform_index >= transform_data.position_array.Length()
            || center_transform_index >= transform_data.rotation_array.Length()
            || center_transform_index >= transform_data.scale_array.Length()) {
            continue;
        }

        const float3 component_world_position = transform_data.position_array[center_transform_index];
        const quaternion component_world_rotation =
            transform_data.rotation_array[center_transform_index];
        const float3 component_world_scale = transform_data.scale_array[center_transform_index];
        center_data.component_world_position = component_world_position;
        center_data.component_world_rotation = component_world_rotation;
        center_data.component_world_scale = component_world_scale;

        const float init_scale_length = std::max(Length(team_data.init_scale), define::system::Epsilon);
        const float component_scale_ratio = Length(component_world_scale) / init_scale_length;

        const float3 old_scale_direction = team_data.negative_scale_direction;
        team_data.negative_scale_direction = ComponentSign(component_world_scale);
        team_data.negative_scale_change = float3{
            old_scale_direction.x * team_data.negative_scale_direction.x,
            old_scale_direction.y * team_data.negative_scale_direction.y,
            old_scale_direction.z * team_data.negative_scale_direction.z,
        };

        const bool negative_scale =
            component_world_scale.x < 0.0f
            || component_world_scale.y < 0.0f
            || component_world_scale.z < 0.0f;
        if (negative_scale) {
            team_data.negative_scale_sign = -1.0f;
            team_data.negative_scale_quaternion_value =
                NegativeScaleQuaternionValue(team_data.negative_scale_direction);
            team_data.negative_scale_triangle_sign = float2{
                component_world_scale.x < 0.0f || component_world_scale.z < 0.0f ? -1.0f : 1.0f,
                component_world_scale.x < 0.0f ? -1.0f : 1.0f,
            };
            team_data.flag.Set(FlagNegativeScale, true);
        } else {
            team_data.negative_scale_sign = 1.0f;
            team_data.negative_scale_quaternion_value = float4{1.0f, 1.0f, 1.0f, 1.0f};
            team_data.negative_scale_triangle_sign = float2{1.0f, 1.0f};
            team_data.flag.Set(FlagNegativeScale, false);
        }

        if (team_data.negative_scale_change.x < 0.0f
            || team_data.negative_scale_change.y < 0.0f
            || team_data.negative_scale_change.z < 0.0f) {
            team_data.flag.Set(FlagNegativeScaleTeleport, true);
            const float4x4 now_component =
                TRS(component_world_position, component_world_rotation, component_world_scale);
            const float4x4 old_component = TRS(
                center_data.old_component_world_position,
                center_data.old_component_world_rotation,
                center_data.old_component_world_scale
            );
            const float4x4 component_negative_matrix =
                Multiply(now_component, InverseAffine(old_component));
            center_data.old_component_world_position =
                TransformPoint(center_data.old_component_world_position, component_negative_matrix);
            center_data.old_component_world_scale = component_world_scale;
            center_data.old_anchor_position =
                TransformPoint(center_data.old_anchor_position, component_negative_matrix);
            center_data.smoothing_velocity =
                TransformVector(center_data.smoothing_velocity, component_negative_matrix);
        }

        float3 old_component_position = center_data.old_component_world_position;
        quaternion old_component_rotation = center_data.old_component_world_rotation;
        float3 old_component_scale = center_data.old_component_world_scale;
        float3 center_world_position = component_world_position;
        quaternion center_world_rotation = component_world_rotation;

        if (team_data.fixed_data_chunk.IsValid()
            && team_data.fixed_data_chunk.data_length > 0
            && team_data.proxy_common_chunk.IsValid()) {
            float3 center_sum{};
            float3 normal_sum{};
            float3 tangent_sum{};
            int valid_fixed_count = 0;
            const int vertex_start = team_data.proxy_common_chunk.start_index;
            for (int i = 0; i < team_data.fixed_data_chunk.data_length; ++i) {
                const int fixed_index = team_data.fixed_data_chunk.start_index + i;
                if (fixed_index < 0 || fixed_index >= fixed_array.Length()) {
                    continue;
                }
                const int vertex_index = vertex_start + fixed_array[fixed_index];
                if (vertex_index < 0
                    || vertex_index >= proxy_positions.Length()
                    || vertex_index >= proxy_rotations.Length()
                    || vertex_index >= vertex_bind_pose_rotations.Length()) {
                    continue;
                }

                center_sum = Add(center_sum, proxy_positions[vertex_index]);
                quaternion fixed_rotation = proxy_rotations[vertex_index];
                if (team_data.negative_scale_sign < 0.0f) {
                    fixed_rotation = RotationFromNormalTangent(fixed_rotation);
                }
                fixed_rotation = Normalize(Multiply(
                    fixed_rotation,
                    vertex_bind_pose_rotations[vertex_index]
                ));
                float3 normal;
                float3 tangent;
                ToNormalTangent(fixed_rotation, normal, tangent);
                normal_sum = Add(normal_sum, normal);
                tangent_sum = Add(tangent_sum, tangent);
                ++valid_fixed_count;
            }

            if (valid_fixed_count > 0
                && LengthSquared(normal_sum) > define::system::Epsilon
                && LengthSquared(tangent_sum) > define::system::Epsilon) {
                center_world_position =
                    Scale(center_sum, 1.0f / static_cast<float>(valid_fixed_count));
                const float normal_flip =
                    team_data.negative_scale_direction.x < 0.0f
                        || team_data.negative_scale_direction.z < 0.0f
                        ? -1.0f
                        : 1.0f;
                const float tangent_flip =
                    team_data.negative_scale_direction.x < 0.0f
                        || team_data.negative_scale_direction.y < 0.0f
                        ? -1.0f
                        : 1.0f;
                center_world_rotation = ToRotation(
                    Normalize(Scale(normal_sum, normal_flip), float3{0.0f, 1.0f, 0.0f}),
                    Normalize(Scale(tangent_sum, tangent_flip), float3{0.0f, 0.0f, 1.0f})
                );
            }
        }

        if (team_data.IsNegativeScaleTeleport()) {
            const float4x4 now_center =
                TRS(center_world_position, center_world_rotation, component_world_scale);
            const float4x4 old_center = TRS(
                center_data.old_frame_world_position,
                center_data.old_frame_world_rotation,
                center_data.old_frame_world_scale
            );
            center_data.negative_scale_matrix = Multiply(now_center, InverseAffine(old_center));
        }

        float3 anchor_delta_vector{};
        quaternion anchor_delta_rotation{};
        if (team_data.flag.IsSet(FlagAnchorReset) || team_data.IsReset()) {
            center_data.old_anchor_position = center_data.anchor_position;
            center_data.old_anchor_rotation = center_data.anchor_rotation;
            center_data.anchor_component_local_position = InverseTransformPoint(
                component_world_position,
                center_data.anchor_position,
                center_data.anchor_rotation,
                float3{1.0f, 1.0f, 1.0f}
            );
        }
        if (team_data.flag.IsSet(FlagAnchor)) {
            const float3 anchor_center_position = TransformPoint(
                center_data.anchor_component_local_position,
                center_data.anchor_position,
                center_data.anchor_rotation,
                float3{1.0f, 1.0f, 1.0f}
            );
            anchor_delta_vector = Subtract(anchor_center_position, old_component_position);
            anchor_delta_rotation =
                FromToRotation(center_data.old_anchor_rotation, center_data.anchor_rotation);
            const float anchor_ratio = 1.0f - parameters.inertia_constraint.anchor_inertia;
            anchor_delta_vector = Scale(anchor_delta_vector, anchor_ratio);
            anchor_delta_rotation = Slerp(quaternion{}, anchor_delta_rotation, anchor_ratio);
            old_component_position = Add(old_component_position, anchor_delta_vector);
            old_component_rotation = Multiply(anchor_delta_rotation, old_component_rotation);
            team_data.flag.Set(FlagInertiaShift, true);
        }

        const float3 frame_delta_vector =
            Subtract(component_world_position, old_component_position);
        const float frame_delta_angle =
            Angle(old_component_rotation, component_world_rotation);
        if (parameters.inertia_constraint.teleport_mode != 0 && !team_data.IsReset()) {
            bool is_teleport =
                Length(frame_delta_vector)
                    >= parameters.inertia_constraint.teleport_distance * component_scale_ratio;
            is_teleport = is_teleport
                || frame_delta_angle * radians_to_degrees
                    >= parameters.inertia_constraint.teleport_rotation;
            if (is_teleport) {
                if (parameters.inertia_constraint.teleport_mode == 1) {
                    team_data.flag.Set(FlagReset, true);
                } else if (parameters.inertia_constraint.teleport_mode == 2) {
                    team_data.flag.Set(FlagKeepTeleport, true);
                }
            }
        }

        float3 smooth_delta_vector{};
        if (parameters.inertia_constraint.movement_inertia_smoothing >= 1.0e-6f) {
            if (team_data.IsRunning()) {
                float3 frame_delta_velocity =
                    team_data.frame_delta_time > 0.0f
                        ? Scale(frame_delta_vector, 1.0f / team_data.frame_delta_time)
                        : float3{};
                const float movement_speed_limit =
                    parameters.inertia_constraint.movement_speed_limit * component_scale_ratio;
                if (movement_speed_limit >= 0.0f) {
                    frame_delta_velocity = ClampVector(frame_delta_velocity, movement_speed_limit);
                }
                const float average_ratio = Clamp01(
                    std::pow(1.0f - parameters.inertia_constraint.movement_inertia_smoothing, 3.0f)
                        * 0.99f
                    + 0.01f
                );
                center_data.smoothing_velocity =
                    Lerp(center_data.smoothing_velocity, frame_delta_velocity, average_ratio);
            }

            const float3 smooth_position = Subtract(
                component_world_position,
                Scale(center_data.smoothing_velocity, team_data.frame_delta_time)
            );
            smooth_delta_vector = Subtract(smooth_position, old_component_position);
            old_component_position = smooth_position;
            team_data.flag.Set(FlagInertiaShift, true);
        }

        center_data.frame_world_position = center_world_position;
        center_data.frame_world_rotation = center_world_rotation;
        center_data.frame_world_scale = component_world_scale;
        if (team_data.IsReset()) {
            center_data.old_component_world_position = component_world_position;
            center_data.old_component_world_rotation = component_world_rotation;
            center_data.old_component_world_scale = component_world_scale;
            old_component_position = component_world_position;
            old_component_rotation = component_world_rotation;
            old_component_scale = component_world_scale;

            center_data.old_frame_world_position = center_world_position;
            center_data.old_frame_world_rotation = center_world_rotation;
            center_data.old_frame_world_scale = component_world_scale;
            center_data.now_world_position = center_world_position;
            center_data.now_world_rotation = center_world_rotation;
            center_data.old_world_position = center_world_position;
            center_data.old_world_rotation = center_world_rotation;
        } else if (team_data.IsNegativeScaleTeleport()) {
            center_data.old_frame_world_position = center_world_position;
            center_data.old_frame_world_rotation = center_world_rotation;
            center_data.old_frame_world_scale = component_world_scale;
            center_data.now_world_position = center_world_position;
            center_data.now_world_rotation = center_world_rotation;
            center_data.old_world_position = center_world_position;
            center_data.old_world_rotation = center_world_rotation;
        }

        float3 work_old_component_position = old_component_position;
        quaternion work_old_component_rotation = old_component_rotation;
        if (team_data.IsReset()) {
            center_data.frame_component_shift_vector = float3{};
            center_data.frame_component_shift_rotation = quaternion{};
            center_data.smoothing_velocity = float3{};
            smooth_delta_vector = float3{};
        } else {
            center_data.frame_component_shift_vector =
                Subtract(component_world_position, old_component_position);
            center_data.frame_component_shift_rotation =
                FromToRotation(old_component_rotation, component_world_rotation);
            float move_shift_ratio = 0.0f;
            float rotation_shift_ratio = 0.0f;

            float movement_shift = 1.0f - parameters.inertia_constraint.world_inertia;
            float rotation_shift = 1.0f - parameters.inertia_constraint.world_inertia;
            const bool keep = team_data.IsKeepReset() || team_data.IsCullingInvisible();
            movement_shift = keep ? 1.0f : movement_shift;
            rotation_shift = keep ? 1.0f : rotation_shift;

            if (movement_shift > define::system::Epsilon
                || rotation_shift > define::system::Epsilon) {
                team_data.flag.Set(FlagInertiaShift, true);
                move_shift_ratio = movement_shift;
                rotation_shift_ratio = rotation_shift;
                work_old_component_position =
                    Lerp(work_old_component_position, component_world_position, movement_shift);
                work_old_component_rotation =
                    Slerp(work_old_component_rotation, component_world_rotation, rotation_shift);
            }

            const float movement_speed_limit =
                parameters.inertia_constraint.movement_speed_limit * component_scale_ratio;
            const float rotation_speed_limit =
                parameters.inertia_constraint.rotation_speed_limit;
            const float3 delta_vector =
                Subtract(component_world_position, work_old_component_position);
            const float delta_angle =
                Angle(work_old_component_rotation, component_world_rotation);
            const float frame_speed =
                team_data.frame_delta_time > 0.0f
                    ? Length(delta_vector) / team_data.frame_delta_time
                    : 0.0f;
            const float frame_rotation_speed =
                team_data.frame_delta_time > 0.0f
                    ? delta_angle * radians_to_degrees / team_data.frame_delta_time
                    : 0.0f;
            if (frame_speed > movement_speed_limit && movement_speed_limit >= 0.0f) {
                team_data.flag.Set(FlagInertiaShift, true);
                const float move_limit_ratio =
                    Clamp01(std::max(frame_speed - movement_speed_limit, 0.0f) / frame_speed);
                move_shift_ratio = move_shift_ratio + (1.0f - move_shift_ratio) * move_limit_ratio;
                work_old_component_position =
                    Lerp(work_old_component_position, component_world_position, move_limit_ratio);
            }
            if (frame_rotation_speed > rotation_speed_limit && rotation_speed_limit >= 0.0f) {
                team_data.flag.Set(FlagInertiaShift, true);
                const float rotation_limit_ratio = Clamp01(
                    std::max(frame_rotation_speed - rotation_speed_limit, 0.0f)
                        / frame_rotation_speed
                );
                rotation_shift_ratio =
                    rotation_shift_ratio + (1.0f - rotation_shift_ratio) * rotation_limit_ratio;
                work_old_component_rotation =
                    Slerp(work_old_component_rotation, component_world_rotation, rotation_limit_ratio);
            }

            float other_shift_ratio = 0.0f;
            if (team_data.skip_count > 0
                && team_data.frame_delta_time > define::system::Epsilon
                && team_data.now_time_scale > define::system::Epsilon) {
                other_shift_ratio = other_shift_ratio
                    + (1.0f - other_shift_ratio)
                        * Clamp01(
                            static_cast<float>(team_data.skip_count) * simulation_delta_time
                            / (team_data.frame_delta_time * team_data.now_time_scale)
                        );
            }
            if (team_data.velocity_weight < 1.0f) {
                other_shift_ratio =
                    other_shift_ratio + (1.0f - other_shift_ratio) * (1.0f - team_data.velocity_weight);
            }
            if (team_data.now_time_scale < 1.0f) {
                other_shift_ratio =
                    other_shift_ratio + (1.0f - other_shift_ratio) * (1.0f - team_data.now_time_scale);
            }
            if (other_shift_ratio > 0.0f) {
                team_data.flag.Set(FlagInertiaShift, true);
                move_shift_ratio = move_shift_ratio + (1.0f - move_shift_ratio) * other_shift_ratio;
                rotation_shift_ratio =
                    rotation_shift_ratio + (1.0f - rotation_shift_ratio) * other_shift_ratio;
                work_old_component_position =
                    Lerp(work_old_component_position, component_world_position, other_shift_ratio);
                work_old_component_rotation =
                    Slerp(work_old_component_rotation, component_world_rotation, other_shift_ratio);
            }

            if (team_data.IsInertiaShift()) {
                center_data.frame_component_shift_vector =
                    Scale(center_data.frame_component_shift_vector, move_shift_ratio);
                center_data.frame_component_shift_rotation =
                    Slerp(quaternion{}, center_data.frame_component_shift_rotation, rotation_shift_ratio);
                center_data.frame_component_shift_vector =
                    Add(center_data.frame_component_shift_vector, anchor_delta_vector);
                center_data.frame_component_shift_rotation =
                    Multiply(anchor_delta_rotation, center_data.frame_component_shift_rotation);
                center_data.frame_component_shift_vector =
                    Add(center_data.frame_component_shift_vector, smooth_delta_vector);

                center_data.old_frame_world_position = ShiftPosition(
                    center_data.old_frame_world_position,
                    center_data.old_component_world_position,
                    center_data.frame_component_shift_vector,
                    center_data.frame_component_shift_rotation
                );
                center_data.old_frame_world_rotation =
                    Multiply(center_data.frame_component_shift_rotation, center_data.old_frame_world_rotation);
                center_data.now_world_position = ShiftPosition(
                    center_data.now_world_position,
                    center_data.old_component_world_position,
                    center_data.frame_component_shift_vector,
                    center_data.frame_component_shift_rotation
                );
                center_data.now_world_rotation =
                    Multiply(center_data.frame_component_shift_rotation, center_data.now_world_rotation);
            }
        }

        const float3 moving_vector =
            Subtract(component_world_position, work_old_component_position);
        const float moving_length = Length(moving_vector);
        center_data.frame_moving_speed =
            team_data.frame_delta_time > 0.0f
                ? moving_length / team_data.frame_delta_time
                : 0.0f;
        center_data.frame_moving_speed *=
            team_data.now_time_scale > 1.0e-6f ? 1.0f / team_data.now_time_scale : 0.0f;
        center_data.frame_moving_direction =
            moving_length > 1.0e-6f ? Scale(moving_vector, 1.0f / moving_length) : float3{};
        center_data.frame_local_position = InverseTransformPoint(
            center_world_position,
            center_world_position,
            center_world_rotation,
            component_world_scale
        );

        if (team_data.flag.IsSet(FlagReset) || team_data.flag.IsSet(FlagTimeReset)) {
            team_data.velocity_weight =
                parameters.stabilization_time_after_reset > 1.0e-6f ? 0.0f : 1.0f;
            team_data.blend_weight = team_data.velocity_weight;
        }

        UpdateTeamWind(team_id, parameters, center_world_position, wind_manager);

        (void)old_component_scale;
    }
}

void TeamManager::UpdateTeamWind(
    int team_id,
    const ClothParameters& parameters,
    const float3& center_world_position,
    const WindManager& wind_manager
)
{
    if (!IsValidTeam(team_id)) {
        return;
    }

    const TeamWindData old_wind_data = team_wind_array_[team_id];
    TeamWindData new_wind_data;

    if (parameters.wind.IsValid()) {
        float min_volume = std::numeric_limits<float>::max();
        int addition_count = 0;
        int latest_wind_id = -1;
        const auto& wind_data_array = wind_manager.WindDataArray();

        for (int wind_id = 0; wind_id < wind_data_array.Length(); ++wind_id) {
            const WindManager::WindData& wind_data = wind_data_array[wind_id];
            if (!wind_data.IsValid() || !wind_data.IsEnabled()) {
                continue;
            }

            const bool is_addition = wind_data.IsAddition();
            if (is_addition && addition_count >= 3) {
                continue;
            }

            const float3 local_position =
                TransformPoint(center_world_position, wind_data.world_to_local_matrix);
            const float local_length = Length(local_position);

            switch (wind_data.mode) {
            case WindZoneMode::BoxDirection: {
                const float3 local_size{
                    std::abs(local_position.x) * 2.0f,
                    std::abs(local_position.y) * 2.0f,
                    std::abs(local_position.z) * 2.0f,
                };
                if (local_size.x > wind_data.size.x
                    || local_size.y > wind_data.size.y
                    || local_size.z > wind_data.size.z) {
                    continue;
                }
                break;
            }
            case WindZoneMode::SphereDirection:
            case WindZoneMode::SphereRadial:
                if (local_length > wind_data.size.x) {
                    continue;
                }
                break;
            case WindZoneMode::GlobalDirection:
                break;
            }

            if (!is_addition && wind_data.zone_volume > min_volume) {
                continue;
            }

            float3 main_direction = wind_data.world_wind_direction;
            if (wind_data.mode == WindZoneMode::SphereRadial) {
                if (local_length <= 1.0e-6f) {
                    continue;
                }
                main_direction =
                    Normalize(Subtract(center_world_position, wind_data.world_position));
            }

            float wind_main = wind_data.main;
            if (wind_data.mode == WindZoneMode::SphereRadial) {
                if (local_length <= 1.0e-6f || wind_data.size.x <= 1.0e-6f) {
                    continue;
                }
                const float depth = Clamp01(local_length / wind_data.size.x);
                const float attenuation = MC2EvaluateCurveClamp01(wind_data.attenuation, depth);
                wind_main *= attenuation;
            }

            TeamWindInfo wind_info;
            wind_info.wind_id = wind_id;
            wind_info.time = -define::system::WindMaxTime;
            wind_info.main = wind_main;
            wind_info.direction = main_direction;

            if (is_addition) {
                if (new_wind_data.AddOrReplaceWindZone(wind_info, old_wind_data)) {
                    ++addition_count;
                }
            } else {
                new_wind_data.RemoveWindZone(latest_wind_id);
                new_wind_data.AddOrReplaceWindZone(wind_info, old_wind_data);
                min_volume = wind_data.zone_volume;
                latest_wind_id = wind_id;
            }
        }
    }

    new_wind_data.moving_wind = old_wind_data.moving_wind;
    team_wind_array_[team_id] = new_wind_data;
}

void TeamManager::PostTeamUpdate()
{
    if (!initialized_) {
        return;
    }

    constexpr float limit_time = 3600.0f;
    for (int team_id = 1; team_id < team_data_array_.Count(); ++team_id) {
        TeamData& team_data = team_data_array_[team_id];
        if (!team_data.IsProcess()) {
            continue;
        }

        InertiaCenterData& center_data = center_data_array_[team_id];
        center_data.old_component_world_position = center_data.component_world_position;
        center_data.old_component_world_rotation = center_data.component_world_rotation;
        center_data.old_component_world_scale = center_data.component_world_scale;

        if (team_data.IsRunning()) {
            center_data.old_frame_world_position = center_data.frame_world_position;
            center_data.old_frame_world_rotation = center_data.frame_world_rotation;
            center_data.old_frame_world_scale = center_data.frame_world_scale;
            team_data.force_mode = ClothForceMode::None;
            team_data.impact_force = float3{};
            team_data.skip_count = 0;
        }

        center_data.old_anchor_position = center_data.anchor_position;
        center_data.old_anchor_rotation = center_data.anchor_rotation;
        center_data.anchor_component_local_position = InverseTransformPoint(
            center_data.component_world_position,
            center_data.anchor_position,
            center_data.anchor_rotation,
            float3{1.0f, 1.0f, 1.0f}
        );

        team_data.flag.Set(FlagReset, false);
        team_data.flag.Set(FlagTimeReset, false);
        team_data.flag.Set(FlagRunning, false);
        team_data.flag.Set(FlagStepRunning, false);
        team_data.flag.Set(FlagKeepTeleport, false);
        team_data.flag.Set(FlagInertiaShift, false);
        team_data.flag.Set(FlagNegativeScaleTeleport, false);

        if (team_data.time > limit_time * 2.0f) {
            team_data.time -= limit_time;
            team_data.old_time -= limit_time;
            team_data.now_update_time -= limit_time;
            team_data.old_update_time -= limit_time;
            team_data.frame_update_time -= limit_time;
            team_data.frame_old_time -= limit_time;
        }
    }
}

}  // namespace hocloth::mc2
