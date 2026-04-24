from importlib import import_module
import math


_TAIL_TIP_SUFFIX = "__hocloth_tail_tip__"


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


def _remap_bone_damping(value):
    t = _clamp(value, 0.0, 1.0)
    return (t ** 2.2) * 0.2


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


def _baseline_joint_paths(chain):
    baselines = getattr(chain, "baselines", None) or []
    paths = []
    for baseline in baselines:
        joint_indices = list(getattr(baseline, "joint_indices", []) or [])
        if joint_indices:
            paths.append(joint_indices)
    if paths:
        return paths

    if not getattr(chain, "bones", None):
        return []

    leaf_indices = []
    parent_to_children = {}
    for index, bone in enumerate(chain.bones):
        parent_index = getattr(bone, "parent_index", -1)
        if parent_index >= 0:
            parent_to_children.setdefault(parent_index, []).append(index)
    for index in range(len(chain.bones)):
        if index not in parent_to_children:
            leaf_indices.append(index)

    fallback_paths = []
    for leaf_index in leaf_indices:
        path = []
        current_index = leaf_index
        while current_index >= 0:
            path.append(current_index)
            current_index = getattr(chain.bones[current_index], "parent_index", -1)
        fallback_paths.append(list(reversed(path)))
    return fallback_paths


