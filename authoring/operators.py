import json
import os

import bpy

from ..components import mc2
from ..runtime.authoring_snapshot import (
    build_authoring_snapshot,
    runtime_scene_view_from_authoring_snapshot,
)
from ..runtime.exchange import wrap_compiled_scene
from ..runtime.inputs import build_runtime_inputs
from ..runtime.live import start_live_runtime, stop_live_runtime
from ..runtime.pose_apply import (
    apply_runtime_mesh_outputs_to_scene,
    apply_runtime_transforms_to_scene,
    capture_pose_baseline,
    clear_pose_transforms,
    restore_pose_state,
)
from ..runtime.session import (
    build_runtime,
    destroy_runtime,
    get_backend_scene_view,
    get_last_mesh_outputs,
    get_last_transforms,
    get_pose_baseline,
    reset_runtime,
    set_pose_baseline,
    set_runtime_inputs_only,
    step_runtime,
)
from .extract import extract_active_bone_chain


def _join_component_id_list(component_ids: list[str]) -> str:
    return mc2.join_component_id_list(component_ids)


def _find_cloth(scene, component_id: str):
    return mc2.find_magica_cloth(scene, component_id)


def _append_collider_to_cloth(cloth, collider_id: str) -> bool:
    collider_ids = mc2.parse_component_id_list(getattr(cloth, "collider_ids", ""))
    if collider_id in collider_ids:
        return False
    collider_ids.append(collider_id)
    cloth.collider_ids = _join_component_id_list(collider_ids)
    return True


def _auto_link_existing_colliders(scene, cloth) -> int:
    linked_count = 0
    for collider in scene.hocloth_mc2_colliders:
        if collider.collider_object is not None and _append_collider_to_cloth(cloth, collider.component_id):
            linked_count += 1
    return linked_count


def _link_collider_to_all_cloths(scene, collider_id: str) -> int:
    linked_count = 0
    for cloth in scene.hocloth_mc2_magica_cloths:
        if _append_collider_to_cloth(cloth, collider_id):
            linked_count += 1
    return linked_count


def _clear_runtime_mesh_writeback_stats(scene):
    scene.hocloth_runtime_mesh_output_count = 0
    scene.hocloth_runtime_mesh_applied_count = 0
    scene.hocloth_runtime_mesh_vertex_count = 0
    scene.hocloth_runtime_mesh_missing_object_count = 0
    scene.hocloth_runtime_mesh_topology_mismatch_count = 0


def _store_runtime_mesh_writeback_stats(scene, mesh_outputs, apply_result: dict):
    scene.hocloth_runtime_mesh_output_count = len(mesh_outputs or [])
    scene.hocloth_runtime_mesh_applied_count = int(apply_result.get("applied_mesh_count", 0))
    scene.hocloth_runtime_mesh_vertex_count = int(apply_result.get("applied_vertex_count", 0))
    scene.hocloth_runtime_mesh_missing_object_count = int(apply_result.get("missing_object_count", 0))
    scene.hocloth_runtime_mesh_topology_mismatch_count = int(apply_result.get("topology_mismatch_count", 0))


def _mesh_writeback_status_suffix(scene) -> str:
    if scene.hocloth_runtime_mesh_output_count <= 0:
        return ""
    suffix = (
        f", mesh_outputs={scene.hocloth_runtime_mesh_output_count}, "
        f"meshes={scene.hocloth_runtime_mesh_applied_count}, "
        f"verts={scene.hocloth_runtime_mesh_vertex_count}"
    )
    if scene.hocloth_runtime_mesh_missing_object_count:
        suffix += f", missing_mesh_objects={scene.hocloth_runtime_mesh_missing_object_count}"
    if scene.hocloth_runtime_mesh_topology_mismatch_count:
        suffix += f", mesh_topology_mismatch={scene.hocloth_runtime_mesh_topology_mismatch_count}"
    return suffix


def _current_backend_scene_view(scene):
    authoring_snapshot = build_authoring_snapshot(scene)
    return authoring_snapshot, runtime_scene_view_from_authoring_snapshot(authoring_snapshot)


