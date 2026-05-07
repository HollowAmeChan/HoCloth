from __future__ import annotations

from .bone_sampling import build_spring_rest_joints, resolve_bone_chain_names
from .compiled_scene import (
    CompiledCollisionBinding,
    CompiledCollisionObject,
    CompiledMeshWritebackTarget,
    CompiledScene,
    CompiledSpringBaseline,
    CompiledSpringBone,
    CompiledSpringJoint,
    CompiledSpringLine,
)
from ..components import mc2
from .exchange import wrap_authoring_snapshot


def _vec3(value) -> tuple[float, float, float]:
    return (float(value[0]), float(value[1]), float(value[2]))


def _quat(value) -> tuple[float, float, float, float]:
    return (float(value.w), float(value.x), float(value.y), float(value.z))


def _joint_override_map(typed_item) -> dict[str, object]:
    return {
        item.bone_name: item
        for item in getattr(typed_item, "joint_overrides", [])
        if item.bone_name
    }


def _sample_chain_joints(scene, armature_object, typed_item) -> tuple[list[dict], tuple[float, float, float]]:
    bone_names = resolve_bone_chain_names(scene, armature_object, typed_item.root_bone_name)
    joints, armature_scale = build_spring_rest_joints(armature_object, bone_names) if bone_names else ([], (1.0, 1.0, 1.0))
    overrides = _joint_override_map(typed_item)
    runtime_stiffness = float(typed_item.distance_constraint.stiffness.value)
    runtime_damping = float(typed_item.damping_curve.value)
    runtime_drag = max(0.0, min(1.0, 1.0 - float(typed_item.angle_restoration_constraint.velocity_attenuation)))
    sampled = []
    for joint in joints:
        override = overrides.get(joint.name)
        use_override = override is not None and override.enabled
        sampled.append(
            {
                "name": joint.name,
                "parent_index": int(joint.parent_index),
                "depth": int(joint.depth),
                "length": float(joint.length),
                "radius": float(override.radius) if use_override else float(typed_item.joint_radius),
                "stiffness": float(override.stiffness) if use_override else runtime_stiffness,
                "damping": float(override.damping) if use_override else runtime_damping,
                "drag": float(override.drag) if use_override else runtime_drag,
                "gravity_scale": float(override.gravity_scale) if use_override else 1.0,
                "rest_head_local": tuple(joint.rest_head_local),
                "rest_tail_local": tuple(joint.rest_tail_local),
                "rest_local_translation": tuple(joint.rest_local_translation),
                "rest_local_rotation": tuple(joint.rest_local_rotation),
            }
        )
    return sampled, tuple(float(axis) for axis in armature_scale)