def _chain_line_indices(chain):
    explicit_lines = getattr(chain, "lines", None) or []
    if explicit_lines:
        return [
            (int(line.start_index), int(line.end_index))
            for line in explicit_lines
            if int(line.start_index) >= 0 and int(line.end_index) >= 0
        ]
    return [
        (int(getattr(bone, "parent_index", -1)), index)
        for index, bone in enumerate(getattr(chain, "bones", []) or [])
        if int(getattr(bone, "parent_index", -1)) >= 0
    ]


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
            "collision_object_lookup": self._build_collision_object_lookup(compiled_scene),
            "collision_binding_lookup": self._build_collision_binding_lookup(compiled_scene),
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
            scene["collision_object_lookup"] = self._build_collision_object_lookup(scene["compiled_scene"])
            scene["collision_binding_lookup"] = self._build_collision_binding_lookup(scene["compiled_scene"])
        return True

    def set_runtime_inputs(self, handle, runtime_inputs):
        scene = self._scenes.get(handle)
        if scene is None:
            raise RuntimeError(f"Unknown runtime handle: {handle}")
        scene["runtime_inputs"] = runtime_inputs or {"bone_chains": []}
        self._apply_collision_object_inputs(scene, scene["runtime_inputs"])
        return True

    def step_scene(self, handle, dt, simulation_frequency):
        scene = self._scenes.get(handle)
        if scene is None:
            raise RuntimeError(f"Unknown runtime handle: {handle}")
        frequency = min(150, max(30, int(simulation_frequency or 90)))
        simulation_dt = 1.0 / frequency
        self._step_scene_states(scene, simulation_dt)
        scene["steps"] += 1
        compiled_scene = scene["compiled_scene"]
        return {
            "handle": handle,
            "dt": dt,
            "simulation_frequency": frequency,
            "executed_steps": 1,
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
                if bone.name.endswith(_TAIL_TIP_SUFFIX) or bone.parent_index < 0:
                    continue
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

    def _build_collision_object_lookup(self, compiled_scene):
        return {
            collision_object.collision_object_id: collision_object
            for collision_object in getattr(compiled_scene, "collision_objects", [])
        }

    def _build_collision_binding_lookup(self, compiled_scene):
        object_lookup = self._build_collision_object_lookup(compiled_scene)
        binding_lookup = {}
        for binding in getattr(compiled_scene, "collision_bindings", []):
            binding_lookup[binding.binding_id] = [
                object_lookup[collision_object_id]
                for collision_object_id in binding.collision_object_ids
                if collision_object_id in object_lookup
            ]
        return binding_lookup

    def _apply_collision_object_inputs(self, scene, runtime_inputs):
        object_lookup = scene.get("collision_object_lookup", {})
        for collision_input in runtime_inputs.get("collision_objects", []):
            collision_object = object_lookup.get(collision_input.get("collision_object_id"))
            if collision_object is None:
                continue
            collision_object.world_translation = tuple(collision_input.get("world_translation", collision_object.world_translation))
            collision_object.world_rotation = tuple(collision_input.get("world_rotation", collision_object.world_rotation))
            if "linear_velocity" in collision_input:
                collision_object.linear_velocity = tuple(collision_input.get("linear_velocity", collision_object.linear_velocity))

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

    def _apply_collision_response(self, chain, bone, bone_state, collision_objects):
        if not collision_objects:
            return

        point = (bone_state["roll"], bone_state["pitch"], 0.0)
        point_scale = max(0.05, bone.length)
        for collision_object in collision_objects:
            world_translation = getattr(collision_object, "world_translation", (0.0, 0.0, 0.0))
            radius = max(0.0, float(getattr(collision_object, "radius", 0.0)))
            collider_center = (
                world_translation[0] * 0.35,
                world_translation[2] * 0.35,
                0.0,
            )
            if getattr(collision_object, "shape_type", "SPHERE") == "CAPSULE":
                height = max(0.0, float(getattr(collision_object, "height", 0.0)))
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
            gravity = 0.0
            max_angle = 1.15
            max_velocity = 8.0
            max_depth = max(1, max((getattr(bone, "depth", 0) for bone in chain.bones), default=0))
            runtime_input = self._find_runtime_input(scene, chain.component_id)
            collision_objects = []
            baseline_paths = _baseline_joint_paths(chain)
            line_indices = _chain_line_indices(chain)
            for binding_id in getattr(chain, "collision_binding_ids", []):
                collision_objects.extend(scene.get("collision_binding_lookup", {}).get(binding_id, []))
            for root_index in {path[0] for path in baseline_paths if path}:
                root_state = chain_state[root_index]
                root_state["pitch"] = 0.0
                root_state["roll"] = 0.0
                root_state["pitch_velocity"] = 0.0
                root_state["roll_velocity"] = 0.0

            for path in baseline_paths:
                for bone_index in path[1:]:
                    bone_state = chain_state[bone_index]
                    bone = chain.bones[bone_index]
                    stiffness = _clamp(getattr(bone, "stiffness", chain.stiffness), 0.0, 1.0)
                    damping = _remap_bone_damping(getattr(bone, "damping", chain.damping))
                    drag = _clamp(getattr(bone, "drag", chain.drag), 0.0, 1.0)
                    per_joint_gravity_scale = max(0.0, float(getattr(bone, "gravity_scale", 1.0)))
                    stiffness_gain = 10.0 + stiffness * 24.0
                    damping_gain = 1.2 + damping * 5.0
                    drag_factor = max(0.0, 1.0 - drag * dt * 6.0)
                    inherit_factor = 0.55 + stiffness * 0.20
                    path_depth = max(1, len(path) - 1)
                    local_depth = float(path.index(bone_index)) / float(path_depth)
                    tree_depth = float(getattr(bone, "depth", 0) + 1) / float(max_depth + 1)
                    depth = max(local_depth, tree_depth)
                    length_scale = max(0.05, bone.length)
                    gravity_scale = (
                        gravity
                        * per_joint_gravity_scale
                        * (0.45 + depth * 0.85)
                        * math.sqrt(length_scale)
                    )

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

            for start_index, end_index in line_indices:
                if start_index < 0 or end_index < 0:
                    continue
                parent_state = chain_state[start_index]
                child_state = chain_state[end_index]
                line_bone = chain.bones[end_index]
                relax = 0.08 + _clamp(getattr(line_bone, "stiffness", chain.stiffness), 0.0, 1.0) * 0.18
                child_state["pitch"] = _clamp(
                    child_state["pitch"] * (1.0 - relax) + parent_state["pitch"] * relax,
                    -max_angle,
                    max_angle,
                )
                child_state["roll"] = _clamp(
                    child_state["roll"] * (1.0 - relax) + parent_state["roll"] * relax,
                    -max_angle,
                    max_angle,
                )

            for bone_index, bone in enumerate(chain.bones):
                if bone.parent_index < 0:
                    continue
                bone_state = chain_state[bone_index]
                if collision_objects:
                    self._apply_collision_response(chain, bone, bone_state, collision_objects)

            for start_index, end_index in reversed(line_indices):
                if start_index < 0 or end_index < 0:
                    continue
                parent_state = chain_state[start_index]
                child_state = chain_state[end_index]
                line_bone = chain.bones[end_index]
                relax = 0.12 + _clamp(getattr(line_bone, "stiffness", chain.stiffness), 0.0, 1.0) * 0.22
                child_state["pitch"] = _clamp(
                    child_state["pitch"] * (1.0 - relax) + parent_state["pitch"] * relax,
                    -max_angle,
                    max_angle,
                )
                child_state["roll"] = _clamp(
                    child_state["roll"] * (1.0 - relax) + parent_state["roll"] * relax,
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

    def step_scene(self, handle, dt, simulation_frequency):
        return self._module.step_scene(handle, dt, simulation_frequency)

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
