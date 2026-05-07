from importlib import import_module

from .exchange import (
    is_exchange_envelope,
    wrap_authoring_snapshot,
    wrap_frame_inputs,
)


class NativeModuleBridge:
    def __init__(self, module):
        self._module = module

    def build_scene(self, build_input):
        if (
            hasattr(self._module, "build_authoring_snapshot")
            and is_exchange_envelope(build_input)
            and build_input.get("payload_type") == "authoring_snapshot"
        ):
            return self._module.build_authoring_snapshot(wrap_authoring_snapshot(build_input))
        raise RuntimeError("Native build now expects an authoring_snapshot payload.")

    def destroy_scene(self, handle):
        return self._module.destroy_scene(handle)

    def reset_scene(self, handle):
        return self._module.reset_scene(handle)

    def step_scene(self, handle, dt, simulation_frequency):
        return self._module.step_scene(handle, dt, simulation_frequency)

    def set_runtime_inputs(self, handle, runtime_inputs):
        return self._module.set_runtime_inputs(handle, wrap_frame_inputs(runtime_inputs))

    def get_bone_transforms(self, handle):
        return self._module.get_bone_transforms(handle)

    def get_mesh_outputs(self, handle):
        if hasattr(self._module, "get_mesh_outputs"):
            return self._module.get_mesh_outputs(handle)
        return []


def load_bridge(use_native: bool | None = None):
    if use_native is False:
        raise RuntimeError("HoCloth now requires the native MC2 backend; Python solver fallback was removed.")

    try:
        module = import_module("hocloth_native")
    except Exception as exc:
        raise RuntimeError(f"Failed to import hocloth_native native MC2 backend: {exc}") from exc
    return NativeModuleBridge(module)
