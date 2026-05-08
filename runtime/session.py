from .bridge import load_bridge
from .exchange import (
    empty_build_output,
    empty_frame_inputs,
    frame_inputs_payload,
    wrap_step_output,
)


_runtime_state = {
    "handle": 0,
    "summary": "",
    "backend": "none",
    "step_count": 0,
    "last_dt": 0.0,
    "simulation_frequency": 90,
    "last_executed_steps": 0,
    "bone_transform_count": 0,
    "non_identity_transform_count": 0,
    "max_rotation_degrees": 0.0,
    "max_translation": 0.0,
    "write_mode_summary": "",
    "fixed_step_dt": 1.0 / 90.0,
    "accumulated_time": 0.0,
}
_active_bridge = None
_last_authoring_snapshot = None
_last_transforms = []
_last_mesh_outputs = []
_last_build_output = empty_build_output()
_pose_baseline = {}
_last_debug_lines = []
_detailed_native_debug_enabled = False


def set_detailed_native_debug_enabled(enabled: bool):
    global _detailed_native_debug_enabled
    _detailed_native_debug_enabled = bool(enabled)


def _summarize_transforms(transforms: list[dict]) -> dict:
    import math

    def _fmt_quat(value) -> str:
        try:
            return "(" + "/".join(f"{float(part):.4f}" for part in list(value)[:4]) + ")"
        except Exception:
            return "()"

    def _fmt_vec(value) -> str:
        try:
            return "(" + "/".join(f"{float(part):.4f}" for part in list(value)[:3]) + ")"
        except Exception:
            return "()"

    max_rotation_degrees = 0.0
    max_translation = 0.0
    max_local_rotation_delta_degrees = 0.0
    max_local_write_delta_degrees = 0.0
    max_world_write_delta_degrees = 0.0
    max_fallback_delta_degrees = 0.0
    worst_local_write = None
    worst_world_write = None
    worst_fallback = None
    non_identity_count = 0
    write_modes = {}
    for transform in transforms or []:
        rotation = transform.get("rotation_quaternion", (1.0, 0.0, 0.0, 0.0))
        if len(rotation) >= 4:
            w = max(-1.0, min(1.0, abs(float(rotation[0]))))
            angle = math.degrees(2.0 * math.acos(w))
            if angle > 180.0:
                angle = 360.0 - angle
            max_rotation_degrees = max(max_rotation_degrees, angle)
            if angle > 0.01:
                non_identity_count += 1
        translation = transform.get("translation", (0.0, 0.0, 0.0))
        if len(translation) >= 3:
            length = math.sqrt(sum(float(value) * float(value) for value in translation[:3]))
            max_translation = max(max_translation, length)
        write_mode = str(transform.get("write_mode", "unknown") or "unknown")
        write_modes[write_mode] = write_modes.get(write_mode, 0) + 1
        local_delta = abs(float(transform.get("local_rotation_delta_degrees", 0.0) or 0.0))
        max_local_rotation_delta_degrees = max(max_local_rotation_delta_degrees, local_delta)
        if write_mode == "mc2_local_rotation":
            if worst_local_write is None or local_delta > abs(float(worst_local_write.get("local_rotation_delta_degrees", 0.0) or 0.0)):
                worst_local_write = transform
            max_local_write_delta_degrees = max(max_local_write_delta_degrees, local_delta)
        elif write_mode == "mc2_world_rotation":
            if worst_world_write is None or local_delta > abs(float(worst_world_write.get("local_rotation_delta_degrees", 0.0) or 0.0)):
                worst_world_write = transform
            max_world_write_delta_degrees = max(max_world_write_delta_degrees, local_delta)
        elif write_mode.startswith("fallback"):
            if worst_fallback is None or local_delta > abs(float(worst_fallback.get("local_rotation_delta_degrees", 0.0) or 0.0)):
                worst_fallback = transform
            max_fallback_delta_degrees = max(max_fallback_delta_degrees, local_delta)

    def _debug_part(label: str, transform: dict | None) -> str | None:
        if not transform:
            return None
        detailed_part = ""
        if _detailed_native_debug_enabled:
            detailed_part = (
                f";has_rest_matrix={int(bool(transform.get('has_rest_local_to_world_matrix', False)))}"
                f";proxy_input_delta={float(transform.get('proxy_to_input_world_delta_degrees', 0.0) or 0.0):.2f}deg"
                f";p_lpos={_fmt_vec(transform.get('proxy_local_position', ())) }"
                f";p_lnor={_fmt_vec(transform.get('proxy_local_normal', ())) }"
                f";p_ltan={_fmt_vec(transform.get('proxy_local_tangent', ())) }"
                f";p_posed={_fmt_vec(transform.get('proxy_posed_position', ())) }"
                f";p_pnor={_fmt_vec(transform.get('proxy_posed_normal', ())) }"
                f";p_ptan={_fmt_vec(transform.get('proxy_posed_tangent', ())) }"
                f";p_wnor={_fmt_vec(transform.get('proxy_world_normal', ())) }"
                f";p_wtan={_fmt_vec(transform.get('proxy_world_tangent', ())) }"
            )
        return (
            f"{label}:"
            f"component={transform.get('component_id', '')}"
            f";bone={transform.get('bone_name', '')}"
            f";parent={transform.get('parent_bone_name', '')}"
            f";joint={transform.get('joint_index', -1)}"
            f";parent_index={transform.get('parent_index', -1)}"
            f";attr=0x{int(transform.get('vertex_attribute', 0) or 0):02x}"
            f";flags=0x{int(transform.get('transform_flags', 0) or 0):02x}"
            f";local_delta={float(transform.get('local_rotation_delta_degrees', 0.0) or 0.0):.2f}deg"
            f";world_delta={float(transform.get('world_rotation_delta_degrees', 0.0) or 0.0):.2f}deg"
            f";local_pos_delta={float(transform.get('local_position_delta', 0.0) or 0.0):.5f}"
            f";world_pos_delta={float(transform.get('world_position_delta', 0.0) or 0.0):.5f}"
            f";in_local={_fmt_quat(transform.get('input_local_rotation', ())) }"
            f";out_local={_fmt_quat(transform.get('output_local_rotation', transform.get('rotation_quaternion', ())))}"
            f";in_world={_fmt_quat(transform.get('input_world_rotation', ())) }"
            f";out_world={_fmt_quat(transform.get('world_rotation_quaternion', ())) }"
            f";proxy_rot={_fmt_quat(transform.get('proxy_vertex_rotation', ())) }"
            f";v2t={_fmt_quat(transform.get('vertex_to_transform_rotation', ())) }"
            f";parent_world={_fmt_quat(transform.get('parent_world_rotation_quaternion', ())) }"
            f"{detailed_part}"
            f";in_lpos={_fmt_vec(transform.get('input_local_position', ())) }"
            f";out_lpos={_fmt_vec(transform.get('output_local_position', ())) }"
            f";in_wpos={_fmt_vec(transform.get('input_world_position', ())) }"
            f";out_wpos={_fmt_vec(transform.get('output_world_position', ())) }"
        )

    diagnostic_parts = []
    if max_local_rotation_delta_degrees > 0.01:
        diagnostic_parts.append(f"mc2_local_delta_max:{max_local_rotation_delta_degrees:.2f}deg")
    if max_local_write_delta_degrees > 0.01:
        diagnostic_parts.append(f"mc2_local_write_delta_max:{max_local_write_delta_degrees:.2f}deg")
    if max_world_write_delta_degrees > 0.01:
        diagnostic_parts.append(f"mc2_world_write_delta_max:{max_world_write_delta_degrees:.2f}deg")
    if max_fallback_delta_degrees > 0.01:
        diagnostic_parts.append(f"mc2_fallback_delta_max:{max_fallback_delta_degrees:.2f}deg")
    debug_lines = []
    for debug_part in (
        _debug_part("mc2_worst_local_write", worst_local_write),
        _debug_part("mc2_worst_world_write", worst_world_write),
        _debug_part("mc2_worst_fallback", worst_fallback),
    ):
        if debug_part:
            debug_lines.append(debug_part)

    return {
        "non_identity_transform_count": non_identity_count,
        "max_rotation_degrees": max_rotation_degrees,
        "max_translation": max_translation,
        "max_local_rotation_delta_degrees": max_local_rotation_delta_degrees,
        "debug_lines": debug_lines,
        "write_mode_summary": ", ".join(
            f"{key}:{value}" for key, value in sorted(write_modes.items())
        )
        + (", " + ", ".join(diagnostic_parts) if diagnostic_parts else ""),
    }


