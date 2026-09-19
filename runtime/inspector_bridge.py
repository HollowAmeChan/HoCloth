from __future__ import annotations

import os
import tempfile
import time
from typing import Iterable

import bpy

from ..components import mc2
from .authoring_snapshot import build_authoring_snapshot
from .inputs import build_runtime_inputs, reset_runtime_input_tracking
from .live import start_live_runtime, stop_live_runtime
from .pose_apply import (
    apply_runtime_mesh_outputs_to_scene,
    apply_runtime_transforms_to_scene,
    capture_pose_baseline,
    capture_pose_state,
)
from .session import (
    build_runtime,
    get_last_authoring_snapshot,
    reset_runtime,
    set_detailed_native_debug_enabled,
    set_pose_baseline,
    set_runtime_inputs_only,
    step_runtime,
)


_TIMER_REGISTERED = False
_COMMAND_POLL_INTERVAL_SECONDS = 0.20
_STATE_WRITE_INTERVAL_SECONDS = 0.20
_LAST_STATE_WRITE_TIME = 0.0
_STATE_TEXT_NAME = "HoCloth_InspectorState"
_COMMANDS_TEXT_NAME = "HoCloth_InspectorCommands"


def _current_scene():
    context = getattr(bpy, "context", None)
    if context is None:
        return None
    return getattr(context, "scene", None)


def _plugin_root() -> str:
    return os.path.dirname(os.path.dirname(__file__))


def _bridge_root() -> str:
    return os.path.join(_plugin_root(), "_bin", "inspector_bridge")


def _state_path() -> str:
    return os.path.join(_bridge_root(), "state.txt")


def _commands_dir() -> str:
    return os.path.join(_bridge_root(), "commands")


def _ensure_bridge_dirs() -> None:
    os.makedirs(_bridge_root(), exist_ok=True)
    os.makedirs(_commands_dir(), exist_ok=True)


def _get_or_create_text_block(name: str):
    text_block = bpy.data.texts.get(name)
    if text_block is None:
        text_block = bpy.data.texts.new(name)
    return text_block


def _replace_text_block_content(name: str, content: str) -> None:
    text_block = _get_or_create_text_block(name)
    if text_block.as_string() == content:
        return
    text_block.clear()
    if content:
        text_block.write(content)


def _read_text_block_content(name: str) -> str:
    text_block = bpy.data.texts.get(name)
    if text_block is None:
        return ""
    return text_block.as_string()


def _sanitize_text(value) -> str:
    text = str(value or "")
    return text.replace("\t", " ").replace("\r", " ").replace("\n", " ")


def _bool_text(value: bool) -> str:
    return "1" if bool(value) else "0"


def _float_text(value) -> str:
    return f"{float(value):.6f}"


def _join_list(values: Iterable[str]) -> str:
    return ";".join(_sanitize_text(value) for value in values if value)


def _write_atomic_text(path: str, lines: list[str]) -> None:
    directory = os.path.dirname(path)
    os.makedirs(directory, exist_ok=True)
    handle = None
    temp_path = ""
    try:
        handle, temp_path = tempfile.mkstemp(prefix="hocloth_inspector_", suffix=".tmp", dir=directory, text=True)
        with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as stream:
            handle = None
            stream.write("\n".join(lines))
            stream.write("\n")
        os.replace(temp_path, path)
    finally:
        if handle is not None:
            os.close(handle)
        if temp_path and os.path.exists(temp_path):
            try:
                os.remove(temp_path)
            except OSError:
                pass


def _build_text_payload(lines: list[str]) -> str:
    return "\n".join(lines) + "\n"


def _iter_components(scene: bpy.types.Scene):
    for item in scene.hocloth_mc2_components:
        if item.component_type == "MAGICA_CLOTH":
            typed = mc2.find_magica_cloth(scene, item.component_id)
            if typed is not None:
                yield item, typed
        elif item.component_type in {"SPHERE_COLLIDER", "CAPSULE_COLLIDER", "PLANE_COLLIDER"}:
            typed = mc2.find_collider(scene, item.component_id)
            if typed is not None:
                yield item, typed
        elif item.component_type == "CACHE_OUTPUT":
            typed = mc2.find_cache_output(scene, item.component_id)
            if typed is not None:
                yield item, typed


def _curve_line(component_id: str, path: str, parameter) -> str:
    return "\t".join(
        (
            "curve",
            component_id,
            path,
            _float_text(parameter.value),
            _bool_text(parameter.use_curve),
        )
    )


