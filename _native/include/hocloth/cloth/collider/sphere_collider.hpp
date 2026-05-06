#pragma once

#include "hocloth/cloth/collider/collider_component.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Collider/MagicaSphereCollider.cs
class MagicaSphereCollider final : public ColliderComponent {
public:
    [[nodiscard]] ColliderManager::ColliderType GetColliderType() const override
    {
        return ColliderManager::ColliderType::Sphere;
    }

    void DataValidate() override
    {
        size_.x = ClampColliderSize(size_.x);
    }

    void SetSize(float radius)
    {
        ColliderComponent::SetSize(float3{radius, 0.0f, 0.0f});
    }

    [[nodiscard]] float Radius()
    {
        DataValidate();
        return size_.x;
    }
};

}  // namespace hocloth::mc2
