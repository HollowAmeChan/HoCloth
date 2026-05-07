#pragma once

#include "hocloth/core/interface/i_valid.hpp"
#include "hocloth/core/define/system_define.hpp"
#include "hocloth/utility/grid/grid_map.hpp"
#include "hocloth/utility/jobs/job_utility.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/virtual_mesh/vertex_attribute.hpp"

#include <algorithm>
#include <limits>
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

    [[nodiscard]] static SelectionData CreateBoneClothDefault(
        const std::vector<float3>& source_positions,
        const std::vector<int>& root_indices
    )
    {
        SelectionData selection(static_cast<int>(source_positions.size()));
        selection.positions = source_positions;
        selection.Fill(VertexAttribute::Move());
        for (int root_index : root_indices) {
            if (root_index < 0 || root_index >= selection.Count()) {
                continue;
            }
            selection.attributes[static_cast<std::size_t>(root_index)] = VertexAttribute::Fixed();
        }
        selection.user_edit = false;
        return selection;
    }

    [[nodiscard]] static GridMap<int> CreateGridMapRun(
        float grid_size,
        const std::vector<float3>& source_positions,
        const std::vector<VertexAttribute>& source_attributes,
        bool move = true,
        bool fix = true,
        bool ignore = true,
        bool invalid = true
    )
    {
        (void)ignore;
        GridMap<int> grid_map(static_cast<int>(source_positions.size()));
        GridMap<int>::Map& map = grid_map.GetMultiHashMap();
        const int count = std::min(
            static_cast<int>(source_positions.size()),
            static_cast<int>(source_attributes.size())
        );
        for (int index = 0; index < count; ++index) {
            const VertexAttribute attribute = source_attributes[static_cast<std::size_t>(index)];
            if (!move && attribute.IsMove()) {
                continue;
            }
            if (!fix && attribute.IsFixed()) {
                continue;
            }
            if (!invalid && attribute.IsInvalid()) {
                continue;
            }
            GridMap<int>::AddGrid(
                source_positions[static_cast<std::size_t>(index)],
                index,
                map,
                grid_size
            );
        }
        return grid_map;
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

    void ConvertFrom(const SelectionData& from)
    {
        if (from.Count() == 0 || Count() == 0) {
            return;
        }

        const AABB aabb = job_utility::CalcAABB(positions, Count());
        float search_radius = MaxSideLength(aabb) * 0.2f;
        search_radius = std::max(search_radius, define::system::MinimumGridSize);
        const float grid_size = search_radius * 0.5f;
        GridMap<int> grid_map = CreateGridMapRun(grid_size, from.positions, from.attributes);
        const GridMap<int>::Map& map = grid_map.GetMultiHashMap();

        const int count = Count();
        if (attributes.size() < positions.size()) {
            attributes.resize(positions.size());
        }
        for (int index = 0; index < count; ++index) {
            const float3& position = positions[static_cast<std::size_t>(index)];
            VertexAttribute attribute = VertexAttribute::Invalid();
            float min_distance = std::numeric_limits<float>::max();

            for (const int3& grid : GridMap<int>::GetArea(position, search_radius, map, grid_size)) {
                const auto found = map.find(grid);
                if (found == map.end()) {
                    continue;
                }
                for (int source_index : found->second) {
                    if (source_index < 0 || source_index >= from.Count()) {
                        continue;
                    }
                    const float distance = Distance(
                        position,
                        from.positions[static_cast<std::size_t>(source_index)]
                    );
                    if (distance > search_radius || distance > min_distance) {
                        continue;
                    }
                    min_distance = distance;
                    attribute = from.attributes[static_cast<std::size_t>(source_index)];
                }
            }

            attributes[static_cast<std::size_t>(index)] = attribute;
        }
    }
};

}  // namespace hocloth::mc2