def _curve_point_line(component_id: str, path: str, index: int, point) -> str:
    return "\t".join(
        (
            "curve_point",
            component_id,
            path,
            str(int(index)),
            _float_text(point.time),
            _float_text(point.value),
        )
    )


def _append_curve_lines(lines: list[str], component_id: str, path: str, parameter) -> None:
    lines.append(_curve_line(component_id, path, parameter))
    mc2.ensure_curve_control_points(parameter)
    mc2.normalize_curve_control_points(parameter)
    for index, point in enumerate(parameter.control_points):
        lines.append(_curve_point_line(component_id, path, index, point))


def _float_line(component_id: str, path: str, value) -> str:
    return "\t".join(("float", component_id, path, _float_text(value)))


def _bool_line(component_id: str, path: str, value: bool) -> str:
    return "\t".join(("bool", component_id, path, _bool_text(value)))


def _enum_line(component_id: str, path: str, value: str) -> str:
    return "\t".join(("enum", component_id, path, _sanitize_text(value)))


def _text_line(component_id: str, path: str, value: str) -> str:
    return "\t".join(("text", component_id, path, _sanitize_text(value)))


def _list_line(component_id: str, path: str, values: Iterable[str]) -> str:
    return "\t".join(("list", component_id, path, _join_list(values)))


def _scene_runtime_lines(scene: bpy.types.Scene) -> list[str]:
    return [
        "\t".join(("runtime", "status", _sanitize_text(scene.hocloth_runtime_status))),
        "\t".join(("runtime", "backend", _sanitize_text(scene.hocloth_runtime_backend))),
        "\t".join(("runtime", "step_count", str(int(scene.hocloth_runtime_step_count)))),
        "\t".join(("runtime", "transform_count", str(int(scene.hocloth_runtime_transform_count)))),
        "\t".join(("runtime", "apply_pose_on_step", _bool_text(scene.hocloth_apply_pose_on_step))),
        "\t".join(("runtime", "dt", _float_text(scene.hocloth_runtime_dt))),
        "\t".join(("runtime", "simulation_frequency", str(int(scene.hocloth_simulation_frequency)))),
        "\t".join(("runtime", "live_running", _bool_text(scene.hocloth_runtime_live_running))),
    ]


