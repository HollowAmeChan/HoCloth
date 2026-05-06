#pragma once

#include "hocloth/manager/manager_status.hpp"

#include <string>
#include <vector>

namespace hocloth::mc2 {

struct BackendStatus {
    std::string name;
    std::string phase;
    std::vector<ManagerStatus> managers;

    [[nodiscard]] std::string Summary() const;
};

[[nodiscard]] BackendStatus CreateBootstrapBackendStatus();

}  // namespace hocloth::mc2
