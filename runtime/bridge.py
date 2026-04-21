from importlib import import_module
import math


def _quat_mul(a, b):
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (
        aw * bw - ax * bx - ay * by - az * bz,
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
    )


def _clamp(value, low, high):
    return max(low, min(high, value))


def _normalize_vector3(value):
    length = math.sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2])
    if length <= 1.0e-6:
        return (0.0, -1.0, 0.0)
    return (value[0] / length, value[1] / length, value[2] / length)


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
            "chain_states": self._make_chain_states(compiled_scene),
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
            scene["runtime_inputs"] = {"bone_chains": []}
            scene["chain_states"] = self._make_chain_states(scene["compiled_scene"])
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
        step_count = max(1, int(substeps))
        fixed_dt = dt if dt and dt > 0.0 else (1.0 / 60.0)
        substep_dt = fixed_dt / step_count
        for _ in range(step_count):
            self._step_scene_states(scene, substep_dt)
        scene["steps"] += step_count
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
        for chain_index, chain in enumerate(scene["compiled_scene"].bone_chains):
            chain_state = scene["chain_states"][chain_index]
            for bone_index, bone in enumerate(chain.bones):
                bone_state = chain_state[bone_index]
                pitch = bone_state["pitch"]
                roll = bone_state["roll"]
                half_pitch = pitch * 0.5
                half_roll = roll * 0.5
                quat_x = (math.cos(half_pitch), math.sin(half_pitch), 0.0, 0.0)
                quat_z = (math.cos(half_roll), 0.0, 0.0, math.sin(half_roll))
                delta_quat = _quat_mul(quat_z, quat_x)
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
                        "rotation_quaternion": delta_quat,
                    }
                )
        return transforms

    def _make_chain_states(self, compiled_scene):
        return [
            [
                {
                    "pitch": 0.0,
                    "roll": 0.0,
                    "pitch_velocity": 0.0,
                    "roll_velocity": 0.0,
                }
                for _bone in chain.bones
            ]
            for chain in compiled_scene.bone_chains
        ]

    def _step_scene_states(self, scene, dt):
        if dt <= 0.0:
            return

        for chain_index, chain in enumerate(scene["compiled_scene"].bone_chains):
            chain_state = scene["chain_states"][chain_index]
            if not chain_state:
                continue

            gravity_dir = _normalize_vector3(chain.gravity_direction)
            stiffness = _clamp(chain.stiffness, 0.0, 1.0)
            damping = _clamp(chain.damping, 0.0, 1.0)
            drag = _clamp(chain.drag, 0.0, 1.0)
            gravity = max(0.0, chain.gravity_strength)
            stiffness_gain = 18.0 + stiffness * 54.0
            damping_gain = 3.0 + damping * 10.0
            drag_factor = max(0.0, 1.0 - drag * dt * 10.0)
            inherit_factor = 0.72 + stiffness * 0.18
            max_angle = 1.15
            max_velocity = 8.0
            chain_size = max(1, len(chain_state))

            for bone_index, bone_state in enumerate(chain_state):
                bone = chain.bones[bone_index]
                depth = float(bone_index + 1) / float(chain_size)
                length_scale = max(0.05, bone.length)
                gravity_scale = gravity * (0.18 + depth * 0.48) * math.sqrt(length_scale)

                parent_pitch = 0.0
                parent_roll = 0.0
                if bone.parent_index >= 0 and bone.parent_index < len(chain_state):
                    parent_state = chain_state[bone.parent_index]
                    parent_pitch = parent_state["pitch"] * inherit_factor
                    parent_roll = parent_state["roll"] * inherit_factor

                target_pitch = parent_pitch + (-gravity_dir[1]) * gravity_scale
                target_roll = parent_roll + gravity_dir[0] * gravity_scale
                pitch_accel = (target_pitch - bone_state["pitch"]) * stiffness_gain - (
                    bone_state["pitch_velocity"] * damping_gain
                )
                roll_accel = (target_roll - bone_state["roll"]) * stiffness_gain - (
                    bone_state["roll_velocity"] * damping_gain
                )

                bone_state["pitch_velocity"] = _clamp(
                    (bone_state["pitch_velocity"] + pitch_accel * dt) * drag_factor,
                    -max_velocity,
                    max_velocity,
                )
                bone_state["roll_velocity"] = _clamp(
                    (bone_state["roll_velocity"] + roll_accel * dt) * drag_factor,
                    -max_velocity,
                    max_velocity,
                )
                bone_state["pitch"] = _clamp(
                    bone_state["pitch"] + bone_state["pitch_velocity"] * dt,
                    -max_angle,
                    max_angle,
                )
                bone_state["roll"] = _clamp(
                    bone_state["roll"] + bone_state["roll_velocity"] * dt,
                    -max_angle,
                    max_angle,
                )


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
