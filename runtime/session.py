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


def _summarize_transforms(transforms: list[dict]) -> dict:
    import math

    max_rotation_degrees = 0.0
    max_translation = 0.0
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

    return {
        "non_identity_transform_count": non_identity_count,
        "max_rotation_degrees": max_rotation_degrees,
        "max_translation": max_translation,
        "write_mode_summary": ", ".join(
            f"{key}:{value}" for key, value in sorted(write_modes.items())
        ),
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
    global _active_bridge, _last_authoring_snapshot, _last_transforms, _last_mesh_outputs, _last_build_output, _pose_baseline
    reset_runtime_state()
    _active_bridge = None
    _last_authoring_snapshot = None
    _last_transforms = []
    _last_mesh_outputs = []
    _last_build_output = empty_build_output()
    _pose_baseline = {}


def reset_runtime():
    global _last_transforms, _last_mesh_outputs
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
    global _last_transforms, _last_mesh_outputs
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
    _runtime_state.update(_summarize_transforms(transforms))
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


def get_last_build_output():
    return dict(_last_build_output)


def set_pose_baseline(baseline: dict):
    global _pose_baseline
    _pose_baseline = dict(baseline)


def get_pose_baseline():
    return dict(_pose_baseline)