def _current_bridge():
    global _active_bridge
    if _active_bridge is None:
        _active_bridge = load_bridge()
    return _active_bridge


def build_runtime(build_input, force_native_backend: bool | None = None):
    global _active_bridge, _last_authoring_snapshot, _last_transforms, _last_mesh_outputs, _last_build_output
    bridge = load_bridge(force_native_backend)
    _active_bridge = bridge
    _last_authoring_snapshot = build_input if isinstance(build_input, dict) and build_input.get("payload_type") == "authoring_snapshot" else None
    _last_transforms = []
    _last_mesh_outputs = []
    result = bridge.build_scene(build_input)
    _last_build_output = result.get("build_output") or empty_build_output()
    result_state = dict(result)
    result_state.pop("build_output", None)
    _runtime_state.update(result_state)
    _runtime_state["step_count"] = 0
    _runtime_state["last_dt"] = 0.0
    _runtime_state["last_executed_steps"] = 0
    _runtime_state["bone_transform_count"] = 0
    _runtime_state["non_identity_transform_count"] = 0
    _runtime_state["max_rotation_degrees"] = 0.0
    _runtime_state["max_translation"] = 0.0
    _runtime_state["write_mode_summary"] = ""
    _runtime_state["fixed_step_dt"] = 1.0 / max(int(_runtime_state["simulation_frequency"]), 1)
    _runtime_state["accumulated_time"] = 0.0
    return dict(_runtime_state)