def _bone_chain_snapshot(scene, item, typed_item):
    armature_object = typed_item.armature_object
    if armature_object is None or armature_object.type != "ARMATURE":
        return None

    joints, armature_scale = _sample_chain_joints(scene, armature_object, typed_item)
    center_object_name = ""
    center_bone_name = ""
    if typed_item.center_source == "OBJECT" and typed_item.center_object is not None:
        center_object_name = typed_item.center_object.name
    elif typed_item.center_source == "BONE" and typed_item.center_armature_object is not None:
        center_object_name = typed_item.center_armature_object.name
        center_bone_name = typed_item.center_bone_name

    collider_ids = mc2.parse_component_id_list(getattr(typed_item, "collider_ids", ""))
    runtime_stiffness = float(typed_item.distance_constraint.stiffness.value)
    runtime_damping = float(typed_item.damping_curve.value)
    runtime_drag = max(0.0, min(1.0, 1.0 - float(typed_item.angle_restoration_constraint.velocity_attenuation)))
    serialize_data = {
        "clothType": "BoneCloth" if typed_item.authoring_mode == "BONE_CLOTH" else "BoneSpring",
        "rootBones": [typed_item.root_bone_name] if typed_item.root_bone_name else [],
        "gravity": float(typed_item.gravity_strength),
        "gravityDirection": _vec3(typed_item.gravity_direction),
        "damping": {"value": float(typed_item.damping_curve.value)},
        "radius": {"value": float(typed_item.joint_radius)},
        "inertiaConstraint": {
            "worldInertia": float(typed_item.inertia_constraint.world_inertia),
            "movementInertiaSmoothing": float(typed_item.inertia_constraint.movement_inertia_smoothing),
            "movementSpeedLimit": {
                "use": bool(typed_item.inertia_constraint.movement_speed_limit.use),
                "value": float(typed_item.inertia_constraint.movement_speed_limit.value),
            },
            "rotationSpeedLimit": {
                "use": bool(typed_item.inertia_constraint.rotation_speed_limit.use),
                "value": float(typed_item.inertia_constraint.rotation_speed_limit.value),
            },
            "localInertia": float(typed_item.inertia_constraint.local_inertia),
            "localMovementSpeedLimit": {
                "use": bool(typed_item.inertia_constraint.local_movement_speed_limit.use),
                "value": float(typed_item.inertia_constraint.local_movement_speed_limit.value),
            },
            "localRotationSpeedLimit": {
                "use": bool(typed_item.inertia_constraint.local_rotation_speed_limit.use),
                "value": float(typed_item.inertia_constraint.local_rotation_speed_limit.value),
            },
            "depthInertia": float(typed_item.inertia_constraint.depth_inertia),
            "centrifugalAcceleration": float(typed_item.inertia_constraint.centrifugal_acceleration),
            "particleSpeedLimit": {
                "use": bool(typed_item.inertia_constraint.particle_speed_limit.use),
                "value": float(typed_item.inertia_constraint.particle_speed_limit.value),
            },
        },
        "tetherConstraint": {
            "distanceCompression": float(typed_item.tether_constraint.distance_compression),
        },
        "distanceConstraint": {
            "stiffness": {"value": float(typed_item.distance_constraint.stiffness.value)},
        },
        "angleRestorationConstraint": {
            "useAngleRestoration": bool(typed_item.angle_restoration_constraint.use_angle_restoration),
            "stiffness": {"value": float(typed_item.angle_restoration_constraint.stiffness.value)},
            "velocityAttenuation": float(typed_item.angle_restoration_constraint.velocity_attenuation),
        },
        "springConstraint": {
            "useSpring": bool(typed_item.spring_constraint.use_spring),
            "springPower": float(typed_item.spring_constraint.spring_power),
            "limitDistance": float(typed_item.spring_constraint.limit_distance),
            "normalLimitRatio": float(typed_item.spring_constraint.normal_limit_ratio),
            "springNoise": float(typed_item.spring_constraint.spring_noise),
        },
        "colliderCollisionConstraint": {
            "mode": "Point",
            "friction": float(typed_item.collider_collision_constraint.friction),
            "colliderList": list(collider_ids),
            "collisionBones": [],
            "limitDistance": {
                "value": float(typed_item.collider_collision_constraint.limit_distance.value),
            },
        },
    }

    return {
        "component_id": item.component_id,
        "component_type": "BONE_CLOTH" if typed_item.authoring_mode == "BONE_CLOTH" else "SPRING_BONE",
        "mc2_component_type": "MagicaCloth",
        "mc2_authoring_mode": "BoneCloth" if typed_item.authoring_mode == "BONE_CLOTH" else "BoneSpring",
        "cloth_type": "BoneCloth" if typed_item.authoring_mode == "BONE_CLOTH" else "BoneSpring",
        "display_name": item.display_name,
        "serialize_data": serialize_data,
        "armature_name": armature_object.name,
        "armature_data_name": armature_object.data.name if armature_object.data else "",
        "root_bone_name": typed_item.root_bone_name,
        "center_object_name": center_object_name,
        "center_bone_name": center_bone_name,
        "joint_radius": float(typed_item.joint_radius),
        "collider_ids": collider_ids,
        "collider_group_ids": [],
        "stiffness": runtime_stiffness,
        "damping": runtime_damping,
        "drag": runtime_drag,
        "damping_curve_value": float(typed_item.damping_curve.value),
        "inertia_world_inertia": float(typed_item.inertia_constraint.world_inertia),
        "inertia_movement_inertia_smoothing": float(typed_item.inertia_constraint.movement_inertia_smoothing),
        "inertia_movement_speed_limit_enabled": bool(typed_item.inertia_constraint.movement_speed_limit.use),
        "inertia_movement_speed_limit": float(typed_item.inertia_constraint.movement_speed_limit.value),
        "inertia_rotation_speed_limit_enabled": bool(typed_item.inertia_constraint.rotation_speed_limit.use),
        "inertia_rotation_speed_limit": float(typed_item.inertia_constraint.rotation_speed_limit.value),
        "inertia_local_inertia": float(typed_item.inertia_constraint.local_inertia),
        "inertia_local_movement_speed_limit_enabled": bool(typed_item.inertia_constraint.local_movement_speed_limit.use),
        "inertia_local_movement_speed_limit": float(typed_item.inertia_constraint.local_movement_speed_limit.value),
        "inertia_local_rotation_speed_limit_enabled": bool(typed_item.inertia_constraint.local_rotation_speed_limit.use),
        "inertia_local_rotation_speed_limit": float(typed_item.inertia_constraint.local_rotation_speed_limit.value),
        "inertia_depth_inertia": float(typed_item.inertia_constraint.depth_inertia),
        "inertia_centrifugal_acceleration": float(typed_item.inertia_constraint.centrifugal_acceleration),
        "inertia_particle_speed_limit_enabled": bool(typed_item.inertia_constraint.particle_speed_limit.use),
        "inertia_particle_speed_limit": float(typed_item.inertia_constraint.particle_speed_limit.value),
        "tether_distance_compression": float(typed_item.tether_constraint.distance_compression),
        "distance_stiffness": float(typed_item.distance_constraint.stiffness.value),
        "angle_restoration_enabled": bool(typed_item.angle_restoration_constraint.use_angle_restoration),
        "angle_restoration_stiffness": float(typed_item.angle_restoration_constraint.stiffness.value),
        "angle_restoration_velocity_attenuation": float(typed_item.angle_restoration_constraint.velocity_attenuation),
        "use_spring": bool(typed_item.spring_constraint.use_spring),
        "spring_power": float(typed_item.spring_constraint.spring_power),
        "limit_distance": float(typed_item.spring_constraint.limit_distance),
        "normal_limit_ratio": float(typed_item.spring_constraint.normal_limit_ratio),
        "spring_noise": float(typed_item.spring_constraint.spring_noise),
        "collider_friction": float(typed_item.collider_collision_constraint.friction),
        "collider_limit_distance": float(typed_item.collider_collision_constraint.limit_distance.value),
        "gravity_strength": float(typed_item.gravity_strength),
        "gravity_direction": _vec3(typed_item.gravity_direction),
        "armature_scale": armature_scale,
        "joints": joints,
    }


