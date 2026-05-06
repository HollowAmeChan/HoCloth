#pragma once

namespace hocloth::mc2 {

// Data port for Scripts/Core/Settings/ClothDebugSettings.cs.
struct ClothDebugSettings {
    enum class DebugAxis {
        None = 0,
        Normal = 1,
        All = 2,
    };

    bool enable = false;
    bool ztest = false;
    bool position = true;
    DebugAxis axis = DebugAxis::None;
    bool shape = false;
    bool base_line = false;
    bool depth = false;
    bool collider = true;
    bool animated_position = false;
    DebugAxis animated_axis = DebugAxis::None;
    bool animated_shape = false;
    bool inertia_center = true;
    bool custom_skinning_bone = true;

    float point_size = 0.01f;
    bool refer_old_pos = false;
    bool radius = true;
    bool local_number = false;
    bool particle_number = false;
    bool triangle_number = false;
    bool friction = false;
    bool static_friction = false;
    bool attribute = false;
    bool collision_normal = false;
    bool cell_cube = false;
    bool base_line_pos = false;
    int vertex_min_index = 0;
    int vertex_max_index = 100000;
    int triangle_min_index = 0;
    int triangle_max_index = 100000;

    [[nodiscard]] bool CheckParticleDrawing(int index) const
    {
        return index >= vertex_min_index && index <= vertex_max_index;
    }

    [[nodiscard]] bool CheckTriangleDrawing(int index) const
    {
        return index >= triangle_min_index && index <= triangle_max_index;
    }

    [[nodiscard]] bool CheckRadiusDrawing() const
    {
        return radius;
    }

    [[nodiscard]] float GetPointSize() const
    {
        return point_size;
    }

    [[nodiscard]] float GetLineSize() const
    {
        return 0.05f;
    }

    [[nodiscard]] float GetInertiaCenterRadius() const
    {
        return 0.01f;
    }

    [[nodiscard]] float GetCustomSkinningRadius() const
    {
        return 0.02f;
    }

    [[nodiscard]] bool IsReferOldPos() const
    {
        return refer_old_pos;
    }
};

}  // namespace hocloth::mc2