def destroy_runtime():
    global _active_bridge, _last_authoring_snapshot, _last_transforms, _last_mesh_outputs, _last_build_output, _pose_baseline, _last_debug_lines
    reset_runtime_state()
    _active_bridge = None
    _last_authoring_snapshot = None
    _last_transforms = []
    _last_mesh_outputs = []
    _last_build_output = empty_build_output()
    _pose_baseline = {}
    _last_debug_lines = []


def reset_runtime():
    global _last_transforms, _last_mesh_outputs, _last_debug_lines
    if not _runtime_state["handle"]:
        return dict(_runtime_state)

    _current_bridge().reset_scene(_runtime_state["handle"])

    _runtime_state["step_count"] = 0
    _runtime_state["last_dt"] = 0.0
    _runtime_state["last_executed_steps"] = 0
    _runtime_state["bone_transform_count"] = 0
    _runtime_state["non_identity_transform_count"] = 0
    _runtime_state["max_rotation_degrees"] = 0.0
    _runtime_state["max_translation"] = 0.0
    _runtime_state["write_mode_summary"] = ""
    _runtime_state["accumulated_time"] = 0.0
    _last_transforms = []
    _last_mesh_outputs = []
    _last_debug_lines = []
    return dict(_runtime_state)


def set_runtime_inputs_only(runtime_inputs: dict | None = None):
    if not _runtime_state["handle"]:
        raise RuntimeError("Runtime has not been built yet.")

    _current_bridge().set_runtime_inputs(_runtime_state["handle"], runtime_inputs or empty_frame_inputs())
    return dict(_runtime_state)


def step_runtime(
    dt: float = 1.0 / 30.0,
    simulation_frequency: int = 90,
    runtime_inputs: dict | None = None,
):
    global _last_transforms, _last_mesh_outputs, _last_debug_lines
    if not _runtime_state["handle"]:
        raise RuntimeError("Runtime has not been built yet.")

    bridge = _current_bridge()
    if runtime_inputs is not None:
        bridge.set_runtime_inputs(_runtime_state["handle"], runtime_inputs)

    fixed_step_dt = 1.0 / max(int(simulation_frequency), 1)
    _runtime_state["fixed_step_dt"] = fixed_step_dt
    frame_dt = max(float(dt), 0.0)
    _runtime_state["accumulated_time"] += frame_dt
    result = bridge.step_scene(
        _runtime_state["handle"],
        frame_dt,
        simulation_frequency,
    )
    transforms = bridge.get_bone_transforms(_runtime_state["handle"])
    mesh_outputs = bridge.get_mesh_outputs(_runtime_state["handle"])
    _last_transforms = transforms
    _last_mesh_outputs = mesh_outputs
    _runtime_state["step_count"] = result["steps"]
    _runtime_state["last_dt"] = dt
    _runtime_state["simulation_frequency"] = simulation_frequency
    _runtime_state["last_executed_steps"] = int(result.get("executed_steps", 0))
    _runtime_state["bone_transform_count"] = len(transforms)
    summary = _summarize_transforms(transforms)
    debug_lines = list(summary.pop("debug_lines", []))
    _runtime_state.update(summary)
    _last_debug_lines = debug_lines
    for line in debug_lines:
        print(line, flush=True)
    _runtime_state["accumulated_time"] = max(
        0.0,
        _runtime_state["accumulated_time"] - (fixed_step_dt * _runtime_state["last_executed_steps"]),
    )
    frame_inputs = frame_inputs_payload(runtime_inputs) if runtime_inputs is not None else empty_frame_inputs()
    return {
        "runtime_state": dict(_runtime_state),
        "transforms": transforms,
        "mesh_outputs": mesh_outputs,
        "frame_inputs": frame_inputs,
        "exchange": wrap_step_output(_runtime_state, transforms, mesh_outputs),
    }


def reset_runtime_state():
    _runtime_state["handle"] = 0
    _runtime_state["summary"] = ""
    _runtime_state["backend"] = "none"
    _runtime_state["step_count"] = 0
    _runtime_state["last_dt"] = 0.0
    _runtime_state["simulation_frequency"] = 90
    _runtime_state["last_executed_steps"] = 0
    _runtime_state["bone_transform_count"] = 0
    _runtime_state["non_identity_transform_count"] = 0
    _runtime_state["max_rotation_degrees"] = 0.0
    _runtime_state["max_translation"] = 0.0
    _runtime_state["write_mode_summary"] = ""
    _runtime_state["fixed_step_dt"] = 1.0 / 90.0
    _runtime_state["accumulated_time"] = 0.0


def get_runtime_state():
    return dict(_runtime_state)


def get_last_authoring_snapshot():
    return _last_authoring_snapshot


def get_last_transforms():
    return list(_last_transforms)


def get_last_mesh_outputs():
    return list(_last_mesh_outputs)


def get_last_debug_lines():
    return list(_last_debug_lines)


def get_last_build_output():
    return dict(_last_build_output)


def set_pose_baseline(baseline: dict):
    global _pose_baseline
    _pose_baseline = dict(baseline)


def get_pose_baseline():
    return dict(_pose_baseline)
