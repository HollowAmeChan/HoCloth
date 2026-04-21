from .bridge import load_bridge


_runtime_state = {
    "handle": 0,
    "summary": "",
    "backend": "none",
    "step_count": 0,
    "last_dt": 0.0,
    "last_substeps": 0,
    "bone_transform_count": 0,
}
_active_bridge = None
_compiled_scene = None
_last_transforms = []
_pose_baseline = {}


def _current_bridge():
    global _active_bridge
    if _active_bridge is None:
        _active_bridge = load_bridge()
    return _active_bridge


def build_runtime(compiled_scene):
    global _active_bridge, _compiled_scene, _last_transforms
    bridge = load_bridge()
    if _runtime_state["handle"]:
        _current_bridge().destroy_scene(_runtime_state["handle"])
    _active_bridge = bridge
    _compiled_scene = compiled_scene
    _last_transforms = []
    result = bridge.build_scene(compiled_scene)
    _runtime_state.update(result)
    _runtime_state["step_count"] = 0
    _runtime_state["last_dt"] = 0.0
    _runtime_state["last_substeps"] = 0
    _runtime_state["bone_transform_count"] = 0
    return dict(_runtime_state)


def destroy_runtime():
    global _active_bridge, _compiled_scene, _last_transforms, _pose_baseline
    if _runtime_state["handle"]:
        _current_bridge().destroy_scene(_runtime_state["handle"])
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
    _runtime_state["last_substeps"] = 0
    _runtime_state["bone_transform_count"] = 0
    _last_transforms = []
    return dict(_runtime_state)


def step_runtime(dt: float = 1.0 / 60.0, substeps: int = 1, runtime_inputs: dict | None = None):
    global _last_transforms
    if not _runtime_state["handle"]:
        raise RuntimeError("Runtime has not been built yet.")

    bridge = _current_bridge()
    if runtime_inputs is not None:
        bridge.set_runtime_inputs(_runtime_state["handle"], runtime_inputs)
    result = bridge.step_scene(_runtime_state["handle"], dt, substeps)
    transforms = bridge.get_bone_transforms(_runtime_state["handle"])
    _last_transforms = transforms
    _runtime_state["step_count"] = result["steps"]
    _runtime_state["last_dt"] = dt
    _runtime_state["last_substeps"] = substeps
    _runtime_state["bone_transform_count"] = len(transforms)
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
    _runtime_state["last_substeps"] = 0
    _runtime_state["bone_transform_count"] = 0


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
