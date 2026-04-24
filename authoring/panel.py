import bpy

from ..compile.compiler import resolve_bone_chain_branching_names, resolve_bone_chain_names
from ..components.properties import _parse_component_id_list, find_component_by_id, list_component_display_names


_BONE_CHAIN_PRESET_BUTTONS = (
    ("SOFT_HAIR", "Soft Hair"),
    ("BALANCED", "Balanced"),
    ("ROPE", "Rope"),
    ("HEAVY", "Heavy"),
)


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
    title_col.label(text=item.display_name, icon="BONE_DATA")
    title_col.label(text=f"{bone_count} joints", icon="ARMATURE_DATA")

    actions = header.row(align=True)
    actions.prop(item, "enabled", text="")
    remove_op = actions.operator("hocloth.remove_component", text="", icon="X")
    remove_op.component_id = item.component_id

    if not item.ui_expanded:
        return

    body = layout.box()
    body.prop(chain, "armature_object", text="Armature")
    if armature_object and armature_object.data:
        root_row = body.row()
        root_row.alert = bool(chain.root_bone_name) and not root_bone_is_valid
        root_row.prop_search(chain, "root_bone_name", armature_object.data, "bones", text="Root Bone")
    else:
        body.prop(chain, "root_bone_name", text="Root Bone")
        body.label(text="Select an armature object first", icon="INFO")

    if armature_object and not chain.root_bone_name:
        body.label(text="Select a root bone", icon="INFO")
    elif armature_object and chain.root_bone_name and not root_bone_is_valid:
        warn = body.row()
        warn.alert = True
        warn.label(text="Stored root bone is no longer valid on this armature", icon="ERROR")

    params = body.column(align=True)
    preset_box = body.box()
    preset_box.label(text="Presets")
    preset_row = preset_box.row(align=True)
    for preset_id, label in _BONE_CHAIN_PRESET_BUTTONS[:2]:
        preset_op = preset_row.operator("hocloth.apply_spring_bone_preset", text=label)
        preset_op.component_id = item.component_id
        preset_op.preset_id = preset_id
    preset_row = preset_box.row(align=True)
    for preset_id, label in _BONE_CHAIN_PRESET_BUTTONS[2:]:
        preset_op = preset_row.operator("hocloth.apply_spring_bone_preset", text=label)
        preset_op.component_id = item.component_id
        preset_op.preset_id = preset_id
    preset_box.label(text="Apply preset, then rebuild runtime", icon="INFO")

    params.label(text="Spring")
    params.prop(chain, "center_source", text="Center Anchor")
    if chain.center_source == "OBJECT":
        params.prop(chain, "center_object", text="Center Object")
    elif chain.center_source == "BONE":
        params.prop(chain, "center_armature_object", text="Center Armature")
        if chain.center_armature_object and chain.center_armature_object.data:
            params.prop_search(chain, "center_bone_name", chain.center_armature_object.data, "bones", text="Center Bone")
        else:
            params.prop(chain, "center_bone_name", text="Center Bone")
    params.prop(chain, "joint_radius", text="Particle Radius")
    params.prop(chain, "append_tail_tip", text="Append Tail Tip Joint")
    params.prop(chain, "stiffness")
    params.prop(chain, "damping")
    params.prop(chain, "drag")
    params.prop(chain, "use_spring")
    spring_col = params.column(align=True)
    spring_col.enabled = chain.use_spring
    spring_col.prop(chain, "spring_power")
    spring_col.prop(chain, "limit_distance")
    spring_col.prop(chain, "normal_limit_ratio")
    spring_col.prop(chain, "spring_noise")
    params.prop(chain, "gravity_strength")
    params.prop(chain, "gravity_direction")
    params.label(text="MC2-style BoneSpring currently ignores gravity strength.", icon="INFO")
    params.separator()
    params.label(text="Collision")
    params.prop(chain, "collider_group_ids", text="Collision Bindings")
    sync_op = params.operator("hocloth.sync_spring_bone_joints", icon="FILE_REFRESH")
    sync_op.component_id = item.component_id
    group_link_op = params.operator("hocloth.assign_all_groups_to_spring_bone", icon="LINKED")
    group_link_op.component_id = item.component_id

    group_names = list_component_display_names(scene, _parse_component_id_list(chain.collider_group_ids))
    if group_names:
        body.label(text="Collision Bindings", icon="LINKED")
        for group_name in group_names[:4]:
            body.label(text=group_name, icon="DOT")

    summary_box = body.box()
    summary_box.label(text="Config Summary")
    summary_box.label(text=f"Root: {chain.root_bone_name or 'None'}", icon="BONE_DATA")
    summary_box.label(text=f"Joints: {bone_count}", icon="ARMATURE_DATA")
    summary_box.label(text=f"Particle Radius: {chain.joint_radius:.3f}", icon="MESH_UVSPHERE")
    summary_box.label(text=f"Tail Tip: {'On' if chain.append_tail_tip else 'Off'}", icon="BONE_DATA")
    summary_box.label(
        text=(
            f"Spring: {'On' if chain.use_spring else 'Off'}"
            f" / power {chain.spring_power:.3f}"
            f" / limit {chain.limit_distance:.3f}"
        ),
        icon="FORCE_HARMONIC",
    )
    if chain.center_source == "OBJECT" and chain.center_object is not None:
        summary_box.label(text=f"Center Object: {chain.center_object.name}", icon="EMPTY_AXIS")
    elif chain.center_source == "BONE" and chain.center_bone_name:
        summary_box.label(text=f"Center Bone: {chain.center_bone_name}", icon="CON_ARMATURE")
    else:
        summary_box.label(text="Center: None", icon="INFO")
    summary_box.label(text=f"Collision Bindings: {len(group_names)}", icon="LINKED")
    if branching_bones:
        summary_box.label(text=f"Branch Points: {len(branching_bones)}", icon="NODETREE")

    if chain.joint_overrides:
        joint_box = body.box()
        joint_box.label(text="Joint Overrides")
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
                detail.prop(entry, "radius")
                detail.prop(entry, "stiffness")
                detail.prop(entry, "damping")
                detail.prop(entry, "drag")
                detail.prop(entry, "gravity_scale")
        if len(chain.joint_overrides) > preview_count:
            joint_box.label(text=f"... and {len(chain.joint_overrides) - preview_count} more joints", icon="INFO")

    if not bone_names:
        body.label(text="No bones resolved from root", icon="INFO")
        return

    if branching_bones:
        info = body.box()
        info.label(text="Branching topology detected", icon="NODETREE")
        for branch_name in branching_bones[:4]:
            info.label(text=branch_name, icon="DOT")
        if len(branching_bones) > 4:
            info.label(text=f"... and {len(branching_bones) - 4} more", icon="INFO")

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
    body.label(text="Collision Object")
    body.prop(collider, "collider_object", text="Source Object")
    body.prop(collider, "shape_type", text="Shape")
    body.prop(collider, "radius", text="Radius")
    if collider.shape_type == "CAPSULE":
        body.prop(collider, "height", text="Capsule Height")

    if collider_object is None:
        body.label(text="Assign an object for this collision object", icon="INFO")
    else:
        body.label(text=f"Transform Source: {collider_object.name}", icon="OUTLINER_OB_EMPTY")
        body.label(text=f"Type: {collider_object.type}", icon="INFO")


