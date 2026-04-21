from .bridge import load_bridge


_runtime_state = {
    "handle": 0,
    "summary": "",
    "backend": "none",
}


def build_runtime(compiled_scene):
    bridge = load_bridge()
    result = bridge.build_scene(compiled_scene)
    _runtime_state.update(result)
    return dict(_runtime_state)


def destroy_runtime():
    if _runtime_state["handle"]:
        bridge = load_bridge()
        bridge.destroy_scene(_runtime_state["handle"])
    reset_runtime_state()


def reset_runtime_state():
    _runtime_state["handle"] = 0
    _runtime_state["summary"] = ""
    _runtime_state["backend"] = "none"


def get_runtime_state():
    return dict(_runtime_state)
