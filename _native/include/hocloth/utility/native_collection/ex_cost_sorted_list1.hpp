#pragma once

#include <cassert>
#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/NativeCollection/ExCostSortedList1.cs
struct ExCostSortedList1 {
    float cost = -1.0f;
    int data = 0;

    explicit ExCostSortedList1(float invalid_cost)
        : cost(invalid_cost)
    {
    }

    ExCostSortedList1(float invalid_cost, int init_data)
        : cost(invalid_cost)
        , data(init_data)
    {
    }

    [[nodiscard]] bool IsValid() const
    {
        return cost >= 0.0f;
    }

    [[nodiscard]] int Count() const
    {
        return IsValid() ? 1 : 0;
    }

    [[nodiscard]] float Cost() const
    {
        return cost;
    }

    [[nodiscard]] int Data() const
    {
        return data;
    }

    void Add(float new_cost, int item)
    {
        assert(new_cost >= 0.0f);
        if (!IsValid() || new_cost < cost) {
            cost = new_cost;
            data = item;
        }
    }

    [[nodiscard]] int CompareTo(const ExCostSortedList1& other) const
    {
        if (cost == other.cost) {
            return 0;
        }
        return cost < other.cost ? -1 : 1;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "(" << cost << " : " << data << ")";
        return stream.str();
    }
};

}  // namespace hocloth::mc2