def _build_runtime_from_scene(context, report=None) -> bool:
    scene = context.scene
    stop_live_runtime(scene, "Live runtime stopped")
    screen = context.screen
    if screen is not None and getattr(screen, "is_animation_playing", False):
        bpy.ops.screen.animation_cancel(restore_frame=False)

    authoring_snapshot, compiled = _current_backend_scene_view(scene)
    scene.hocloth_backend_summary = compiled.summary()
    if not compiled.spring_bones:
        if report is not None:
            report({"ERROR"}, "No enabled MagicaCloth BoneCloth/BoneSpring components found.")
        scene.hocloth_runtime_status = "Build failed: no MagicaCloth components"
        return False
    if compiled.total_bone_count() == 0:
        if report is not None:
            report({"ERROR"}, "MagicaCloth components resolved 0 bones.")
        scene.hocloth_runtime_status = "Build failed: resolved 0 bones"
        return False

    runtime_state = build_runtime(authoring_snapshot, True, backend_scene_view=compiled)
    initial_inputs = build_runtime_inputs(scene, compiled)
    set_runtime_inputs_only(initial_inputs)
    set_pose_baseline(capture_pose_baseline(scene, compiled))
    scene.hocloth_runtime_handle = runtime_state["handle"]
    scene.hocloth_runtime_backend = runtime_state.get("backend", "unknown")
    scene.hocloth_runtime_step_count = runtime_state["step_count"]
    scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
    scene.hocloth_runtime_applied_count = 0
    scene.hocloth_runtime_missing_bone_count = 0
    scene.hocloth_runtime_missing_armature_count = 0
    scene.hocloth_runtime_apply_armature_count = 0
    _clear_runtime_mesh_writeback_stats(scene)
    scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)
    build_message = runtime_state.get("build_message", "")
    solver_ready = runtime_state.get("physics_scene_ready", False)
    scene.hocloth_runtime_status = (
        f"Runtime ready via {runtime_state['backend']}: {runtime_state['summary']}, "
        f"build_input=authoring_snapshot, solver_ready={solver_ready}"
        f"{', ' + build_message if build_message else ''}"
    )
    return True


class HOCLOTH_OT_add_active_spring_bone(bpy.types.Operator):
    bl_idname = "hocloth.add_active_spring_bone"
    bl_label = "Add BoneSpring"
    bl_description = "Create an MC2 MagicaCloth BoneSpring component from the active armature selection"

    def execute(self, context):
        try:
            extracted = extract_active_bone_chain(context)
        except RuntimeError as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        cloth = mc2.create_magica_cloth(
            context.scene,
            "BONE_SPRING",
            f"MagicaCloth BoneSpring: {extracted.root_bone_name}",
        )
        cloth.armature_object = context.object
        cloth.root_bone_name = extracted.root_bone_name
        linked = _auto_link_existing_colliders(context.scene, cloth)
        mc2.sync_joint_override_names(cloth, extracted.bone_names)
        context.scene.hocloth_runtime_status = (
            f"Added BoneSpring with {len(extracted.bone_names)} bones"
            + (f"; linked {linked} colliders" if linked else "")
        )
        return {"FINISHED"}


class HOCLOTH_OT_add_active_bone_cloth(bpy.types.Operator):
    bl_idname = "hocloth.add_active_bone_cloth"
    bl_label = "Add BoneCloth"
    bl_description = "Create an MC2 MagicaCloth BoneCloth component from the active armature selection"

    def execute(self, context):
        try:
            extracted = extract_active_bone_chain(context)
        except RuntimeError as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        cloth = mc2.create_magica_cloth(
            context.scene,
            "BONE_CLOTH",
            f"MagicaCloth BoneCloth: {extracted.root_bone_name}",
        )
        cloth.armature_object = context.object
        cloth.root_bone_name = extracted.root_bone_name
        cloth.spring_constraint.use_spring = False
        linked = _auto_link_existing_colliders(context.scene, cloth)
        mc2.sync_joint_override_names(cloth, extracted.bone_names)
        context.scene.hocloth_runtime_status = (
            f"Added BoneCloth with {len(extracted.bone_names)} bones"
            + (f"; linked {linked} colliders" if linked else "")
        )
        return {"FINISHED"}


