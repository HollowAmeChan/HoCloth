from .compiled import (
    CompiledCollisionBinding,
    CompiledCollisionObject,
    CompiledCollider,
    CompiledColliderGroup,
    CompiledScene,
    CompiledSpringBone,
    CompiledSpringJoint,
    SimulationCacheDescriptor,
)
from .spring_prebuild import (
    build_spring_rest_joints,
    build_topology_prebuild,
    resolve_armature_object,
    resolve_bone_chain_branching_names,
    resolve_bone_chain_names,
)
from ..components.properties import _parse_component_id_list, find_component_by_id


def _spring_joint_override_map(typed_item) -> dict[str, object]:
    return {
        item.bone_name: item
        for item in typed_item.joint_overrides
        if item.bone_name
    }


def _apply_joint_parameters(compiled_bones: list[CompiledSpringJoint], typed_item) -> list[CompiledSpringJoint]:
    overrides = _spring_joint_override_map(typed_item)
    configured_bones: list[CompiledSpringJoint] = []

    for joint in compiled_bones:
        override = overrides.get(joint.name)
        use_override = override is not None and override.enabled
        configured_bones.append(
            CompiledSpringJoint(
                name=joint.name,
                parent_index=joint.parent_index,
                depth=joint.depth,
                length=joint.length,
                radius=float(override.radius) if use_override else float(typed_item.joint_radius),
                stiffness=float(override.stiffness) if use_override else float(typed_item.stiffness),
                damping=float(override.damping) if use_override else float(typed_item.damping),
                drag=float(override.drag) if use_override else float(typed_item.drag),
                gravity_scale=float(override.gravity_scale) if use_override else 1.0,
                rest_head_local=joint.rest_head_local,
                rest_tail_local=joint.rest_tail_local,
                rest_local_translation=joint.rest_local_translation,
                rest_local_rotation=joint.rest_local_rotation,
            )
        )

    return configured_bones


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


def _compiled_collision_object_from_collider(compiled_collider: CompiledCollider) -> CompiledCollisionObject:
    return CompiledCollisionObject(
        collision_object_id=f"collision::{compiled_collider.component_id}",
        owner_component_id=compiled_collider.component_id,
        motion_type="KINEMATIC",
        shape_type=compiled_collider.shape_type,
        world_translation=compiled_collider.world_translation,
        world_rotation=compiled_collider.world_rotation,
        radius=float(compiled_collider.radius),
        height=float(compiled_collider.height),
        source_object_name=compiled_collider.object_name,
    )


def _resolve_component(scene, component_type: str, component_id: str):
    if component_type in {"SPRING_BONE", "BONE_CHAIN"}:
        return find_component_by_id(scene.hocloth_spring_bone_components, component_id)
    if component_type == "COLLIDER":
        return find_component_by_id(scene.hocloth_collider_components, component_id)
    if component_type == "COLLIDER_GROUP":
        return find_component_by_id(scene.hocloth_collider_group_components, component_id)
    if component_type == "CACHE_OUTPUT":
        return find_component_by_id(scene.hocloth_cache_output_components, component_id)
    return None


def _build_topology_hash(source_object) -> str:
    if source_object is None:
        return ""
    mesh_data = getattr(source_object, "data", None)
    if source_object.type == "MESH" and mesh_data is not None:
        return f"{source_object.name}:{len(mesh_data.vertices)}:{len(mesh_data.edges)}:{len(mesh_data.polygons)}"
    return f"{source_object.name}:{source_object.type}"


