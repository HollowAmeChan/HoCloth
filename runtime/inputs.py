from mathutils import Matrix

from ..compile.compiler import resolve_armature_object


def _matrix_translation_rotation(matrix: Matrix) -> tuple[tuple[float, float, float], tuple[float, float, float, float]]:
    translation = tuple(matrix.to_translation())
    rotation = tuple(matrix.to_quaternion())
    return translation, rotation


def build_runtime_inputs(scene, compiled_scene) -> dict:
    bone_chains = []
    if compiled_scene is None:
        return {"bone_chains": bone_chains}

    for chain in compiled_scene.bone_chains:
        armature_object = resolve_armature_object(scene, chain.armature_name)
        if armature_object is None or armature_object.pose is None:
            continue

        pose_bone = armature_object.pose.bones.get(chain.root_bone_name)
        if pose_bone is None:
            continue

        root_world_matrix = armature_object.matrix_world @ pose_bone.matrix
        translation, rotation = _matrix_translation_rotation(root_world_matrix)
        bone_chains.append(
            {
                "component_id": chain.component_id,
                "armature_name": chain.armature_name,
                "root_bone_name": chain.root_bone_name,
                "root_translation": translation,
                "root_rotation_quaternion": rotation,
            }
        )

    return {"bone_chains": bone_chains}