class HOCLOTH_OT_add_active_collider(bpy.types.Operator):
    bl_idname = "hocloth.add_active_collider"
    bl_label = "Add Collider"
    bl_description = "Create an MC2 collider component from the active object"

    def execute(self, context):
        active_object = context.object
        if active_object is None:
            self.report({"ERROR"}, "Select an object to use as a collider.")
            return {"CANCELLED"}

        collider_type = "SPHERE" if active_object.type == "EMPTY" else "CAPSULE"
        collider = mc2.create_collider(
            context.scene,
            collider_type,
            f"Magica{collider_type.title()}Collider: {active_object.name}",
        )
        collider.collider_object = active_object
        linked = _link_collider_to_all_cloths(context.scene, collider.component_id)
        context.scene.hocloth_runtime_status = (
            f"Added {collider.collider_type} collider from {active_object.name}"
            + (f"; linked {linked} cloth components" if linked else "")
        )
        return {"FINISHED"}


class HOCLOTH_OT_add_cache_output(bpy.types.Operator):
    bl_idname = "hocloth.add_cache_output"
    bl_label = "Add Cache Output"
    bl_description = "Create a Blender cache-output component"

    def execute(self, context):
        cache = mc2.create_cache_output(context.scene, "Blender Cache Output")
        if context.object is not None:
            cache.source_object = context.object
            main_item = next(
                (item for item in context.scene.hocloth_mc2_components if item.component_id == cache.component_id),
                None,
            )
            if main_item is not None:
                main_item.display_name = f"Blender Cache Output: {context.object.name}"
        context.scene.hocloth_runtime_status = "Added cache output"
        return {"FINISHED"}


class HOCLOTH_OT_assign_selected_colliders_to_spring_bone(bpy.types.Operator):
    bl_idname = "hocloth.assign_selected_colliders_to_spring_bone"
    bl_label = "Use Selected Colliders"
    bl_description = "Use selected MC2 collider components on this MagicaCloth component"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None:
            self.report({"ERROR"}, "MagicaCloth component was not found.")
            return {"CANCELLED"}

        selected_object_names = {obj.name for obj in context.selected_objects}
        collider_ids = [
            collider.component_id
            for collider in context.scene.hocloth_mc2_colliders
            if collider.collider_object is not None and collider.collider_object.name in selected_object_names
        ]
        cloth.collider_ids = _join_component_id_list(collider_ids)
        context.scene.hocloth_runtime_status = f"Assigned {len(collider_ids)} colliders"
        return {"FINISHED"}


class HOCLOTH_OT_assign_all_groups_to_spring_bone(bpy.types.Operator):
    bl_idname = "hocloth.assign_all_groups_to_spring_bone"
    bl_label = "Use All Colliders"
    bl_description = "Assign all current MC2 colliders to this MagicaCloth component"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None:
            self.report({"ERROR"}, "MagicaCloth component was not found.")
            return {"CANCELLED"}

        collider_ids = [
            collider.component_id
            for collider in context.scene.hocloth_mc2_colliders
            if collider.collider_object is not None
        ]
        cloth.collider_ids = _join_component_id_list(collider_ids)
        context.scene.hocloth_runtime_status = f"Assigned {len(collider_ids)} colliders"
        return {"FINISHED"}


class HOCLOTH_OT_remove_component(bpy.types.Operator):
    bl_idname = "hocloth.remove_component"
    bl_label = "Remove Component"
    bl_description = "Remove an MC2 component and its typed authoring data"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        if not self.component_id:
            self.report({"ERROR"}, "No component id was provided.")
            return {"CANCELLED"}
        if not mc2.delete_component(context.scene, self.component_id):
            self.report({"ERROR"}, "Component was not found.")
            return {"CANCELLED"}
        context.scene.hocloth_runtime_status = "Component removed"
        return {"FINISHED"}


class HOCLOTH_OT_apply_spring_bone_preset(bpy.types.Operator):
    bl_idname = "hocloth.apply_spring_bone_preset"
    bl_label = "Apply MC2 Preset"
    bl_description = "Apply an MC2 preset to this MagicaCloth component"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None:
            self.report({"ERROR"}, "MagicaCloth component was not found.")
            return {"CANCELLED"}
        preset = mc2.HOCLOTH_MC2_BONE_SPRING_PRESETS.get(cloth.preset_profile)
        if preset is None:
            self.report({"ERROR"}, f"Unknown preset: {cloth.preset_profile}")
            return {"CANCELLED"}
        mc2.apply_preset(cloth, cloth.preset_profile)
        context.scene.hocloth_runtime_status = f"Applied preset {preset['label']}"
        return {"FINISHED"}


