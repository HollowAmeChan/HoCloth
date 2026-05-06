#include "hocloth/cloth/constraints/inertia_constraint.hpp"

namespace hocloth::mc2 {

void InertiaConstraint::Dispose()
{
    fixed_array_.Dispose();
}

int InertiaConstraint::FixedCount() const
{
    return fixed_array_.Count();
}

void InertiaConstraint::Register(int team_id, const ConstraintData& data, TeamManager& team_manager)
{
    if (!team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    InertiaCenterData center_data = data.center_data;
    center_data.center_transform_index = team_data.center_transform_index;
    center_data.init_local_gravity_direction = data.init_local_gravity_direction;
    team_manager.SetCenterData(team_id, center_data);

    if (!data.fixed_array.empty()) {
        team_data.fixed_data_chunk = fixed_array_.AddRange(data.fixed_array);
    }
}

void InertiaConstraint::Exit(int team_id, TeamManager& team_manager)
{
    if (!team_manager.IsValidTeam(team_id)) {
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
    fixed_array_.Remove(team_data.fixed_data_chunk);
    team_data.fixed_data_chunk.Clear();
    team_manager.SetCenterData(team_id, InertiaCenterData{});
}

const ExNativeArray<std::uint16_t>& InertiaConstraint::FixedArray() const
{
    return fixed_array_;
}

}  // namespace hocloth::mc2
