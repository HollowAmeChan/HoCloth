from mathutils import Matrix, Quaternion, Vector

from .compiled import CompiledBone, CompiledBoneChain, CompiledCollider, CompiledScene


def _scale_vector(value, scale) -> tuple[float, float, float]:
    return (
        float(value[0]) * float(scale[0]),
        float(value[1]) * float(scale[1]),
        float(value[2]) * float(scale[2]),
    )


def _scaled_distance(a, b, scale) -> float:
    delta = _scale_vector((
        float(b[0]) - float(a[0]),
        float(b[1]) - float(a[1]),
        float(b[2]) - float(a[2]),
    ), scale)
    return (delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]) ** 0.5


def _tuple_to_vector(value) -> Vector:
    return Vector((float(value[0]), float(value[1]), float(value[2])))


def _vector_to_tuple(value: Vector) -> tuple[float, float, float]:
    return (float(value.x), float(value.y), float(value.z))


def _quaternion_to_tuple(value: Quaternion) -> tuple[float, float, float, float]:
    return (float(value.w), float(value.x), float(value.y), float(value.z))


def _segment_direction(head_local, tail_local, fallback: Vector | None = None) -> Vector:
    direction = _tuple_to_vector(tail_local) - _tuple_to_vector(head_local)
    if direction.length <= 1.0e-6:
        return fallback.copy() if fallback is not None else Vector((0.0, 1.0, 0.0))
    return direction.normalized()


def _direction_to_rest_rotation(direction: Vector) -> tuple[float, float, float, float]:
    canonical_axis = Vector((0.0, 1.0, 0.0))
    rotation = canonical_axis.rotation_difference(direction.normalized())
    return (float(rotation.w), float(rotation.x), float(rotation.y), float(rotation.z))


def _transform_point(matrix: Matrix, point) -> Vector:
    return matrix @ Vector((float(point[0]), float(point[1]), float(point[2]), 1.0))


def _scale_chain_point(point: Vector, scale) -> tuple[float, float, float]:
    return (
        float(point.x) * float(scale[0]),
        float(point.y) * float(scale[1]),
        float(point.z) * float(scale[2]),
    )


def _chain_space_local_translation(
    head_local: tuple[float, float, float],
    parent_head_local: tuple[float, float, float] | None,
) -> tuple[float, float, float]:
    if parent_head_local is None:
        return (0.0, 0.0, 0.0)
    return _vector_to_tuple(_tuple_to_vector(head_local) - _tuple_to_vector(parent_head_local))


def _chain_space_local_rotation(
    bone,
    parent_bone,
    root_rotation_inv: Quaternion,
) -> tuple[float, float, float, float]:
    current_global = root_rotation_inv @ bone.matrix_local.to_quaternion()
    if parent_bone is None:
        return _quaternion_to_tuple(current_global)

    parent_global = root_rotation_inv @ parent_bone.matrix_local.to_quaternion()
    local_rotation = parent_global.conjugated() @ current_global
    return _quaternion_to_tuple(local_rotation)


def _extract_chain_space_rest_data(bone, parent_bone, root_matrix_inv: Matrix, root_rotation_inv: Quaternion, armature_scale) -> dict:
    head_root = _transform_point(root_matrix_inv, bone.head_local).to_3d()
    tail_root = _transform_point(root_matrix_inv, bone.tail_local).to_3d()
    scaled_head = _scale_chain_point(head_root, armature_scale)
    scaled_tail = _scale_chain_point(tail_root, armature_scale)
    parent_head = None
    if parent_bone is not None:
        parent_head_root = _transform_point(root_matrix_inv, parent_bone.head_local).to_3d()
        parent_head = _scale_chain_point(parent_head_root, armature_scale)

    return {
        "head_local": scaled_head,
        "tail_local": scaled_tail,
        "local_translation": _chain_space_local_translation(scaled_head, parent_head),
        "local_rotation": _chain_space_local_rotation(bone, parent_bone, root_rotation_inv),
    }


def _collect_bone_subtree_names(bone) -> list[str]:
    names = [bone.name]
    for child in bone.children:
        names.extend(_collect_bone_subtree_names(child))
    return names


def resolve_armature_object(scene, armature_reference: str):
    if armature_reference is None:
        return None

    if hasattr(armature_reference, "type"):
        return armature_reference if armature_reference.type == "ARMATURE" else None

    armature_object = scene.objects.get(armature_reference)
    if armature_object is not None and armature_object.type == "ARMATURE":
        return armature_object

    for obj in scene.objects:
        if obj.type == "ARMATURE" and obj.data and obj.data.name == armature_reference:
            return obj

    return None


