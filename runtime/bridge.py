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


def _vector_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _vector_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _vector_scale(value, scalar):
    return (value[0] * scalar, value[1] * scalar, value[2] * scalar)


def _vector_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _vector_length(value):
    return math.sqrt(_vector_dot(value, value))


def _closest_point_on_segment(point, a, b):
    ab = _vector_sub(b, a)
    ab_len_sq = _vector_dot(ab, ab)
    if ab_len_sq <= 1.0e-8:
        return a
    t = _clamp(_vector_dot(_vector_sub(point, a), ab) / ab_len_sq, 0.0, 1.0)
    return _vector_add(a, _vector_scale(ab, t))


class NativeBridgeStub:
    """Temporary stand-in until the native extension is available."""

    def __init__(self, import_error=None):
        self._next_handle = 1
        self._scenes = {}
        self._import_error = import_error

    def build_scene(self, compiled_scene):
        handle = self._next_handle
        self._next_handle += 1
        self._scenes[handle] = {
            "compiled_scene": compiled_scene,
            "steps": 0,
            "runtime_inputs": {"bone_chains": []},
            "chain_states": self._make_chain_states(compiled_scene),
            "collider_lookup": self._build_collider_lookup(compiled_scene),
        }
        return {
            "handle": handle,
            "summary": compiled_scene.summary(),
            "backend": "stub",
            "build_message": self._import_error or "hocloth_native import failed; using Python stub backend.",
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
            scene["collider_lookup"] = self._build_collider_lookup(scene["compiled_scene"])
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

    def _build_collider_lookup(self, compiled_scene):
        collider_by_id = {collider.component_id: collider for collider in compiled_scene.colliders}
        group_lookup = {}
        for group in getattr(compiled_scene, "collider_groups", []):
            group_lookup[group.component_id] = [
                collider_by_id[collider_id]
                for collider_id in group.collider_ids
                if collider_id in collider_by_id
            ]
        return group_lookup

    def _find_runtime_input(self, scene, component_id):
        runtime_inputs = scene.get("runtime_inputs") or {}
        for item in runtime_inputs.get("bone_chains", []):
            if item.get("component_id") == component_id:
                return item
        return None

    def _apply_center_motion(self, chain, runtime_input, target_pitch, target_roll):
        if runtime_input is None:
            return target_pitch, target_roll
        center_translation = runtime_input.get("center_translation", (0.0, 0.0, 0.0))
        root_translation = runtime_input.get("root_translation", (0.0, 0.0, 0.0))
        center_offset = _vector_sub(center_translation, root_translation)
        offset_scale = 0.18
        return (
            target_pitch + center_offset[1] * offset_scale,
            target_roll + center_offset[0] * offset_scale,
        )

    def _apply_collider_response(self, chain, bone, bone_state, colliders):
        if not colliders:
            return

        point = (bone_state["roll"], bone_state["pitch"], 0.0)
        point_scale = max(0.05, bone.length)
        for collider in colliders:
            world_translation = getattr(collider, "world_translation", (0.0, 0.0, 0.0))
            radius = max(0.0, float(getattr(collider, "radius", 0.0)))
            collider_center = (
                world_translation[0] * 0.35,
                world_translation[2] * 0.35,
                0.0,
            )
            if getattr(collider, "shape_type", "SPHERE") == "CAPSULE":
                height = max(0.0, float(getattr(collider, "height", 0.0)))
                a = (collider_center[0], collider_center[1] - height * 0.2, 0.0)
                b = (collider_center[0], collider_center[1] + height * 0.2, 0.0)
                closest = _closest_point_on_segment(point, a, b)
            else:
                closest = collider_center

            delta = _vector_sub(point, closest)
            distance = _vector_length(delta)
            influence_radius = max(0.01, radius / point_scale)
            if distance >= influence_radius:
                continue

            if distance <= 1.0e-6:
                normal = (0.0, 1.0, 0.0)
            else:
                normal = _vector_scale(delta, 1.0 / distance)
            penetration = influence_radius - distance
            point = _vector_add(point, _vector_scale(normal, penetration * 0.85))

        bone_state["roll"] = _clamp(point[0], -1.15, 1.15)
        bone_state["pitch"] = _clamp(point[1], -1.15, 1.15)

    def _step_scene_states(self, scene, dt):
        if dt <= 0.0:
            return

        for chain_index, chain in enumerate(scene["compiled_scene"].bone_chains):
            chain_state = scene["chain_states"][chain_index]
            if not chain_state:
                continue

            gravity_dir = _normalize_vector3(chain.gravity_direction)
            gravity = _clamp(float(chain.gravity_strength), 0.0, 10.0)
            max_angle = 1.15
            max_velocity = 8.0
            chain_size = max(1, len(chain_state))
            runtime_input = self._find_runtime_input(scene, chain.component_id)
            colliders = []
            for group_id in getattr(chain, "collider_group_ids", []):
                colliders.extend(scene.get("collider_lookup", {}).get(group_id, []))

            for bone_index, bone_state in enumerate(chain_state):
                bone = chain.bones[bone_index]
                stiffness = _clamp(getattr(bone, "stiffness", chain.stiffness), 0.0, 1.0)
                damping = _clamp(getattr(bone, "damping", chain.damping), 0.0, 1.0)
                drag = _clamp(getattr(bone, "drag", chain.drag), 0.0, 1.0)
                per_joint_gravity_scale = max(0.0, float(getattr(bone, "gravity_scale", 1.0)))
                stiffness_gain = 10.0 + stiffness * 24.0
                damping_gain = 1.2 + damping * 5.0
                drag_factor = max(0.0, 1.0 - drag * dt * 6.0)
                inherit_factor = 0.55 + stiffness * 0.20
                depth = float(bone_index + 1) / float(chain_size)
                length_scale = max(0.05, bone.length)
                gravity_scale = gravity * per_joint_gravity_scale * (0.45 + depth * 0.85) * math.sqrt(length_scale)

                parent_pitch = 0.0
                parent_roll = 0.0
                if bone.parent_index >= 0 and bone.parent_index < len(chain_state):
                    parent_state = chain_state[bone.parent_index]
                    parent_pitch = parent_state["pitch"] * inherit_factor
                    parent_roll = parent_state["roll"] * inherit_factor

                target_pitch = parent_pitch + (-gravity_dir[1]) * gravity_scale * (1.0 - stiffness * 0.55)
                target_roll = parent_roll + gravity_dir[0] * gravity_scale * (1.0 - stiffness * 0.55)
                target_pitch, target_roll = self._apply_center_motion(chain, runtime_input, target_pitch, target_roll)
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
                self._apply_collider_response(chain, bone, bone_state, colliders)


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
    except Exception as exc:
        print(f"[HoCloth] hocloth_native import failed, falling back to stub backend: {exc}")
        return NativeBridgeStub(str(exc))
    return NativeModuleBridge(module)
