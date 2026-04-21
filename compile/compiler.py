from .compiled import CompiledBoneChain, CompiledScene


def _collect_bone_subtree_names(bone) -> list[str]:
    names = [bone.name]
    for child in bone.children:
        names.extend(_collect_bone_subtree_names(child))
    return names


def _resolve_bone_chain(scene, armature_name: str, root_bone_name: str) -> list[str]:
    armature_object = scene.objects.get(armature_name)
    if armature_object is None or armature_object.type != "ARMATURE":
        return []

    root_bone = armature_object.data.bones.get(root_bone_name)
    if root_bone is None:
        return []

    return _collect_bone_subtree_names(root_bone)


def resolve_bone_chain_names(scene, armature_name: str, root_bone_name: str) -> list[str]:
    return _resolve_bone_chain(scene, armature_name, root_bone_name)


def compile_scene_from_components(scene) -> CompiledScene:
    compiled = CompiledScene()

    for item in scene.hocloth_components:
        if not item.enabled or item.component_type != "BONE_CHAIN" or item.container_index < 0:
            continue

        typed_item = scene.hocloth_bone_chain_components[item.container_index]
        bone_names = _resolve_bone_chain(scene, typed_item.armature_name, typed_item.root_bone_name)
        compiled.bone_chains.append(
            CompiledBoneChain(
                component_id=item.component_id,
                armature_name=typed_item.armature_name,
                root_bone_name=typed_item.root_bone_name,
                bone_names=bone_names,
            )
        )

    return compiled
