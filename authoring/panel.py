import bpy

from ..runtime.bone_sampling import resolve_bone_chain_names
from ..components import mc2
from ..runtime.session import get_exchange_info


def _draw_mc2_cloth(layout, scene, item):
    cloth = mc2.find_magica_cloth(scene, item.component_id)
    if cloth is None:
        return

    armature_object = cloth.armature_object
    bone_names = resolve_bone_chain_names(scene, armature_object, cloth.root_bone_name)

    header = layout.row(align=True)
    header.prop(item, "ui_expanded", text="", emboss=False, icon="TRIA_DOWN" if item.ui_expanded else "TRIA_RIGHT")
    title = header.column(align=True)
    title.label(text=item.display_name, icon="MOD_CLOTH")
    title.label(text=f"MagicaCloth / {cloth.authoring_mode} / {len(bone_names)} joints", icon="ARMATURE_DATA")
    actions = header.row(align=True)
    actions.prop(item, "enabled", text="")
    remove = actions.operator("hocloth.remove_component", text="", icon="X")
    remove.component_id = item.component_id
    if not item.ui_expanded:
        return

    body = layout.box()
    body.prop(cloth, "authoring_mode", text="模式")
    body.prop(cloth, "armature_object", text="骨架")
    if armature_object is not None and armature_object.data is not None:
        body.prop_search(cloth, "root_bone_name", armature_object.data, "bones", text="Root Bones")
    else:
        body.prop(cloth, "root_bone_name", text="Root Bones")

    preset_row = body.row(align=True)
    preset_row.prop(cloth, "preset_profile", text="MC2 预设")
    preset = preset_row.operator("hocloth.apply_spring_bone_preset", text="应用")
    preset.component_id = item.component_id
    body.prop(cloth, "joint_radius", text="Radius")
    body.prop(cloth.damping_curve, "value", text="Damping")
    body.prop(cloth, "gravity_strength", text="Gravity")
    body.prop(cloth.distance_constraint.stiffness, "value", text="Distance Stiffness")
    body.prop(cloth.tether_constraint, "distance_compression", text="Tether Compression")
    body.prop(cloth.spring_constraint, "spring_power", text="Spring Power")
    body.prop(cloth, "collider_ids", text="Collider List")

    row = body.row(align=True)
    selected = row.operator("hocloth.assign_selected_colliders_to_spring_bone", icon="RESTRICT_SELECT_OFF", text="选中碰撞体")
    selected.component_id = item.component_id
    all_colliders = row.operator("hocloth.assign_all_groups_to_spring_bone", icon="LINKED", text="全部碰撞体")
    all_colliders.component_id = item.component_id

    if scene.hocloth_ui_details_expanded:
        detail = body.box()
        detail.label(text="ClothSerializeData")
        detail.prop(cloth, "center_source", text="Center")
        if cloth.center_source == "OBJECT":
            detail.prop(cloth, "center_object", text="Center Object")
        elif cloth.center_source == "BONE":
            detail.prop(cloth, "center_armature_object", text="Center Armature")
            if cloth.center_armature_object is not None and cloth.center_armature_object.data is not None:
                detail.prop_search(cloth, "center_bone_name", cloth.center_armature_object.data, "bones", text="Center Bone")
            else:
                detail.prop(cloth, "center_bone_name", text="Center Bone")
        detail.prop(cloth, "gravity_direction", text="Gravity Direction")
        detail.prop(cloth.inertia_constraint, "world_inertia", text="World Inertia")
        detail.prop(cloth.inertia_constraint, "movement_inertia_smoothing", text="Movement Smoothing")
        detail.prop(cloth.angle_restoration_constraint, "use_angle_restoration", text="Use Angle Restoration")
        detail.prop(cloth.angle_restoration_constraint.stiffness, "value", text="Angle Stiffness")
        detail.prop(cloth.collider_collision_constraint, "friction", text="Collider Friction")
        detail.prop(cloth.collider_collision_constraint.limit_distance, "value", text="Collider Limit Distance")


def _draw_mc2_collider(layout, item):
    scene = bpy.context.scene
    collider = mc2.find_collider(scene, item.component_id)
    if collider is None:
        return

    header = layout.row(align=True)
    header.prop(item, "ui_expanded", text="", emboss=False, icon="TRIA_DOWN" if item.ui_expanded else "TRIA_RIGHT")
    title = header.column(align=True)
    title.label(text=item.display_name, icon="MESH_UVSPHERE")
    title.label(text=f"Magica{collider.collider_type.title()}Collider", icon="OBJECT_DATA")
    actions = header.row(align=True)
    actions.prop(item, "enabled", text="")
    remove = actions.operator("hocloth.remove_component", text="", icon="X")
    remove.component_id = item.component_id
    if not item.ui_expanded:
        return

    body = layout.box()
    body.prop(collider, "collider_object", text="Object")
    body.prop(collider, "collider_type", text="Type")
    body.prop(collider, "center", text="Center")
    body.prop(collider, "radius", text="Radius")
    if collider.collider_type == "CAPSULE":
        body.prop(collider, "radius_separation", text="Radius Separation")
        if collider.radius_separation:
            body.prop(collider, "end_radius", text="End Radius")
        body.prop(collider, "length", text="Length")
        body.prop(collider, "direction", text="Direction")
        body.prop(collider, "reverse_direction", text="Reverse Direction")
        body.prop(collider, "aligned_on_center", text="Aligned On Center")


