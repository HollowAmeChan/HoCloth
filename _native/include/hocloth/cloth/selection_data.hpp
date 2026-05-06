#pragma once

#include "hocloth/core/interface/i_valid.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/virtual_mesh/vertex_attribute.hpp"

#include <algorithm>
#include <vector>

namespace hocloth::mc2 {

// Native data-layer port for Scripts/Core/Cloth/SelectionData.cs.
// GridMap conversion/search jobs remain deferred until the GridMap utility is ported.
class SelectionData final : public IValid {
public:
    std::vector<float3> positions;
    std::vector<VertexAttribute> attributes;
    float max_connection_distance = 0.0f;
    bool user_edit = false;

    SelectionData() = default;
    explicit SelectionData(int count)
        : positions(static_cast<std::size_t>(count))
        , attributes(static_cast<std::size_t>(count))
    {
    }

    [[nodiscard]] int Count() const
    {
        return static_cast<int>(positions.size());
    }

    [[nodiscard]] bool IsValid() const override
    {
        return !positions.empty() && !attributes.empty() && positions.size() == attributes.size();
    }

    [[nodiscard]] bool IsUserEdit() const
    {
        return user_edit;
    }

    [[nodiscard]] SelectionData Clone() const
    {
        return *this;
    }

    [[nodiscard]] bool Compare(const SelectionData& other) const
    {
        if (positions.size() != other.positions.size() || attributes.size() != other.attributes.size()
            || user_edit != other.user_edit) {
            return false;
        }

        for (std::size_t index = 0; index < attributes.size(); ++index) {
            if (attributes[index] != other.attributes[index]) {
                return false;
            }
            const float3& a = positions[index];
            const float3& b = other.positions[index];
            if (a.x != b.x || a.y != b.y || a.z != b.z) {
                return false;
            }
        }
        return true;
    }

    void AddRange(
        const std::vector<float3>& add_positions,
        const std::vector<VertexAttribute>* add_attributes = nullptr
    )
    {
        const std::size_t old_count = positions.size();
        positions.insert(positions.end(), add_positions.begin(), add_positions.end());

        attributes.resize(positions.size());
        if (add_attributes != nullptr) {
            const std::size_t copy_count = std::min(add_attributes->size(), add_positions.size());
            std::copy_n(add_attributes->begin(), copy_count, attributes.begin() + old_count);
        }
    }

    void Fill(VertexAttribute attribute)
    {
        std::fill(attributes.begin(), attributes.end(), attribute);
    }

    void Merge(const SelectionData& from)
    {
        if (from.Count() == 0) {
            return;
        }
        positions.insert(positions.end(), from.positions.begin(), from.positions.end());
        attributes.insert(attributes.end(), from.attributes.begin(), from.attributes.end());
        max_connection_distance = std::max(max_connection_distance, from.max_connection_distance);
        user_edit = user_edit || from.user_edit;
    }
};

}  // namespace hocloth::mc2