class HOCLOTH_OT_sync_spring_bone_joints(bpy.types.Operator):
    bl_idname = "hocloth.sync_spring_bone_joints"
    bl_label = "Sync Joints"
    bl_description = "Refresh the joint override list from the current armature subtree"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None or cloth.armature_object is None:
            self.report({"ERROR"}, "Assign an armature before syncing joints.")
            return {"CANCELLED"}

        from ..runtime.bone_sampling import resolve_bone_chain_names

        bone_names = resolve_bone_chain_names(context.scene, cloth.armature_object, cloth.root_bone_name)
        synced = mc2.sync_joint_override_names(cloth, bone_names)
        context.scene.hocloth_runtime_status = f"Synced {synced} joints"
        return {"FINISHED"}


class HOCLOTH_OT_reset_spring_joint_override(bpy.types.Operator):
    bl_idname = "hocloth.reset_spring_joint_override"
    bl_label = "Reset Joint Override"
    bl_description = "Reset a joint override back to component defaults"

    component_id: bpy.props.StringProperty(name="Component ID")
    bone_name: bpy.props.StringProperty(name="Bone Name")

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None:
            self.report({"ERROR"}, "MagicaCloth component was not found.")
            return {"CANCELLED"}
        entry = next((item for item in cloth.joint_overrides if item.bone_name == self.bone_name), None)
        if entry is None:
            self.report({"ERROR"}, "Joint override was not found.")
            return {"CANCELLED"}
        default_stiffness, default_damping, default_drag = mc2.cloth_runtime_defaults(cloth)
        entry.enabled = False
        entry.radius = cloth.joint_radius
        entry.stiffness = default_stiffness
        entry.damping = default_damping
        entry.drag = default_drag
        entry.gravity_scale = 1.0
        context.scene.hocloth_runtime_status = f"Reset joint override: {self.bone_name}"
        return {"FINISHED"}


class HOCLOTH_OT_rebuild_scene(bpy.types.Operator):
    bl_idname = "hocloth.rebuild_scene"
    bl_label = "Build Runtime"
    bl_description = "Build the native runtime from MC2 authoring data"

    def execute(self, context):
        if not _build_runtime_from_scene(context, self.report):
            return {"CANCELLED"}
        return {"FINISHED"}


class HOCLOTH_OT_export_mc2_snapshot(bpy.types.Operator):
    bl_idname = "hocloth.export_mc2_snapshot"
    bl_label = "Export MC2 Snapshot"
    bl_description = "Export the current MC2 authoring snapshot and backend runtime view"

    def execute(self, context):
        authoring_snapshot, compiled = _current_backend_scene_view(context.scene)
        context.scene.hocloth_backend_summary = compiled.summary()

        plugin_root = os.path.dirname(os.path.dirname(__file__))
        export_dir = os.path.join(plugin_root, "_build")
        os.makedirs(export_dir, exist_ok=True)
        snapshot_path = os.path.join(export_dir, "authoring_snapshot_preview.json")
        backend_path = os.path.join(export_dir, "backend_scene_preview.json")

        with open(snapshot_path, "w", encoding="utf-8") as handle:
            json.dump(authoring_snapshot, handle, indent=2, ensure_ascii=False)
        with open(backend_path, "w", encoding="utf-8") as handle:
            json.dump(wrap_compiled_scene(compiled), handle, indent=2, ensure_ascii=False)

        context.scene.hocloth_runtime_status = f"Authoring snapshot exported: {snapshot_path}"
        self.report({"INFO"}, f"Authoring snapshot exported to {snapshot_path}")
        return {"FINISHED"}


