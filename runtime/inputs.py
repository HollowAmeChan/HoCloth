from mathutils import Matrix, Vector

from ..compile.compiler import resolve_armature_object


_INPUT_STATE = {
    "last_scene_frame": None,
    "chain_states": {},
    "collision_object_states": {},
}


def _matrix_translation_rotation(matrix: Matrix) -> tuple[tuple[float, float, float], tuple[float, float, float, float]]:
    translation = tuple(matrix.to_translation())
    rotation = tuple(matrix.to_quaternion())
    return translation, rotation


def _vector_subtract(a, b) -> tuple[float, float, float]:
    return (
        float(a[0]) - float(b[0]),
        float(a[1]) - float(b[1]),
        float(a[2]) - float(b[2]),
    )


def _vector_scale(value, scalar: float) -> tuple[float, float, float]:
    return (
        float(value[0]) * scalar,
        float(value[1]) * scalar,
        float(value[2]) * scalar,
    )


def _vector_tuple(value) -> tuple[float, float, float]:
    return (float(value[0]), float(value[1]), float(value[2]))


def _quat_tuple(value) -> tuple[float, float, float, float]:
    return (float(value.w), float(value.x), float(value.y), float(value.z))


def _safe_fps(scene) -> float:
    render = getattr(scene, "render", None)
    if render is None:
        return 24.0
    fps = float(getattr(render, "fps", 24.0) or 24.0)
    fps_base = float(getattr(render, "fps_base", 1.0) or 1.0)
    return fps / fps_base if fps_base > 1.0e-6 else fps


def _object_transform_input(obj) -> tuple[tuple[float, float, float], tuple[float, float, float, float], tuple[float, float, float]]:
    if obj is None:
        return (0.0, 0.0, 0.0), (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0)
    translation, rotation = _matrix_translation_rotation(obj.matrix_world)
    scale = tuple(float(axis) for axis in obj.matrix_world.to_scale())
    return translation, rotation, scale


def _bone_transform_input(scene, armature_name: str, bone_name: str):
    armature_object = resolve_armature_object(scene, armature_name)
    if armature_object is None or armature_object.pose is None or not bone_name:
        return (0.0, 0.0, 0.0), (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0)
    pose_bone = armature_object.pose.bones.get(bone_name)
    if pose_bone is None:
        return (0.0, 0.0, 0.0), (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0)
    world_matrix = armature_object.matrix_world @ pose_bone.matrix
    return _object_transform_input(type("BoneProxy", (), {"matrix_world": world_matrix})())


def _append_vec3(buffer: list[float], value) -> None:
    buffer.extend((float(value[0]), float(value[1]), float(value[2])))


def _append_quat(buffer: list[float], value) -> None:
    buffer.extend((float(value[0]), float(value[1]), float(value[2]), float(value[3])))


def reset_runtime_input_tracking():
    _INPUT_STATE["last_scene_frame"] = None
    _INPUT_STATE["chain_states"] = {}
    _INPUT_STATE["collision_object_states"] = {}


