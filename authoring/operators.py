import bpy

from ..components import mc2
from ..runtime.authoring_snapshot import (
    build_authoring_snapshot,
)
from ..runtime.inputs import build_runtime_inputs, reset_runtime_input_tracking
from ..runtime.live import start_live_runtime, stop_live_runtime
from ..runtime.pose_apply import (
    apply_runtime_mesh_outputs_to_scene,
    apply_runtime_transforms_to_scene,
    capture_pose_baseline,
    capture_pose_state,
    restore_pose_state,
)
from ..runtime.session import (
    build_runtime,
    destroy_runtime,
    get_last_authoring_snapshot,
    get_last_mesh_outputs,
    get_last_transforms,
    get_pose_baseline,
    reset_runtime,
    set_detailed_native_debug_enabled,
    set_pose_baseline,
    set_runtime_inputs_only,
    step_runtime,
)
from .extract import extract_active_bone_chain


def _find_cloth(scene, component_id: str):
    return mc2.find_magica_cloth(scene, component_id)


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


def _current_authoring_snapshot(scene):
    return build_authoring_snapshot(scene)


def _iter_snapshot_pose_bones(scene, authoring_snapshot: dict | None):
    payload = (authoring_snapshot or {}).get("payload", {})
    armature_cache = {}
    seen = set()
    for chain in payload.get("bone_chains", []):
        armature_name = chain.get("armature_name", "")
        if armature_name not in armature_cache:
            from ..runtime.blender_refs import resolve_armature_object

            armature_cache[armature_name] = resolve_armature_object(scene, armature_name)
        armature_object = armature_cache.get(armature_name)
        if armature_object is None or armature_object.pose is None:
            continue

        for bone in chain.get("bones", []):
            bone_name = bone.get("name", "")
            key = (armature_object.name, bone_name)
            if key in seen:
                continue
            pose_bone = armature_object.pose.bones.get(bone_name)
            if pose_bone is None:
                continue
            seen.add(key)
            yield armature_object, pose_bone


def _keyframe_snapshot_pose_bones(scene, authoring_snapshot: dict | None, frame: int) -> int:
    inserted = 0
    for _armature_object, pose_bone in _iter_snapshot_pose_bones(scene, authoring_snapshot):
        if pose_bone.rotation_mode != "QUATERNION":
            pose_bone.rotation_mode = "QUATERNION"
        pose_bone.keyframe_insert(data_path="location", frame=frame)
        pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
        inserted += 1
    return inserted


def _clear_snapshot_pose_bone_bake(scene, authoring_snapshot: dict | None) -> int:
    removed = 0
    by_armature = {}
    for armature_object, pose_bone in _iter_snapshot_pose_bones(scene, authoring_snapshot):
        by_armature.setdefault(armature_object.name, (armature_object, set()))[1].add(pose_bone.name)

    for armature_object, bone_names in (entry for entry in by_armature.values()):
        action = getattr(getattr(armature_object, "animation_data", None), "action", None)
        if action is None:
            continue
        for fcurve in list(action.fcurves):
            data_path = fcurve.data_path
            if not data_path.startswith('pose.bones["'):
                continue
            try:
                bone_name = data_path.split('"', 2)[1]
            except IndexError:
                continue
            if bone_name not in bone_names:
                continue
            if not (
                data_path.endswith(".location")
                or data_path.endswith(".rotation_quaternion")
            ):
                continue
            action.fcurves.remove(fcurve)
            removed += 1
        armature_object.update_tag()
    return removed


