import bpy
import json
import os

from ..compile.compiler import compile_scene_from_components
from ..components.properties import create_component, delete_component, rebuild_component_indices
from ..runtime.pose_apply import apply_runtime_transforms_to_scene, capture_pose_baseline
from ..runtime.inputs import build_runtime_inputs
from ..runtime.live import start_live_runtime, stop_live_runtime
from ..runtime.session import (
    build_runtime,
    destroy_runtime,
    get_compiled_scene,
    get_last_transforms,
    get_pose_baseline,
    reset_runtime,
    set_runtime_inputs_only,
    set_pose_baseline,
    step_runtime,
)
from .extract import extract_active_bone_chain


_BONE_CHAIN_PRESETS = {
    "SOFT_HAIR": {
        "label": "Soft Hair",
        "stiffness": 0.35,
        "damping": 0.18,
        "drag": 0.04,
        "gravity_strength": 0.45,
    },
    "BALANCED": {
        "label": "Balanced",
        "stiffness": 0.75,
        "damping": 0.45,
        "drag": 0.10,
        "gravity_strength": 0.75,
    },
    "ROPE": {
        "label": "Rope",
        "stiffness": 1.15,
        "damping": 0.62,
        "drag": 0.16,
        "gravity_strength": 1.05,
    },
    "HEAVY": {
        "label": "Heavy",
        "stiffness": 1.45,
        "damping": 0.82,
        "drag": 0.28,
        "gravity_strength": 1.35,
    },
}


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


class HOCLOTH_OT_add_active_collider(bpy.types.Operator):
    bl_idname = "hocloth.add_active_collider"
    bl_label = "Add Active Collider"
    bl_description = "Create a collider component from the active object"

    def execute(self, context):
        active_object = context.object
        if active_object is None:
            self.report({"ERROR"}, "Select an object to use as a collider.")
            return {"CANCELLED"}

        main_item, typed_item = create_component(
            context.scene,
            "COLLIDER",
            f"Collider: {active_object.name}",
        )
        typed_item.collider_object = active_object
        typed_item.shape_type = "SPHERE" if active_object.type == "EMPTY" else "CAPSULE"
        main_item.display_name = f"Collider: {active_object.name}"
        context.scene.hocloth_runtime_status = f"Added collider from {active_object.name}"
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


class HOCLOTH_OT_apply_bone_chain_preset(bpy.types.Operator):
    bl_idname = "hocloth.apply_bone_chain_preset"
    bl_label = "Apply Bone Chain Preset"
    bl_description = "Apply a preset spring profile to this bone-chain component"

    component_id: bpy.props.StringProperty(name="Component ID")
    preset_id: bpy.props.StringProperty(name="Preset ID")

    def execute(self, context):
        if not self.component_id:
            self.report({"ERROR"}, "No component id was provided.")
            return {"CANCELLED"}

        preset = _BONE_CHAIN_PRESETS.get(self.preset_id)
        if preset is None:
            self.report({"ERROR"}, f"Unknown preset: {self.preset_id}")
            return {"CANCELLED"}

        chain = next(
            (item for item in context.scene.hocloth_bone_chain_components if item.component_id == self.component_id),
            None,
        )
        if chain is None:
            self.report({"ERROR"}, "Bone-chain component was not found.")
            return {"CANCELLED"}

        chain.stiffness = preset["stiffness"]
        chain.damping = preset["damping"]
        chain.drag = preset["drag"]
        chain.gravity_strength = preset["gravity_strength"]
        context.scene.hocloth_runtime_status = (
            f"Applied preset {preset['label']} to {chain.root_bone_name or 'bone chain'}; rebuild runtime to test"
        )
        return {"FINISHED"}


