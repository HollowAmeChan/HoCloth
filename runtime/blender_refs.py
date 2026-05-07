from __future__ import annotations


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
