from __future__ import annotations

from mathutils import Matrix, Quaternion, Vector

from .blender_bone_refs import resolve_bone_forest_names
from ..components import mc2
from .exchange import wrap_authoring_snapshot


def _vec3(value) -> tuple[float, float, float]:
    return (float(value[0]), float(value[1]), float(value[2]))


def _quat(value) -> tuple[float, float, float, float]:
    return (float(value.w), float(value.x), float(value.y), float(value.z))


def _curve_parameter(value) -> dict:
    samples = list(getattr(value, "curve_samples", []))
    if len(samples) != 16:
        samples = [1.0] * 16
    return {
        "value": float(value.value),
        "useCurve": bool(value.use_curve),
        "samples": [float(sample) for sample in samples],
    }


def _joint_override_map(typed_item) -> dict[str, object]:
    return {
        item.bone_name: item
        for item in getattr(typed_item, "joint_overrides", [])
        if item.bone_name
    }


def _bone_attribute_overrides(typed_item) -> list[dict]:
    overrides = []
    for item in getattr(typed_item, "joint_overrides", []):
        attribute = getattr(item, "mc2_attribute", "DEFAULT")
        if not item.bone_name or attribute == "DEFAULT":
            continue
        overrides.append({"bone_name": item.bone_name, "attribute": attribute})
    return overrides


def _vector_to_tuple(value: Vector) -> tuple[float, float, float]:
    return (float(value.x), float(value.y), float(value.z))


def _quaternion_to_tuple(value: Quaternion) -> tuple[float, float, float, float]:
    return (float(value.w), float(value.x), float(value.y), float(value.z))


def _matrix_to_tuple(matrix: Matrix) -> tuple[float, ...]:
    return tuple(float(matrix[row][column]) for row in range(4) for column in range(4))


def _bone_parent_local_matrix(armature_object, bone, world_matrix):
    if bone.parent is not None:
        parent_pose_bone = armature_object.pose.bones.get(bone.parent.name) if armature_object.pose else None
        parent_pose_matrix = (
            parent_pose_bone.matrix.copy()
            if parent_pose_bone is not None
            else bone.parent.matrix_local.copy()
        )
        return (armature_object.matrix_world @ parent_pose_matrix).inverted_safe() @ world_matrix
    return armature_object.matrix_world.inverted_safe() @ world_matrix


def _sample_chain_bones(
    scene,
    armature_object,
    typed_item,
    root_bone_names: list[str],
) -> list[dict]:
    bone_names = resolve_bone_forest_names(scene, armature_object, root_bone_names)
    if not bone_names:
        return []

    armature_matrix = armature_object.matrix_world.copy()
    name_to_index = {name: index for index, name in enumerate(bone_names)}
    overrides = _joint_override_map(typed_item)
    runtime_stiffness = float(typed_item.distance_constraint.stiffness.value)
    runtime_damping = float(typed_item.damping_curve.value)
    runtime_drag = max(0.0, min(1.0, 1.0 - float(typed_item.angle_restoration_constraint.velocity_attenuation)))
    sampled = []
    for bone_name in bone_names:
        bone = armature_object.data.bones.get(bone_name)
        if bone is None:
            continue

        pose_bone = armature_object.pose.bones.get(bone_name) if armature_object.pose else None
        pose_matrix = pose_bone.matrix.copy() if pose_bone is not None else bone.matrix_local.copy()
        world_matrix = armature_matrix @ pose_matrix
        parent_name = bone.parent.name if bone.parent and bone.parent.name in name_to_index else ""
        parent_index = name_to_index[parent_name] if parent_name else -1
        depth = sampled[parent_index]["depth"] + 1 if parent_index >= 0 and parent_index < len(sampled) else 0
        head_world = world_matrix.to_translation()
        length = max(float(bone.length), 1.0e-6)
        tail_world = head_world + (world_matrix.to_quaternion() @ Vector((0.0, length, 0.0)))
        rest_head_local = _vector_to_tuple(head_world)
        rest_tail_local = _vector_to_tuple(tail_world)
        local_matrix = _bone_parent_local_matrix(armature_object, bone, world_matrix)
        rest_local_translation = _vector_to_tuple(local_matrix.to_translation())
        rest_local_rotation = _quaternion_to_tuple(local_matrix.to_quaternion())

        override = overrides.get(bone_name)
        use_override = override is not None and override.enabled
        sampled.append(
            {
                "name": bone_name,
                "parent_name": parent_name,
                "parent_index": int(parent_index),
                "depth": int(depth),
                "length": float((tail_world - head_world).length),
                "radius": float(override.radius) if use_override else float(typed_item.joint_radius),
                "stiffness": float(override.stiffness) if use_override else runtime_stiffness,
                "damping": float(override.damping) if use_override else runtime_damping,
                "drag": float(override.drag) if use_override else runtime_drag,
                "gravity_scale": float(override.gravity_scale) if use_override else 1.0,
                "head_local": _vec3(bone.head_local),
                "tail_local": _vec3(bone.tail_local),
                "rest_head_local": rest_head_local,
                "rest_tail_local": rest_tail_local,
                "rest_local_translation": rest_local_translation,
                "rest_local_rotation": rest_local_rotation,
                "rest_world_rotation": _quaternion_to_tuple(world_matrix.to_quaternion()),
                "rest_world_scale": _vector_to_tuple(world_matrix.to_scale()),
                "rest_local_to_world_matrix": _matrix_to_tuple(world_matrix),
            }
        )
    return sampled