def _resolve_bone_chain(scene, armature_name: str, root_bone_name: str) -> list[str]:
    armature_object = resolve_armature_object(scene, armature_name)
    if armature_object is None:
        return []

    if not root_bone_name:
        return []

    root_bone = armature_object.data.bones.get(root_bone_name)
    if root_bone is None:
        return []

    return _collect_bone_subtree_names(root_bone)


def resolve_bone_chain_names(scene, armature_name: str, root_bone_name: str) -> list[str]:
    return _resolve_bone_chain(scene, armature_name, root_bone_name)


def _build_compiled_bones(armature_object, bone_names: list[str]) -> list[CompiledBone]:
    name_to_index = {name: index for index, name in enumerate(bone_names)}
    compiled_bones: list[CompiledBone] = []
    armature_scale = tuple(float(axis) for axis in armature_object.matrix_world.to_scale())
    root_bone = armature_object.data.bones.get(bone_names[0]) if bone_names else None
    root_matrix_inv = root_bone.matrix_local.inverted() if root_bone is not None else Matrix.Identity(4)
    root_rotation_inv = root_bone.matrix_local.to_quaternion().conjugated() if root_bone is not None else Quaternion()
    for bone_name in bone_names:
        bone = armature_object.data.bones.get(bone_name)
        parent_name = bone.parent.name if bone and bone.parent and bone.parent.name in name_to_index else None
        if bone is None:
            continue

        parent_bone = bone.parent if bone.parent and bone.parent.name in name_to_index else None
        chain_space = _extract_chain_space_rest_data(
            bone,
            parent_bone,
            root_matrix_inv,
            root_rotation_inv,
            armature_scale,
        )
        compiled_bones.append(
            CompiledBone(
                name=bone_name,
                parent_index=name_to_index[parent_name] if parent_name else -1,
                length=_scaled_distance(bone.head_local, bone.tail_local, armature_scale),
                rest_head_local=chain_space["head_local"],
                rest_tail_local=chain_space["tail_local"],
                rest_local_translation=chain_space["local_translation"],
                rest_local_rotation=chain_space["local_rotation"],
            )
        )
    return compiled_bones


def _compile_collider(item, typed_item):
    collider_object = typed_item.collider_object
    if collider_object is None:
        return None

    world_matrix = collider_object.matrix_world.copy()
    return CompiledCollider(
        component_id=item.component_id,
        object_name=collider_object.name,
        shape_type=typed_item.shape_type,
        radius=typed_item.radius,
        height=typed_item.height,
        world_translation=tuple(world_matrix.to_translation()),
        world_rotation=tuple(world_matrix.to_quaternion()),
    )


def compile_scene_from_components(scene) -> CompiledScene:
    compiled = CompiledScene()

    for item in scene.hocloth_components:
        if not item.enabled or item.container_index < 0:
            continue

        if item.component_type == "BONE_CHAIN":
            if item.container_index >= len(scene.hocloth_bone_chain_components):
                continue

            typed_item = scene.hocloth_bone_chain_components[item.container_index]
            armature_object = typed_item.armature_object
            if armature_object is None or armature_object.type != "ARMATURE":
                continue

            bone_names = _resolve_bone_chain(scene, armature_object, typed_item.root_bone_name)
            compiled_bones = _build_compiled_bones(armature_object, bone_names) if armature_object else []
            compiled.bone_chains.append(
                CompiledBoneChain(
                    component_id=item.component_id,
                    armature_name=armature_object.name,
                    root_bone_name=typed_item.root_bone_name,
                    stiffness=typed_item.stiffness,
                    damping=typed_item.damping,
                    drag=typed_item.drag,
                    gravity_strength=typed_item.gravity_strength,
                    gravity_direction=tuple(typed_item.gravity_direction),
                    armature_scale=tuple(float(axis) for axis in armature_object.matrix_world.to_scale()),
                    bones=compiled_bones,
                )
            )
            continue

        if item.component_type == "COLLIDER":
            if item.container_index >= len(scene.hocloth_collider_components):
                continue

            typed_item = scene.hocloth_collider_components[item.container_index]
            compiled_collider = _compile_collider(item, typed_item)
            if compiled_collider is not None:
                compiled.colliders.append(compiled_collider)

    return compiled
