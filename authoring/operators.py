import bpy
import json
import os

from ..compile.compiler import (
    compile_scene_from_components,
    resolve_bone_chain_branching_names,
)
from ..components.properties import create_component, delete_component, rebuild_component_indices, sync_joint_override_names
from ..runtime.pose_apply import (
    apply_runtime_transforms_to_scene,
    capture_pose_baseline,
    clear_pose_transforms,
    restore_pose_state,
)
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
        "stiffness": 0.10,
        "damping": 0.22,
        "drag": 0.00,
        "gravity_strength": 1.15,
    },
    "BALANCED": {
        "label": "Balanced",
        "stiffness": 0.24,
        "damping": 0.38,
        "drag": 0.02,
        "gravity_strength": 0.85,
    },
    "ROPE": {
        "label": "Rope",
        "stiffness": 0.48,
        "damping": 0.58,
        "drag": 0.07,
        "gravity_strength": 1.45,
    },
    "HEAVY": {
        "label": "Heavy",
        "stiffness": 0.72,
        "damping": 0.82,
        "drag": 0.16,
        "gravity_strength": 2.0,
    },
}


def _find_spring_bone_component(scene, component_id: str):
    return next(
        (item for item in scene.hocloth_spring_bone_components if item.component_id == component_id),
        None,
    )


def _find_collider_group_component(scene, component_id: str):
    return next(
        (item for item in scene.hocloth_collider_group_components if item.component_id == component_id),
        None,
    )


def _build_runtime_from_scene(context, report=None) -> bool:
    scene = context.scene
    stop_live_runtime(scene, "Live runtime stopped")
    screen = context.screen
    if screen is not None and getattr(screen, "is_animation_playing", False):
        bpy.ops.screen.animation_cancel(restore_frame=False)

    rebuild_component_indices(scene)
    compiled = compile_scene_from_components(scene)
    scene.hocloth_compile_summary = compiled.summary()
    if not compiled.spring_bones:
        if report is not None:
            report({"ERROR"}, "No enabled spring-bone components could be compiled.")
        scene.hocloth_runtime_status = "Build failed: no valid spring bones"
        return False

    if compiled.total_bone_count() == 0:
        if report is not None:
            report({"ERROR"}, "Compiled bone chains contain no resolvable bones.")
        scene.hocloth_runtime_status = "Build failed: resolved 0 bones"
        return False

    runtime_state = build_runtime(compiled)
    initial_inputs = build_runtime_inputs(scene, compiled)
    set_runtime_inputs_only(initial_inputs)
    set_pose_baseline(capture_pose_baseline(scene, compiled))
    scene.hocloth_runtime_handle = runtime_state["handle"]
    scene.hocloth_runtime_step_count = runtime_state["step_count"]
    scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
    scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)
    build_message = runtime_state.get("build_message", "")
    solver_ready = runtime_state.get("physics_scene_ready", False)
    scene.hocloth_runtime_status = (
        f"Runtime ready via {runtime_state['backend']}: {runtime_state['summary']}"
        f", solver_ready={solver_ready}"
        f"{', ' + build_message if build_message else ''}"
        ", rebuild after changing chain parameters"
    )
    if not solver_ready and runtime_state["backend"] == "stub" and not build_message:
        scene.hocloth_runtime_status += ", native module unavailable so Python placeholder backend is active"
    return True


class HOCLOTH_OT_add_active_spring_bone(bpy.types.Operator):
    bl_idname = "hocloth.add_active_spring_bone"
    bl_label = "Add Active Spring Bone"
    bl_description = "Create a spring-bone component from the active armature selection"

    def execute(self, context):
        try:
            extracted = extract_active_bone_chain(context)
        except RuntimeError as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        main_item, typed_item = create_component(
            context.scene,
            "SPRING_BONE",
            f"Spring Bone: {extracted.root_bone_name}",
        )
        typed_item.armature_object = context.object
        typed_item.root_bone_name = extracted.root_bone_name
        sync_joint_override_names(typed_item, extracted.bone_names)
        main_item.display_name = f"Spring Bone: {extracted.root_bone_name}"
        branching_bones = resolve_bone_chain_branching_names(
            context.scene,
            context.object,
            extracted.root_bone_name,
        )
        context.scene.hocloth_runtime_status = (
            f"Added {len(extracted.bone_names)} bones"
            + (
                f"; detected {len(branching_bones)} branch points"
                if branching_bones
                else ""
            )
        )
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


class HOCLOTH_OT_add_collider_group(bpy.types.Operator):
    bl_idname = "hocloth.add_collider_group"
    bl_label = "Add Collision Binding"
    bl_description = "Create a collision-binding component"

    def execute(self, context):
        create_component(context.scene, "COLLIDER_GROUP", "Collision Binding")
        context.scene.hocloth_runtime_status = "Added collision binding"
        return {"FINISHED"}