def _export_state_lines(scene: bpy.types.Scene) -> list[str]:
    lines = [
        "format\t1",
        "\t".join(("scene", "name", _sanitize_text(scene.name))),
        *_scene_runtime_lines(scene),
    ]

    for item, typed in _iter_components(scene):
        if item.component_type == "MAGICA_CLOTH":
            armature_name = typed.armature_object.name if typed.armature_object is not None else ""
            root_bones = mc2.resolve_cloth_root_bone_names(typed)
            lines.append(
                "\t".join(
                    (
                        "component",
                        item.component_type,
                        item.component_id,
                        _sanitize_text(item.display_name),
                        _sanitize_text(armature_name),
                        _bool_text(item.enabled),
                    )
                )
            )
            lines.append(_enum_line(item.component_id, "authoring_mode", typed.authoring_mode))
            lines.append(_enum_line(item.component_id, "preset_profile", typed.preset_profile))
            lines.append(_enum_line(item.component_id, "bone_connection_mode", typed.bone_connection_mode))
            lines.append(_enum_line(item.component_id, "center_source", typed.center_source))
            lines.append(_list_line(item.component_id, "root_bones", root_bones))
            _append_curve_lines(lines, item.component_id, "radius_curve", typed.radius_curve)
            _append_curve_lines(lines, item.component_id, "damping_curve", typed.damping_curve)
            lines.append(_float_line(item.component_id, "gravity_strength", typed.gravity_strength))
            _append_curve_lines(
                lines,
                item.component_id,
                "distance_constraint.stiffness",
                typed.distance_constraint.stiffness,
            )
            lines.append(_float_line(item.component_id, "tether_constraint.distance_compression", typed.tether_constraint.distance_compression))
            lines.append(_float_line(item.component_id, "triangle_bending_constraint.stiffness", typed.triangle_bending_constraint.stiffness))
            lines.append(_bool_line(item.component_id, "spring_constraint.use_spring", typed.spring_constraint.use_spring))
            lines.append(_float_line(item.component_id, "spring_constraint.spring_power", typed.spring_constraint.spring_power))
            lines.append(_float_line(item.component_id, "spring_constraint.limit_distance", typed.spring_constraint.limit_distance))
            lines.append(_float_line(item.component_id, "spring_constraint.normal_limit_ratio", typed.spring_constraint.normal_limit_ratio))
            lines.append(_float_line(item.component_id, "spring_constraint.spring_noise", typed.spring_constraint.spring_noise))
            lines.append(_bool_line(item.component_id, "collider_collision_constraint.enabled", typed.collider_collision_constraint.enabled))
            lines.append(_enum_line(item.component_id, "collider_collision_constraint.mode", typed.collider_collision_constraint.mode))
            lines.append(_float_line(item.component_id, "collider_collision_constraint.friction", typed.collider_collision_constraint.friction))
            _append_curve_lines(
                lines,
                item.component_id,
                "collider_collision_constraint.limit_distance",
                typed.collider_collision_constraint.limit_distance,
            )
            lines.append(_float_line(item.component_id, "inertia_constraint.world_inertia", typed.inertia_constraint.world_inertia))
            lines.append(_float_line(item.component_id, "inertia_constraint.movement_inertia_smoothing", typed.inertia_constraint.movement_inertia_smoothing))
            lines.append(_bool_line(item.component_id, "inertia_constraint.movement_speed_limit.use", typed.inertia_constraint.movement_speed_limit.use))
            lines.append(_float_line(item.component_id, "inertia_constraint.movement_speed_limit.value", typed.inertia_constraint.movement_speed_limit.value))
            lines.append(_bool_line(item.component_id, "inertia_constraint.rotation_speed_limit.use", typed.inertia_constraint.rotation_speed_limit.use))
            lines.append(_float_line(item.component_id, "inertia_constraint.rotation_speed_limit.value", typed.inertia_constraint.rotation_speed_limit.value))
            lines.append(_float_line(item.component_id, "inertia_constraint.local_inertia", typed.inertia_constraint.local_inertia))
            lines.append(_bool_line(item.component_id, "inertia_constraint.local_movement_speed_limit.use", typed.inertia_constraint.local_movement_speed_limit.use))
            lines.append(_float_line(item.component_id, "inertia_constraint.local_movement_speed_limit.value", typed.inertia_constraint.local_movement_speed_limit.value))
            lines.append(_bool_line(item.component_id, "inertia_constraint.local_rotation_speed_limit.use", typed.inertia_constraint.local_rotation_speed_limit.use))
            lines.append(_float_line(item.component_id, "inertia_constraint.local_rotation_speed_limit.value", typed.inertia_constraint.local_rotation_speed_limit.value))
            lines.append(_float_line(item.component_id, "inertia_constraint.depth_inertia", typed.inertia_constraint.depth_inertia))
            lines.append(_float_line(item.component_id, "inertia_constraint.centrifugal_acceleration", typed.inertia_constraint.centrifugal_acceleration))
            lines.append(_bool_line(item.component_id, "inertia_constraint.particle_speed_limit.use", typed.inertia_constraint.particle_speed_limit.use))
            lines.append(_float_line(item.component_id, "inertia_constraint.particle_speed_limit.value", typed.inertia_constraint.particle_speed_limit.value))
            lines.append(_bool_line(item.component_id, "angle_restoration_constraint.use_angle_restoration", typed.angle_restoration_constraint.use_angle_restoration))
            _append_curve_lines(
                lines,
                item.component_id,
                "angle_restoration_constraint.stiffness",
                typed.angle_restoration_constraint.stiffness,
            )
            lines.append(_float_line(item.component_id, "angle_restoration_constraint.velocity_attenuation", typed.angle_restoration_constraint.velocity_attenuation))
            lines.append(_float_line(item.component_id, "angle_restoration_constraint.gravity_falloff", typed.angle_restoration_constraint.gravity_falloff))
            lines.append(_bool_line(item.component_id, "angle_limit_constraint.use_angle_limit", typed.angle_limit_constraint.use_angle_limit))
            _append_curve_lines(
                lines,
                item.component_id,
                "angle_limit_constraint.limit_angle",
                typed.angle_limit_constraint.limit_angle,
            )
            lines.append(_float_line(item.component_id, "angle_limit_constraint.stiffness", typed.angle_limit_constraint.stiffness))
            lines.append(
                _list_line(
                    item.component_id,
                    "collider_refs",
                    [
                        reference.collider_object.name
                        for reference in typed.collider_references
                        if reference.collider_object is not None
                    ],
                )
            )
        elif item.component_type in {"SPHERE_COLLIDER", "CAPSULE_COLLIDER", "PLANE_COLLIDER"}:
            object_name = typed.collider_object.name if typed.collider_object is not None else ""
            lines.append(
                "\t".join(
                    (
                        "component",
                        item.component_type,
                        item.component_id,
                        _sanitize_text(item.display_name),
                        _sanitize_text(object_name),
                        _bool_text(item.enabled),
                    )
                )
            )
            lines.append(_enum_line(item.component_id, "collider_type", typed.collider_type))
            lines.append(_float_line(item.component_id, "radius", typed.radius))
            lines.append(_float_line(item.component_id, "end_radius", typed.end_radius))
            lines.append(_float_line(item.component_id, "length", typed.length))
            lines.append(_bool_line(item.component_id, "radius_separation", typed.radius_separation))
            lines.append(_enum_line(item.component_id, "direction", typed.direction))
        elif item.component_type == "CACHE_OUTPUT":
            object_name = typed.source_object.name if typed.source_object is not None else ""
            lines.append(
                "\t".join(
                    (
                        "component",
                        item.component_type,
                        item.component_id,
                        _sanitize_text(item.display_name),
                        _sanitize_text(object_name),
                        _bool_text(item.enabled),
                    )
                )
            )
            lines.append(_enum_line(item.component_id, "cache_format", typed.cache_format))
            lines.append(_text_line(item.component_id, "cache_path", typed.cache_path))

    return lines


