#pragma once

#include "hocloth_runtime_api.hpp"

namespace hocloth {

struct PhysxBuildResult {
    bool available = false;
    std::string backend = "stub";
    std::string message;
    bool scene_created = false;
    std::uint64_t collider_count = 0;
};

PhysxBuildResult probe_physx_backend();
PhysxBuildResult build_physx_scene(SceneHandle handle, const SceneDescriptor& scene);
void destroy_physx_scene(SceneHandle handle);

}  // namespace hocloth