class HOCLOTH_OT_rebuild_scene(bpy.types.Operator):
    bl_idname = "hocloth.rebuild_scene"
    bl_label = "Build Runtime"
    bl_description = "Compile authoring data and build the native runtime scene"

    def execute(self, context):
        stop_live_runtime(context.scene, "Live runtime stopped")
        screen = context.screen
        if screen is not None and getattr(screen, "is_animation_playing", False):
            bpy.ops.screen.animation_cancel(restore_frame=False)

        rebuild_component_indices(context.scene)
        compiled = compile_scene_from_components(context.scene)
        context.scene.hocloth_compile_summary = compiled.summary()
        if not compiled.bone_chains:
            self.report({"ERROR"}, "No enabled bone-chain components could be compiled.")
            context.scene.hocloth_runtime_status = "Build failed: no valid bone chains"
            return {"CANCELLED"}

        if compiled.total_bone_count() == 0:
            self.report({"ERROR"}, "Compiled bone chains contain no resolvable bones.")
            context.scene.hocloth_runtime_status = "Build failed: resolved 0 bones"
            return {"CANCELLED"}

        runtime_state = build_runtime(compiled)
        initial_inputs = build_runtime_inputs(context.scene, compiled)
        set_runtime_inputs_only(initial_inputs)
        set_pose_baseline(capture_pose_baseline(context.scene, compiled))
        context.scene.hocloth_runtime_handle = runtime_state["handle"]
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        build_message = runtime_state.get("build_message", "")
        solver_ready = runtime_state.get("physics_scene_ready", False)
        context.scene.hocloth_runtime_status = (
            f"Runtime ready via {runtime_state['backend']}: {runtime_state['summary']}"
            f", solver_ready={solver_ready}"
            f"{', ' + build_message if build_message else ''}"
            ", rebuild after changing chain parameters"
        )
        if not solver_ready and runtime_state["backend"] == "stub" and not build_message:
            context.scene.hocloth_runtime_status += ", native module unavailable so Python placeholder backend is active"
        return {"FINISHED"}


class HOCLOTH_OT_export_compiled_scene(bpy.types.Operator):
    bl_idname = "hocloth.export_compiled_scene"
    bl_label = "Export Compiled Scene"
    bl_description = "Export the current compiled scene to a JSON preview file in _build"

    def execute(self, context):
        rebuild_component_indices(context.scene)
        compiled = compile_scene_from_components(context.scene)
        context.scene.hocloth_compile_summary = compiled.summary()

        plugin_root = os.path.dirname(os.path.dirname(__file__))
        export_dir = os.path.join(plugin_root, "_build")
        os.makedirs(export_dir, exist_ok=True)
        export_path = os.path.join(export_dir, "compiled_scene_preview.json")

        with open(export_path, "w", encoding="utf-8") as handle:
            json.dump(compiled.to_dict(), handle, indent=2, ensure_ascii=False)

        context.scene.hocloth_runtime_status = f"Compiled scene exported: {export_path}"
        self.report({"INFO"}, f"Compiled scene exported to {export_path}")
        return {"FINISHED"}


class HOCLOTH_OT_reset_runtime(bpy.types.Operator):
    bl_idname = "hocloth.reset_runtime"
    bl_label = "Reset Runtime"
    bl_description = "Reset the current runtime scene state without rebuilding structure"

    def execute(self, context):
        stop_live_runtime(context.scene, "Live runtime stopped")
        screen = context.screen
        if screen is not None and getattr(screen, "is_animation_playing", False):
            bpy.ops.screen.animation_cancel(restore_frame=False)

        compiled_scene = get_compiled_scene()
        if context.scene.hocloth_runtime_handle and compiled_scene is not None:
            set_runtime_inputs_only(build_runtime_inputs(context.scene, compiled_scene))
        runtime_state = reset_runtime()
        if runtime_state["handle"]:
            set_pose_baseline(capture_pose_baseline(context.scene, compiled_scene))
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

    def execute(self, context):
        try:
            result = step_runtime(
                context.scene.hocloth_runtime_dt,
                context.scene.hocloth_runtime_substeps,
                build_runtime_inputs(context.scene, get_compiled_scene()),
            )
        except RuntimeError as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        runtime_state = result["runtime_state"]
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        status_suffix = ""
        if context.scene.hocloth_apply_pose_on_step:
            apply_result = apply_runtime_transforms_to_scene(
                context.scene,
                get_compiled_scene(),
                result["transforms"],
                get_pose_baseline(),
            )
            context.view_layer.update()
            status_suffix = f", applied={apply_result['applied_count']}"
        context.scene.hocloth_runtime_status = (
            f"Stepped {runtime_state['step_count']} times, transforms={runtime_state['bone_transform_count']}"
            f"{status_suffix}"
        )
        return {"FINISHED"}