def _bone_chain_snapshot(scene, item, typed_item):
    armature_object = typed_item.armature_object
    if armature_object is None or armature_object.type != "ARMATURE":
        return None

    root_bone_names = mc2.resolve_cloth_root_bone_names(typed_item)
    bones = _sample_chain_bones(scene, armature_object, typed_item, root_bone_names)
    armature_matrix = armature_object.matrix_world.copy()
    armature_position = _vector_to_tuple(armature_matrix.to_translation())
    armature_rotation = _quaternion_to_tuple(armature_matrix.to_quaternion())
    armature_scale = _vector_to_tuple(armature_matrix.to_scale())
    center_object_name = ""
    center_bone_name = ""
    if typed_item.center_source == "OBJECT" and typed_item.center_object is not None:
        center_object_name = typed_item.center_object.name
    elif typed_item.center_source == "BONE" and typed_item.center_armature_object is not None:
        center_object_name = typed_item.center_armature_object.name
        center_bone_name = typed_item.center_bone_name

    collider_collision_enabled = bool(typed_item.collider_collision_constraint.enabled)
    collider_ids = mc2.resolve_cloth_collider_ids(scene, typed_item) if collider_collision_enabled else []
    runtime_stiffness = float(typed_item.distance_constraint.stiffness.value)
    runtime_damping = float(typed_item.damping_curve.value)
    runtime_drag = max(0.0, min(1.0, 1.0 - float(typed_item.angle_restoration_constraint.velocity_attenuation)))
    serialize_data = {
        "clothType": "BoneCloth" if typed_item.authoring_mode == "BONE_CLOTH" else "BoneSpring",
        "rootBones": list(root_bone_names),
        "connectionMode": typed_item.bone_connection_mode,
        "gravity": float(typed_item.gravity_strength),
        "gravityDirection": _vec3(typed_item.gravity_direction),
        "damping": _curve_parameter(typed_item.damping_curve),
        "radius": {
            "value": float(typed_item.joint_radius),
            "useCurve": False,
            "samples": [1.0] * 16,
        },
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
            "stiffness": _curve_parameter(typed_item.distance_constraint.stiffness),
        },
        "angleRestorationConstraint": {
            "useAngleRestoration": bool(typed_item.angle_restoration_constraint.use_angle_restoration),
            "stiffness": _curve_parameter(typed_item.angle_restoration_constraint.stiffness),
            "velocityAttenuation": float(typed_item.angle_restoration_constraint.velocity_attenuation),
            "gravityFalloff": float(typed_item.angle_restoration_constraint.gravity_falloff),
        },
        "angleLimitConstraint": {
            "useAngleLimit": bool(typed_item.angle_limit_constraint.use_angle_limit),
            "limitAngle": _curve_parameter(typed_item.angle_limit_constraint.limit_angle),
            "stiffness": float(typed_item.angle_limit_constraint.stiffness),
        },
        "triangleBendingConstraint": {
            "stiffness": float(typed_item.triangle_bending_constraint.stiffness),
        },
        "springConstraint": {
            "useSpring": bool(typed_item.spring_constraint.use_spring),
            "springPower": float(typed_item.spring_constraint.spring_power),
            "limitDistance": float(typed_item.spring_constraint.limit_distance),
            "normalLimitRatio": float(typed_item.spring_constraint.normal_limit_ratio),
            "springNoise": float(typed_item.spring_constraint.spring_noise),
        },
        "colliderCollisionConstraint": {
            "mode": typed_item.collider_collision_constraint.mode
            if typed_item.collider_collision_constraint.enabled
            else "None",
            "friction": float(typed_item.collider_collision_constraint.friction),
            "colliderList": list(collider_ids),
            "collisionBones": [],
            "limitDistance": _curve_parameter(typed_item.collider_collision_constraint.limit_distance),
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
        "root_bone_names": list(root_bone_names),
        "bone_connection_mode": typed_item.bone_connection_mode,
        "pose_space": "WORLD",
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
        "triangle_bending_stiffness": float(typed_item.triangle_bending_constraint.stiffness),
        "angle_restoration_enabled": bool(typed_item.angle_restoration_constraint.use_angle_restoration),
        "angle_restoration_stiffness": float(typed_item.angle_restoration_constraint.stiffness.value),
        "angle_restoration_velocity_attenuation": float(typed_item.angle_restoration_constraint.velocity_attenuation),
        "angle_restoration_gravity_falloff": float(typed_item.angle_restoration_constraint.gravity_falloff),
        "angle_limit_enabled": bool(typed_item.angle_limit_constraint.use_angle_limit),
        "angle_limit_angle": float(typed_item.angle_limit_constraint.limit_angle.value),
        "angle_limit_stiffness": float(typed_item.angle_limit_constraint.stiffness),
        "gravity_falloff": 0.0,
        "stabilization_time_after_reset": 0.1,
        "blend_weight": 1.0,
        "use_spring": bool(typed_item.spring_constraint.use_spring),
        "spring_power": float(typed_item.spring_constraint.spring_power),
        "limit_distance": float(typed_item.spring_constraint.limit_distance),
        "normal_limit_ratio": float(typed_item.spring_constraint.normal_limit_ratio),
        "spring_noise": float(typed_item.spring_constraint.spring_noise),
        "collider_friction": float(typed_item.collider_collision_constraint.friction),
        "collider_limit_distance": float(typed_item.collider_collision_constraint.limit_distance.value),
        "collider_collision_enabled": collider_collision_enabled,
        "collider_collision_mode": typed_item.collider_collision_constraint.mode
        if collider_collision_enabled
        else "None",
        "gravity_strength": float(typed_item.gravity_strength),
        "gravity_direction": _vec3(typed_item.gravity_direction),
        "armature_position": armature_position,
        "armature_rotation": armature_rotation,
        "armature_scale": armature_scale,
        "bones": bones,
        "bone_attribute_overrides": _bone_attribute_overrides(typed_item),
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