def _draw_collider_group_details(layout, scene, item):
    group = _resolve_component(scene, item, "hocloth_collider_group_components")
    if group is None:
        return

    collider_names = list_component_display_names(scene, _parse_component_id_list(group.collider_ids))

    header = layout.row(align=True)
    header.prop(item, "ui_expanded", text="", emboss=False, icon="TRIA_DOWN" if item.ui_expanded else "TRIA_RIGHT")
    title_col = header.column(align=True)
    title_col.label(text=item.display_name, icon="GROUP")
    title_col.label(text=f"{len(collider_names)} collision objects", icon="MESH_UVSPHERE")
    actions = header.row(align=True)
    actions.prop(item, "enabled", text="")
    remove_op = actions.operator("hocloth.remove_component", text="", icon="X")
    remove_op.component_id = item.component_id

    if not item.ui_expanded:
        return

    body = layout.box()
    body.label(text="Collision Binding")
    body.prop(group, "collider_ids", text="Collision Objects")
    body.label(text="Use comma-separated collider component IDs", icon="INFO")
    body.label(text="This compiles into a runtime collision binding", icon="INFO")
    assign_op = body.operator("hocloth.assign_selected_colliders_to_group", icon="RESTRICT_SELECT_OFF")
    assign_op.component_id = item.component_id
    for collider_name in collider_names[:6]:
        body.label(text=collider_name, icon="DOT")


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
    body.prop(cache_output, "source_object")
    body.prop(cache_output, "cache_format")
    body.prop(cache_output, "cache_path")
    if cache_output.source_object is None:
        body.label(text="Assign a source object to describe the cache target", icon="INFO")


