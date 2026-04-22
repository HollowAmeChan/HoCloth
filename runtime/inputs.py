from mathutils import Matrix

from ..compile.compiler import resolve_armature_object


_INPUT_STATE = {
    "last_scene_frame": None,
    "chain_states": {},
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


def _safe_fps(scene) -> float:
    render = getattr(scene, "render", None)
    if render is None:
        return 24.0
    fps = float(getattr(render, "fps", 24.0) or 24.0)
    fps_base = float(getattr(render, "fps_base", 1.0) or 1.0)
    return fps / fps_base if fps_base > 1.0e-6 else fps


def reset_runtime_input_tracking():
    _INPUT_STATE["last_scene_frame"] = None
    _INPUT_STATE["chain_states"] = {}


def build_runtime_inputs(scene, compiled_scene) -> dict:
    bone_chains = []
    if compiled_scene is None:
        return {"bone_chains": bone_chains}

    current_frame = int(scene.frame_current)
    previous_frame = _INPUT_STATE["last_scene_frame"]
    frame_delta = None if previous_frame is None else current_frame - previous_frame
    fps = _safe_fps(scene)
    frame_dt = 1.0 / fps if fps > 1.0e-6 else (1.0 / 24.0)
    next_chain_states = {}

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
        linear_velocity = (0.0, 0.0, 0.0)
        previous_state = _INPUT_STATE["chain_states"].get(chain.component_id)
        if previous_state is not None and frame_delta in (-1, 1):
            translation_delta = _vector_subtract(translation, previous_state["translation"])
            linear_velocity = _vector_scale(translation_delta, 1.0 / max(frame_dt, 1.0e-6))

        next_chain_states[chain.component_id] = {
            "translation": translation,
            "rotation": rotation,
            "armature_scale": armature_scale,
        }
        bone_chains.append(
            {
                "component_id": chain.component_id,
                "armature_name": chain.armature_name,
                "root_bone_name": chain.root_bone_name,
                "root_translation": translation,
                "root_rotation_quaternion": rotation,
                "root_linear_velocity": linear_velocity,
                "root_scale": armature_scale,
            }
        )

    _INPUT_STATE["last_scene_frame"] = current_frame
    _INPUT_STATE["chain_states"] = next_chain_states
    return {"bone_chains": bone_chains}
