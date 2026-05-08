from __future__ import annotations


def _linear_chain_children(bone) -> list:
    return sorted(bone.children, key=lambda child: child.name)


def _resolve_bone_subtree(root_bone) -> tuple[list[str], list[str]]:
    names = []
    branching_bones = []
    stack = [root_bone]
    while stack:
        bone = stack.pop()
        names.append(bone.name)
        children = _linear_chain_children(bone)
        if len(children) > 1:
            branching_bones.append(bone.name)
        stack.extend(reversed(children))
    return names, branching_bones


def resolve_armature_object(scene, armature_reference):
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


def resolve_bone_chain_names(scene, armature_reference, root_bone_name: str) -> list[str]:
    armature_object = resolve_armature_object(scene, armature_reference)
    if armature_object is None or not root_bone_name:
        return []

    root_bone = armature_object.data.bones.get(root_bone_name)
    if root_bone is None:
        return []

    return _resolve_bone_subtree(root_bone)[0]


def resolve_bone_forest_names(scene, armature_reference, root_bone_names: list[str]) -> list[str]:
    armature_object = resolve_armature_object(scene, armature_reference)
    if armature_object is None:
        return []

    names = []
    seen = set()
    for root_bone_name in root_bone_names:
        root_bone = armature_object.data.bones.get(root_bone_name)
        if root_bone is None:
            continue
        for bone_name in _resolve_bone_subtree(root_bone)[0]:
            if bone_name not in seen:
                names.append(bone_name)
                seen.add(bone_name)
    return names


def resolve_bone_chain_branching_names(scene, armature_reference, root_bone_name: str) -> list[str]:
    armature_object = resolve_armature_object(scene, armature_reference)
    if armature_object is None or not root_bone_name:
        return []

    root_bone = armature_object.data.bones.get(root_bone_name)
    if root_bone is None:
        return []

    return _resolve_bone_subtree(root_bone)[1]