def export_state(scene: bpy.types.Scene | None = None) -> None:
    target_scene = scene or _current_scene()
    if target_scene is None:
        return
    _ensure_bridge_dirs()
    lines = _export_state_lines(target_scene)
    payload = _build_text_payload(lines)
    _replace_text_block_content(_STATE_TEXT_NAME, payload)
    _write_atomic_text(_state_path(), lines)


def _resolve_component(scene: bpy.types.Scene, component_id: str):
    item = next((entry for entry in scene.hocloth_mc2_components if entry.component_id == component_id), None)
    if item is None:
        return None, None
    if item.component_type == "MAGICA_CLOTH":
        return item, mc2.find_magica_cloth(scene, component_id)
    if item.component_type in {"SPHERE_COLLIDER", "CAPSULE_COLLIDER", "PLANE_COLLIDER"}:
        return item, mc2.find_collider(scene, component_id)
    if item.component_type == "CACHE_OUTPUT":
        return item, mc2.find_cache_output(scene, component_id)
    return item, None


def _parse_bool_token(value: str) -> bool:
    return value.strip().lower() in {"1", "true", "yes", "on"}


def _apply_scene_field(scene: bpy.types.Scene, path: str, raw_value: str) -> bool:
    if path == "apply_pose_on_step":
        scene.hocloth_apply_pose_on_step = _parse_bool_token(raw_value)
        return True
    if path == "dt":
        scene.hocloth_runtime_dt = float(raw_value)
        return True
    if path == "simulation_frequency":
        scene.hocloth_simulation_frequency = int(raw_value)
        return True
    return False


def _apply_component_field(scene: bpy.types.Scene, component_id: str, path: str, raw_value: str) -> bool:
    item, typed = _resolve_component(scene, component_id)
    if item is None or typed is None:
        return False

    if path == "enabled":
        item.enabled = _parse_bool_token(raw_value)
        return True

    if item.component_type == "MAGICA_CLOTH" and path == "preset_profile":
        typed.preset_profile = raw_value
        mc2.apply_preset(typed, typed.preset_profile)
        return True

    target = typed
    parts = path.split(".")
    for part in parts[:-1]:
        target = getattr(target, part, None)
        if target is None:
            return False

    attribute_name = parts[-1]
    if not hasattr(target, attribute_name):
        return False

    current_value = getattr(target, attribute_name)
    if isinstance(current_value, bool):
        setattr(target, attribute_name, _parse_bool_token(raw_value))
    elif isinstance(current_value, int) and not isinstance(current_value, bool):
        setattr(target, attribute_name, int(raw_value))
    elif isinstance(current_value, float):
        setattr(target, attribute_name, float(raw_value))
    else:
        setattr(target, attribute_name, raw_value)
    return True


def _delete_component(scene: bpy.types.Scene, component_id: str) -> bool:
    if not component_id:
        return False
    removed = mc2.delete_component(scene, component_id)
    if removed:
        scene.hocloth_runtime_status = f"Component removed: {component_id}"
    return removed


