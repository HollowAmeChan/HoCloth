#include "hocloth/manager/team/team_manager.hpp"

#include <stdexcept>

namespace hocloth::mc2 {

namespace {

TeamManager::TeamData MakeEmptyTeamData()
{
    TeamManager::TeamData data;
    data.flag.Clear();
    return data;
}

}  // namespace

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
    return flag.IsSet(TeamManager::FlagCameraCullingInvisible)
        || flag.IsSet(TeamManager::FlagDistanceCullingInvisible);
}

bool TeamManager::TeamData::IsProcess() const
{
    return IsEnabled() && !flag.IsSet(TeamManager::FlagSyncSuspend) && !IsCullingInvisible();
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

Result TeamManager::Initialize()
{
    initialized_ = true;
    team_data_array_.Dispose();
    parameter_array_.Dispose();
    team_data_array_ = ExSimpleNativeArray<TeamData>(1, false);
    parameter_array_ = ExSimpleNativeArray<ClothParameters>(1, false);
    team_data_array_[0] = MakeEmptyTeamData();
    parameter_array_[0] = ClothParameters{};
    free_team_ids_.clear();
    return Result::Ok();
}

void TeamManager::Dispose()
{
    team_data_array_.Dispose();
    parameter_array_.Dispose();
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
    return ManagerStatus{"TeamManager", initialized_, static_cast<std::uint32_t>(valid_count)};
}

int TeamManager::TeamCount() const
{
    return team_data_array_.Count();
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
        parameter_array_.Add(ClothParameters{});
    }

    TeamData data = MakeEmptyTeamData();
    data.flag.Set(FlagValid, true);
    data.flag.Set(FlagEnable, enabled);
    data.flag.Set(FlagSpring, spring);
    data.time_scale = 1.0f;
    data.now_time_scale = 1.0f;
    data.velocity_weight = 1.0f;
    data.distance_weight = 1.0f;
    data.blend_weight = 1.0f;
    data.scale_ratio = 1.0f;
    data.negative_scale_sign = 1.0f;
    team_data_array_[team_id] = data;
    parameter_array_[team_id] = parameters;
    return team_id;
}

void TeamManager::ReleaseTeam(int team_id)
{
    if (!IsValidTeam(team_id)) {
        return;
    }
    team_data_array_[team_id] = MakeEmptyTeamData();
    parameter_array_[team_id] = ClothParameters{};
    free_team_ids_.push_back(team_id);
}

void TeamManager::ClearTeams()
{
    if (team_data_array_.Count() > 0) {
        team_data_array_.SetCount(1);
        parameter_array_.SetCount(1);
        team_data_array_[0] = MakeEmptyTeamData();
        parameter_array_[0] = ClothParameters{};
    }
    free_team_ids_.clear();
}

}  // namespace hocloth::mc2