def build_runtime_inputs(scene, compiled_scene) -> dict:
    bone_chains = []
    collision_objects = []
    if compiled_scene is None:
        return {"bone_chains": bone_chains, "collision_objects": collision_objects}

    current_frame = int(scene.frame_current)
    previous_frame = _INPUT_STATE["last_scene_frame"]
    frame_delta = None if previous_frame is None else current_frame - previous_frame
    fps = _safe_fps(scene)
    frame_dt = 1.0 / fps if fps > 1.0e-6 else (1.0 / 24.0)
    next_chain_states = {}
    next_collision_object_states = {}

    for chain in compiled_scene.bone_chains:
        armature_object = resolve_armature_object(scene, chain.armature_name)
        if armature_object is None or armature_object.pose is None:
            continue

        pose_bone = armature_object.pose.bones.get(chain.root_bone_name)
        if pose_bone is None:
            continue

        root_world_matrix = armature_object.matrix_world @ pose_bone.matrix
        translation, rotation = _matrix_translation_rotation(root_world_matrix)
        armature_scale = tuple(float(axis) for axis in armature_object.matrix_world.to_scale())
        center_object_name = getattr(chain, "center_object_name", "")
        center_bone_name = getattr(chain, "center_bone_name", "")
        if center_bone_name:
            center_translation, center_rotation, center_scale = _bone_transform_input(
                scene,
                center_object_name or chain.armature_name,
                center_bone_name,
            )
        else:
            if center_object_name:
                center_object = scene.objects.get(center_object_name)
                center_translation, center_rotation, center_scale = _object_transform_input(center_object)
            else:
                # MC2-style fallback: when BoneSpring has no explicit center, use the
                # current root transform as the simulation center instead of world origin.
                center_translation = translation
                center_rotation = rotation
                center_scale = armature_scale
        linear_velocity = (0.0, 0.0, 0.0)
        previous_state = _INPUT_STATE["chain_states"].get(chain.component_id)
        if previous_state is not None and frame_delta in (-1, 1):
            translation_delta = _vector_subtract(translation, previous_state["translation"])
            linear_velocity = _vector_scale(translation_delta, 1.0 / max(frame_dt, 1.0e-6))
        center_linear_velocity = (0.0, 0.0, 0.0)
        if previous_state is not None and frame_delta in (-1, 1):
            center_delta = _vector_subtract(center_translation, previous_state["center_translation"])
            center_linear_velocity = _vector_scale(center_delta, 1.0 / max(frame_dt, 1.0e-6))

        basic_head_positions: list[float] = []
        basic_tail_positions: list[float] = []
        basic_rotations: list[float] = []
        pose_bones = armature_object.pose.bones
        for bone in chain.bones:
            pose_bone = pose_bones.get(bone.name)
            if pose_bone is not None:
                pose_matrix = armature_object.matrix_world @ pose_bone.matrix
                head = _vector_tuple(pose_matrix.to_translation())
                rotation = _quat_tuple(pose_matrix.to_quaternion())
                direction = pose_matrix.to_quaternion() @ Vector((0.0, max(float(bone.length), 1.0e-6), 0.0))
                tail = _vector_tuple(Vector(head) + direction)
            else:
                parent_index = int(getattr(bone, "parent_index", -1))
                if parent_index >= 0 and (parent_index * 3 + 2) < len(basic_tail_positions):
                    head = (
                        basic_tail_positions[parent_index * 3 + 0],
                        basic_tail_positions[parent_index * 3 + 1],
                        basic_tail_positions[parent_index * 3 + 2],
                    )
                else:
                    head = translation
                rest_head = Vector(getattr(bone, "rest_head_local", (0.0, 0.0, 0.0)))
                rest_tail = Vector(getattr(bone, "rest_tail_local", (0.0, max(float(getattr(bone, "length", 0.0)), 1.0e-6), 0.0)))
                rest_direction = rest_tail - rest_head
                if rest_direction.length <= 1.0e-6:
                    rest_direction = Vector((0.0, max(float(getattr(bone, "length", 0.0)), 1.0e-6), 0.0))
                tail = _vector_tuple(Vector(head) + rest_direction)
                rotation = tuple(getattr(bone, "rest_local_rotation", (1.0, 0.0, 0.0, 0.0)))

            _append_vec3(basic_head_positions, head)
            _append_vec3(basic_tail_positions, tail)
            _append_quat(basic_rotations, rotation)

        next_chain_states[chain.component_id] = {
            "translation": translation,
            "center_translation": center_translation,
            "rotation": rotation,
            "armature_scale": armature_scale,
        }
        bone_chains.append(
            {
                "component_id": chain.component_id,
                "armature_name": chain.armature_name,
                "root_bone_name": chain.root_bone_name,
                "center_object_name": center_object_name,
                "center_bone_name": center_bone_name,
                "root_translation": translation,
                "root_rotation_quaternion": rotation,
                "root_linear_velocity": linear_velocity,
                "root_scale": armature_scale,
                "center_translation": center_translation,
                "center_rotation_quaternion": center_rotation,
                "center_linear_velocity": center_linear_velocity,
                "center_scale": center_scale,
                "basic_head_positions": tuple(basic_head_positions),
                "basic_tail_positions": tuple(basic_tail_positions),
                "basic_rotations": tuple(basic_rotations),
            }
        )

    for collision_object in getattr(compiled_scene, "collision_objects", []):
        source_object_name = getattr(collision_object, "source_object_name", "")
        source_object = scene.objects.get(source_object_name) if source_object_name else None
        translation, rotation, _scale = _object_transform_input(source_object)
        linear_velocity = (0.0, 0.0, 0.0)
        previous_state = _INPUT_STATE["collision_object_states"].get(collision_object.collision_object_id)
        if previous_state is not None and frame_delta in (-1, 1):
            translation_delta = _vector_subtract(translation, previous_state["translation"])
            linear_velocity = _vector_scale(translation_delta, 1.0 / max(frame_dt, 1.0e-6))

        next_collision_object_states[collision_object.collision_object_id] = {
            "translation": translation,
            "rotation": rotation,
        }
        collision_objects.append(
            {
                "collision_object_id": collision_object.collision_object_id,
                "world_translation": translation,
                "world_rotation": rotation,
                "linear_velocity": linear_velocity,
            }
        )

    _INPUT_STATE["last_scene_frame"] = current_frame
    _INPUT_STATE["chain_states"] = next_chain_states
    _INPUT_STATE["collision_object_states"] = next_collision_object_states
    return {"bone_chains": bone_chains, "collision_objects": collision_objects}
