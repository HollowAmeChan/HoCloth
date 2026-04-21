from dataclasses import dataclass

import bpy


@dataclass
class ExtractedBoneChain:
    armature_name: str
    object_name: str
    root_bone_name: str
    bone_names: list[str]


def _active_armature_object(context: bpy.types.Context):
    obj = context.object
    if obj and obj.type == "ARMATURE":
        return obj
    return None


def _collect_bone_subtree_names(bone) -> list[str]:
    names = [bone.name]
    for child in bone.children:
        names.extend(_collect_bone_subtree_names(child))
    return names


def extract_active_bone_chain(context: bpy.types.Context) -> ExtractedBoneChain:
    armature_object = _active_armature_object(context)
    if armature_object is None:
        raise RuntimeError("Active object must be an armature.")

    if context.mode == "POSE":
        active_bone = context.active_pose_bone
    elif context.mode == "EDIT_ARMATURE":
        active_bone = context.active_bone
    else:
        active_bone = None

    if active_bone is None:
        raise RuntimeError("Select one active root bone in Pose or Edit mode.")

    root_bone = armature_object.data.bones.get(active_bone.name)
    if root_bone is None:
        raise RuntimeError("Active bone could not be resolved from armature data.")

    bone_names = _collect_bone_subtree_names(root_bone)

    return ExtractedBoneChain(
        armature_name=armature_object.data.name,
        object_name=armature_object.name,
        root_bone_name=root_bone.name,
        bone_names=bone_names,
    )
