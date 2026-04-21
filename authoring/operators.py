import bpy

from ..compile.compiler import compile_scene_from_components
from ..components.properties import create_component, delete_component, rebuild_component_indices
from ..runtime.session import build_runtime, destroy_runtime, reset_runtime, step_runtime
from .extract import extract_active_bone_chain


class HOCLOTH_OT_add_active_bone_chain(bpy.types.Operator):
    bl_idname = "hocloth.add_active_bone_chain"
    bl_label = "Add Active Bone Chain"
    bl_description = "Create a bone-chain component from the active armature selection"

    def execute(self, context):
        try:
            extracted = extract_active_bone_chain(context)
        except RuntimeError as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        main_item, typed_item = create_component(
            context.scene,
            "BONE_CHAIN",
            f"Bone Chain: {extracted.root_bone_name}",
        )
        typed_item.armature_object = context.object
        typed_item.root_bone_name = extracted.root_bone_name
        main_item.display_name = f"Bone Chain: {extracted.root_bone_name}"
        context.scene.hocloth_runtime_status = f"Added {len(extracted.bone_names)} bones"
        return {"FINISHED"}


class HOCLOTH_OT_remove_component(bpy.types.Operator):
    bl_idname = "hocloth.remove_component"
    bl_label = "Remove Component"
    bl_description = "Remove a component and its typed authoring data"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        if not self.component_id:
            self.report({"ERROR"}, "No component id was provided.")
            return {"CANCELLED"}

        if not delete_component(context.scene, self.component_id):
            self.report({"ERROR"}, "Component was not found.")
            return {"CANCELLED"}

        context.scene.hocloth_runtime_status = "Component removed"
        return {"FINISHED"}


class HOCLOTH_OT_rebuild_scene(bpy.types.Operator):
    bl_idname = "hocloth.rebuild_scene"
    bl_label = "Build Runtime"
    bl_description = "Compile authoring data and build the native runtime scene"

    def execute(self, context):
        rebuild_component_indices(context.scene)
        compiled = compile_scene_from_components(context.scene)
        if not compiled.bone_chains:
            self.report({"ERROR"}, "No enabled bone-chain components could be compiled.")
            context.scene.hocloth_runtime_status = "Build failed: no valid bone chains"
            return {"CANCELLED"}

        if compiled.total_bone_count() == 0:
            self.report({"ERROR"}, "Compiled bone chains contain no resolvable bones.")
            context.scene.hocloth_runtime_status = "Build failed: resolved 0 bones"
            return {"CANCELLED"}

        runtime_state = build_runtime(compiled)
        context.scene.hocloth_runtime_handle = runtime_state["handle"]
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        context.scene.hocloth_runtime_status = (
            f"Runtime ready via {runtime_state['backend']}: {runtime_state['summary']}"
        )
        return {"FINISHED"}


class HOCLOTH_OT_reset_runtime(bpy.types.Operator):
    bl_idname = "hocloth.reset_runtime"
    bl_label = "Reset Runtime"
    bl_description = "Reset the current runtime scene state without rebuilding structure"

    def execute(self, context):
        runtime_state = reset_runtime()
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        if runtime_state["handle"]:
            context.scene.hocloth_runtime_status = "Runtime reset"
            return {"FINISHED"}
        self.report({"INFO"}, "Runtime is not built yet.")
        return {"CANCELLED"}


class HOCLOTH_OT_step_runtime(bpy.types.Operator):
    bl_idname = "hocloth.step_runtime"
    bl_label = "Step Runtime"
    bl_description = "Execute one placeholder simulation step through the runtime API"

    dt: bpy.props.FloatProperty(name="dt", default=1.0 / 60.0, min=0.0001)
    substeps: bpy.props.IntProperty(name="Substeps", default=1, min=1, soft_max=8)

    def execute(self, context):
        try:
            result = step_runtime(self.dt, self.substeps)
        except RuntimeError as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        runtime_state = result["runtime_state"]
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        context.scene.hocloth_runtime_status = (
            f"Stepped {runtime_state['step_count']} times, transforms={runtime_state['bone_transform_count']}"
        )
        return {"FINISHED"}


class HOCLOTH_OT_destroy_runtime(bpy.types.Operator):
    bl_idname = "hocloth.destroy_runtime"
    bl_label = "Destroy Runtime"
    bl_description = "Destroy the current native runtime scene handle"

    def execute(self, context):
        destroy_runtime()
        context.scene.hocloth_runtime_handle = 0
        context.scene.hocloth_runtime_step_count = 0
        context.scene.hocloth_runtime_transform_count = 0
        context.scene.hocloth_runtime_status = "Runtime destroyed"
        return {"FINISHED"}


CLASSES = (
    HOCLOTH_OT_add_active_bone_chain,
    HOCLOTH_OT_remove_component,
    HOCLOTH_OT_rebuild_scene,
    HOCLOTH_OT_reset_runtime,
    HOCLOTH_OT_step_runtime,
    HOCLOTH_OT_destroy_runtime,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