class HOCLOTH_PT_main_panel(bpy.types.Panel):
    bl_label = "HoCloth"
    bl_idname = "HOCLOTH_PT_main_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "HoCloth"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.label(text="XPBD Spring Runtime")
        quick_add = layout.row(align=True)
        quick_add.operator("hocloth.add_active_spring_bone", icon="BONE_DATA", text="Spring")
        quick_add.operator("hocloth.add_active_collider", icon="MESH_UVSPHERE", text="Collider")
        quick_add.operator("hocloth.add_collider_group", icon="GROUP", text="Binding")
        quick_add.operator("hocloth.add_cache_output", icon="EXPORT", text="Cache")

        row = layout.row(align=True)
        row.operator("hocloth.rebuild_scene", icon="FILE_REFRESH", text="Build")
        row.operator("hocloth.step_runtime", icon="FRAME_NEXT", text="Step")
        row.operator(
            "hocloth.toggle_live_runtime",
            icon="PAUSE" if scene.hocloth_runtime_live_running else "PLAY",
            text="Pause" if scene.hocloth_runtime_live_running else "Live",
        )

        debug_box = layout.box()
        debug_box.prop(scene, "hocloth_ui_debug_expanded", text="Advanced Tools")
        if scene.hocloth_ui_debug_expanded:
            debug_row = debug_box.row(align=True)
            debug_row.operator("hocloth.export_compiled_scene", icon="EXPORT")
            debug_row.operator("hocloth.reset_runtime", icon="LOOP_BACK")
            debug_row.operator("hocloth.restart_runtime_from_baseline", icon="ARMATURE_DATA")
            debug_row.operator("hocloth.apply_runtime_pose", icon="CON_ARMATURE")
            debug_row.operator("hocloth.destroy_runtime", icon="TRASH")

        box = layout.box()
        box.label(text="Components")
        if not scene.hocloth_components:
            box.label(text="No components yet", icon="INFO")
        else:
            for item in scene.hocloth_components:
                if item.component_type in {"SPRING_BONE", "BONE_CHAIN"}:
                    _draw_spring_bone_details(box, scene, item)
                elif item.component_type == "COLLIDER":
                    _draw_collider_details(box, scene, item)
                elif item.component_type == "COLLIDER_GROUP":
                    _draw_collider_group_details(box, scene, item)
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
        runtime_box.label(text="Runtime")
        runtime_box.label(text=f"Compiled: {scene.hocloth_compile_summary}")
        settings_col = runtime_box.column(align=True)
        settings_col.prop(scene, "hocloth_runtime_dt", text="Frame dt")
        settings_col.prop(scene, "hocloth_simulation_frequency", text="Simulation Hz")
        settings_col.prop(scene, "hocloth_apply_pose_on_step", text="Apply Pose On Step")
        runtime_box.label(text=f"Handle: {scene.hocloth_runtime_handle}")
        runtime_box.label(text=f"Steps: {scene.hocloth_runtime_step_count}")
        runtime_box.label(text=f"Last Fixed Steps: {getattr(scene, 'hocloth_runtime_last_fixed_steps', 0)}")
        runtime_box.label(text=f"Transforms: {scene.hocloth_runtime_transform_count}")
        runtime_box.label(text=f"Live: {'Running' if scene.hocloth_runtime_live_running else 'Stopped'}")
        runtime_box.label(text=f"Status: {scene.hocloth_runtime_status}")

        overlay_box = layout.box()
        overlay_box.label(text="Viewport Overlay")
        overlay_box.prop(scene, "hocloth_viewport_overlay_enabled", text="Enable Overlay")
        overlay_col = overlay_box.column(align=True)
        overlay_col.enabled = scene.hocloth_viewport_overlay_enabled
        overlay_col.prop(scene, "hocloth_viewport_draw_bones")
        overlay_col.prop(scene, "hocloth_viewport_draw_particle_radius")
        overlay_col.prop(scene, "hocloth_viewport_draw_colliders")
        overlay_col.prop(scene, "hocloth_viewport_overlay_alpha")


CLASSES = (HOCLOTH_PT_main_panel,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
