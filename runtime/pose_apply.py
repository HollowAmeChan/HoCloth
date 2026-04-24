import bpy
from mathutils import Matrix, Quaternion, Vector

from ..compile.compiler import resolve_armature_object


_TAIL_TIP_SUFFIX = "__hocloth_tail_tip__"


def _component_armature_lookup(compiled_scene) -> dict[str, str]:
    if compiled_scene is None:
        return {}
    return {
        chain.component_id: chain.armature_name
        for chain in compiled_scene.bone_chains
    }


def capture_pose_baseline(scene: bpy.types.Scene, compiled_scene) -> dict:
    component_lookup = _component_armature_lookup(compiled_scene)
    armature_cache = {}
    baseline = {}

    for chain in compiled_scene.bone_chains if compiled_scene is not None else []:
        armature_name = component_lookup.get(chain.component_id, chain.armature_name)
        if armature_name not in armature_cache:
            armature_cache[armature_name] = resolve_armature_object(scene, armature_name)
        armature_object = armature_cache[armature_name]
        if armature_object is None or armature_object.pose is None:
            continue

        for bone in chain.bones:
            pose_bone = armature_object.pose.bones.get(bone.name)
            if pose_bone is None:
                continue

            if pose_bone.rotation_mode != "QUATERNION":
                pose_bone.rotation_mode = "QUATERNION"

            baseline[(chain.component_id, bone.name)] = {
                "location": tuple(pose_bone.location),
                "rotation_quaternion": tuple(pose_bone.rotation_quaternion),
            }

    return baseline


def capture_pose_state(scene: bpy.types.Scene, compiled_scene) -> dict:
    return capture_pose_baseline(scene, compiled_scene)


def clear_pose_transforms(scene: bpy.types.Scene, compiled_scene) -> int:
    if compiled_scene is None:
        return 0

    component_lookup = _component_armature_lookup(compiled_scene)
    armature_cache = {}
    cleared_count = 0

    for chain in compiled_scene.bone_chains:
        armature_name = component_lookup.get(chain.component_id, chain.armature_name)
        if armature_name not in armature_cache:
            armature_cache[armature_name] = resolve_armature_object(scene, armature_name)
        armature_object = armature_cache[armature_name]
        if armature_object is None or armature_object.pose is None:
            continue

        for bone in chain.bones:
            pose_bone = armature_object.pose.bones.get(bone.name)
            if pose_bone is None:
                continue

            if pose_bone.rotation_mode != "QUATERNION":
                pose_bone.rotation_mode = "QUATERNION"
            pose_bone.location = Vector((0.0, 0.0, 0.0))
            pose_bone.rotation_quaternion = Quaternion((1.0, 0.0, 0.0, 0.0))
            pose_bone.rotation_euler = Vector((0.0, 0.0, 0.0))
            pose_bone.scale = Vector((1.0, 1.0, 1.0))
            pose_bone.matrix_basis = Matrix.Identity(4)
            cleared_count += 1

        armature_object.data.update_tag()
        armature_object.update_tag()

    return cleared_count


def restore_pose_state(scene: bpy.types.Scene, compiled_scene, pose_state: dict | None) -> int:
    if compiled_scene is None or not pose_state:
        return 0

    component_lookup = _component_armature_lookup(compiled_scene)
    armature_cache = {}
    restored_count = 0

    for chain in compiled_scene.bone_chains:
        armature_name = component_lookup.get(chain.component_id, chain.armature_name)
        if armature_name not in armature_cache:
            armature_cache[armature_name] = resolve_armature_object(scene, armature_name)
        armature_object = armature_cache[armature_name]
        if armature_object is None or armature_object.pose is None:
            continue

        for bone in chain.bones:
            state = pose_state.get((chain.component_id, bone.name))
            if state is None:
                continue

            pose_bone = armature_object.pose.bones.get(bone.name)
            if pose_bone is None:
                continue

            if pose_bone.rotation_mode != "QUATERNION":
                pose_bone.rotation_mode = "QUATERNION"
            pose_bone.location = Vector((0.0, 0.0, 0.0))
            pose_bone.rotation_quaternion = Quaternion((1.0, 0.0, 0.0, 0.0))
            pose_bone.rotation_euler = Vector((0.0, 0.0, 0.0))
            pose_bone.scale = Vector((1.0, 1.0, 1.0))
            pose_bone.matrix_basis = Matrix.Identity(4)
            pose_bone.location = Vector(state["location"])
            pose_bone.rotation_quaternion = Quaternion(state["rotation_quaternion"])
            restored_count += 1

        armature_object.data.update_tag()
        armature_object.update_tag()

    return restored_count


def _multiply_quaternion(a: Quaternion, b: Quaternion) -> Quaternion:
    return Quaternion((
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    ))


def apply_runtime_transforms_to_scene(scene: bpy.types.Scene, compiled_scene, transforms: list[dict], pose_baseline: dict | None = None) -> dict:
    component_lookup = _component_armature_lookup(compiled_scene)
    armature_cache = {}
    touched_armatures = set()
    applied_count = 0
    missing_bone_count = 0
    missing_armature_count = 0
    pose_baseline = pose_baseline or {}

    for transform in transforms:
        component_id = transform.get("component_id", "")
        armature_name = transform.get("armature_name") or component_lookup.get(component_id)
        if not armature_name:
            missing_armature_count += 1
            continue

        if armature_name not in armature_cache:
            armature_cache[armature_name] = resolve_armature_object(scene, armature_name)
        armature_object = armature_cache[armature_name]
        if armature_object is None or armature_object.pose is None:
            missing_armature_count += 1
            continue

        bone_name = transform.get("bone_name", "")
        if bone_name.endswith(_TAIL_TIP_SUFFIX):
            continue
        pose_bone = armature_object.pose.bones.get(bone_name)
        if pose_bone is None:
            missing_bone_count += 1
            continue

        translation = Vector(transform.get("translation", (0.0, 0.0, 0.0)))
        rotation_delta = Quaternion(transform.get("rotation_quaternion", (1.0, 0.0, 0.0, 0.0)))
        baseline = pose_baseline.get((component_id, bone_name))
        if baseline is None:
            baseline_location = Vector((0.0, 0.0, 0.0))
            baseline_rotation = Quaternion((1.0, 0.0, 0.0, 0.0))
        else:
            baseline_location = Vector(baseline["location"])
            baseline_rotation = Quaternion(baseline["rotation_quaternion"])

        if pose_bone.rotation_mode != "QUATERNION":
            pose_bone.rotation_mode = "QUATERNION"
        pose_bone.location = baseline_location + translation
        pose_bone.rotation_quaternion = _multiply_quaternion(baseline_rotation, rotation_delta)

        touched_armatures.add(armature_object.name)
        applied_count += 1

    for armature_name in touched_armatures:
        armature_object = armature_cache.get(armature_name)
        if armature_object is not None:
            armature_object.update_tag()

    return {
        "applied_count": applied_count,
        "missing_bone_count": missing_bone_count,
        "missing_armature_count": missing_armature_count,
        "armature_count": len(touched_armatures),
    }
