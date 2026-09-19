import bpy

from ..runtime.inspector import get_inspector_status, is_inspector_running


def _draw_binding_section(layout, scene):
    box = layout.box()
    box.label(text="Bindings", icon="OUTLINER_COLLECTION")

    quick_add = box.row(align=True)
    quick_add.operator("hocloth.add_active_bone_cloth", icon="MOD_CLOTH", text="BoneCloth")
    quick_add.operator("hocloth.add_active_spring_bone", icon="BONE_DATA", text="BoneSpring")

    extra_add = box.row(align=True)
    extra_add.operator("hocloth.add_active_collider", icon="MESH_UVSPHERE", text="Collider")
    extra_add.operator("hocloth.add_cache_output", icon="EXPORT", text="Cache")

    box.label(text=f"Registered Components: {len(scene.hocloth_mc2_components)}")
    box.label(text="Parameters and curves now edit in Inspector")


def _draw_inspector_section(layout, scene):
    box = layout.box()
    box.label(text="Inspector", icon="WINDOW")

    inspector_running = is_inspector_running()
    box.operator(
        "hocloth.toggle_inspector",
        icon="CANCEL" if inspector_running else "WINDOW",
        text="Close Inspector" if inspector_running else "Open Inspector",
    )
    box.label(text=f"Status: {get_inspector_status()}")
    if scene.hocloth_runtime_live_running:
        box.label(text="Live run will rebuild from Blender first", icon="INFO")


def _draw_runtime_section(layout, scene):
    box = layout.box()
    box.label(text="Runtime", icon="MODIFIER")

    run = box.row(align=True)
    run.operator("hocloth.rebuild_scene", icon="FILE_REFRESH", text="Build")
    run.operator("hocloth.step_runtime", icon="FRAME_NEXT", text="Step")
    run.operator(
        "hocloth.toggle_live_runtime",
        icon="PAUSE" if scene.hocloth_runtime_live_running else "PLAY",
        text="Pause Live" if scene.hocloth_runtime_live_running else "Start Live",
    )

    manage = box.row(align=True)
    manage.operator("hocloth.restart_runtime_from_baseline", icon="ARMATURE_DATA", text="Baseline")
    manage.operator("hocloth.reset_runtime", icon="LOOP_BACK", text="Reset")
    manage.operator("hocloth.destroy_runtime", icon="TRASH", text="Destroy")

    bake = box.row(align=True)
    bake.operator("hocloth.bake_runtime_action", icon="REC", text="Bake")
    bake.operator("hocloth.clear_baked_action", icon="X", text="Clear")


def _draw_status_section(layout, scene):
    box = layout.box()
    box.label(text="Status", icon="INFO")
    box.label(text=scene.hocloth_runtime_status or "Idle")
    box.label(
        text=(
            f"steps={scene.hocloth_runtime_step_count}, "
            f"transforms={scene.hocloth_runtime_transform_count}, "
            f"applied={getattr(scene, 'hocloth_runtime_applied_count', 0)}"
        )
    )


def _draw_overlay_section(layout, scene):
    box = layout.box()
    box.label(text="Viewport", icon="HIDE_OFF")
    box.prop(scene, "hocloth_viewport_overlay_enabled", text="Show Build Result")
    if scene.hocloth_viewport_overlay_enabled:
        col = box.column(align=True)
        col.prop(scene, "hocloth_viewport_draw_bones", text="Bones")
        col.prop(scene, "hocloth_viewport_draw_particle_radius", text="Particle Radius")
        col.prop(scene, "hocloth_viewport_draw_colliders", text="Colliders")
        col.prop(scene, "hocloth_viewport_overlay_alpha", text="Alpha")


class HOCLOTH_PT_main_panel(bpy.types.Panel):
    bl_label = "HoCloth"
    bl_idname = "HOCLOTH_PT_main_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "HoCloth"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        _draw_binding_section(layout, scene)
        _draw_inspector_section(layout, scene)
        _draw_runtime_section(layout, scene)
        _draw_status_section(layout, scene)
        _draw_overlay_section(layout, scene)


CLASSES = (HOCLOTH_PT_main_panel,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
