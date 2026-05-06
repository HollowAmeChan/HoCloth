#pragma once

#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace hocloth::mc2 {

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

    void Register(int team_id)
    {
        team_id_set_.insert(team_id);
    }

    void Exit(int team_id)
    {
        team_id_set_.erase(team_id);
    }

    void ClearTeams()
    {
        team_id_set_.clear();
    }

    [[nodiscard]] bool IsRegistered(int team_id) const
    {
        return team_id_set_.contains(team_id);
    }

    [[nodiscard]] std::vector<int> RegisteredTeamIds() const
    {
        return std::vector<int>(team_id_set_.begin(), team_id_set_.end());
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
    std::unordered_set<int> team_id_set_;
};

inline float ClampColliderSize(float value)
{
    return std::max(value, 0.001f);
}

}  // namespace hocloth::mc2
