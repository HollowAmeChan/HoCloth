#pragma once

#include <array>
#include <cstddef>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2:
// Scripts/Core/Utility/NativeCollection/FixedList*BytesExtensions.cs
template <typename T, int Capacity>
class FixedList {
    static_assert(Capacity > 0, "FixedList capacity must be positive.");

public:
    static constexpr int CapacityValue = Capacity;

    [[nodiscard]] int Length() const
    {
        return length_;
    }

    [[nodiscard]] int CapacityCount() const
    {
        return Capacity;
    }

    [[nodiscard]] bool IsCapacity() const
    {
        return length_ >= Capacity;
    }

    [[nodiscard]] bool IsFull() const
    {
        return IsCapacity();
    }

    [[nodiscard]] bool IsEmpty() const
    {
        return length_ <= 0;
    }

    [[nodiscard]] bool Contains(const T& item) const
    {
        for (int index = 0; index < length_; ++index) {
            if (values_[static_cast<std::size_t>(index)] == item) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] const T& operator[](int index) const
    {
        return values_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] T& operator[](int index)
    {
        return values_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] const T* Data() const
    {
        return values_.data();
    }

    [[nodiscard]] T* Data()
    {
        return values_.data();
    }

    [[nodiscard]] const T* begin() const
    {
        return values_.data();
    }

    [[nodiscard]] const T* end() const
    {
        return values_.data() + length_;
    }

    bool Add(const T& item)
    {
        if (IsCapacity()) {
            return false;
        }
        values_[static_cast<std::size_t>(length_)] = item;
        ++length_;
        return true;
    }

    bool Set(const T& item)
    {
        if (Contains(item)) {
            return true;
        }
        return Add(item);
    }

    bool SetLimit(const T& item)
    {
        if (IsCapacity()) {
            return false;
        }
        if (Contains(item)) {
            return true;
        }
        return Add(item);
    }

    bool RemoveAt(int index)
    {
        if (index < 0 || index >= length_) {
            return false;
        }
        for (int shift_index = index; shift_index < length_ - 1; ++shift_index) {
            values_[static_cast<std::size_t>(shift_index)] =
                values_[static_cast<std::size_t>(shift_index + 1)];
        }
        --length_;
        values_[static_cast<std::size_t>(length_)] = T{};
        return true;
    }

    bool RemoveAtSwapBack(int index)
    {
        if (index < 0 || index >= length_) {
            return false;
        }
        --length_;
        values_[static_cast<std::size_t>(index)] = values_[static_cast<std::size_t>(length_)];
        values_[static_cast<std::size_t>(length_)] = T{};
        return true;
    }

    bool RemoveItemAtSwapBack(const T& item)
    {
        for (int index = 0; index < length_; ++index) {
            if (values_[static_cast<std::size_t>(index)] == item) {
                return RemoveAtSwapBack(index);
            }
        }
        return false;
    }

    bool Push(const T& item)
    {
        return Add(item);
    }

    T Pop()
    {
        if (IsEmpty()) {
            return T{};
        }
        const int index = length_ - 1;
        T item = values_[static_cast<std::size_t>(index)];
        RemoveAt(index);
        return item;
    }

    bool Enqueue(const T& item)
    {
        return Add(item);
    }

    T Dequeue()
    {
        if (IsEmpty()) {
            return T{};
        }
        T item = values_[0];
        RemoveAt(0);
        return item;
    }

    T Dequque()
    {
        return Dequeue();
    }

    bool MC2Set(const T& item)
    {
        return Set(item);
    }

    bool MC2SetLimit(const T& item)
    {
        return SetLimit(item);
    }

    bool MC2RemoveItemAtSwapBack(const T& item)
    {
        return RemoveItemAtSwapBack(item);
    }

    bool MC2Push(const T& item)
    {
        return Push(item);
    }

    T MC2Pop()
    {
        return Pop();
    }

    bool MC2Enqueue(const T& item)
    {
        return Enqueue(item);
    }

    T MC2Dequque()
    {
        return Dequque();
    }

    void Clear()
    {
        values_.fill(T{});
        length_ = 0;
    }

private:
    std::array<T, static_cast<std::size_t>(Capacity)> values_{};
    int length_ = 0;
};

}  // namespace hocloth::mc2