def _build_runtime_from_scene(context, report=None) -> bool:
    scene = context.scene
    set_detailed_native_debug_enabled(getattr(scene, "hocloth_debug_detailed_native", False))
    stop_live_runtime(scene, "Live runtime stopped")
    screen = context.screen
    if screen is not None and getattr(screen, "is_animation_playing", False):
        bpy.ops.screen.animation_cancel(restore_frame=False)

    authoring_snapshot = _current_authoring_snapshot(scene)
    payload = authoring_snapshot.get("payload", {})
    bone_chains = payload.get("bone_chains", [])
    bone_count = sum(len(chain.get("bones", [])) for chain in bone_chains)
    if not bone_chains:
        if report is not None:
            report({"ERROR"}, "No enabled MagicaCloth BoneCloth/BoneSpring components found.")
        scene.hocloth_runtime_status = "Build failed: no MagicaCloth components"
        return False
    if bone_count == 0:
        if report is not None:
            report({"ERROR"}, "MagicaCloth components resolved 0 bones.")
        scene.hocloth_runtime_status = "Build failed: resolved 0 bones"
        return False

    runtime_state = build_runtime(authoring_snapshot, True)
    reset_runtime_input_tracking()
    initial_inputs = build_runtime_inputs(scene, authoring_snapshot)
    set_runtime_inputs_only(initial_inputs)
    set_pose_baseline(capture_pose_baseline(scene, authoring_snapshot))
    scene.hocloth_runtime_handle = runtime_state["handle"]
    scene.hocloth_runtime_backend = runtime_state.get("backend", "unknown")
    scene.hocloth_runtime_step_count = runtime_state["step_count"]
    scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
    scene.hocloth_runtime_non_identity_transform_count = runtime_state.get("non_identity_transform_count", 0)
    scene.hocloth_runtime_max_rotation_degrees = runtime_state.get("max_rotation_degrees", 0.0)
    scene.hocloth_runtime_max_translation = runtime_state.get("max_translation", 0.0)
    scene.hocloth_runtime_write_mode_summary = runtime_state.get("write_mode_summary", "")
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
        mc2.sync_joint_override_names(cloth, extracted.bone_names)
        context.scene.hocloth_runtime_status = f"Added BoneSpring with {len(extracted.bone_names)} bones"
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
        reference = cloth.root_bone_references.add()
        reference.bone_name = extracted.root_bone_name
        cloth.spring_constraint.use_spring = False
        mc2.sync_joint_override_names(cloth, extracted.bone_names)
        context.scene.hocloth_runtime_status = f"Added BoneCloth with {len(extracted.bone_names)} bones"
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
        context.scene.hocloth_runtime_status = f"Added {collider.collider_type} collider from {active_object.name}"
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

        from ..runtime.blender_bone_refs import resolve_bone_forest_names

        bone_names = resolve_bone_forest_names(
            context.scene,
            cloth.armature_object,
            mc2.resolve_cloth_root_bone_names(cloth),
        )
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


class HOCLOTH_OT_add_collider_reference(bpy.types.Operator):
    bl_idname = "hocloth.add_collider_reference"
    bl_label = "Add Collider Reference"
    bl_description = "Add a MagicaCollider object reference to this MagicaCloth component"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None:
            self.report({"ERROR"}, "MagicaCloth component was not found.")
            return {"CANCELLED"}
        reference = cloth.collider_references.add()
        cloth.collider_reference_index = max(len(cloth.collider_references) - 1, 0)
        active_object = context.object
        if mc2.find_collider_by_object(context.scene, active_object) is not None:
            reference.collider_object = active_object
        context.scene.hocloth_runtime_status = "Added collider reference"
        return {"FINISHED"}


class HOCLOTH_OT_remove_collider_reference(bpy.types.Operator):
    bl_idname = "hocloth.remove_collider_reference"
    bl_label = "Remove Collider Reference"
    bl_description = "Remove a MagicaCollider object reference from this MagicaCloth component"

    component_id: bpy.props.StringProperty(name="Component ID")
    index: bpy.props.IntProperty(name="Index", default=-1)

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None:
            self.report({"ERROR"}, "MagicaCloth component was not found.")
            return {"CANCELLED"}
        index = self.index if self.index >= 0 else cloth.collider_reference_index
        if index < 0 or index >= len(cloth.collider_references):
            self.report({"ERROR"}, "Collider reference was not found.")
            return {"CANCELLED"}
        cloth.collider_references.remove(index)
        cloth.collider_reference_index = min(cloth.collider_reference_index, max(len(cloth.collider_references) - 1, 0))
        context.scene.hocloth_runtime_status = "Removed collider reference"
        return {"FINISHED"}