class HOCLOTH_OT_export_frame_inputs(bpy.types.Operator):
    bl_idname = "hocloth.export_frame_inputs"
    bl_label = "Export Frame Inputs"
    bl_description = "Export current frame inputs sent to the runtime"

    def execute(self, context):
        compiled_scene = get_backend_scene_view()
        if compiled_scene is None:
            _authoring_snapshot, compiled_scene = _current_backend_scene_view(context.scene)
            context.scene.hocloth_backend_summary = compiled_scene.summary()

        frame_inputs = build_runtime_inputs(context.scene, compiled_scene)
        plugin_root = os.path.dirname(os.path.dirname(__file__))
        export_dir = os.path.join(plugin_root, "_build")
        os.makedirs(export_dir, exist_ok=True)
        export_path = os.path.join(export_dir, "frame_inputs_preview.json")
        with open(export_path, "w", encoding="utf-8") as handle:
            json.dump(frame_inputs, handle, indent=2, ensure_ascii=False)

        context.scene.hocloth_runtime_status = f"Frame inputs exported: {export_path}"
        self.report({"INFO"}, f"Frame inputs exported to {export_path}")
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

        compiled_scene = get_backend_scene_view()
        if context.scene.hocloth_runtime_handle and compiled_scene is not None:
            set_runtime_inputs_only(build_runtime_inputs(context.scene, compiled_scene))
        runtime_state = reset_runtime()
        if runtime_state["handle"]:
            set_pose_baseline(capture_pose_baseline(context.scene, compiled_scene))
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        context.scene.hocloth_runtime_applied_count = 0
        context.scene.hocloth_runtime_missing_bone_count = 0
        context.scene.hocloth_runtime_missing_armature_count = 0
        context.scene.hocloth_runtime_apply_armature_count = 0
        _clear_runtime_mesh_writeback_stats(context.scene)
        context.scene.hocloth_runtime_last_fixed_steps = 0
        context.scene.hocloth_runtime_status = "Runtime reset"
        return {"FINISHED"}


class HOCLOTH_OT_restart_runtime_from_baseline(bpy.types.Operator):
    bl_idname = "hocloth.restart_runtime_from_baseline"
    bl_label = "Return To First Frame"
    bl_description = "Stop live runtime, restore initial pose, jump to the start frame, and rebuild"

    def execute(self, context):
        scene = context.scene
        stop_live_runtime(scene, "Live runtime stopped")
        screen = context.screen
        if screen is not None and getattr(screen, "is_animation_playing", False):
            bpy.ops.screen.animation_cancel(restore_frame=False)

        compiled_scene = get_backend_scene_view()
        pose_state = get_pose_baseline()
        if pose_state:
            restore_pose_state(scene, compiled_scene, pose_state)
        else:
            clear_pose_transforms(scene, compiled_scene)

        target_frame = scene.frame_start
        scene.frame_set(target_frame)
        reset_runtime()
        if context.view_layer is not None:
            context.view_layer.update()
        scene.hocloth_runtime_status = f"Returned to frame {target_frame}"
        return {"FINISHED"}


class HOCLOTH_OT_step_runtime(bpy.types.Operator):
    bl_idname = "hocloth.step_runtime"
    bl_label = "Step Runtime"
    bl_description = "Execute one fixed simulation step through the runtime API"

    def execute(self, context):
        compiled_scene = get_backend_scene_view()
        try:
            result = step_runtime(
                context.scene.hocloth_runtime_dt,
                context.scene.hocloth_simulation_frequency,
                build_runtime_inputs(context.scene, compiled_scene),
            )
        except RuntimeError as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        runtime_state = result["runtime_state"]
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        context.scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)
        context.scene.hocloth_runtime_applied_count = 0
        context.scene.hocloth_runtime_missing_bone_count = 0
        context.scene.hocloth_runtime_missing_armature_count = 0
        context.scene.hocloth_runtime_apply_armature_count = 0
        mesh_outputs = result.get("mesh_outputs", [])
        mesh_apply_result = apply_runtime_mesh_outputs_to_scene(context.scene, mesh_outputs)
        _store_runtime_mesh_writeback_stats(context.scene, mesh_outputs, mesh_apply_result)

        status_suffix = ", not applied"
        if context.scene.hocloth_apply_pose_on_step:
            apply_result = apply_runtime_transforms_to_scene(
                context.scene,
                compiled_scene,
                result["transforms"],
                get_pose_baseline(),
            )
            context.view_layer.update()
            context.scene.hocloth_runtime_applied_count = apply_result["applied_count"]
            context.scene.hocloth_runtime_missing_bone_count = apply_result["missing_bone_count"]
            context.scene.hocloth_runtime_missing_armature_count = apply_result["missing_armature_count"]
            context.scene.hocloth_runtime_apply_armature_count = apply_result["armature_count"]
            status_suffix = (
                f", applied={apply_result['applied_count']}, "
                f"missing_bones={apply_result['missing_bone_count']}, "
                f"missing_armatures={apply_result['missing_armature_count']}"
            )
        if mesh_apply_result["applied_mesh_count"] and context.view_layer is not None:
            context.view_layer.update()
        status_suffix += _mesh_writeback_status_suffix(context.scene)
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
        scene.hocloth_runtime_status = "Live runtime armed"
        return {"FINISHED"}


