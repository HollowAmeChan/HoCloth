import bpy

from ..compile.compiler import resolve_bone_chain_names


def _draw_bone_chain_details(layout, scene, item):
    if item.container_index < 0 or item.container_index >= len(scene.hocloth_bone_chain_components):
        layout.label(text="Bone chain data is missing", icon="ERROR")
        return

    chain = scene.hocloth_bone_chain_components[item.container_index]
    bone_names = resolve_bone_chain_names(scene, chain.armature_name, chain.root_bone_name)
    bone_count = len(bone_names)

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
    body.label(text=f"Armature: {chain.armature_name or 'None'}")
    body.label(text=f"Root Bone: {chain.root_bone_name or 'None'}")

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
        status_box.label(text="Runtime")
        status_box.label(text=f"Handle: {scene.hocloth_runtime_handle}")
        status_box.label(text=f"Status: {scene.hocloth_runtime_status}")


CLASSES = (HOCLOTH_PT_main_panel,)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