class HOCLOTH_OT_add_root_bone_reference(bpy.types.Operator):
    bl_idname = "hocloth.add_root_bone_reference"
    bl_label = "Add Root Bone"
    bl_description = "Add a root bone to this MC2 BoneCloth component"

    component_id: bpy.props.StringProperty(name="Component ID")

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None:
            self.report({"ERROR"}, "MagicaCloth component was not found.")
            return {"CANCELLED"}
        bone_names: list[str] = []
        if context.selected_pose_bones:
            bone_names = [bone.name for bone in context.selected_pose_bones]
        elif context.selected_bones:
            bone_names = [bone.name for bone in context.selected_bones]
        elif context.active_pose_bone is not None:
            bone_names = [context.active_pose_bone.name]
        elif context.active_bone is not None:
            bone_names = [context.active_bone.name]

        existing = {reference.bone_name for reference in cloth.root_bone_references if reference.bone_name}
        added = 0
        for bone_name in bone_names:
            if not bone_name or bone_name in existing:
                continue
            reference = cloth.root_bone_references.add()
            reference.bone_name = bone_name
            existing.add(bone_name)
            added += 1
        if len(cloth.root_bone_references) == 1:
            cloth.root_bone_name = cloth.root_bone_references[0].bone_name
        cloth.root_bone_reference_index = max(len(cloth.root_bone_references) - 1, 0)
        context.scene.hocloth_runtime_status = f"Added {added} root bone references"
        return {"FINISHED"}


class HOCLOTH_OT_remove_root_bone_reference(bpy.types.Operator):
    bl_idname = "hocloth.remove_root_bone_reference"
    bl_label = "Remove Root Bone"
    bl_description = "Remove a root bone from this MC2 BoneCloth component"

    component_id: bpy.props.StringProperty(name="Component ID")
    index: bpy.props.IntProperty(name="Index", default=-1)

    def execute(self, context):
        cloth = _find_cloth(context.scene, self.component_id)
        if cloth is None:
            self.report({"ERROR"}, "MagicaCloth component was not found.")
            return {"CANCELLED"}
        index = self.index if self.index >= 0 else cloth.root_bone_reference_index
        if index < 0 or index >= len(cloth.root_bone_references):
            self.report({"ERROR"}, "Root bone reference was not found.")
            return {"CANCELLED"}
        cloth.root_bone_references.remove(index)
        cloth.root_bone_reference_index = min(
            cloth.root_bone_reference_index,
            max(len(cloth.root_bone_references) - 1, 0),
        )
        context.scene.hocloth_runtime_status = "Removed root bone reference"
        return {"FINISHED"}


