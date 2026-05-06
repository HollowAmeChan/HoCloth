#pragma once

#include "hocloth/utility/math/math_types.hpp"

#include <cassert>
#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/NativeCollection/ExCostSortedList4.cs
struct ExCostSortedList4 {
    float4 costs{-1.0f, -1.0f, -1.0f, -1.0f};
    int4 data{};

    explicit ExCostSortedList4(float invalid_cost)
        : costs{invalid_cost, invalid_cost, invalid_cost, invalid_cost}
    {
    }

    [[nodiscard]] int Count() const
    {
        for (int index = 3; index >= 0; --index) {
            if (costs[index] >= 0.0f) {
                return index + 1;
            }
        }
        return 0;
    }

    [[nodiscard]] bool IsValid() const
    {
        return costs[0] >= 0.0f;
    }

    bool Add(float cost, int item)
    {
        assert(cost >= 0.0f);
        if (costs[3] >= 0.0f && cost > costs[3]) {
            return false;
        }

        for (int index = 0; index < 4; ++index) {
            const float current_cost = costs[index];
            if (current_cost < 0.0f) {
                costs[index] = cost;
                data[index] = item;
                return true;
            }
            if (cost >= current_cost) {
                continue;
            }

            for (int move_index = 2; move_index >= index; --move_index) {
                costs[move_index + 1] = costs[move_index];
                data[move_index + 1] = data[move_index];
            }
            costs[index] = cost;
            data[index] = item;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool Contains(int item) const
    {
        return IndexOf(item) >= 0;
    }

    [[nodiscard]] int IndexOf(int item) const
    {
        for (int index = 0; index < 4; ++index) {
            if (costs[index] >= 0.0f && data[index] == item) {
                return index;
            }
        }
        return -1;
    }

    void RemoveItem(int item)
    {
        const int item_index = IndexOf(item);
        if (item_index < 0) {
            return;
        }

        for (int index = item_index; index < 3; ++index) {
            costs[index] = costs[index + 1];
            data[index] = data[index + 1];
        }
        costs[3] = -1.0f;
        data[3] = 0;
    }

    [[nodiscard]] float MinCost() const
    {
        return costs[0];
    }

    [[nodiscard]] float MaxCost() const
    {
        for (int index = 3; index >= 0; --index) {
            if (costs[index] >= 0.0f) {
                return costs[index];
            }
        }
        return 0.0f;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        for (int index = 0; index < Count(); ++index) {
            stream << "(" << costs[index] << " : " << data[index] << ") ";
        }
        return stream.str();
    }
};

}  // namespace hocloth::mc2