class HOCLOTH_OT_add_cache_output(bpy.types.Operator):
    bl_idname = "hocloth.add_cache_output"
    bl_label = "Add Cache Output"
    bl_description = "Create a cache-output component"

    def execute(self, context):
        active_object = context.object
        main_item, typed_item = create_component(context.scene, "CACHE_OUTPUT", "Cache Output")
        if active_object is not None:
            typed_item.source_object = active_object
            main_item.display_name = f"Cache Output: {active_object.name}"
        context.scene.hocloth_runtime_status = "Added cache output"
        return {"FINISHED"}


class HOCLOTH_OT_assign_selected_colliders_to_group(bpy.types.Operator):
    bl_idname = "hocloth.assign_selected_colliders_to_group"
    bl_label = "Use Selected Collision Objects"
    bl_description = "Fill this collision binding from the currently selected collider source objects"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        group = _find_collider_group_component(context.scene, self.component_id)
        if group is None:
            self.report({"ERROR"}, "Collision binding was not found.")
            return {"CANCELLED"}

        selected_object_names = {obj.name for obj in context.selected_objects}
        matched_ids = [
            collider.component_id
            for collider in context.scene.hocloth_collider_components
            if collider.collider_object is not None and collider.collider_object.name in selected_object_names
        ]
        group.collider_ids = ", ".join(matched_ids)
        context.scene.hocloth_runtime_status = f"Assigned {len(matched_ids)} collision objects to binding"
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


class HOCLOTH_OT_apply_spring_bone_preset(bpy.types.Operator):
    bl_idname = "hocloth.apply_spring_bone_preset"
    bl_label = "Apply Spring Bone Preset"
    bl_description = "Apply a preset spring profile to this spring-bone component"

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

        chain = _find_spring_bone_component(context.scene, self.component_id)
        if chain is None:
            self.report({"ERROR"}, "Spring-bone component was not found.")
            return {"CANCELLED"}

        chain.stiffness = preset["stiffness"]
        chain.damping = preset["damping"]
        chain.drag = preset["drag"]
        chain.gravity_strength = preset["gravity_strength"]
        context.scene.hocloth_runtime_status = (
            f"Applied preset {preset['label']} to {chain.root_bone_name or 'bone chain'}; rebuild runtime to test"
        )
        return {"FINISHED"}


class HOCLOTH_OT_sync_spring_bone_joints(bpy.types.Operator):
    bl_idname = "hocloth.sync_spring_bone_joints"
    bl_label = "Sync Spring Joints"
    bl_description = "Refresh the spring-joint override list from the current armature subtree"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        chain = _find_spring_bone_component(context.scene, self.component_id)
        if chain is None:
            self.report({"ERROR"}, "Spring-bone component was not found.")
            return {"CANCELLED"}

        armature_object = chain.armature_object
        if armature_object is None or armature_object.type != "ARMATURE":
            self.report({"ERROR"}, "Assign an armature before syncing joints.")
            return {"CANCELLED"}

        from ..compile.compiler import resolve_bone_chain_names

        bone_names = resolve_bone_chain_names(context.scene, armature_object, chain.root_bone_name)
        branching_bones = resolve_bone_chain_branching_names(
            context.scene,
            armature_object,
            chain.root_bone_name,
        )
        synced_count = sync_joint_override_names(chain, bone_names)
        context.scene.hocloth_runtime_status = (
            f"Synced {synced_count} spring joints"
            + (
                f"; detected {len(branching_bones)} branch points"
                if branching_bones
                else ""
            )
        )
        return {"FINISHED"}


class HOCLOTH_OT_reset_spring_joint_override(bpy.types.Operator):
    bl_idname = "hocloth.reset_spring_joint_override"
    bl_label = "Reset Joint Override"
    bl_description = "Reset a spring-joint override back to component defaults"

    component_id: bpy.props.StringProperty(name="Component ID")
    bone_name: bpy.props.StringProperty(name="Bone Name")

    def execute(self, context):
        chain = _find_spring_bone_component(context.scene, self.component_id)
        if chain is None:
            self.report({"ERROR"}, "Spring-bone component was not found.")
            return {"CANCELLED"}

        entry = next((item for item in chain.joint_overrides if item.bone_name == self.bone_name), None)
        if entry is None:
            self.report({"ERROR"}, "Joint override was not found.")
            return {"CANCELLED"}

        entry.enabled = False
        entry.radius = chain.joint_radius
        entry.stiffness = chain.stiffness
        entry.damping = chain.damping
        entry.drag = chain.drag
        entry.gravity_scale = 1.0
        context.scene.hocloth_runtime_status = f"Reset joint override: {self.bone_name}"
        return {"FINISHED"}