class HOCLOTH_OT_rebuild_scene(bpy.types.Operator):
    bl_idname = "hocloth.rebuild_scene"
    bl_label = "Build Runtime"
    bl_description = "Build the native runtime from MC2 authoring data"

    def execute(self, context):
        if not _build_runtime_from_scene(context, self.report):
            return {"CANCELLED"}
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

        authoring_snapshot = get_last_authoring_snapshot()
        runtime_state = reset_runtime()
        if runtime_state["handle"]:
            reset_runtime_input_tracking()
            if authoring_snapshot is not None:
                set_runtime_inputs_only(build_runtime_inputs(context.scene, authoring_snapshot))
            set_pose_baseline(capture_pose_baseline(context.scene, authoring_snapshot))
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        context.scene.hocloth_runtime_non_identity_transform_count = runtime_state.get("non_identity_transform_count", 0)
        context.scene.hocloth_runtime_max_rotation_degrees = runtime_state.get("max_rotation_degrees", 0.0)
        context.scene.hocloth_runtime_max_translation = runtime_state.get("max_translation", 0.0)
        context.scene.hocloth_runtime_write_mode_summary = runtime_state.get("write_mode_summary", "")
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

        authoring_snapshot = get_last_authoring_snapshot()
        pose_state = get_pose_baseline()
        target_frame = scene.frame_start
        scene.frame_set(target_frame)
        if pose_state:
            restore_pose_state(scene, authoring_snapshot, pose_state)
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
        set_detailed_native_debug_enabled(
            getattr(context.scene, "hocloth_debug_detailed_native", False)
        )
        if not context.scene.hocloth_runtime_handle:
            if not _build_runtime_from_scene(context, self.report):
                return {"CANCELLED"}

        authoring_snapshot = get_last_authoring_snapshot()
        source_pose = capture_pose_state(context.scene, authoring_snapshot)
        try:
            result = step_runtime(
                context.scene.hocloth_runtime_dt,
                context.scene.hocloth_simulation_frequency,
                build_runtime_inputs(context.scene, authoring_snapshot),
            )
        except Exception as exc:
            context.scene.hocloth_runtime_status = f"Step failed: {exc}"
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}

        runtime_state = result["runtime_state"]
        context.scene.hocloth_runtime_step_count = runtime_state["step_count"]
        context.scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        context.scene.hocloth_runtime_non_identity_transform_count = runtime_state.get("non_identity_transform_count", 0)
        context.scene.hocloth_runtime_max_rotation_degrees = runtime_state.get("max_rotation_degrees", 0.0)
        context.scene.hocloth_runtime_max_translation = runtime_state.get("max_translation", 0.0)
        context.scene.hocloth_runtime_write_mode_summary = runtime_state.get("write_mode_summary", "")
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
                result["transforms"],
                source_pose,
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
            f"transforms={runtime_state['bone_transform_count']}, "
            f"non_identity={runtime_state.get('non_identity_transform_count', 0)}, "
            f"max_rot={runtime_state.get('max_rotation_degrees', 0.0):.3f}"
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
        transforms = get_last_transforms()
        if not transforms:
            self.report({"INFO"}, "No runtime transforms are available yet.")
            return {"CANCELLED"}

        apply_result = apply_runtime_transforms_to_scene(
            context.scene,
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


class HOCLOTH_OT_bake_runtime_action(bpy.types.Operator):
    bl_idname = "hocloth.bake_runtime_action"
    bl_label = "Bake Runtime Action"
    bl_description = "Bake HoCloth runtime output to keyframes on the involved pose bones"

    _timer = None
    _authoring_snapshot = None
    _pose_baseline = None
    _frame = 0
    _end_frame = 0
    _baked_frames = 0
    _keyed_bones = 0
    _original_apply_on_step = True

    def _finish(self, context, status: str, cancelled: bool = False):
        wm = context.window_manager
        if self._timer is not None:
            wm.event_timer_remove(self._timer)
            self._timer = None
        context.scene.hocloth_apply_pose_on_step = self._original_apply_on_step
        context.scene.hocloth_runtime_status = status
        self.report({"WARNING"} if cancelled else {"INFO"}, status)
        return {"CANCELLED"} if cancelled else {"FINISHED"}

    def _bake_one_frame(self, context):
        scene = context.scene
        scene.frame_set(self._frame)
        if context.view_layer is not None:
            context.view_layer.update()

        source_pose = capture_pose_state(scene, self._authoring_snapshot)
        runtime_inputs = build_runtime_inputs(scene, self._authoring_snapshot)
        result = step_runtime(
            scene.hocloth_runtime_dt,
            scene.hocloth_simulation_frequency,
            runtime_inputs,
        )
        apply_result = apply_runtime_transforms_to_scene(
            scene,
            result["transforms"],
            source_pose or self._pose_baseline,
        )
        if context.view_layer is not None:
            context.view_layer.update()

        self._keyed_bones += _keyframe_snapshot_pose_bones(
            scene,
            self._authoring_snapshot,
            self._frame,
        )
        self._baked_frames += 1

        runtime_state = result["runtime_state"]
        scene.hocloth_runtime_step_count = runtime_state["step_count"]
        scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
        scene.hocloth_runtime_non_identity_transform_count = runtime_state.get("non_identity_transform_count", 0)
        scene.hocloth_runtime_max_rotation_degrees = runtime_state.get("max_rotation_degrees", 0.0)
        scene.hocloth_runtime_max_translation = runtime_state.get("max_translation", 0.0)
        scene.hocloth_runtime_write_mode_summary = runtime_state.get("write_mode_summary", "")
        scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)
        scene.hocloth_runtime_applied_count = apply_result["applied_count"]
        scene.hocloth_runtime_missing_bone_count = apply_result["missing_bone_count"]
        scene.hocloth_runtime_missing_armature_count = apply_result["missing_armature_count"]
        scene.hocloth_runtime_apply_armature_count = apply_result["armature_count"]
        scene.hocloth_runtime_status = (
            f"Baking HoCloth frame {self._frame}/{self._end_frame}, "
            f"frames={self._baked_frames}, key ops={self._keyed_bones}"
        )
        self._frame += 1

    def modal(self, context, event):
        if event.type == "ESC":
            return self._finish(
                context,
                f"HoCloth bake cancelled at frame {self._frame}",
                cancelled=True,
            )
        if event.type != "TIMER":
            return {"PASS_THROUGH"}

        if self._frame > self._end_frame:
            return self._finish(
                context,
                f"Baked HoCloth action: frames={self._baked_frames}, key ops={self._keyed_bones}",
            )

        try:
            self._bake_one_frame(context)
        except Exception as exc:
            return self._finish(
                context,
                f"Bake failed at frame {self._frame}: {exc}",
                cancelled=True,
            )
        return {"RUNNING_MODAL"}

    def execute(self, context):
        scene = context.scene
        stop_live_runtime(scene, "Live runtime stopped")
        screen = context.screen
        if screen is not None and getattr(screen, "is_animation_playing", False):
            bpy.ops.screen.animation_cancel(restore_frame=False)

        start_frame = int(scene.frame_start)
        end_frame = int(scene.frame_end)
        if end_frame < start_frame:
            self.report({"ERROR"}, "Scene frame range is invalid.")
            return {"CANCELLED"}

        scene.frame_set(start_frame)
        if context.view_layer is not None:
            context.view_layer.update()
        if not _build_runtime_from_scene(context, self.report):
            return {"CANCELLED"}

        self._authoring_snapshot = get_last_authoring_snapshot()
        self._pose_baseline = get_pose_baseline()
        self._frame = start_frame
        self._end_frame = end_frame
        self._baked_frames = 0
        self._keyed_bones = 0
        self._original_apply_on_step = scene.hocloth_apply_pose_on_step
        scene.hocloth_apply_pose_on_step = True
        scene.hocloth_runtime_status = (
            f"HoCloth bake started: frames {start_frame}-{end_frame}"
        )

        wm = context.window_manager
        self._timer = wm.event_timer_add(0.001, window=context.window)
        wm.modal_handler_add(self)
        return {"RUNNING_MODAL"}


