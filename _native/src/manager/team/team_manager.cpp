#include "hocloth/manager/team/team_manager.hpp"

#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cmath>
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
    team_data_array_ = ExSimpleNativeArray<TeamData>(1, false);
    team_mapping_index_array_ = ExSimpleNativeArray<TeamMappingList>(1, false);
    mapping_data_array_ = ExNativeArray<MappingData>(32);
    parameter_array_ = ExSimpleNativeArray<ClothParameters>(1, false);
    center_data_array_ = ExSimpleNativeArray<InertiaCenterData>(1, false);
    team_data_array_[0] = MakeEmptyTeamData();
    team_mapping_index_array_[0] = TeamMappingList{};
    parameter_array_[0] = ClothParameters{};
    center_data_array_[0] = InertiaCenterData{};
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
            + " mapping=" + std::to_string(MappingCount()),
    };
}

int TeamManager::TeamCount() const
{
    return team_data_array_.Count();
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
    }

    TeamData data = MakeEmptyTeamData();
    data.flag.Set(FlagValid, true);
    data.flag.Set(FlagEnable, enabled);
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
    return team_id;
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
    free_team_ids_.push_back(team_id);
}

void TeamManager::ClearTeams()
{
    if (team_data_array_.Count() > 0) {
        team_data_array_.SetCount(1);
        parameter_array_.SetCount(1);
        center_data_array_.SetCount(1);
        team_mapping_index_array_.SetCount(1);
        mapping_data_array_.Clear();
        team_data_array_[0] = MakeEmptyTeamData();
        team_mapping_index_array_[0] = TeamMappingList{};
        parameter_array_[0] = ClothParameters{};
        center_data_array_[0] = InertiaCenterData{};
    }
    free_team_ids_.clear();
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

}  // namespace hocloth::mc2
