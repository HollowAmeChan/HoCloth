import bpy

from ..compile.compiler import resolve_bone_chain_branching_names, resolve_bone_chain_names
from ..components.properties import find_component_by_id
from ..runtime.session import get_exchange_info


def _resolve_component(scene, item, container_name):
    container = getattr(scene, container_name)
    component = find_component_by_id(container, item.component_id)
    if component is not None:
        return component
    if 0 <= item.container_index < len(container):
        fallback = container[item.container_index]
        if fallback.component_id == item.component_id:
            return fallback
    return None


def _draw_spring_bone_details(layout, scene, item):
    chain = _resolve_component(scene, item, "hocloth_spring_bone_components")
    if chain is None:
        return

    armature_object = chain.armature_object
    bone_names = resolve_bone_chain_names(scene, armature_object, chain.root_bone_name)
    branching_bones = resolve_bone_chain_branching_names(scene, armature_object, chain.root_bone_name)
    bone_count = len(bone_names)
    root_bone_is_valid = (
        armature_object is not None
        and armature_object.type == "ARMATURE"
        and armature_object.data is not None
        and chain.root_bone_name in armature_object.data.bones
    )

    header = layout.row(align=True)
    header.prop(
        item,
        "ui_expanded",
        text="",
        emboss=False,
        icon="TRIA_DOWN" if item.ui_expanded else "TRIA_RIGHT",
    )

    title_col = header.column(align=True)
    component_label = "BoneCloth" if item.component_type == "BONE_CLOTH" else "BoneSpring"
    title_col.label(text=item.display_name, icon="BONE_DATA")
    title_col.label(text=component_label, icon="MOD_CLOTH")
    title_col.label(text=f"{bone_count} joints", icon="ARMATURE_DATA")

    actions = header.row(align=True)
    actions.prop(item, "enabled", text="")
    remove_op = actions.operator("hocloth.remove_component", text="", icon="X")
    remove_op.component_id = item.component_id

    if not item.ui_expanded:
        return

    body = layout.box()
    body.prop(chain, "armature_object", text="骨架")
    if armature_object and armature_object.data:
        root_row = body.row()
        root_row.alert = bool(chain.root_bone_name) and not root_bone_is_valid
        root_row.prop_search(chain, "root_bone_name", armature_object.data, "bones", text="根骨骼")
    else:
        body.prop(chain, "root_bone_name", text="根骨骼")
        body.label(text="先选择骨架对象", icon="INFO")

    if armature_object and not chain.root_bone_name:
        body.label(text="选择一个根骨骼", icon="INFO")
    elif armature_object and chain.root_bone_name and not root_bone_is_valid:
        warn = body.row()
        warn.alert = True
        warn.label(text="记录的根骨骼在当前骨架中无效", icon="ERROR")

    params = body.column(align=True)
    preset_row = params.row(align=True)
    preset_row.prop(chain, "preset_profile", text="MC2预设")
    preset_op = preset_row.operator("hocloth.apply_spring_bone_preset", text="应用")
    preset_op.component_id = item.component_id

    params.prop(chain, "center_source", text="中心锚点")
    if chain.center_source == "OBJECT":
        params.prop(chain, "center_object", text="中心对象")
    elif chain.center_source == "BONE":
        params.prop(chain, "center_armature_object", text="中心骨架")
        if chain.center_armature_object and chain.center_armature_object.data:
            params.prop_search(chain, "center_bone_name", chain.center_armature_object.data, "bones", text="中心骨骼")
        else:
            params.prop(chain, "center_bone_name", text="中心骨骼")
    params.prop(chain, "joint_radius", text="粒子半径")
    params.prop(chain.damping_curve, "value", text="阻尼")
    params.prop(chain.distance_constraint.stiffness, "value", text="距离刚度")
    params.prop(chain.tether_constraint, "distance_compression", text="系绳压缩")
    params.prop(chain.spring_constraint, "spring_power", text="弹簧强度")
    params.prop(chain, "gravity_strength", text="重力强度")

    detail_row = body.row(align=True)
    detail_row.prop(
        scene,
        "hocloth_ui_details_expanded",
        text="显示详细信息",
        icon="TRIA_DOWN" if scene.hocloth_ui_details_expanded else "TRIA_RIGHT",
    )
    if scene.hocloth_ui_details_expanded:
        damping_box = body.box()
        damping_box.label(text="MC2 阻尼")
        damping_box.prop(chain.damping_curve, "use_curve", text="使用曲线")
        damping_box.prop(chain.damping_curve, "value", text="阻尼值")
        inertia_box = body.box()
        inertia_box.label(text="MC2 惯性约束")
        inertia_box.prop(chain.inertia_constraint, "world_inertia", text="世界惯性")
        inertia_box.prop(chain.inertia_constraint, "movement_inertia_smoothing", text="移动惯性平滑")
        inertia_box.prop(chain.inertia_constraint.movement_speed_limit, "use", text="启用移动速度限制")
        if chain.inertia_constraint.movement_speed_limit.use:
            inertia_box.prop(chain.inertia_constraint.movement_speed_limit, "value", text="移动速度限制")
        inertia_box.prop(chain.inertia_constraint.rotation_speed_limit, "use", text="启用旋转速度限制")
        if chain.inertia_constraint.rotation_speed_limit.use:
            inertia_box.prop(chain.inertia_constraint.rotation_speed_limit, "value", text="旋转速度限制")
        inertia_box.prop(chain.inertia_constraint, "local_inertia", text="局部惯性")
        inertia_box.prop(chain.inertia_constraint.local_movement_speed_limit, "use", text="启用局部移动速度限制")
        if chain.inertia_constraint.local_movement_speed_limit.use:
            inertia_box.prop(chain.inertia_constraint.local_movement_speed_limit, "value", text="局部移动速度限制")
        inertia_box.prop(chain.inertia_constraint.local_rotation_speed_limit, "use", text="启用局部旋转速度限制")
        if chain.inertia_constraint.local_rotation_speed_limit.use:
            inertia_box.prop(chain.inertia_constraint.local_rotation_speed_limit, "value", text="局部旋转速度限制")
        inertia_box.prop(chain.inertia_constraint, "depth_inertia", text="深度惯性")
        inertia_box.prop(chain.inertia_constraint, "centrifugal_acceleration", text="离心加速度")
        inertia_box.prop(chain.inertia_constraint.particle_speed_limit, "use", text="启用粒子速度限制")
        if chain.inertia_constraint.particle_speed_limit.use:
            inertia_box.prop(chain.inertia_constraint.particle_speed_limit, "value", text="粒子速度限制")
        distance_box = body.box()
        distance_box.label(text="MC2 距离 / 系绳")
        distance_box.prop(chain.distance_constraint.stiffness, "use_curve", text="距离刚度使用曲线")
        distance_box.prop(chain.distance_constraint.stiffness, "value", text="距离刚度")
        distance_box.prop(chain.tether_constraint, "distance_compression", text="系绳距离压缩")
        angle_box = body.box()
        angle_box.label(text="MC2 角度复原约束")
        angle_box.prop(chain.angle_restoration_constraint, "use_angle_restoration", text="启用角度复原")
        angle_col = angle_box.column(align=True)
        angle_col.enabled = chain.angle_restoration_constraint.use_angle_restoration
        angle_col.prop(chain.angle_restoration_constraint.stiffness, "use_curve", text="复原刚度使用曲线")
        angle_col.prop(chain.angle_restoration_constraint.stiffness, "value", text="复原刚度")
        angle_col.prop(chain.angle_restoration_constraint, "velocity_attenuation", text="速度衰减")
        spring_box = body.box()
        spring_box.label(text="MC2 弹簧约束")
        spring_box.prop(chain.spring_constraint, "use_spring", text="启用弹簧")
        spring_col = spring_box.column(align=True)
        spring_col.enabled = chain.spring_constraint.use_spring
        spring_col.prop(chain.spring_constraint, "spring_power", text="弹簧强度")
        spring_col.prop(chain.spring_constraint, "limit_distance", text="限制距离")
        spring_col.prop(chain.spring_constraint, "normal_limit_ratio", text="法线限制比例")
        spring_col.prop(chain.spring_constraint, "spring_noise", text="弹簧噪声")
        collision_box = body.box()
        collision_box.label(text="MC2 碰撞体碰撞约束")
        collision_box.prop(chain.collider_collision_constraint, "friction", text="摩擦")
        collision_box.prop(chain.collider_collision_constraint.limit_distance, "use_curve", text="推出距离使用曲线")
        collision_box.prop(chain.collider_collision_constraint.limit_distance, "value", text="推出限制距离")
    params.separator()
    params.prop(chain, "collider_ids", text="碰撞体")
    selected_collider_op = params.operator("hocloth.assign_selected_colliders_to_spring_bone", icon="RESTRICT_SELECT_OFF", text="使用选中碰撞体")
    selected_collider_op.component_id = item.component_id
    all_collider_op = params.operator("hocloth.assign_all_groups_to_spring_bone", icon="LINKED", text="使用全部碰撞体")
    all_collider_op.component_id = item.component_id
    sync_op = params.operator("hocloth.sync_spring_bone_joints", icon="FILE_REFRESH", text="同步骨骼")
    sync_op.component_id = item.component_id

    if chain.joint_overrides and scene.hocloth_ui_details_expanded:
        joint_box = body.box()
        joint_box.label(text="关节覆盖")
        preview_count = min(6, len(chain.joint_overrides))
        for entry in chain.joint_overrides[:preview_count]:
            row = joint_box.row(align=True)
            row.prop(entry, "enabled", text="")
            row.label(text=entry.bone_name, icon="BONE_DATA")
            if entry.enabled:
                row.label(text=f"r={entry.radius:.3f} k={entry.stiffness:.2f} d={entry.damping:.2f}")
            reset_op = row.operator("hocloth.reset_spring_joint_override", text="", icon="LOOP_BACK")
            reset_op.component_id = item.component_id
            reset_op.bone_name = entry.bone_name
            if entry.enabled:
                detail = joint_box.column(align=True)
                detail.prop(entry, "radius", text="半径")
                detail.prop(entry, "stiffness", text="刚度")
                detail.prop(entry, "damping", text="阻尼")
                detail.prop(entry, "drag", text="拖拽")
        if len(chain.joint_overrides) > preview_count:
            joint_box.label(text=f"... and {len(chain.joint_overrides) - preview_count} more joints", icon="INFO")

    if not bone_names:
        body.label(text="No bones resolved from root", icon="INFO")
        return

    if branching_bones:
        body.label(text=f"检测到 {len(branching_bones)} 个分支点", icon="NODETREE")

    if scene.hocloth_ui_details_expanded:
        preview_count = min(8, bone_count)
        for bone_name in bone_names[:preview_count]:
            body.label(text=bone_name, icon="DOT")
        if bone_count > preview_count:
            body.label(text=f"... and {bone_count - preview_count} more", icon="INFO")


