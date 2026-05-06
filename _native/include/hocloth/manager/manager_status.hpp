#pragma once

#include <cstdint>
#include <string>

namespace hocloth::mc2 {

struct ManagerStatus {
    std::string name;
    bool initialized = false;
    std::uint32_t item_count = 0;
    std::string detail;
};

}  // namespace hocloth::mc2
