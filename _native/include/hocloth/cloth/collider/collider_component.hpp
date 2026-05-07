#pragma once

#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

class TeamManager;
class TransformManager;

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Collider/ColliderComponent.cs
class ColliderComponent {
public:
    float3 center{};
    bool enabled = true;

    virtual ~ColliderComponent() = default;

    [[nodiscard]] virtual ColliderManager::ColliderType GetColliderType() const = 0;
    virtual void DataValidate() = 0;

    [[nodiscard]] virtual float3 GetSize() const
    {
        return size_;
    }

    void SetSize(const float3& size)
    {
        size_ = size;
    }

    [[nodiscard]] virtual float GetScale() const
    {
        return 1.0f;
    }

    [[nodiscard]] virtual bool IsReverseDirection() const
    {
        return false;
    }

    void Register(int team_id, int local_collider_index = -1)
    {
        team_slots_[team_id] = local_collider_index;
    }

    void Exit(int team_id)
    {
        team_slots_.erase(team_id);
    }

    void ClearTeams()
    {
        team_slots_.clear();
    }

    [[nodiscard]] bool IsRegistered(int team_id) const
    {
        return team_slots_.contains(team_id);
    }

    [[nodiscard]] int GetLocalColliderIndex(int team_id) const
    {
        const auto iterator = team_slots_.find(team_id);
        return iterator != team_slots_.end() ? iterator->second : -1;
    }

    [[nodiscard]] std::vector<int> RegisteredTeamIds() const
    {
        std::vector<int> ids;
        ids.reserve(team_slots_.size());
        for (const auto& entry : team_slots_) {
            ids.push_back(entry.first);
        }
        return ids;
    }

    void UpdateParameters(
        ColliderManager& collider_manager,
        TeamManager& team_manager,
        TransformManager& transform_manager
    )
    {
        DataValidate();
        for (const auto& entry : team_slots_) {
            if (entry.second < 0) {
                continue;
            }
            collider_manager.UpdateColliderData(
                entry.first,
                entry.second,
                team_manager,
                transform_manager,
                ToColliderData()
            );
        }
    }

    void SetEnabled(
        bool enabled_value,
        ColliderManager& collider_manager,
        TeamManager& team_manager,
        TransformManager& transform_manager
    )
    {
        enabled = enabled_value;
        for (const auto& entry : team_slots_) {
            if (entry.second < 0) {
                continue;
            }
            collider_manager.EnableCollider(
                entry.first,
                entry.second,
                team_manager,
                transform_manager,
                enabled
            );
        }
    }

    void Destroy(ColliderManager& collider_manager, TeamManager& team_manager, TransformManager& transform_manager)
    {
        const auto slots = team_slots_;
        for (const auto& entry : slots) {
            if (entry.second < 0) {
                continue;
            }
            collider_manager.RemoveColliderData(
                entry.first,
                entry.second,
                team_manager,
                transform_manager
            );
        }
        ClearTeams();
    }

    [[nodiscard]] ColliderManager::ColliderData ToColliderData(
        const float3& frame_position = float3{},
        const quaternion& frame_rotation = quaternion{},
        const float3& frame_scale = float3{1.0f, 1.0f, 1.0f}
    )
    {
        DataValidate();
        return ColliderManager::ColliderData{
            GetColliderType(),
            enabled,
            IsReverseDirection(),
            center,
            GetSize(),
            frame_position,
            frame_rotation,
            frame_scale,
        };
    }

    void ApplyTo(ColliderManager& collider_manager, int collider_index)
    {
        collider_manager.SetCollider(collider_index, ToColliderData());
    }

protected:
    float3 size_{};

private:
    std::unordered_map<int, int> team_slots_;
};

inline float ClampColliderSize(float value)
{
    return std::max(value, 0.001f);
}

}  // namespace hocloth::mc2