class HOCLOTH_OT_assign_all_groups_to_spring_bone(bpy.types.Operator):
    bl_idname = "hocloth.assign_all_groups_to_spring_bone"
    bl_label = "Use All Collision Bindings"
    bl_description = "Link all current collision bindings to this spring bone"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        chain = _find_spring_bone_component(context.scene, self.component_id)
        if chain is None:
            self.report({"ERROR"}, "Spring-bone component was not found.")
            return {"CANCELLED"}

        group_ids = [item.component_id for item in context.scene.hocloth_collider_group_components]
        chain.collider_group_ids = ", ".join(group_ids)
        context.scene.hocloth_runtime_status = f"Linked {len(group_ids)} collision bindings to spring bone"
        return {"FINISHED"}


class HOCLOTH_OT_rebuild_scene(bpy.types.Operator):
    bl_idname = "hocloth.rebuild_scene"
    bl_label = "Build Runtime"
    bl_description = "Compile authoring data and build the native runtime scene"

    def execute(self, context):
        if not _build_runtime_from_scene(context, self.report):
            return {"CANCELLED"}
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
        context.scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)
        if runtime_state["handle"]:
            context.scene.hocloth_runtime_status = "Runtime reset"
            return {"FINISHED"}
        self.report({"INFO"}, "Runtime is not built yet.")
        return {"CANCELLED"}


class HOCLOTH_OT_restart_runtime_from_baseline(bpy.types.Operator):
    bl_idname = "hocloth.restart_runtime_from_baseline"
    bl_label = "Restart From Frame 1"
    bl_description = "Jump back to the first frame and clear simulated bone transforms beyond the captured baseline"

    def execute(self, context):
        stop_live_runtime(context.scene, "Live runtime stopped")
        screen = context.screen
        if screen is not None and getattr(screen, "is_animation_playing", False):
            bpy.ops.screen.animation_cancel(restore_frame=False)

        target_frame = max(1, int(context.scene.frame_start))
        context.scene.frame_set(target_frame)

        compiled_scene = get_compiled_scene()
        cleared_count = 0
        if compiled_scene is not None:
            baseline = capture_pose_baseline(context.scene, compiled_scene)
            cleared_count = clear_pose_transforms(context.scene, compiled_scene)
            set_pose_baseline(baseline)
            if context.view_layer is not None:
                context.view_layer.update()

        if context.scene.hocloth_runtime_handle and compiled_scene is not None:
            runtime_state = reset_runtime()
            set_runtime_inputs_only(build_runtime_inputs(context.scene, compiled_scene))
            set_pose_baseline(capture_pose_baseline(context.scene, compiled_scene))
            context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
            context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
            context.scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)
            context.scene.hocloth_runtime_status = (
                f"Returned to frame {target_frame} and cleared simulated pose ({cleared_count} bones)"
            )
            return {"FINISHED"}

        context.scene.hocloth_runtime_step_count = 0
        context.scene.hocloth_runtime_transform_count = 0
        context.scene.hocloth_runtime_last_fixed_steps = 0
        context.scene.hocloth_runtime_status = (
            f"Returned to frame {target_frame}"
            + (f" and cleared simulated pose ({cleared_count} bones)" if cleared_count else "")
        )
        if context.view_layer is not None:
            context.view_layer.update()
        return {"FINISHED"}


class HOCLOTH_OT_step_runtime(bpy.types.Operator):
    bl_idname = "hocloth.step_runtime"
    bl_label = "Step Runtime"
    bl_description = "Execute one placeholder simulation step through the runtime API"

    def execute(self, context):
        try:
            result = step_runtime(
                context.scene.hocloth_runtime_dt,
                context.scene.hocloth_simulation_frequency,
                build_runtime_inputs(context.scene, get_compiled_scene()),
            )
        except RuntimeError as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        runtime_state = result["runtime_state"]
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        context.scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)
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
            f"Stepped {runtime_state['step_count']} fixed steps, "
            f"last={runtime_state.get('last_executed_steps', 0)}, "
            f"transforms={runtime_state['bone_transform_count']}"
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

        if not _build_runtime_from_scene(context, self.report):
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
        context.scene.hocloth_runtime_last_fixed_steps = 0
        context.scene.hocloth_runtime_status = "Runtime destroyed"
        return {"FINISHED"}


CLASSES = (
    HOCLOTH_OT_add_active_spring_bone,
    HOCLOTH_OT_add_active_collider,
    HOCLOTH_OT_add_collider_group,
    HOCLOTH_OT_add_cache_output,
    HOCLOTH_OT_assign_selected_colliders_to_group,
    HOCLOTH_OT_remove_component,
    HOCLOTH_OT_apply_spring_bone_preset,
    HOCLOTH_OT_sync_spring_bone_joints,
    HOCLOTH_OT_reset_spring_joint_override,
    HOCLOTH_OT_assign_all_groups_to_spring_bone,
    HOCLOTH_OT_rebuild_scene,
    HOCLOTH_OT_export_compiled_scene,
    HOCLOTH_OT_reset_runtime,
    HOCLOTH_OT_restart_runtime_from_baseline,
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