class HOCLOTH_OT_clear_baked_action(bpy.types.Operator):
    bl_idname = "hocloth.clear_baked_action"
    bl_label = "Clear Baked Action"
    bl_description = "Remove HoCloth keyframes from involved pose bones and return to the first frame"

    def execute(self, context):
        scene = context.scene
        stop_live_runtime(scene, "Live runtime stopped")
        screen = context.screen
        if screen is not None and getattr(screen, "is_animation_playing", False):
            bpy.ops.screen.animation_cancel(restore_frame=False)

        authoring_snapshot = get_last_authoring_snapshot() or _current_authoring_snapshot(scene)
        removed = _clear_snapshot_pose_bone_bake(scene, authoring_snapshot)
        pose_state = get_pose_baseline()
        scene.frame_set(scene.frame_start)
        if pose_state:
            restore_pose_state(scene, authoring_snapshot, pose_state)
        reset_runtime()
        if context.view_layer is not None:
            context.view_layer.update()
        scene.hocloth_runtime_step_count = 0
        scene.hocloth_runtime_transform_count = 0
        scene.hocloth_runtime_applied_count = 0
        scene.hocloth_runtime_last_fixed_steps = 0
        scene.hocloth_runtime_status = (
            f"Cleared HoCloth bake: removed {removed} f-curves, returned to frame {scene.frame_start}"
        )
        self.report({"INFO"}, scene.hocloth_runtime_status)
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
    HOCLOTH_OT_remove_component,
    HOCLOTH_OT_apply_spring_bone_preset,
    HOCLOTH_OT_sync_spring_bone_joints,
    HOCLOTH_OT_reset_spring_joint_override,
    HOCLOTH_OT_add_collider_reference,
    HOCLOTH_OT_remove_collider_reference,
    HOCLOTH_OT_add_root_bone_reference,
    HOCLOTH_OT_remove_root_bone_reference,
    HOCLOTH_OT_rebuild_scene,
    HOCLOTH_OT_reset_runtime,
    HOCLOTH_OT_restart_runtime_from_baseline,
    HOCLOTH_OT_step_runtime,
    HOCLOTH_OT_toggle_live_runtime,
    HOCLOTH_OT_apply_runtime_pose,
    HOCLOTH_OT_apply_runtime_mesh_output,
    HOCLOTH_OT_bake_runtime_action,
    HOCLOTH_OT_clear_baked_action,
    HOCLOTH_OT_destroy_runtime,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
