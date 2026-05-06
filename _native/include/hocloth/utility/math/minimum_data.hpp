#pragma once

#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Math/MinimumData.cs
template <typename DistanceT, typename DataT>
class MinimumData {
public:
    void Add(const DistanceT& distance, const DataT& data)
    {
        if (!is_valid_ || distance < min_distance_) {
            min_distance_ = distance;
            min_data_ = data;
            is_valid_ = true;
        }
    }

    void Clear()
    {
        is_valid_ = false;
    }

    [[nodiscard]] bool IsValid() const
    {
        return is_valid_;
    }

    [[nodiscard]] const DistanceT& MinDistance() const
    {
        return min_distance_;
    }

    [[nodiscard]] const DataT& MinData() const
    {
        return min_data_;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "MinimumData. IsValid:" << (is_valid_ ? "true" : "false")
               << ", minDist:" << min_distance_
               << ", minData:" << min_data_;
        return stream.str();
    }

private:
    DistanceT min_distance_{};
    DataT min_data_{};
    bool is_valid_ = false;
};

}  // namespace hocloth::mc2
