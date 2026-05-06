#pragma once

#include "hocloth/cloth/collider/collider_component.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Collider/MagicaPlaneCollider.cs
class MagicaPlaneCollider final : public ColliderComponent {
public:
    [[nodiscard]] ColliderManager::ColliderType GetColliderType() const override
    {
        return ColliderManager::ColliderType::Plane;
    }

    void DataValidate() override
    {
    }
};

}  // namespace hocloth::mc2
