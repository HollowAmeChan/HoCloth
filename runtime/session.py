import json
import os

from .bridge import load_bridge


_runtime_state = {
    "handle": 0,
    "summary": "",
    "backend": "none",
    "step_count": 0,
    "last_dt": 0.0,
    "simulation_frequency": 90,
    "last_executed_steps": 0,
    "bone_transform_count": 0,
    "fixed_step_dt": 1.0 / 90.0,
    "accumulated_time": 0.0,
}
_active_bridge = None
_compiled_scene = None
_last_transforms = []
_pose_baseline = {}


def _debug_dump_path() -> str:
    plugin_root = os.path.dirname(os.path.dirname(__file__))
    build_dir = os.path.join(plugin_root, "_build")
    os.makedirs(build_dir, exist_ok=True)
    return os.path.join(build_dir, "runtime_debug_latest.json")


def _write_runtime_debug_dump(runtime_inputs: dict | None, transforms: list[dict]):
    debug_payload = {
        "runtime_state": dict(_runtime_state),
        "compiled_scene": _compiled_scene.to_dict() if _compiled_scene is not None else None,
        "runtime_inputs": runtime_inputs or {"bone_chains": []},
        "transforms": transforms,
    }
    with open(_debug_dump_path(), "w", encoding="utf-8") as handle:
        json.dump(debug_payload, handle, indent=2, ensure_ascii=False)


def _current_bridge():
    global _active_bridge
    if _active_bridge is None:
        _active_bridge = load_bridge()
    return _active_bridge


def build_runtime(compiled_scene):
    global _active_bridge, _compiled_scene, _last_transforms
    bridge = load_bridge()
    _active_bridge = bridge
    _compiled_scene = compiled_scene
    _last_transforms = []
    result = bridge.build_scene(compiled_scene)
    _runtime_state.update(result)
    _runtime_state["step_count"] = 0
    _runtime_state["last_dt"] = 0.0
    _runtime_state["last_executed_steps"] = 0
    _runtime_state["bone_transform_count"] = 0
    _runtime_state["fixed_step_dt"] = 1.0 / max(int(_runtime_state["simulation_frequency"]), 1)
    _runtime_state["accumulated_time"] = 0.0
    return dict(_runtime_state)


def destroy_runtime():
    global _active_bridge, _compiled_scene, _last_transforms, _pose_baseline
    reset_runtime_state()
    _active_bridge = None
    _compiled_scene = None
    _last_transforms = []
    _pose_baseline = {}


def reset_runtime():
    global _last_transforms
    if not _runtime_state["handle"]:
        return dict(_runtime_state)

    _current_bridge().reset_scene(_runtime_state["handle"])

    _runtime_state["step_count"] = 0
    _runtime_state["last_dt"] = 0.0
    _runtime_state["last_executed_steps"] = 0
    _runtime_state["bone_transform_count"] = 0
    _runtime_state["accumulated_time"] = 0.0
    _last_transforms = []
    return dict(_runtime_state)


def set_runtime_inputs_only(runtime_inputs: dict | None = None):
    if not _runtime_state["handle"]:
        raise RuntimeError("Runtime has not been built yet.")

    _current_bridge().set_runtime_inputs(_runtime_state["handle"], runtime_inputs or {"bone_chains": []})
    return dict(_runtime_state)


def step_runtime(
    dt: float = 1.0 / 30.0,
    simulation_frequency: int = 90,
    runtime_inputs: dict | None = None,
):
    global _last_transforms
    if not _runtime_state["handle"]:
        raise RuntimeError("Runtime has not been built yet.")

    bridge = _current_bridge()
    if runtime_inputs is not None:
        bridge.set_runtime_inputs(_runtime_state["handle"], runtime_inputs)

    fixed_step_dt = 1.0 / max(int(simulation_frequency), 1)
    _runtime_state["fixed_step_dt"] = fixed_step_dt
    _runtime_state["accumulated_time"] += max(float(dt), 0.0)
    result = bridge.step_scene(
        _runtime_state["handle"],
        fixed_step_dt,
        simulation_frequency,
    )
    transforms = bridge.get_bone_transforms(_runtime_state["handle"])
    _last_transforms = transforms
    _runtime_state["step_count"] = result["steps"]
    _runtime_state["last_dt"] = dt
    _runtime_state["simulation_frequency"] = simulation_frequency
    _runtime_state["last_executed_steps"] = int(result.get("executed_steps", 0))
    _runtime_state["bone_transform_count"] = len(transforms)
    _runtime_state["accumulated_time"] = max(
        0.0,
        _runtime_state["accumulated_time"] - (fixed_step_dt * _runtime_state["last_executed_steps"]),
    )
    _write_runtime_debug_dump(runtime_inputs, transforms)
    return {
        "runtime_state": dict(_runtime_state),
        "transforms": transforms,
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
    _runtime_state["fixed_step_dt"] = 1.0 / 90.0
    _runtime_state["accumulated_time"] = 0.0


def get_runtime_state():
    return dict(_runtime_state)


def get_compiled_scene():
    return _compiled_scene


def get_last_transforms():
    return list(_last_transforms)


def set_pose_baseline(baseline: dict):
    global _pose_baseline
    _pose_baseline = dict(baseline)


def get_pose_baseline():
    return dict(_pose_baseline)