class HOCLOTH_OT_toggle_live_runtime(bpy.types.Operator):
    bl_idname = "hocloth.toggle_live_runtime"
    bl_label = "Toggle Live Runtime"
    bl_description = "Start or stop continuous runtime stepping while Blender animation playback is running"

    def execute(self, context):
        scene = context.scene
        if scene.hocloth_runtime_live_running:
            stop_live_runtime(scene, "Live runtime stopped")
            screen = context.screen
            if screen is not None and getattr(screen, "is_animation_playing", False):
                bpy.ops.screen.animation_cancel(restore_frame=False)
            return {"FINISHED"}

        if scene.hocloth_runtime_handle == 0 or get_compiled_scene() is None:
            self.report({"ERROR"}, "Build the runtime before starting live stepping.")
            return {"CANCELLED"}

        scene.sync_mode = "NONE"
        start_live_runtime(scene)
        screen = context.screen
        if screen is not None and not getattr(screen, "is_animation_playing", False):
            bpy.ops.screen.animation_play()
        scene.hocloth_runtime_status = "Live runtime armed, waiting for playback"
        return {"FINISHED"}


class HOCLOTH_OT_apply_runtime_pose(bpy.types.Operator):
    bl_idname = "hocloth.apply_runtime_pose"
    bl_label = "Apply Runtime Pose"
    bl_description = "Apply the most recently fetched runtime transforms back onto Blender pose bones"

    def execute(self, context):
        compiled_scene = get_compiled_scene()
        transforms = get_last_transforms()
        if compiled_scene is None or not transforms:
            self.report({"INFO"}, "No runtime transforms are available yet.")
            return {"CANCELLED"}

        apply_result = apply_runtime_transforms_to_scene(
            context.scene,
            compiled_scene,
            transforms,
            get_pose_baseline(),
        )
        context.view_layer.update()
        context.scene.hocloth_runtime_status = (
            f"Applied pose to {apply_result['applied_count']} bones "
            f"across {apply_result['armature_count']} armatures"
        )
        return {"FINISHED"}


class HOCLOTH_OT_destroy_runtime(bpy.types.Operator):
    bl_idname = "hocloth.destroy_runtime"
    bl_label = "Destroy Runtime"
    bl_description = "Destroy the current native runtime scene handle"

    def execute(self, context):
        stop_live_runtime(context.scene, "Live runtime stopped")
        screen = context.screen
        if screen is not None and getattr(screen, "is_animation_playing", False):
            bpy.ops.screen.animation_cancel(restore_frame=False)
        destroy_runtime()
        context.scene.hocloth_runtime_handle = 0
        context.scene.hocloth_runtime_step_count = 0
        context.scene.hocloth_runtime_transform_count = 0
        context.scene.hocloth_runtime_status = "Runtime destroyed"
        return {"FINISHED"}


CLASSES = (
    HOCLOTH_OT_add_active_bone_chain,
    HOCLOTH_OT_add_active_collider,
    HOCLOTH_OT_remove_component,
    HOCLOTH_OT_apply_bone_chain_preset,
    HOCLOTH_OT_rebuild_scene,
    HOCLOTH_OT_export_compiled_scene,
    HOCLOTH_OT_reset_runtime,
    HOCLOTH_OT_step_runtime,
    HOCLOTH_OT_toggle_live_runtime,
    HOCLOTH_OT_apply_runtime_pose,
    HOCLOTH_OT_destroy_runtime,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