def _resolve_curve_parameter(scene: bpy.types.Scene, component_id: str, path: str):
    _item, typed = _resolve_component(scene, component_id)
    if typed is None:
        return None
    try:
        parameter = mc2.resolve_curve_parameter(typed, path)
    except Exception:
        return None
    if parameter is None or not hasattr(parameter, "control_points"):
        return None
    return parameter


def _apply_curve_point_set(
    scene: bpy.types.Scene,
    component_id: str,
    path: str,
    index: int,
    time_value: str,
    raw_value: str,
) -> bool:
    parameter = _resolve_curve_parameter(scene, component_id, path)
    if parameter is None:
        return False
    mc2.ensure_curve_control_points(parameter)
    if index < 0 or index >= len(parameter.control_points):
        return False
    point = parameter.control_points[index]
    point.time = float(time_value)
    point.value = float(raw_value)
    mc2.sync_curve_parameter_samples(parameter)
    return True


def _apply_curve_point_add(
    scene: bpy.types.Scene,
    component_id: str,
    path: str,
    time_value: str,
    raw_value: str,
) -> bool:
    parameter = _resolve_curve_parameter(scene, component_id, path)
    if parameter is None:
        return False
    mc2.add_curve_control_point(parameter, float(time_value), float(raw_value))
    return True


def _apply_curve_point_remove(
    scene: bpy.types.Scene,
    component_id: str,
    path: str,
    index: int,
) -> bool:
    parameter = _resolve_curve_parameter(scene, component_id, path)
    if parameter is None:
        return False
    mc2.remove_curve_control_point(parameter, index)
    return True


def _build_runtime_from_scene(scene: bpy.types.Scene) -> bool:
    stop_live_runtime(scene, "Live runtime stopped")
    authoring_snapshot = build_authoring_snapshot(scene)
    payload = authoring_snapshot.get("payload", {})
    bone_chains = payload.get("bone_chains", [])
    bone_count = sum(len(chain.get("bones", [])) for chain in bone_chains)
    if not bone_chains or bone_count == 0:
        scene.hocloth_runtime_status = "Build failed: no valid MagicaCloth chains"
        return False

    set_detailed_native_debug_enabled(getattr(scene, "hocloth_debug_detailed_native", False))
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
    scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)
    scene.hocloth_runtime_status = f"Runtime ready via {runtime_state['backend']}: {runtime_state['summary']}"
    return True


def _step_runtime_from_scene(scene: bpy.types.Scene) -> bool:
    set_detailed_native_debug_enabled(getattr(scene, "hocloth_debug_detailed_native", False))
    if not scene.hocloth_runtime_handle:
        if not _build_runtime_from_scene(scene):
            return False

    authoring_snapshot = get_last_authoring_snapshot()
    if authoring_snapshot is None:
        if not _build_runtime_from_scene(scene):
            return False
        authoring_snapshot = get_last_authoring_snapshot()
        if authoring_snapshot is None:
            return False

    source_pose = capture_pose_state(scene, authoring_snapshot)
    result = step_runtime(
        scene.hocloth_runtime_dt,
        scene.hocloth_simulation_frequency,
        build_runtime_inputs(scene, authoring_snapshot),
    )
    runtime_state = result["runtime_state"]
    scene.hocloth_runtime_step_count = runtime_state["step_count"]
    scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]
    scene.hocloth_runtime_non_identity_transform_count = runtime_state.get("non_identity_transform_count", 0)
    scene.hocloth_runtime_max_rotation_degrees = runtime_state.get("max_rotation_degrees", 0.0)
    scene.hocloth_runtime_max_translation = runtime_state.get("max_translation", 0.0)
    scene.hocloth_runtime_write_mode_summary = runtime_state.get("write_mode_summary", "")
    scene.hocloth_runtime_last_fixed_steps = runtime_state.get("last_executed_steps", 0)

    mesh_outputs = result.get("mesh_outputs", [])
    apply_runtime_mesh_outputs_to_scene(scene, mesh_outputs)
    if scene.hocloth_apply_pose_on_step:
        apply_result = apply_runtime_transforms_to_scene(scene, result["transforms"], source_pose)
        scene.hocloth_runtime_applied_count = apply_result["applied_count"]
        scene.hocloth_runtime_missing_bone_count = apply_result["missing_bone_count"]
        scene.hocloth_runtime_missing_armature_count = apply_result["missing_armature_count"]
        scene.hocloth_runtime_apply_armature_count = apply_result["armature_count"]
        if bpy.context.view_layer is not None:
            bpy.context.view_layer.update()
    scene.hocloth_runtime_status = (
        f"Stepped {runtime_state['step_count']} fixed steps, "
        f"last={runtime_state.get('last_executed_steps', 0)}, "
        f"transforms={runtime_state['bone_transform_count']}"
    )
    return True


