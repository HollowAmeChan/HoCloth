import bpy

from ..components import mc2
from ..runtime.blender_bone_refs import resolve_bone_chain_names


def _draw_collider_reference_list(layout, scene, item, cloth):
    collider_box = layout.box()
    collider = cloth.collider_collision_constraint
    header = collider_box.row(align=True)
    header.prop(collider, "enabled", text="碰撞")
    add = header.operator("hocloth.add_collider_reference", text="", icon="ADD")
    add.component_id = item.component_id

    if not collider.enabled:
        return

    if not cloth.collider_references:
        collider_box.label(text="未指定碰撞体", icon="INFO")
    for index, reference in enumerate(cloth.collider_references):
        row = collider_box.row(align=True)
        row.prop(reference, "collider_object", text="")
        remove = row.operator("hocloth.remove_collider_reference", text="", icon="X")
        remove.component_id = item.component_id
        remove.index = index

    resolved_count = len(mc2.resolve_cloth_collider_ids(scene, cloth))
    collider_box.label(text=f"Collider List: {resolved_count}")


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
    title.label(text=f"MagicaCloth / {cloth.authoring_mode} / {len(bone_names)} bones", icon="ARMATURE_DATA")
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
        body.prop_search(cloth, "root_bone_name", armature_object.data, "bones", text="根骨骼")
    else:
        body.prop(cloth, "root_bone_name", text="根骨骼")

    preset_row = body.row(align=True)
    preset_row.prop(cloth, "preset_profile", text="MC2 预设")
    preset = preset_row.operator("hocloth.apply_spring_bone_preset", text="应用")
    preset.component_id = item.component_id

    body.prop(cloth, "joint_radius", text="粒子半径")
    body.prop(cloth.damping_curve, "value", text="阻尼")
    body.prop(cloth, "gravity_strength", text="重力")
    body.prop(cloth.distance_constraint.stiffness, "value", text="距离刚度")
    body.prop(cloth.tether_constraint, "distance_compression", text="压缩限制")
    body.prop(cloth.spring_constraint, "spring_power", text="弹簧强度")
    _draw_collider_reference_list(body, scene, item, cloth)

    if scene.hocloth_ui_details_expanded:
        detail = body.box()
        detail.label(text="MagicaCloth 详细参数")
        detail.prop(cloth, "center_source", text="中心")
        if cloth.center_source == "OBJECT":
            detail.prop(cloth, "center_object", text="中心物体")
        elif cloth.center_source == "BONE":
            detail.prop(cloth, "center_armature_object", text="中心骨架")
            if cloth.center_armature_object is not None and cloth.center_armature_object.data is not None:
                detail.prop_search(cloth, "center_bone_name", cloth.center_armature_object.data, "bones", text="中心骨骼")
            else:
                detail.prop(cloth, "center_bone_name", text="中心骨骼")
        detail.prop(cloth, "gravity_direction", text="重力方向")
        detail.prop(cloth.inertia_constraint, "world_inertia", text="世界惯性")
        detail.prop(cloth.inertia_constraint, "movement_inertia_smoothing", text="移动惯性平滑")
        detail.prop(cloth.angle_restoration_constraint, "use_angle_restoration", text="角度复原")
        detail.prop(cloth.angle_restoration_constraint.stiffness, "value", text="角度刚度")
        detail.prop(cloth.collider_collision_constraint, "mode", text="碰撞模式")
        detail.prop(cloth.collider_collision_constraint, "friction", text="碰撞摩擦")
        detail.prop(cloth.collider_collision_constraint.limit_distance, "value", text="碰撞限制距离")


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
    body.prop(collider, "collider_object", text="物体")
    body.prop(collider, "collider_type", text="类型")
    body.prop(collider, "center", text="中心")
    body.prop(collider, "radius", text="半径")
    if collider.collider_type == "CAPSULE":
        body.prop(collider, "radius_separation", text="分离半径")
        if collider.radius_separation:
            body.prop(collider, "end_radius", text="末端半径")
        body.prop(collider, "length", text="长度")
        body.prop(collider, "direction", text="方向")
        body.prop(collider, "reverse_direction", text="反向")
        body.prop(collider, "aligned_on_center", text="中心对齐")


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
    body.prop(cache, "source_object", text="源物体")
    body.prop(cache, "cache_format", text="格式")
    body.prop(cache, "cache_path", text="路径")


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

        reset_row = layout.row(align=True)
        reset_row.operator("hocloth.reset_runtime", icon="LOOP_BACK", text="重置运行时")
        reset_row.operator("hocloth.destroy_runtime", icon="TRASH", text="销毁运行时")

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
        runtime.prop(scene, "hocloth_simulation_frequency", text="模拟频率")
        runtime.prop(scene, "hocloth_apply_pose_on_step", text="Step 写回姿态")
        runtime.label(text=f"状态: {scene.hocloth_runtime_status}")
        runtime.label(
            text=(
                f"steps={scene.hocloth_runtime_step_count}, "
                f"transforms={scene.hocloth_runtime_transform_count}, "
                f"applied={getattr(scene, 'hocloth_runtime_applied_count', 0)}"
            )
        )
        if scene.hocloth_ui_details_expanded:
            runtime.label(
                text=(
                    f"non_identity={getattr(scene, 'hocloth_runtime_non_identity_transform_count', 0)}, "
                    f"max_rot={getattr(scene, 'hocloth_runtime_max_rotation_degrees', 0.0):.3f}, "
                    f"max_pos={getattr(scene, 'hocloth_runtime_max_translation', 0.0):.5f}"
                )
            )
            runtime.label(
                text=f"write_modes={getattr(scene, 'hocloth_runtime_write_mode_summary', '')}"
            )

        overlay = layout.box()
        overlay.label(text="Viewport")
        overlay.prop(scene, "hocloth_viewport_overlay_enabled", text="显示构建结果")
        col = overlay.column(align=True)
        col.enabled = scene.hocloth_viewport_overlay_enabled
        col.prop(scene, "hocloth_viewport_draw_bones", text="骨骼")
        col.prop(scene, "hocloth_viewport_draw_particle_radius", text="半径")
        col.prop(scene, "hocloth_viewport_draw_colliders", text="碰撞体")
        col.prop(scene, "hocloth_viewport_overlay_alpha", text="透明度")


CLASSES = (HOCLOTH_PT_main_panel,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