def _mc2_cloth_snapshot(scene, item, cloth):
    return _bone_chain_snapshot(scene, item, cloth)


def _mc2_collider_snapshot(item, collider):
    collider_object = collider.collider_object
    if collider_object is None:
        return None

    world_matrix = collider_object.matrix_world.copy()
    shape_type = collider.collider_type
    mc2_component_type = {
        "SPHERE": "MagicaSphereCollider",
        "CAPSULE": "MagicaCapsuleCollider",
        "PLANE": "MagicaPlaneCollider",
    }.get(shape_type, "ColliderComponent")
    size = (float(collider.radius), 0.0, 0.0)
    if shape_type == "CAPSULE":
        size = (
            float(collider.radius),
            float(collider.end_radius if collider.radius_separation else collider.radius),
            float(collider.length),
        )
    elif shape_type == "PLANE":
        size = (0.0, 0.0, 0.0)

    return {
        "component_id": item.component_id,
        "mc2_component_type": mc2_component_type,
        "display_name": item.display_name,
        "object_name": collider_object.name,
        "center": _vec3(collider.center),
        "size": size,
        "shape_type": shape_type,
        "radius": float(collider.radius),
        "height": float(collider.length),
        "capsule_direction": collider.direction,
        "capsule_aligned_on_center": bool(collider.aligned_on_center),
        "capsule_reverse_direction": bool(collider.reverse_direction),
        "capsule_end_radius": float(size[1] if shape_type == "CAPSULE" else collider.radius),
        "world_translation": _vec3(world_matrix.to_translation()),
        "world_rotation": _quat(world_matrix.to_quaternion()),
    }


