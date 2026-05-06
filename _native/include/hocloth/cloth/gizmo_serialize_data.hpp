#pragma once

#include "hocloth/settings/cloth_debug_settings.hpp"
#include "hocloth/settings/virtual_mesh_debug_settings.hpp"

namespace hocloth::mc2 {

// Data port for Scripts/Core/Cloth/GizmoSerializeData.cs.
struct GizmoSerializeData {
    bool always = false;
    ClothDebugSettings cloth_debug_settings;
    VirtualMeshDebugSettings proxy_debug_settings;
    VirtualMeshDebugSettings mapping_debug_settings;
    int debug_mapping_index = 0;

    GizmoSerializeData()
    {
        cloth_debug_settings.enable = true;
        cloth_debug_settings.shape = true;
    }

    [[nodiscard]] bool IsAlways() const
    {
        return always;
    }
};

}  // namespace hocloth::mc2
