#pragma once

#include <string>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/NativeCollection/ExProcessingList.cs
template <typename T>
class ExProcessingList {
public:
    ExProcessingList() = default;

    [[nodiscard]] bool IsValid() const
    {
        return valid_;
    }

    void Dispose()
    {
        buffer_.clear();
        counter_ = 0;
        valid_ = false;
    }

    void UpdateBuffer(int capacity)
    {
        if (capacity > static_cast<int>(buffer_.size())) {
            buffer_.resize(static_cast<std::size_t>(capacity));
        }
        valid_ = true;
    }

    void Clear()
    {
        counter_ = 0;
    }

    [[nodiscard]] int* GetJobSchedulePtr()
    {
        return &counter_;
    }

    [[nodiscard]] const int* GetJobSchedulePtr() const
    {
        return &counter_;
    }

    int Add(const T& value)
    {
        if (counter_ >= static_cast<int>(buffer_.size())) {
            UpdateBuffer(buffer_.empty() ? 16 : static_cast<int>(buffer_.size()) * 2);
        }
        buffer_[static_cast<std::size_t>(counter_)] = value;
        return counter_++;
    }

    [[nodiscard]] int Count() const
    {
        return counter_;
    }

    [[nodiscard]] int& Counter()
    {
        return counter_;
    }

    [[nodiscard]] const int& Counter() const
    {
        return counter_;
    }

    [[nodiscard]] int Capacity() const
    {
        return static_cast<int>(buffer_.size());
    }

    [[nodiscard]] const std::vector<T>& Buffer() const
    {
        return buffer_;
    }

    [[nodiscard]] std::vector<T>& Buffer()
    {
        return buffer_;
    }

    [[nodiscard]] std::string ToString() const;

private:
    std::vector<T> buffer_;
    int counter_ = 0;
    bool valid_ = true;
};

extern template class ExProcessingList<int>;
extern template class ExProcessingList<unsigned int>;

}  // namespace hocloth::mc2
