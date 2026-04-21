from .compiled import CompiledBone, CompiledBoneChain, CompiledCollider, CompiledScene


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
    for bone_name in bone_names:
        bone = armature_object.data.bones.get(bone_name)
        parent_name = bone.parent.name if bone and bone.parent and bone.parent.name in name_to_index else None
        if bone is None:
            continue

        if bone.parent and bone.parent.name in name_to_index:
            local_matrix = bone.parent.matrix_local.inverted() @ bone.matrix_local
        else:
            local_matrix = bone.matrix_local.copy()

        compiled_bones.append(
            CompiledBone(
                name=bone_name,
                parent_index=name_to_index[parent_name] if parent_name else -1,
                length=(bone.tail_local - bone.head_local).length,
                rest_head_local=tuple(bone.head_local),
                rest_tail_local=tuple(bone.tail_local),
                rest_local_translation=tuple(local_matrix.to_translation()),
                rest_local_rotation=tuple(local_matrix.to_quaternion()),
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
