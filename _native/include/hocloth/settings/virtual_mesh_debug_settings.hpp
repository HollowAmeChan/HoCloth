#pragma once

namespace hocloth::mc2 {

// Data port for Scripts/Core/Settings/VirtualMeshDebugSettings.cs.
struct VirtualMeshDebugSettings {
    enum class DebugAxis {
        None = 0,
        Normal = 1,
        All = 2,
    };

    bool enable = false;
    float point_size = 0.01f;
    float line_size = 0.03f;
    bool position = true;
    bool axis = false;
    bool index_number = false;
    bool bone_weight = false;
    bool uv = false;
    bool depth = false;
    bool root_index = false;
    bool parent_index = false;
    bool line = true;
    bool edge_number = false;
    bool triangle = true;
    bool triangle_normal = false;
    bool triangle_tangent = false;
    bool triangle_number = false;
    bool base_line = false;
    bool bone_name = false;
    int vertex_min_index = 0;
    int vertex_max_index = 100000;
    int edge_min_index = 0;
    int edge_max_index = 100000;
    int triangle_min_index = 0;
    int triangle_max_index = 100000;
};

}  // namespace hocloth::mc2
