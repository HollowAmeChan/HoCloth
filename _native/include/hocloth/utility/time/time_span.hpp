#pragma once

#include <chrono>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Time/TimeSpan.cs
class TimeSpan {
public:
    TimeSpan();
    explicit TimeSpan(std::string name);

    void Start();
    void Finish();
    [[nodiscard]] double TotalSeconds();
    [[nodiscard]] double TotalMilliSeconds();
    [[nodiscard]] std::string ToString();

private:
    std::string name_;
    std::chrono::steady_clock::time_point start_time_{};
    std::chrono::steady_clock::time_point end_time_{};
};

}  // namespace hocloth::mc2
