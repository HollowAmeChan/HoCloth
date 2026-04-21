import bpy

from ..compile.compiler import resolve_bone_chain_names


def _draw_bone_chain_details(layout, scene, item):
    if item.container_index < 0 or item.container_index >= len(scene.hocloth_bone_chain_components):
        layout.label(text="Bone chain data is missing", icon="ERROR")
        return

    chain = scene.hocloth_bone_chain_components[item.container_index]
    armature_object = chain.armature_object
    bone_names = resolve_bone_chain_names(scene, armature_object, chain.root_bone_name)
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
    title_col.label(text=f"{bone_count} bones", icon="ARMATURE_DATA")

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

    if not bone_names:
        body.label(text="No bones resolved from root", icon="INFO")
        return

    preview_count = min(8, bone_count)
    for bone_name in bone_names[:preview_count]:
        body.label(text=bone_name, icon="DOT")

    if bone_count > preview_count:
        body.label(text=f"... and {bone_count - preview_count} more", icon="INFO")


class HOCLOTH_PT_main_panel(bpy.types.Panel):
    bl_label = "HoCloth"
    bl_idname = "HOCLOTH_PT_main_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "HoCloth"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.label(text="MVP Host Architecture")
        layout.operator("hocloth.add_active_bone_chain", icon="BONE_DATA")

        row = layout.row(align=True)
        row.operator("hocloth.rebuild_scene", icon="FILE_REFRESH")
        row.operator("hocloth.export_compiled_scene", icon="EXPORT")
        row.operator("hocloth.reset_runtime", icon="LOOP_BACK")
        row.operator("hocloth.step_runtime", icon="FRAME_NEXT")
        row.operator("hocloth.apply_runtime_pose", icon="CON_ARMATURE")
        row.operator("hocloth.destroy_runtime", icon="TRASH")

        box = layout.box()
        box.label(text="Components")
        if not scene.hocloth_components:
            box.label(text="No components yet", icon="INFO")
        else:
            for item in scene.hocloth_components:
                if item.component_type == "BONE_CHAIN":
                    _draw_bone_chain_details(box, scene, item)
                else:
                    sub = box.row(align=True)
                    sub.prop(item, "enabled", text="")
                    sub.label(text=item.display_name)
                    sub.label(text=item.component_type)
                    op = sub.operator("hocloth.remove_component", text="", icon="X")
                    op.component_id = item.component_id

        status_box = layout.box()
        status_box.label(text=f"Compiled: {scene.hocloth_compile_summary}")
        settings_col = status_box.column(align=True)
        settings_col.prop(scene, "hocloth_runtime_dt", text="dt")
        settings_col.prop(scene, "hocloth_runtime_substeps", text="Substeps")
        settings_col.prop(scene, "hocloth_apply_pose_on_step", text="Apply Pose On Step")
        status_box.label(text="Runtime")
        status_box.label(text=f"Handle: {scene.hocloth_runtime_handle}")
        status_box.label(text=f"Steps: {scene.hocloth_runtime_step_count}")
        status_box.label(text=f"Transforms: {scene.hocloth_runtime_transform_count}")
        status_box.label(text=f"Status: {scene.hocloth_runtime_status}")


CLASSES = (HOCLOTH_PT_main_panel,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