class HOCLOTH_OT_apply_runtime_pose(bpy.types.Operator):
    bl_idname = "hocloth.apply_runtime_pose"
    bl_label = "Apply Runtime Pose"
    bl_description = "Apply the most recently fetched runtime transforms back onto Blender pose bones"

    def execute(self, context):
        compiled_scene = get_backend_scene_view()
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
        context.scene.hocloth_runtime_applied_count = apply_result["applied_count"]
        context.scene.hocloth_runtime_missing_bone_count = apply_result["missing_bone_count"]
        context.scene.hocloth_runtime_missing_armature_count = apply_result["missing_armature_count"]
        context.scene.hocloth_runtime_apply_armature_count = apply_result["armature_count"]
        context.scene.hocloth_runtime_status = f"Applied pose to {apply_result['applied_count']} bones"
        return {"FINISHED"}


class HOCLOTH_OT_apply_runtime_mesh_output(bpy.types.Operator):
    bl_idname = "hocloth.apply_runtime_mesh_output"
    bl_label = "Apply Runtime Mesh Output"
    bl_description = "Apply the most recently fetched runtime mesh output back onto Blender mesh vertices"

    def execute(self, context):
        mesh_outputs = get_last_mesh_outputs()
        if not mesh_outputs:
            self.report({"INFO"}, "No runtime mesh outputs are available yet.")
            return {"CANCELLED"}

        apply_result = apply_runtime_mesh_outputs_to_scene(context.scene, mesh_outputs)
        _store_runtime_mesh_writeback_stats(context.scene, mesh_outputs, apply_result)
        if apply_result["applied_mesh_count"] and context.view_layer is not None:
            context.view_layer.update()
        context.scene.hocloth_runtime_status = (
            f"Applied mesh output to {apply_result['applied_mesh_count']} meshes, "
            f"verts={apply_result['applied_vertex_count']}"
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
        context.scene.hocloth_runtime_backend = "none"
        context.scene.hocloth_runtime_step_count = 0
        context.scene.hocloth_runtime_transform_count = 0
        context.scene.hocloth_runtime_applied_count = 0
        context.scene.hocloth_runtime_missing_bone_count = 0
        context.scene.hocloth_runtime_missing_armature_count = 0
        context.scene.hocloth_runtime_apply_armature_count = 0
        _clear_runtime_mesh_writeback_stats(context.scene)
        context.scene.hocloth_runtime_last_fixed_steps = 0
        context.scene.hocloth_runtime_status = "Runtime destroyed"
        return {"FINISHED"}


CLASSES = (
    HOCLOTH_OT_add_active_bone_cloth,
    HOCLOTH_OT_add_active_spring_bone,
    HOCLOTH_OT_add_active_collider,
    HOCLOTH_OT_add_cache_output,
    HOCLOTH_OT_assign_selected_colliders_to_spring_bone,
    HOCLOTH_OT_assign_all_groups_to_spring_bone,
    HOCLOTH_OT_remove_component,
    HOCLOTH_OT_apply_spring_bone_preset,
    HOCLOTH_OT_sync_spring_bone_joints,
    HOCLOTH_OT_reset_spring_joint_override,
    HOCLOTH_OT_rebuild_scene,
    HOCLOTH_OT_export_mc2_snapshot,
    HOCLOTH_OT_export_frame_inputs,
    HOCLOTH_OT_reset_runtime,
    HOCLOTH_OT_restart_runtime_from_baseline,
    HOCLOTH_OT_step_runtime,
    HOCLOTH_OT_toggle_live_runtime,
    HOCLOTH_OT_apply_runtime_pose,
    HOCLOTH_OT_apply_runtime_mesh_output,
    HOCLOTH_OT_destroy_runtime,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
