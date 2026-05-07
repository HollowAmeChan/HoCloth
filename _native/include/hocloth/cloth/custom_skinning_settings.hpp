#pragma once

#include "hocloth/core/interface/i_data_validate.hpp"
#include "hocloth/core/interface/i_valid.hpp"

#include <algorithm>
#include <vector>

namespace hocloth::mc2 {

// Native data port for Scripts/Core/Cloth/CustomSkinningSettings.cs.
// Unity Transform references are represented by backend transform ids.
struct CustomSkinningSettings final : public IValid, public IDataValidate {
    bool enable = false;
    std::vector<int> skinning_bone_ids;

    void DataValidate() override
    {
    }

    [[nodiscard]] bool IsValid() const override
    {
        return enable
            && std::any_of(
                skinning_bone_ids.begin(),
                skinning_bone_ids.end(),
                [](int transform_id) { return transform_id != 0; }
            );
    }

    [[nodiscard]] CustomSkinningSettings Clone() const
    {
        return *this;
    }

    [[nodiscard]] int GetHashCode() const
    {
        return 0;
    }
};

}  // namespace hocloth::mc2