def _topology_hash(source_object) -> str:
    if source_object is None:
        return ""
    mesh_data = getattr(source_object, "data", None)
    if source_object.type == "MESH" and mesh_data is not None:
        return f"{source_object.name}:{len(mesh_data.vertices)}:{len(mesh_data.edges)}:{len(mesh_data.polygons)}"
    return f"{source_object.name}:{source_object.type}"


def _cache_output_snapshot(item, typed_item):
    source_object = typed_item.source_object
    payload = {
        "component_id": item.component_id,
        "display_name": getattr(item, "display_name", getattr(item, "name", "")),
        "source_object_name": source_object.name if source_object is not None else "",
        "topology_hash": _topology_hash(source_object),
        "cache_format": typed_item.cache_format,
        "cache_path": typed_item.cache_path,
    }
    if source_object is not None and source_object.type == "MESH" and source_object.data is not None:
        mesh_data = source_object.data
        payload["mesh_writeback_target"] = {
            "component_id": item.component_id,
            "source_object_name": source_object.name,
            "vertex_count": len(mesh_data.vertices),
            "edge_count": len(mesh_data.edges),
            "face_count": len(mesh_data.polygons),
            "topology_hash": payload["topology_hash"],
            "space": "object_local",
        }
    return payload


def build_authoring_snapshot(scene):
    payload = {
        "schema_note": "MC2-style Unity component snapshot; Blender fields are compatibility aliases only.",
        "components": [],
        "bone_chains": [],
        "colliders": [],
        "collider_groups": [],
        "cache_outputs": [],
        "mesh_writeback_targets": [],
    }

    for item in scene.hocloth_mc2_components:
        payload["components"].append(
            {
                "component_id": item.component_id,
                "component_type": item.component_type,
                "mc2_component_type": (
                    "MagicaCloth" if item.component_type == "MAGICA_CLOTH" else item.component_type
                ),
                "mc2_authoring_mode": "",
                "display_name": item.display_name,
                "enabled": bool(item.enabled),
            }
        )
        if not item.enabled:
            continue

        if item.component_type == "MAGICA_CLOTH":
            cloth = mc2.find_magica_cloth(scene, item.component_id)
            if cloth is not None:
                chain = _mc2_cloth_snapshot(scene, item, cloth)
                if chain is not None:
                    payload["bone_chains"].append(chain)
        elif item.component_type in {"SPHERE_COLLIDER", "CAPSULE_COLLIDER", "PLANE_COLLIDER"}:
            collider = mc2.find_collider(scene, item.component_id)
            if collider is not None:
                collider = _mc2_collider_snapshot(item, collider)
            if collider is not None:
                payload["colliders"].append(collider)
        elif item.component_type == "CACHE_OUTPUT":
            cache_output = mc2.find_cache_output(scene, item.component_id)
            if cache_output is not None:
                cache_payload = _cache_output_snapshot(item, cache_output)
                payload["cache_outputs"].append(cache_payload)
                if cache_payload.get("mesh_writeback_target") is not None:
                    payload["mesh_writeback_targets"].append(cache_payload["mesh_writeback_target"])

    return wrap_authoring_snapshot(payload)


def _payload_from_snapshot(authoring_snapshot: dict) -> dict:
    if authoring_snapshot.get("payload_type") == "authoring_snapshot":
        return authoring_snapshot.get("payload") or {}
    return authoring_snapshot


def _build_lines(joints: list[CompiledSpringJoint]) -> list[CompiledSpringLine]:
    return [
        CompiledSpringLine(start_index=joint.parent_index, end_index=index)
        for index, joint in enumerate(joints)
        if joint.parent_index >= 0
    ]


def _build_baselines(joints: list[CompiledSpringJoint]) -> list[CompiledSpringBaseline]:
    children = {index: [] for index in range(len(joints))}
    for index, joint in enumerate(joints):
        if joint.parent_index >= 0:
            children.setdefault(joint.parent_index, []).append(index)

    baselines = []
    for index in range(len(joints)):
        if children.get(index):
            continue
        path = []
        current = index
        while 0 <= current < len(joints):
            path.append(current)
            current = joints[current].parent_index
        baselines.append(CompiledSpringBaseline(joint_indices=list(reversed(path))))
    return baselines


