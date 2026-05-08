import bpy
from mathutils import Matrix, Quaternion, Vector

from .blender_refs import resolve_armature_object
from .exchange import unwrap_payload


def _snapshot_payload(authoring_snapshot: dict | None) -> dict:
    return unwrap_payload(authoring_snapshot, "authoring_snapshot") if authoring_snapshot else {}


def capture_pose_baseline(scene: bpy.types.Scene, authoring_snapshot: dict | None) -> dict:
    payload = _snapshot_payload(authoring_snapshot)
    armature_cache = {}
    baseline = {}

    for chain in payload.get("bone_chains", []):
        component_id = chain.get("component_id", "")
        armature_name = chain.get("armature_name", "")
        if armature_name not in armature_cache:
            armature_cache[armature_name] = resolve_armature_object(scene, armature_name)
        armature_object = armature_cache[armature_name]
        if armature_object is None or armature_object.pose is None:
            continue

        for bone in chain.get("bones", []):
            bone_name = bone.get("name", "")
            pose_bone = armature_object.pose.bones.get(bone_name)
            if pose_bone is None:
                continue

            if pose_bone.rotation_mode != "QUATERNION":
                pose_bone.rotation_mode = "QUATERNION"

            baseline[(component_id, bone_name)] = {
                "location": tuple(pose_bone.location),
                "rotation_quaternion": tuple(pose_bone.rotation_quaternion),
            }

    return baseline


def capture_pose_state(scene: bpy.types.Scene, authoring_snapshot: dict | None) -> dict:
    return capture_pose_baseline(scene, authoring_snapshot)


def clear_pose_transforms(scene: bpy.types.Scene, authoring_snapshot: dict | None) -> int:
    payload = _snapshot_payload(authoring_snapshot)
    armature_cache = {}
    cleared_count = 0

    for chain in payload.get("bone_chains", []):
        armature_name = chain.get("armature_name", "")
        if armature_name not in armature_cache:
            armature_cache[armature_name] = resolve_armature_object(scene, armature_name)
        armature_object = armature_cache[armature_name]
        if armature_object is None or armature_object.pose is None:
            continue

        for bone in chain.get("bones", []):
            pose_bone = armature_object.pose.bones.get(bone.get("name", ""))
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


def restore_pose_state(scene: bpy.types.Scene, authoring_snapshot: dict | None, pose_state: dict | None) -> int:
    if not pose_state:
        return 0

    payload = _snapshot_payload(authoring_snapshot)
    armature_cache = {}
    restored_count = 0

    for chain in payload.get("bone_chains", []):
        component_id = chain.get("component_id", "")
        armature_name = chain.get("armature_name", "")
        if armature_name not in armature_cache:
            armature_cache[armature_name] = resolve_armature_object(scene, armature_name)
        armature_object = armature_cache[armature_name]
        if armature_object is None or armature_object.pose is None:
            continue

        for bone in chain.get("bones", []):
            bone_name = bone.get("name", "")
            state = pose_state.get((component_id, bone_name))
            if state is None:
                continue

            pose_bone = armature_object.pose.bones.get(bone_name)
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


def _pose_local_matrix(pose_bone) -> Matrix:
    if pose_bone.parent is not None:
        return pose_bone.parent.matrix.inverted_safe() @ pose_bone.matrix
    return pose_bone.matrix.copy()


def _compose_pose_local_matrix(pose_bone, local_rotation: Quaternion) -> Matrix:
    current_local = _pose_local_matrix(pose_bone)
    translation = Matrix.Translation(current_local.to_translation())
    scale = Matrix.Diagonal((*current_local.to_scale(), 1.0))
    return translation @ local_rotation.to_matrix().to_4x4() @ scale


def _set_pose_from_mc2_local_rotation(pose_bone, local_rotation: Quaternion, translation: Vector) -> bool:
    target_local = _compose_pose_local_matrix(pose_bone, local_rotation)
    if translation.length_squared > 0.0:
        target_local.translation = target_local.to_translation() + translation
    target_pose = pose_bone.parent.matrix @ target_local if pose_bone.parent is not None else target_local

    pose_bone.matrix = target_pose
    return True