def _process_command_line(scene: bpy.types.Scene, line: str) -> bool:
    tokens = [token for token in line.rstrip("\n").split("\t")]
    if not tokens:
        return False

    command_type = tokens[0]
    if command_type == "scene" and len(tokens) >= 3:
        return _apply_scene_field(scene, tokens[1], tokens[2])
    if command_type == "set" and len(tokens) >= 4:
        return _apply_component_field(scene, tokens[1], tokens[2], tokens[3])
    if command_type == "curve_point_set" and len(tokens) >= 6:
        return _apply_curve_point_set(scene, tokens[1], tokens[2], int(tokens[3]), tokens[4], tokens[5])
    if command_type == "curve_point_add" and len(tokens) >= 5:
        return _apply_curve_point_add(scene, tokens[1], tokens[2], tokens[3], tokens[4])
    if command_type == "curve_point_remove" and len(tokens) >= 4:
        return _apply_curve_point_remove(scene, tokens[1], tokens[2], int(tokens[3]))
    if command_type == "delete_component" and len(tokens) >= 2:
        return _delete_component(scene, tokens[1])
    if command_type == "action" and len(tokens) >= 2:
        action = tokens[1]
        if action == "rebuild":
            return _build_runtime_from_scene(scene)
        if action == "step":
            return _step_runtime_from_scene(scene)
        if action == "toggle_live":
            if scene.hocloth_runtime_live_running:
                stop_live_runtime(scene, "Live runtime stopped")
            else:
                if not _build_runtime_from_scene(scene):
                    return False
                start_live_runtime(scene)
            return True
    return False


def _iter_text_block_commands() -> list[str]:
    content = _read_text_block_content(_COMMANDS_TEXT_NAME)
    if not content:
        return []
    return [line.strip() for line in content.splitlines() if line.strip()]


def _clear_text_block_commands() -> None:
    if bpy.data.texts.get(_COMMANDS_TEXT_NAME) is None:
        return
    _replace_text_block_content(_COMMANDS_TEXT_NAME, "")


def poll_commands(scene: bpy.types.Scene | None = None) -> bool:
    target_scene = scene or _current_scene()
    if target_scene is None:
        return False

    applied = False
    text_block_commands = _iter_text_block_commands()
    if text_block_commands:
        for command in text_block_commands:
            applied = _process_command_line(target_scene, command) or applied
        _clear_text_block_commands()

    if not os.path.isdir(_commands_dir()):
        return applied

    for filename in sorted(os.listdir(_commands_dir())):
        command_path = os.path.join(_commands_dir(), filename)
        if not os.path.isfile(command_path):
            continue
        try:
            with open(command_path, "r", encoding="utf-8") as stream:
                for raw_line in stream:
                    stripped = raw_line.strip()
                    if not stripped:
                        continue
                    applied = _process_command_line(target_scene, stripped) or applied
        finally:
            try:
                os.remove(command_path)
            except OSError:
                pass
    return applied


def _timer_tick():
    global _LAST_STATE_WRITE_TIME

    scene = _current_scene()
    if scene is None:
        return _COMMAND_POLL_INTERVAL_SECONDS

    changed = poll_commands(scene)
    now = time.monotonic()
    if changed or (now - _LAST_STATE_WRITE_TIME) >= _STATE_WRITE_INTERVAL_SECONDS:
        export_state(scene)
        _LAST_STATE_WRITE_TIME = now
    return _COMMAND_POLL_INTERVAL_SECONDS


def register():
    global _TIMER_REGISTERED, _LAST_STATE_WRITE_TIME
    _ensure_bridge_dirs()
    export_state(_current_scene())
    _LAST_STATE_WRITE_TIME = time.monotonic()
    if not _TIMER_REGISTERED:
        bpy.app.timers.register(_timer_tick, first_interval=_COMMAND_POLL_INTERVAL_SECONDS, persistent=True)
        _TIMER_REGISTERED = True


def unregister():
    global _TIMER_REGISTERED
    if _TIMER_REGISTERED:
        try:
            bpy.app.timers.unregister(_timer_tick)
        except Exception:
            pass
        _TIMER_REGISTERED = False
