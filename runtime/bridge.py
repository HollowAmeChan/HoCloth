from importlib import import_module
import math


class NativeBridgeStub:
    """Temporary stand-in until the native extension is available."""

    def __init__(self):
        self._next_handle = 1
        self._scenes = {}

    def build_scene(self, compiled_scene):
        handle = self._next_handle
        self._next_handle += 1
        self._scenes[handle] = {
            "compiled_scene": compiled_scene,
            "steps": 0,
            "runtime_inputs": {"bone_chains": []},
        }
        return {
            "handle": handle,
            "summary": compiled_scene.summary(),
            "backend": "stub",
        }

    def destroy_scene(self, handle):
        self._scenes.pop(handle, None)
        return handle

    def reset_scene(self, handle):
        scene = self._scenes.get(handle)
        if scene is not None:
            scene["steps"] = 0
        return True

    def set_runtime_inputs(self, handle, runtime_inputs):
        scene = self._scenes.get(handle)
        if scene is None:
            raise RuntimeError(f"Unknown runtime handle: {handle}")
        scene["runtime_inputs"] = runtime_inputs or {"bone_chains": []}
        return True

    def step_scene(self, handle, dt, substeps):
        scene = self._scenes.get(handle)
        if scene is None:
            raise RuntimeError(f"Unknown runtime handle: {handle}")
        scene["steps"] += max(1, int(substeps))
        compiled_scene = scene["compiled_scene"]
        return {
            "handle": handle,
            "dt": dt,
            "substeps": substeps,
            "steps": scene["steps"],
            "summary": compiled_scene.summary(),
        }

    def get_bone_transforms(self, handle):
        scene = self._scenes.get(handle)
        if scene is None:
            return []

        transforms = []
        for chain in scene["compiled_scene"].bone_chains:
            for bone_index, bone in enumerate(chain.bones):
                angle = 0.015 * math.sin(scene["steps"] * 0.08 + bone_index * 0.3)
                half_angle = angle * 0.5
                transforms.append(
                    {
                        "component_id": chain.component_id,
                        "armature_name": chain.armature_name,
                        "bone_name": bone.name,
                        # Placeholder mode keeps pose translation untouched to avoid
                        # fighting Blender's connected-bone local offsets.
                        "translation": (0.0, 0.0, 0.0),
                        # Blender pose channels expect a delta from rest pose, not
                        # the bone's absolute local rest orientation.
                        "rotation_quaternion": (
                            math.cos(half_angle),
                            math.sin(half_angle),
                            0.0,
                            0.0,
                        ),
                    }
                )
        return transforms


_STUB_BRIDGE = NativeBridgeStub()


class NativeModuleBridge:
    def __init__(self, module):
        self._module = module

    def build_scene(self, compiled_scene):
        return self._module.build_scene(compiled_scene.to_native_dict())

    def destroy_scene(self, handle):
        return self._module.destroy_scene(handle)

    def reset_scene(self, handle):
        return self._module.reset_scene(handle)

    def step_scene(self, handle, dt, substeps):
        return self._module.step_scene(handle, dt, substeps)

    def set_runtime_inputs(self, handle, runtime_inputs):
        return self._module.set_runtime_inputs(handle, runtime_inputs)

    def get_bone_transforms(self, handle):
        return self._module.get_bone_transforms(handle)


def load_bridge():
    try:
        module = import_module("hocloth_native")
    except Exception:
        return _STUB_BRIDGE
    return NativeModuleBridge(module)
