from dataclasses import dataclass, field

from mathutils import Matrix, Quaternion, Vector

from .compiled import CompiledSpringBaseline, CompiledSpringJoint, CompiledSpringLine


_TAIL_TIP_SUFFIX = "__hocloth_tail_tip__"


@dataclass
class SpringTopologyPrebuild:
    bone_names: list[str] = field(default_factory=list)
    joints: list[CompiledSpringJoint] = field(default_factory=list)
    lines: list[CompiledSpringLine] = field(default_factory=list)
    baselines: list[CompiledSpringBaseline] = field(default_factory=list)
    line_start_indices: list[int] = field(default_factory=list)
    line_counts: list[int] = field(default_factory=list)
    line_data: list[int] = field(default_factory=list)
    baseline_start_indices: list[int] = field(default_factory=list)
    baseline_counts: list[int] = field(default_factory=list)
    baseline_data: list[int] = field(default_factory=list)
    armature_scale: tuple[float, float, float] = (1.0, 1.0, 1.0)


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


def resolve_bone_chain_names(scene, armature_name: str, root_bone_name: str) -> list[str]:
    armature_object = resolve_armature_object(scene, armature_name)
    if armature_object is None or not root_bone_name:
        return []

    root_bone = armature_object.data.bones.get(root_bone_name)
    if root_bone is None:
        return []

    return _resolve_bone_subtree(root_bone)[0]


def resolve_bone_chain_branching_names(scene, armature_name: str, root_bone_name: str) -> list[str]:
    armature_object = resolve_armature_object(scene, armature_name)
    if armature_object is None or not root_bone_name:
        return []

    root_bone = armature_object.data.bones.get(root_bone_name)
    if root_bone is None:
        return []

    return _resolve_bone_subtree(root_bone)[1]


def build_spring_rest_joints(armature_object, bone_names: list[str]) -> tuple[list[CompiledSpringJoint], tuple[float, float, float]]:
    name_to_index = {name: index for index, name in enumerate(bone_names)}
    compiled_bones: list[CompiledSpringJoint] = []
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
            CompiledSpringJoint(
                name=bone_name,
                parent_index=name_to_index[parent_name] if parent_name else -1,
                depth=(
                    compiled_bones[name_to_index[parent_name]].depth + 1
                    if parent_name and name_to_index[parent_name] < len(compiled_bones)
                    else 0
                ),
                length=_scaled_distance(bone.head_local, bone.tail_local, armature_scale),
                radius=0.0,
                stiffness=0.0,
                damping=0.0,
                drag=0.0,
                gravity_scale=1.0,
                rest_head_local=chain_space["head_local"],
                rest_tail_local=chain_space["tail_local"],
                rest_local_translation=chain_space["local_translation"],
                rest_local_rotation=chain_space["local_rotation"],
            )
        )
    return compiled_bones, armature_scale


def _child_indices_by_joint(compiled_bones: list[CompiledSpringJoint]) -> dict[int, list[int]]:
    child_indices: dict[int, list[int]] = {index: [] for index in range(len(compiled_bones))}
    for index, joint in enumerate(compiled_bones):
        if joint.parent_index >= 0:
            child_indices.setdefault(joint.parent_index, []).append(index)
    return child_indices


def append_tail_tip_joints(compiled_bones: list[CompiledSpringJoint], append_tail_tip: bool) -> list[CompiledSpringJoint]:
    if not append_tail_tip or not compiled_bones:
        return compiled_bones

    joints = list(compiled_bones)
    child_indices = _child_indices_by_joint(joints)
    leaf_indices = [index for index in range(len(joints)) if not child_indices.get(index)]
    for leaf_index in leaf_indices:
        leaf_joint = joints[leaf_index]
        head = Vector(leaf_joint.rest_head_local)
        tail = Vector(leaf_joint.rest_tail_local)
        direction = tail - head
        if direction.length <= 1.0e-6:
            continue

        tip_head = tail
        tip_tail = tail + direction
        joints.append(
            CompiledSpringJoint(
                name=f"{leaf_joint.name}{_TAIL_TIP_SUFFIX}",
                parent_index=leaf_index,
                depth=leaf_joint.depth + 1,
                length=float(leaf_joint.length),
                radius=float(leaf_joint.radius),
                stiffness=float(leaf_joint.stiffness),
                damping=float(leaf_joint.damping),
                drag=float(leaf_joint.drag),
                gravity_scale=float(leaf_joint.gravity_scale),
                rest_head_local=(float(tip_head.x), float(tip_head.y), float(tip_head.z)),
                rest_tail_local=(float(tip_tail.x), float(tip_tail.y), float(tip_tail.z)),
                rest_local_translation=(
                    float(tip_head.x - head.x),
                    float(tip_head.y - head.y),
                    float(tip_head.z - head.z),
                ),
                rest_local_rotation=leaf_joint.rest_local_rotation,
            )
        )
    return joints


def build_topology_prebuild(
    armature_object,
    configured_joints: list[CompiledSpringJoint],
    append_tail_tip: bool,
) -> SpringTopologyPrebuild:
    final_joints = append_tail_tip_joints(configured_joints, append_tail_tip)
    lines = [
        CompiledSpringLine(start_index=joint.parent_index, end_index=index)
        for index, joint in enumerate(final_joints)
        if joint.parent_index >= 0
    ]
    child_indices = _child_indices_by_joint(final_joints)
    line_start_indices: list[int] = []
    line_counts: list[int] = []
    line_data: list[int] = []
    for joint_index in range(len(final_joints)):
        children = list(child_indices.get(joint_index, []))
        line_start_indices.append(len(line_data))
        line_counts.append(len(children))
        line_data.extend(children)
    leaf_indices = [index for index in range(len(final_joints)) if not child_indices.get(index)]
    baselines: list[CompiledSpringBaseline] = []
    baseline_start_indices: list[int] = []
    baseline_counts: list[int] = []
    baseline_data: list[int] = []
    for leaf_index in leaf_indices:
        path = []
        current_index = leaf_index
        while current_index >= 0:
            path.append(current_index)
            current_index = final_joints[current_index].parent_index
        joint_indices = list(reversed(path))
        baselines.append(CompiledSpringBaseline(joint_indices=joint_indices))
        baseline_start_indices.append(len(baseline_data))
        baseline_counts.append(len(joint_indices))
        baseline_data.extend(joint_indices)

    return SpringTopologyPrebuild(
        bone_names=[joint.name for joint in final_joints],
        joints=final_joints,
        lines=lines,
        baselines=baselines,
        line_start_indices=line_start_indices,
        line_counts=line_counts,
        line_data=line_data,
        baseline_start_indices=baseline_start_indices,
        baseline_counts=baseline_counts,
        baseline_data=baseline_data,
        armature_scale=tuple(float(axis) for axis in armature_object.matrix_world.to_scale()),
    )
