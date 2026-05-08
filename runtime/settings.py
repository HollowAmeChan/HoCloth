import bpy


SCENE_PROPERTIES = (
    "hocloth_runtime_status",
    "hocloth_runtime_backend",
    "hocloth_runtime_handle",
    "hocloth_runtime_step_count",
    "hocloth_runtime_transform_count",
    "hocloth_runtime_non_identity_transform_count",
    "hocloth_runtime_max_rotation_degrees",
    "hocloth_runtime_max_translation",
    "hocloth_runtime_write_mode_summary",
    "hocloth_runtime_applied_count",
    "hocloth_runtime_missing_bone_count",
    "hocloth_runtime_missing_armature_count",
    "hocloth_runtime_apply_armature_count",
    "hocloth_runtime_last_fixed_steps",
    "hocloth_runtime_mesh_output_count",
    "hocloth_runtime_mesh_applied_count",
    "hocloth_runtime_mesh_vertex_count",
    "hocloth_runtime_mesh_missing_object_count",
    "hocloth_runtime_mesh_topology_mismatch_count",
    "hocloth_runtime_dt",
    "hocloth_simulation_frequency",
    "hocloth_apply_pose_on_step",
    "hocloth_runtime_live_running",
    "hocloth_ui_details_expanded",
    "hocloth_debug_detailed_native",
    "hocloth_viewport_overlay_enabled",
    "hocloth_viewport_draw_bones",
    "hocloth_viewport_draw_particle_radius",
    "hocloth_viewport_draw_colliders",
    "hocloth_viewport_overlay_alpha",
)


def register():
    bpy.types.Scene.hocloth_runtime_status = bpy.props.StringProperty(
        name="Runtime Status",
        default="Not built",
    )
    bpy.types.Scene.hocloth_runtime_backend = bpy.props.StringProperty(
        name="Runtime Backend",
        default="none",
    )
    bpy.types.Scene.hocloth_runtime_handle = bpy.props.IntProperty(
        name="Runtime Handle",
        default=0,
    )
    bpy.types.Scene.hocloth_runtime_step_count = bpy.props.IntProperty(
        name="Runtime Steps",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_transform_count = bpy.props.IntProperty(
        name="Runtime Transforms",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_non_identity_transform_count = bpy.props.IntProperty(
        name="Non Identity Transforms",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_max_rotation_degrees = bpy.props.FloatProperty(
        name="Max Rotation Delta",
        default=0.0,
        min=0.0,
        precision=3,
    )
    bpy.types.Scene.hocloth_runtime_max_translation = bpy.props.FloatProperty(
        name="Max Translation Delta",
        default=0.0,
        min=0.0,
        precision=5,
    )
    bpy.types.Scene.hocloth_runtime_write_mode_summary = bpy.props.StringProperty(
        name="Write Mode Summary",
        default="",
    )
    bpy.types.Scene.hocloth_runtime_applied_count = bpy.props.IntProperty(
        name="Applied Bones",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_missing_bone_count = bpy.props.IntProperty(
        name="Missing Bones",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_missing_armature_count = bpy.props.IntProperty(
        name="Missing Armatures",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_apply_armature_count = bpy.props.IntProperty(
        name="Applied Armatures",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_last_fixed_steps = bpy.props.IntProperty(
        name="Last Fixed Steps",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_mesh_output_count = bpy.props.IntProperty(
        name="Mesh Outputs",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_mesh_applied_count = bpy.props.IntProperty(
        name="Applied Meshes",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_mesh_vertex_count = bpy.props.IntProperty(
        name="Applied Mesh Vertices",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_mesh_missing_object_count = bpy.props.IntProperty(
        name="Missing Mesh Objects",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_mesh_topology_mismatch_count = bpy.props.IntProperty(
        name="Mesh Topology Mismatches",
        default=0,
        min=0,
    )
    bpy.types.Scene.hocloth_runtime_dt = bpy.props.FloatProperty(
        name="Step Delta Time",
        default=1.0 / 30.0,
        min=0.0,
        soft_max=1.0 / 10.0,
        precision=5,
    )
    bpy.types.Scene.hocloth_simulation_frequency = bpy.props.IntProperty(
        name="Simulation Frequency",
        default=90,
        min=1,
        soft_max=240,
    )
    bpy.types.Scene.hocloth_apply_pose_on_step = bpy.props.BoolProperty(
        name="Apply Pose On Step",
        default=True,
    )
    bpy.types.Scene.hocloth_runtime_live_running = bpy.props.BoolProperty(
        name="Live Runtime",
        default=False,
    )
    bpy.types.Scene.hocloth_ui_details_expanded = bpy.props.BoolProperty(
        name="Show Details",
        default=False,
    )
    bpy.types.Scene.hocloth_debug_detailed_native = bpy.props.BoolProperty(
        name="Detailed Native Debug",
        default=False,
    )
    bpy.types.Scene.hocloth_viewport_overlay_enabled = bpy.props.BoolProperty(
        name="Viewport Overlay",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_bones = bpy.props.BoolProperty(
        name="Draw Bones",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_particle_radius = bpy.props.BoolProperty(
        name="Draw Radius",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_colliders = bpy.props.BoolProperty(
        name="Draw Colliders",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_overlay_alpha = bpy.props.FloatProperty(
        name="Overlay Alpha",
        default=0.75,
        min=0.0,
        max=1.0,
    )


def unregister():
    for property_name in reversed(SCENE_PROPERTIES):
        if hasattr(bpy.types.Scene, property_name):
            delattr(bpy.types.Scene, property_name)
