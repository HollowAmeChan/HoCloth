#pragma once

#include "hocloth/core/interface/i_data_validate.hpp"
#include "hocloth/core/interface/i_valid.hpp"

namespace hocloth::mc2 {

// Native data port for Scripts/Core/Cloth/NormalAlignmentSettings.cs.
// adjustment_transform is represented as a backend transform id instead of a Unity Transform reference.
struct NormalAlignmentSettings final : public IValid, public IDataValidate {
    enum class AlignmentMode {
        None = 0,
        BoundingBoxCenter = 1,
        Transform = 2,
    };

    AlignmentMode alignment_mode = AlignmentMode::None;
    int adjustment_transform_id = 0;

    void DataValidate() override
    {
    }

    [[nodiscard]] bool IsValid() const override
    {
        switch (alignment_mode) {
        case AlignmentMode::None:
        case AlignmentMode::BoundingBoxCenter:
        case AlignmentMode::Transform:
            return true;
        }
        return false;
    }

    [[nodiscard]] NormalAlignmentSettings Clone() const
    {
        return *this;
    }

    [[nodiscard]] int GetHashCode() const
    {
        int hash = static_cast<int>(alignment_mode) * 105;
        if (adjustment_transform_id != 0) {
            hash += adjustment_transform_id;
        }
        return hash;
    }
};

}  // namespace hocloth::mc2
