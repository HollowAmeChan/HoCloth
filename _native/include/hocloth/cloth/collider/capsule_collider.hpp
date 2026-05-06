#pragma once

#include "hocloth/cloth/collider/collider_component.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Collider/MagicaCapsuleCollider.cs
class MagicaCapsuleCollider final : public ColliderComponent {
public:
    enum class Direction {
        X = 0,
        Y = 1,
        Z = 2,
    };

    Direction direction = Direction::X;
    bool reverse_direction = false;
    bool radius_separation = false;
    bool aligned_on_center = true;

    [[nodiscard]] ColliderManager::ColliderType GetColliderType() const override
    {
        switch (direction) {
        case Direction::X:
            return aligned_on_center
                ? ColliderManager::ColliderType::CapsuleXCenter
                : ColliderManager::ColliderType::CapsuleXStart;
        case Direction::Y:
            return aligned_on_center
                ? ColliderManager::ColliderType::CapsuleYCenter
                : ColliderManager::ColliderType::CapsuleYStart;
        case Direction::Z:
        default:
            return aligned_on_center
                ? ColliderManager::ColliderType::CapsuleZCenter
                : ColliderManager::ColliderType::CapsuleZStart;
        }
    }

    void SetSize(float start_radius, float end_radius, float length)
    {
        ColliderComponent::SetSize(float3{start_radius, end_radius, length});
        radius_separation = start_radius != end_radius;
    }

    [[nodiscard]] float3 GetSize() const override
    {
        if (radius_separation) {
            return size_;
        }
        return float3{size_.x, size_.x, size_.z};
    }

    [[nodiscard]] float3 GetLocalDir() const
    {
        const float rev = reverse_direction ? -1.0f : 1.0f;
        switch (direction) {
        case Direction::X:
            return float3{rev, 0.0f, 0.0f};
        case Direction::Y:
            return float3{0.0f, rev, 0.0f};
        case Direction::Z:
        default:
            return float3{0.0f, 0.0f, rev};
        }
    }

    [[nodiscard]] float3 GetLocalUp() const
    {
        switch (direction) {
        case Direction::X:
            return float3{0.0f, 1.0f, 0.0f};
        case Direction::Y:
            return float3{0.0f, 0.0f, 1.0f};
        case Direction::Z:
        default:
            return float3{0.0f, 1.0f, 0.0f};
        }
    }

    [[nodiscard]] bool IsReverseDirection() const override
    {
        return reverse_direction;
    }

    void DataValidate() override
    {
        size_.x = ClampColliderSize(size_.x);
        size_.y = ClampColliderSize(size_.y);
        size_.z = ClampColliderSize(size_.z);
    }
};

}  // namespace hocloth::mc2