def _set_pose_from_mc2_world_rotation(
    armature_object,
    pose_bone,
    world_rotation: Quaternion,
    translation: Vector,
) -> bool:
    current_world = armature_object.matrix_world @ pose_bone.matrix
    target_world = (
        Matrix.Translation(current_world.to_translation())
        @ world_rotation.to_matrix().to_4x4()
        @ Matrix.Diagonal((*current_world.to_scale(), 1.0))
    )
    if translation.length_squared > 0.0:
        target_world.translation = target_world.to_translation() + translation
    pose_bone.matrix = armature_object.matrix_world.inverted_safe() @ target_world
    return True


def apply_runtime_transforms_to_scene(scene: bpy.types.Scene, transforms: list[dict], pose_baseline: dict | None = None) -> dict:
    armature_cache = {}
    touched_armatures = set()
    applied_count = 0
    world_write_applied_count = 0
    local_write_applied_count = 0
    missing_bone_count = 0
    missing_armature_count = 0
    pose_baseline = pose_baseline or {}

    for transform in transforms:
        component_id = transform.get("component_id", "")
        armature_name = transform.get("armature_name", "")
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
        write_mode = str(transform.get("write_mode", "") or "")
        if write_mode == "mc2_world_rotation":
            continue
        elif write_mode == "mc2_local_rotation":
            if not _set_pose_from_mc2_local_rotation(pose_bone, rotation_delta, translation):
                continue
            local_write_applied_count += 1
        else:
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
        "world_write_applied_count": world_write_applied_count,
        "local_write_applied_count": local_write_applied_count,
        "missing_bone_count": missing_bone_count,
        "missing_armature_count": missing_armature_count,
        "armature_count": len(touched_armatures),
    }


def _mesh_output_object_name(mesh_output: dict) -> str:
    return (
        mesh_output.get("object_name")
        or mesh_output.get("source_object_name")
        or mesh_output.get("mesh_object_name")
        or ""
    )


def _mesh_output_positions(mesh_output: dict) -> list:
    positions = mesh_output.get("positions")
    if positions is None:
        positions = mesh_output.get("vertices")
    if positions is None:
        positions = mesh_output.get("local_positions")
    return list(positions or [])


def _coerce_vector3(value) -> Vector:
    return Vector((float(value[0]), float(value[1]), float(value[2])))


def apply_runtime_mesh_outputs_to_scene(scene: bpy.types.Scene, mesh_outputs: list[dict]) -> dict:
    applied_vertex_count = 0
    applied_mesh_count = 0
    missing_object_count = 0
    topology_mismatch_count = 0

    for mesh_output in mesh_outputs or []:
        object_name = _mesh_output_object_name(mesh_output)
        if not object_name:
            missing_object_count += 1
            continue

        obj = bpy.data.objects.get(object_name)
        if obj is None or obj.type != "MESH" or obj.data is None:
            missing_object_count += 1
            continue

        positions = _mesh_output_positions(mesh_output)
        vertex_count = len(obj.data.vertices)
        if len(positions) != vertex_count:
            topology_mismatch_count += 1
            continue

        space = str(mesh_output.get("space", "object_local")).lower()
        to_local = obj.matrix_world.inverted() if space in {"world", "blender_world"} else None
        for vertex, position in zip(obj.data.vertices, positions):
            local_position = _coerce_vector3(position)
            if to_local is not None:
                local_position = to_local @ local_position
            vertex.co = local_position
            applied_vertex_count += 1

        obj.data.update()
        obj.update_tag()
        applied_mesh_count += 1

    return {
        "applied_mesh_count": applied_mesh_count,
        "applied_vertex_count": applied_vertex_count,
        "missing_object_count": missing_object_count,
        "topology_mismatch_count": topology_mismatch_count,
    }