def _draw_mc2_cache(layout, item):
    scene = bpy.context.scene
    cache = mc2.find_cache_output(scene, item.component_id)
    if cache is None:
        return

    header = layout.row(align=True)
    header.prop(item, "ui_expanded", text="", emboss=False, icon="TRIA_DOWN" if item.ui_expanded else "TRIA_RIGHT")
    title = header.column(align=True)
    title.label(text=item.display_name, icon="EXPORT")
    title.label(text=cache.cache_format.upper(), icon="FILE")
    actions = header.row(align=True)
    actions.prop(item, "enabled", text="")
    remove = actions.operator("hocloth.remove_component", text="", icon="X")
    remove.component_id = item.component_id
    if not item.ui_expanded:
        return

    body = layout.box()
    body.prop(cache, "source_object", text="Source Object")
    body.prop(cache, "cache_format", text="Format")
    body.prop(cache, "cache_path", text="Path")


class HOCLOTH_PT_main_panel(bpy.types.Panel):
    bl_label = "HoCloth"
    bl_idname = "HOCLOTH_PT_main_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "HoCloth"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        quick_add = layout.row(align=True)
        quick_add.operator("hocloth.add_active_bone_cloth", icon="MOD_CLOTH", text="BoneCloth")
        quick_add.operator("hocloth.add_active_spring_bone", icon="BONE_DATA", text="BoneSpring")
        quick_add.operator("hocloth.add_active_collider", icon="MESH_UVSPHERE", text="Collider")

        run = layout.row(align=True)
        run.operator("hocloth.rebuild_scene", icon="FILE_REFRESH", text="构建")
        run.operator("hocloth.step_runtime", icon="FRAME_NEXT", text="Step")
        run.operator(
            "hocloth.toggle_live_runtime",
            icon="PAUSE" if scene.hocloth_runtime_live_running else "PLAY",
            text="暂停" if scene.hocloth_runtime_live_running else "实时",
        )
        run.operator("hocloth.restart_runtime_from_baseline", icon="ARMATURE_DATA", text="第一帧")

        layout.prop(scene, "hocloth_ui_details_expanded", text="详细信息")

        components = layout.box()
        components.label(text="MC2 Components")
        if not scene.hocloth_mc2_components:
            components.label(text="还没有 MC2 组件", icon="INFO")
        for item in scene.hocloth_mc2_components:
            if item.component_type == "MAGICA_CLOTH":
                _draw_mc2_cloth(components, scene, item)
            elif item.component_type in {"SPHERE_COLLIDER", "CAPSULE_COLLIDER", "PLANE_COLLIDER"}:
                _draw_mc2_collider(components, item)
            elif item.component_type == "CACHE_OUTPUT":
                _draw_mc2_cache(components, item)

        runtime = layout.box()
        runtime.label(text="Runtime")
        runtime.prop(scene, "hocloth_runtime_dt", text="dt")
        runtime.prop(scene, "hocloth_simulation_frequency", text="Frequency")
        runtime.prop(scene, "hocloth_apply_pose_on_step", text="Step 写回姿态")
        runtime.label(text=f"状态: {scene.hocloth_runtime_status}")
        runtime.label(
            text=(
                f"steps={scene.hocloth_runtime_step_count}, "
                f"transforms={scene.hocloth_runtime_transform_count}, "
                f"applied={getattr(scene, 'hocloth_runtime_applied_count', 0)}"
            )
        )

        debug = layout.box()
        debug.prop(scene, "hocloth_ui_debug_expanded", text="调试")
        if scene.hocloth_ui_debug_expanded:
            row = debug.row(align=True)
            row.operator("hocloth.export_mc2_snapshot", icon="EXPORT", text="导出 MC2 快照")
            row.operator("hocloth.export_frame_inputs", icon="FILE_TEXT", text="导出帧输入")
            row.operator("hocloth.reset_runtime", icon="LOOP_BACK", text="重置")
            row.operator("hocloth.destroy_runtime", icon="TRASH", text="销毁")
            exchange_info = get_exchange_info()
            debug.label(text=f"Exchange: {exchange_info['schema']} v{exchange_info['schema_version']}")
            if scene.hocloth_ui_details_expanded:
                debug.label(text=f"Debug Dump: {exchange_info['debug_dump_path']}", icon="FILE_TEXT")

        overlay = layout.box()
        overlay.label(text="Viewport")
        overlay.prop(scene, "hocloth_viewport_overlay_enabled", text="显示构建结果")
        col = overlay.column(align=True)
        col.enabled = scene.hocloth_viewport_overlay_enabled
        col.prop(scene, "hocloth_viewport_draw_bones", text="Bones")
        col.prop(scene, "hocloth_viewport_draw_particle_radius", text="Radius")
        col.prop(scene, "hocloth_viewport_draw_colliders", text="Colliders")
        col.prop(scene, "hocloth_viewport_overlay_alpha", text="Alpha")


CLASSES = (HOCLOTH_PT_main_panel,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
