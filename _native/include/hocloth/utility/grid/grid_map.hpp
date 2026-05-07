#pragma once

#include "hocloth/utility/math/math_types.hpp"

#include <cmath>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hocloth::mc2 {

struct Int3Hash {
    [[nodiscard]] std::size_t operator()(const int3& value) const
    {
        std::size_t seed = 0;
        auto combine = [&seed](int component) {
            seed ^= std::hash<int>{}(component) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        };
        combine(value.x);
        combine(value.y);
        combine(value.z);
        return seed;
    }
};

struct Int3Equal {
    [[nodiscard]] bool operator()(const int3& a, const int3& b) const
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};

// Lightweight native port of Scripts/Core/Utility/Grid/GridMap.cs.
template <typename T>
class GridMap {
public:
    using Map = std::unordered_map<int3, std::vector<T>, Int3Hash, Int3Equal>;

    explicit GridMap(int capacity = 0)
    {
        grid_map_.reserve(static_cast<std::size_t>(capacity));
    }

    void Dispose()
    {
        grid_map_.clear();
    }

    [[nodiscard]] const Map& GetMultiHashMap() const
    {
        return grid_map_;
    }

    [[nodiscard]] Map& GetMultiHashMap()
    {
        return grid_map_;
    }

    [[nodiscard]] const Map& GetMap() const
    {
        return grid_map_;
    }

    [[nodiscard]] Map& GetMap()
    {
        return grid_map_;
    }

    [[nodiscard]] int DataCount() const
    {
        int count = 0;
        for (const auto& [grid, values] : grid_map_) {
            (void)grid;
            count += static_cast<int>(values.size());
        }
        return count;
    }

    static std::vector<int3> GetArea(int3 start_grid, int3 end_grid)
    {
        if (end_grid.x < start_grid.x) {
            std::swap(start_grid.x, end_grid.x);
        }
        if (end_grid.y < start_grid.y) {
            std::swap(start_grid.y, end_grid.y);
        }
        if (end_grid.z < start_grid.z) {
            std::swap(start_grid.z, end_grid.z);
        }

        std::vector<int3> grids;
        for (int z = start_grid.z; z <= end_grid.z; ++z) {
            for (int y = start_grid.y; y <= end_grid.y; ++y) {
                for (int x = start_grid.x; x <= end_grid.x; ++x) {
                    grids.push_back(int3{x, y, z});
                }
            }
        }
        return grids;
    }

    static std::vector<int3> GetArea(int3 start_grid, int3 end_grid, const Map& grid_map)
    {
        (void)grid_map;
        return GetArea(start_grid, end_grid);
    }

    static std::vector<int3> GetArea(const float3& position, float radius, float grid_size)
    {
        const int3 min_grid = GetGrid(
            float3{position.x - radius, position.y - radius, position.z - radius},
            grid_size
        );
        const int3 max_grid = GetGrid(
            float3{position.x + radius, position.y + radius, position.z + radius},
            grid_size
        );
        return GetArea(min_grid, max_grid);
    }

    static std::vector<int3> GetArea(
        const float3& position,
        float radius,
        const Map& grid_map,
        float grid_size
    )
    {
        (void)grid_map;
        return GetArea(position, radius, grid_size);
    }

    static int3 GetGrid(const float3& position, float grid_size)
    {
        return int3{
            static_cast<int>(std::floor(position.x / grid_size)),
            static_cast<int>(std::floor(position.y / grid_size)),
            static_cast<int>(std::floor(position.z / grid_size)),
        };
    }

    static void AddGrid(int3 grid, const T& data, Map& grid_map)
    {
        grid_map[grid].push_back(data);
    }

    static int3 AddGrid(const float3& position, const T& data, Map& grid_map, float grid_size)
    {
        const int3 grid = GetGrid(position, grid_size);
        AddGrid(grid, data, grid_map);
        return grid;
    }

    static bool RemoveGrid(int3 grid, const T& data, Map& grid_map)
    {
        const auto found = grid_map.find(grid);
        if (found == grid_map.end()) {
            return false;
        }

        auto& values = found->second;
        for (auto iter = values.begin(); iter != values.end(); ++iter) {
            if (*iter == data) {
                values.erase(iter);
                if (values.empty()) {
                    grid_map.erase(found);
                }
                return true;
            }
        }
        return false;
    }

    static bool MoveGrid(int3 from_grid, int3 to_grid, const T& data, Map& grid_map)
    {
        if (Int3Equal{}(from_grid, to_grid)) {
            return false;
        }
        RemoveGrid(from_grid, data, grid_map);
        AddGrid(to_grid, data, grid_map);
        return true;
    }

private:
    Map grid_map_;
};

}  // namespace hocloth::mc2