def _draw_collider_details(layout, scene, item):
    collider = _resolve_component(scene, item, "hocloth_collider_components")
    if collider is None:
        return

    collider_object = collider.collider_object

    header = layout.row(align=True)
    header.prop(
        item,
        "ui_expanded",
        text="",
        emboss=False,
        icon="TRIA_DOWN" if item.ui_expanded else "TRIA_RIGHT",
    )

    title_col = header.column(align=True)
    title_col.label(text=item.display_name, icon="MESH_UVSPHERE")
    title_col.label(
        text=collider_object.name if collider_object is not None else "No object",
        icon="OBJECT_DATA",
    )

    actions = header.row(align=True)
    actions.prop(item, "enabled", text="")
    remove_op = actions.operator("hocloth.remove_component", text="", icon="X")
    remove_op.component_id = item.component_id

    if not item.ui_expanded:
        return

    body = layout.box()
    body.label(text="碰撞体")
    body.prop(collider, "collider_object", text="源对象")
    body.prop(collider, "shape_type", text="形状")
    body.prop(collider, "radius", text="半径")
    if collider.shape_type == "CAPSULE":
        body.prop(collider, "height", text="胶囊高度")
        body.prop(collider, "capsule_end_radius", text="末端半径")
        body.prop(collider, "capsule_direction", text="胶囊方向")
        body.prop(collider, "capsule_aligned_on_center", text="中心对齐")
        body.prop(collider, "capsule_reverse_direction", text="反向")

    if collider_object is None:
        body.label(text="指定一个对象作为碰撞体", icon="INFO")
    else:
        body.label(text=f"变换源: {collider_object.name}", icon="OUTLINER_OB_EMPTY")
        body.label(text=f"类型: {collider_object.type}", icon="INFO")