def compile_scene_from_components(scene) -> CompiledScene:
    compiled = CompiledScene()
    compiled_collider_ids: set[str] = set()
    collision_object_id_by_collider_id: dict[str, str] = {}
    deferred_collider_groups: list[tuple[str, object]] = []
    deferred_cache_outputs: list[tuple[str, object]] = []

    for item in scene.hocloth_components:
        if not item.enabled:
            continue

        if item.component_type in {"SPRING_BONE", "BONE_CHAIN"}:
            typed_item = _resolve_component(scene, item.component_type, item.component_id)
            if typed_item is None:
                continue
            armature_object = typed_item.armature_object
            if armature_object is None or armature_object.type != "ARMATURE":
                continue

            bone_names = resolve_bone_chain_names(scene, armature_object, typed_item.root_bone_name)
            compiled_bones, armature_scale = build_spring_rest_joints(armature_object, bone_names) if bone_names else ([], (1.0, 1.0, 1.0))
            configured_bones = _apply_joint_parameters(compiled_bones, typed_item)
            topology_prebuild = build_topology_prebuild(
                armature_object,
                configured_bones,
                bool(getattr(typed_item, "append_tail_tip", False)),
            )
            compiled.spring_bones.append(
                CompiledSpringBone(
                    component_id=item.component_id,
                    armature_name=armature_object.name,
                    root_bone_name=typed_item.root_bone_name,
                    center_object_name=(
                        typed_item.center_object.name
                        if typed_item.center_source == "OBJECT" and typed_item.center_object is not None
                        else (
                            typed_item.center_armature_object.name
                            if typed_item.center_source == "BONE" and typed_item.center_armature_object is not None
                            else ""
                        )
                    ),
                    center_bone_name=typed_item.center_bone_name if typed_item.center_source == "BONE" else "",
                    joint_radius=float(typed_item.joint_radius),
                    collider_group_ids=_parse_component_id_list(typed_item.collider_group_ids),
                    collision_binding_ids=_parse_component_id_list(typed_item.collider_group_ids),
                    stiffness=typed_item.stiffness,
                    damping=typed_item.damping,
                    drag=typed_item.drag,
                    gravity_strength=typed_item.gravity_strength,
                    gravity_direction=tuple(typed_item.gravity_direction),
                    armature_scale=armature_scale,
                    joints=topology_prebuild.joints,
                    lines=topology_prebuild.lines,
                    baselines=topology_prebuild.baselines,
                    line_start_indices=topology_prebuild.line_start_indices,
                    line_counts=topology_prebuild.line_counts,
                    line_data=topology_prebuild.line_data,
                    baseline_start_indices=topology_prebuild.baseline_start_indices,
                    baseline_counts=topology_prebuild.baseline_counts,
                    baseline_data=topology_prebuild.baseline_data,
                )
            )
            continue

        if item.component_type == "COLLIDER":
            typed_item = _resolve_component(scene, item.component_type, item.component_id)
            if typed_item is None:
                continue
            compiled_collider = _compile_collider(item, typed_item)
            if compiled_collider is not None:
                compiled.colliders.append(compiled_collider)
                compiled_collider_ids.add(compiled_collider.component_id)
                collision_object = _compiled_collision_object_from_collider(compiled_collider)
                compiled.collision_objects.append(collision_object)
                collision_object_id_by_collider_id[compiled_collider.component_id] = collision_object.collision_object_id
            continue

        if item.component_type == "COLLIDER_GROUP":
            typed_item = _resolve_component(scene, item.component_type, item.component_id)
            if typed_item is not None:
                deferred_collider_groups.append((item.component_id, typed_item))
            continue

        if item.component_type == "CACHE_OUTPUT":
            typed_item = _resolve_component(scene, item.component_type, item.component_id)
            if typed_item is not None:
                deferred_cache_outputs.append((item.component_id, typed_item))

    for component_id, typed_item in deferred_collider_groups:
        collider_ids = [
            collider_id
            for collider_id in _parse_component_id_list(typed_item.collider_ids)
            if collider_id in compiled_collider_ids
        ]
        compiled.collider_groups.append(
            CompiledColliderGroup(
                component_id=component_id,
                collider_ids=collider_ids,
            )
        )
        compiled.collision_bindings.append(
            CompiledCollisionBinding(
                binding_id=component_id,
                owner_component_id=component_id,
                source_group_ids=[component_id],
                collision_object_ids=[
                    collision_object_id_by_collider_id[collider_id]
                    for collider_id in collider_ids
                    if collider_id in collision_object_id_by_collider_id
                ],
            )
        )

    if compiled.colliders and not compiled.collider_groups:
        compiled.collider_groups.append(
            CompiledColliderGroup(
                component_id="__auto_all_colliders__",
                collider_ids=[collider.component_id for collider in compiled.colliders],
            )
        )
        compiled.collision_bindings.append(
            CompiledCollisionBinding(
                binding_id="__auto_all_colliders__",
                owner_component_id="__auto_all_colliders__",
                source_group_ids=["__auto_all_colliders__"],
                collision_object_ids=[
                    collision_object_id_by_collider_id[collider.component_id]
                    for collider in compiled.colliders
                    if collider.component_id in collision_object_id_by_collider_id
                ],
            )
        )

    if compiled.collider_groups:
        all_group_ids = {group.component_id for group in compiled.collider_groups}
        all_binding_ids = {binding.binding_id for binding in compiled.collision_bindings}
        for spring_bone in compiled.spring_bones:
            if spring_bone.collider_group_ids:
                spring_bone.collider_group_ids = [
                    group_id for group_id in spring_bone.collider_group_ids if group_id in all_group_ids
                ]
                spring_bone.collision_binding_ids = [
                    binding_id for binding_id in spring_bone.collision_binding_ids if binding_id in all_binding_ids
                ]
            elif "__auto_all_colliders__" in all_group_ids:
                spring_bone.collider_group_ids = ["__auto_all_colliders__"]
                spring_bone.collision_binding_ids = ["__auto_all_colliders__"]

    for component_id, typed_item in deferred_cache_outputs:
        source_object = typed_item.source_object
        compiled.cache_descriptors.append(
            SimulationCacheDescriptor(
                component_id=component_id,
                source_object_name=source_object.name if source_object is not None else "",
                topology_hash=_build_topology_hash(source_object),
                cache_format=typed_item.cache_format,
                cache_path=typed_item.cache_path,
            )
        )

    return compiled
