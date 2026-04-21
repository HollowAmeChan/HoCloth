#pragma once

#include "hocloth_runtime_api.hpp"

namespace hocloth {

struct PhysxBuildResult {
    bool available = false;
    std::string backend = "stub";
    std::string message;
};

PhysxBuildResult probe_physx_backend();

}  // namespace hocloth