def _draw_cache_output_details(layout, scene, item):
    cache_output = _resolve_component(scene, item, "hocloth_cache_output_components")
    if cache_output is None:
        return

    header = layout.row(align=True)
    header.prop(item, "ui_expanded", text="", emboss=False, icon="TRIA_DOWN" if item.ui_expanded else "TRIA_RIGHT")
    title_col = header.column(align=True)
    title_col.label(text=item.display_name, icon="EXPORT")
    title_col.label(text=cache_output.cache_format.upper(), icon="FILE")
    actions = header.row(align=True)
    actions.prop(item, "enabled", text="")
    remove_op = actions.operator("hocloth.remove_component", text="", icon="X")
    remove_op.component_id = item.component_id

    if not item.ui_expanded:
        return

    body = layout.box()
    body.prop(cache_output, "source_object", text="源对象")
    body.prop(cache_output, "cache_format", text="格式")
    body.prop(cache_output, "cache_path", text="路径")
    if cache_output.source_object is None:
        body.label(text="指定缓存输出源对象", icon="INFO")


class HOCLOTH_PT_main_panel(bpy.types.Panel):
    bl_label = "HoCloth"
    bl_idname = "HOCLOTH_PT_main_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "HoCloth"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.label(text="HoCloth XPBD")
        quick_add = layout.row(align=True)
        quick_add.operator("hocloth.add_active_bone_cloth", icon="MOD_CLOTH", text="BoneCloth")
        quick_add.operator("hocloth.add_active_spring_bone", icon="BONE_DATA", text="弹簧骨骼")
        quick_add.operator("hocloth.add_active_collider", icon="MESH_UVSPHERE", text="碰撞体")
        quick_add.operator("hocloth.add_cache_output", icon="EXPORT", text="缓存")

        row = layout.row(align=True)
        row.operator("hocloth.rebuild_scene", icon="FILE_REFRESH", text="构建")
        row.operator("hocloth.step_runtime", icon="FRAME_NEXT", text="步进")
        row.operator(
            "hocloth.toggle_live_runtime",
            icon="PAUSE" if scene.hocloth_runtime_live_running else "PLAY",
            text="暂停" if scene.hocloth_runtime_live_running else "实时",
        )
        row.operator("hocloth.restart_runtime_from_baseline", icon="ARMATURE_DATA", text="回到第一帧")

        layout.prop(
            scene,
            "hocloth_ui_details_expanded",
            text="显示详细信息",
            icon="TRIA_DOWN" if scene.hocloth_ui_details_expanded else "TRIA_RIGHT",
        )

        debug_box = layout.box()
        debug_box.prop(scene, "hocloth_ui_debug_expanded", text="调试工具")
        if scene.hocloth_ui_debug_expanded:
            debug_row = debug_box.row(align=True)
            debug_row.operator("hocloth.export_compiled_scene", icon="EXPORT", text="导出结构")
            debug_row.operator("hocloth.export_frame_inputs", icon="FILE_TEXT", text="导出帧输入")
            debug_row.operator("hocloth.reset_runtime", icon="LOOP_BACK", text="重置")
            debug_row.operator("hocloth.apply_runtime_pose", icon="CON_ARMATURE", text="应用姿态")
            debug_row.operator("hocloth.apply_runtime_mesh_output", icon="MESH_DATA", text="应用网格")
            debug_row.operator("hocloth.destroy_runtime", icon="TRASH", text="销毁")

        box = layout.box()
        box.label(text="组件")
        if not scene.hocloth_components:
            box.label(text="还没有组件", icon="INFO")
        else:
            for item in scene.hocloth_components:
                if item.component_type in {"BONE_CLOTH", "SPRING_BONE", "BONE_CHAIN"}:
                    _draw_spring_bone_details(box, scene, item)
                elif item.component_type == "COLLIDER":
                    _draw_collider_details(box, scene, item)
                elif item.component_type == "COLLIDER_GROUP":
                    continue
                elif item.component_type == "CACHE_OUTPUT":
                    _draw_cache_output_details(box, scene, item)
                else:
                    sub = box.row(align=True)
                    sub.prop(item, "enabled", text="")
                    sub.label(text=item.display_name)
                    sub.label(text=item.component_type)
                    op = sub.operator("hocloth.remove_component", text="", icon="X")
                    op.component_id = item.component_id

        runtime_box = layout.box()
        runtime_box.label(text="运行时")
        settings_col = runtime_box.column(align=True)
        settings_col.prop(scene, "hocloth_runtime_dt", text="帧间隔")
        settings_col.prop(scene, "hocloth_simulation_frequency", text="模拟频率")
        settings_col.prop(scene, "hocloth_apply_pose_on_step", text="步进后写回姿态")
        runtime_box.label(text=f"状态: {scene.hocloth_runtime_status}")
        runtime_box.label(
            text=(
                f"步数: {scene.hocloth_runtime_step_count}, "
                f"骨骼返回: {scene.hocloth_runtime_transform_count}, "
                f"姿态写回: {getattr(scene, 'hocloth_runtime_applied_count', 0)}, "
                f"网格写回: {getattr(scene, 'hocloth_runtime_mesh_applied_count', 0)}"
            )
        )
        if scene.hocloth_ui_details_expanded:
            exchange_info = get_exchange_info()
            runtime_box.label(text=f"结构: {scene.hocloth_compile_summary}")
            runtime_box.label(text=f"Handle: {scene.hocloth_runtime_handle}")
            runtime_box.label(text=f"Backend: {getattr(scene, 'hocloth_runtime_backend', 'none')}")
            runtime_box.label(text=f"Last Fixed Steps: {getattr(scene, 'hocloth_runtime_last_fixed_steps', 0)}")
            runtime_box.label(
                text=(
                    f"Missing: bones={getattr(scene, 'hocloth_runtime_missing_bone_count', 0)}, "
                    f"armatures={getattr(scene, 'hocloth_runtime_missing_armature_count', 0)}"
                )
            )
            runtime_box.label(
                text=(
                    f"Mesh Output: outputs={getattr(scene, 'hocloth_runtime_mesh_output_count', 0)}, "
                    f"meshes={getattr(scene, 'hocloth_runtime_mesh_applied_count', 0)}, "
                    f"verts={getattr(scene, 'hocloth_runtime_mesh_vertex_count', 0)}"
                )
            )
            runtime_box.label(
                text=(
                    f"Mesh Missing: objects={getattr(scene, 'hocloth_runtime_mesh_missing_object_count', 0)}, "
                    f"topology={getattr(scene, 'hocloth_runtime_mesh_topology_mismatch_count', 0)}"
                )
            )
            runtime_box.label(text=f"Exchange: {exchange_info['schema']} v{exchange_info['schema_version']}")
            if scene.hocloth_ui_debug_expanded:
                runtime_box.label(text=f"Debug Dump: {exchange_info['debug_dump_path']}", icon="FILE_TEXT")

        overlay_box = layout.box()
        overlay_box.label(text="视口显示")
        overlay_box.prop(scene, "hocloth_viewport_overlay_enabled", text="启用覆盖显示")
        overlay_col = overlay_box.column(align=True)
        overlay_col.enabled = scene.hocloth_viewport_overlay_enabled
        overlay_col.prop(scene, "hocloth_viewport_draw_bones", text="骨骼")
        overlay_col.prop(scene, "hocloth_viewport_draw_particle_radius", text="粒子半径")
        overlay_col.prop(scene, "hocloth_viewport_draw_colliders", text="碰撞体")
        overlay_col.prop(scene, "hocloth_viewport_overlay_alpha", text="透明度")


CLASSES = (HOCLOTH_PT_main_panel,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