def compiled_scene_from_authoring_snapshot(authoring_snapshot: dict) -> CompiledScene:
    payload = _payload_from_snapshot(authoring_snapshot)
    compiled = CompiledScene()
    collision_object_id_by_collider_id: dict[str, str] = {}

    for chain_data in payload.get("bone_chains", []):
        joints = [
            CompiledSpringJoint(
                name=joint.get("name", ""),
                parent_index=int(joint.get("parent_index", -1)),
                depth=int(joint.get("depth", 0)),
                length=float(joint.get("length", 0.0)),
                radius=float(joint.get("radius", chain_data.get("joint_radius", 0.02))),
                stiffness=float(joint.get("stiffness", chain_data.get("stiffness", 0.0))),
                damping=float(joint.get("damping", chain_data.get("damping", 0.0))),
                drag=float(joint.get("drag", chain_data.get("drag", 0.0))),
                gravity_scale=float(joint.get("gravity_scale", 1.0)),
                rest_head_local=tuple(joint.get("rest_head_local", (0.0, 0.0, 0.0))),
                rest_tail_local=tuple(joint.get("rest_tail_local", (0.0, 0.0, 0.0))),
                rest_local_translation=tuple(joint.get("rest_local_translation", (0.0, 0.0, 0.0))),
                rest_local_rotation=tuple(joint.get("rest_local_rotation", (1.0, 0.0, 0.0, 0.0))),
            )
            for joint in chain_data.get("joints", [])
        ]
        lines = _build_lines(joints)
        baselines = _build_baselines(joints)
        compiled.spring_bones.append(
            CompiledSpringBone(
                component_id=chain_data.get("component_id", ""),
                component_type=chain_data.get("component_type", "BONE_CLOTH"),
                cloth_type=chain_data.get("cloth_type", "BoneCloth"),
                armature_name=chain_data.get("armature_name", ""),
                root_bone_name=chain_data.get("root_bone_name", ""),
                center_object_name=chain_data.get("center_object_name", ""),
                center_bone_name=chain_data.get("center_bone_name", ""),
                joint_radius=float(chain_data.get("joint_radius", 0.02)),
                stiffness=float(chain_data.get("stiffness", 0.0)),
                damping=float(chain_data.get("damping", 0.0)),
                drag=float(chain_data.get("drag", 0.0)),
                damping_curve_value=float(chain_data.get("damping_curve_value", chain_data.get("damping", 0.0))),
                inertia_world_inertia=float(chain_data.get("inertia_world_inertia", 1.0)),
                inertia_movement_inertia_smoothing=float(chain_data.get("inertia_movement_inertia_smoothing", 0.4)),
                inertia_movement_speed_limit_enabled=bool(chain_data.get("inertia_movement_speed_limit_enabled", False)),
                inertia_movement_speed_limit=float(chain_data.get("inertia_movement_speed_limit", 0.0)),
                inertia_rotation_speed_limit_enabled=bool(chain_data.get("inertia_rotation_speed_limit_enabled", False)),
                inertia_rotation_speed_limit=float(chain_data.get("inertia_rotation_speed_limit", 0.0)),
                inertia_local_inertia=float(chain_data.get("inertia_local_inertia", 1.0)),
                inertia_local_movement_speed_limit_enabled=bool(chain_data.get("inertia_local_movement_speed_limit_enabled", False)),
                inertia_local_movement_speed_limit=float(chain_data.get("inertia_local_movement_speed_limit", 0.0)),
                inertia_local_rotation_speed_limit_enabled=bool(chain_data.get("inertia_local_rotation_speed_limit_enabled", False)),
                inertia_local_rotation_speed_limit=float(chain_data.get("inertia_local_rotation_speed_limit", 0.0)),
                inertia_depth_inertia=float(chain_data.get("inertia_depth_inertia", 0.0)),
                inertia_centrifugal_acceleration=float(chain_data.get("inertia_centrifugal_acceleration", 0.0)),
                inertia_particle_speed_limit_enabled=bool(chain_data.get("inertia_particle_speed_limit_enabled", False)),
                inertia_particle_speed_limit=float(chain_data.get("inertia_particle_speed_limit", 0.0)),
                tether_distance_compression=float(chain_data.get("tether_distance_compression", 0.8)),
                distance_stiffness=float(chain_data.get("distance_stiffness", chain_data.get("stiffness", 0.0))),
                angle_restoration_enabled=bool(chain_data.get("angle_restoration_enabled", True)),
                angle_restoration_stiffness=float(chain_data.get("angle_restoration_stiffness", 0.0)),
                angle_restoration_velocity_attenuation=float(chain_data.get("angle_restoration_velocity_attenuation", 0.6)),
                use_spring=bool(chain_data.get("use_spring", True)),
                spring_power=float(chain_data.get("spring_power", 0.04)),
                limit_distance=float(chain_data.get("limit_distance", 0.1)),
                normal_limit_ratio=float(chain_data.get("normal_limit_ratio", 1.0)),
                spring_noise=float(chain_data.get("spring_noise", 0.0)),
                collider_friction=float(chain_data.get("collider_friction", 0.5)),
                collider_limit_distance=float(chain_data.get("collider_limit_distance", 0.05)),
                gravity_strength=float(chain_data.get("gravity_strength", 0.0)),
                gravity_direction=tuple(chain_data.get("gravity_direction", (0.0, -1.0, 0.0))),
                collider_ids=list(chain_data.get("collider_ids", [])),
                collider_group_ids=list(chain_data.get("collider_group_ids", [])),
                armature_scale=tuple(chain_data.get("armature_scale", (1.0, 1.0, 1.0))),
                joints=joints,
                lines=lines,
                baselines=baselines,
            )
        )

    for collider_data in payload.get("colliders", []):
        component_id = collider_data.get("component_id", "")
        collision_object_id = f"collision::{component_id}"
        collision_object_id_by_collider_id[component_id] = collision_object_id
        compiled.collision_objects.append(
            CompiledCollisionObject(
                collision_object_id=collision_object_id,
                owner_component_id=component_id,
                motion_type="KINEMATIC",
                shape_type=collider_data.get("shape_type", "SPHERE"),
                world_translation=tuple(collider_data.get("world_translation", (0.0, 0.0, 0.0))),
                world_rotation=tuple(collider_data.get("world_rotation", (1.0, 0.0, 0.0, 0.0))),
                radius=float(collider_data.get("radius", 0.0)),
                height=float(collider_data.get("height", 0.0)),
                capsule_direction=collider_data.get("capsule_direction", "Y"),
                capsule_aligned_on_center=bool(collider_data.get("capsule_aligned_on_center", True)),
                capsule_reverse_direction=bool(collider_data.get("capsule_reverse_direction", False)),
                capsule_end_radius=float(collider_data.get("capsule_end_radius", collider_data.get("radius", 0.0))),
                source_object_name=collider_data.get("object_name", ""),
            )
        )

    for chain in compiled.spring_bones:
        object_ids = [
            collision_object_id_by_collider_id[collider_id]
            for collider_id in chain.collider_ids
            if collider_id in collision_object_id_by_collider_id
        ]
        if not object_ids:
            continue
        binding_id = f"chain::{chain.component_id}::colliders"
        compiled.collision_bindings.append(
            CompiledCollisionBinding(
                binding_id=binding_id,
                owner_component_id=chain.component_id,
                collision_object_ids=object_ids,
            )
        )
        chain.collision_binding_ids = [binding_id]

    for target_data in payload.get("mesh_writeback_targets", []):
        compiled.mesh_writeback_targets.append(
            CompiledMeshWritebackTarget(
                component_id=target_data.get("component_id", ""),
                source_object_name=target_data.get("source_object_name", ""),
                vertex_count=int(target_data.get("vertex_count", 0)),
                edge_count=int(target_data.get("edge_count", 0)),
                face_count=int(target_data.get("face_count", 0)),
                topology_hash=target_data.get("topology_hash", ""),
                space=target_data.get("space", "object_local"),
            )
        )

    return compiled
