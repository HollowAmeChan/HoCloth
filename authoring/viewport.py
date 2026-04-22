import math

import bpy
import gpu
from gpu_extras.batch import batch_for_shader
from mathutils import Matrix, Vector

from ..compile.compiler import resolve_bone_chain_names
from ..components.properties import _parse_component_id_list, find_component_by_id


_DRAW_HANDLER = None
_LINE_SHADER = None

_SPRING_COLOR = (0.24, 0.78, 1.0, 1.0)
_RADIUS_COLOR = (1.0, 0.72, 0.18, 1.0)
_COLLIDER_COLOR = (1.0, 0.36, 0.36, 1.0)


def _shader():
    global _LINE_SHADER
    if _LINE_SHADER is None:
        _LINE_SHADER = gpu.shader.from_builtin("UNIFORM_COLOR")
    return _LINE_SHADER


def _viewport_basis(region_data):
    view_rotation = region_data.view_rotation.to_matrix()
    right = Vector((view_rotation[0][0], view_rotation[1][0], view_rotation[2][0])).normalized()
    up = Vector((view_rotation[0][1], view_rotation[1][1], view_rotation[2][1])).normalized()
    forward = Vector((view_rotation[0][2], view_rotation[1][2], view_rotation[2][2])).normalized()
    return right, up, forward


def _circle_points(center: Vector, axis_a: Vector, axis_b: Vector, radius: float, segments: int = 32):
    points = []
    for index in range(segments + 1):
        angle = (math.tau * index) / segments
        offset = (axis_a * math.cos(angle) + axis_b * math.sin(angle)) * radius
        points.append(center + offset)
    return points


def _append_polyline(segments: list[tuple[Vector, Vector]], points: list[Vector]):
    if len(points) < 2:
        return
    for start, end in zip(points, points[1:]):
        segments.append((start, end))


def _append_cross(segments: list[tuple[Vector, Vector]], center: Vector, size: float):
    axis_x = Vector((size, 0.0, 0.0))
    axis_y = Vector((0.0, size, 0.0))
    axis_z = Vector((0.0, 0.0, size))
    segments.append((center - axis_x, center + axis_x))
    segments.append((center - axis_y, center + axis_y))
    segments.append((center - axis_z, center + axis_z))


def _draw_segments(segments: list[tuple[Vector, Vector]], color, alpha_scale: float):
    if not segments:
        return
    coords = []
    for start, end in segments:
        coords.extend((start, end))
    shader = _shader()
    batch = batch_for_shader(shader, "LINES", {"pos": coords})
    gpu.state.blend_set("ALPHA")
    gpu.state.line_width_set(2.0)
    shader.bind()
    shader.uniform_float("color", (color[0], color[1], color[2], color[3] * alpha_scale))
    batch.draw(shader)


def _iter_enabled_spring_bones(scene):
    for item in scene.hocloth_components:
        if not item.enabled or item.component_type not in {"SPRING_BONE", "BONE_CHAIN"}:
            continue
        component = find_component_by_id(scene.hocloth_spring_bone_components, item.component_id)
        if component is not None:
            yield item, component


def _iter_enabled_colliders(scene):
    for item in scene.hocloth_components:
        if not item.enabled or item.component_type != "COLLIDER":
            continue
        component = find_component_by_id(scene.hocloth_collider_components, item.component_id)
        if component is not None:
            yield item, component


def _joint_radius(component, bone_name: str) -> float:
    for entry in component.joint_overrides:
        if entry.bone_name == bone_name and entry.enabled:
            return max(0.0, entry.radius)
    return max(0.0, component.joint_radius)


def _joint_world_positions(component):
    armature_object = component.armature_object
    if armature_object is None or armature_object.type != "ARMATURE" or armature_object.pose is None:
        return []
    bone_names = resolve_bone_chain_names(bpy.context.scene, armature_object, component.root_bone_name)
    positions = []
    for bone_name in bone_names:
        pose_bone = armature_object.pose.bones.get(bone_name)
        if pose_bone is None:
            continue
        world_matrix = armature_object.matrix_world @ pose_bone.matrix
        positions.append((bone_name, world_matrix.translation.copy()))
    return positions


def _append_spring_bone_segments(scene, region_data, spring_segments, radius_segments):
    right, up, _forward = _viewport_basis(region_data)
    for _item, component in _iter_enabled_spring_bones(scene):
        joint_positions = _joint_world_positions(component)
        if len(joint_positions) < 1:
            continue

        if scene.hocloth_viewport_draw_bones:
            for (_, start), (_, end) in zip(joint_positions, joint_positions[1:]):
                spring_segments.append((start, end))
            for _bone_name, position in joint_positions:
                _append_cross(spring_segments, position, 0.01)

        if scene.hocloth_viewport_draw_particle_radius:
            for bone_name, position in joint_positions:
                radius = _joint_radius(component, bone_name)
                if radius <= 0.0:
                    continue
                _append_polyline(radius_segments, _circle_points(position, right, up, radius))


def _append_collider_sphere_segments(segments, center, radius, region_data):
    if radius <= 0.0:
        return
    right, up, forward = _viewport_basis(region_data)
    _append_polyline(segments, _circle_points(center, right, up, radius))
    _append_polyline(segments, _circle_points(center, right, forward, radius))
    _append_polyline(segments, _circle_points(center, up, forward, radius))


def _append_collider_capsule_segments(segments, matrix_world, radius, height, region_data):
    axis = (matrix_world.to_quaternion() @ Vector((0.0, 0.0, 1.0))).normalized()
    center = matrix_world.translation.copy()
    half_height = max(0.0, height) * 0.5
    top = center + axis * half_height
    bottom = center - axis * half_height
    _append_collider_sphere_segments(segments, top, radius, region_data)
    _append_collider_sphere_segments(segments, bottom, radius, region_data)

    if half_height <= 0.0 or radius <= 0.0:
        return

    view_right, view_up, _view_forward = _viewport_basis(region_data)
    side = axis.cross(view_up)
    if side.length < 1e-5:
        side = axis.cross(view_right)
    if side.length < 1e-5:
        return
    side.normalize()
    ortho = axis.cross(side).normalized()
    for offset_axis in (side, -side, ortho, -ortho):
        offset = offset_axis * radius
        segments.append((top + offset, bottom + offset))


def _append_collider_segments(scene, region_data, collider_segments):
    if not scene.hocloth_viewport_draw_colliders:
        return
    for _item, component in _iter_enabled_colliders(scene):
        collider_object = component.collider_object
        if collider_object is None:
            continue
        matrix_world = collider_object.matrix_world.copy()
        center = matrix_world.translation.copy()
        radius = max(0.0, component.radius)
        if component.shape_type == "CAPSULE":
            _append_collider_capsule_segments(
                collider_segments,
                matrix_world,
                radius,
                component.height,
                region_data,
            )
        else:
            _append_collider_sphere_segments(collider_segments, center, radius, region_data)


def _draw_viewport_overlay():
    context = bpy.context
    if context is None or context.scene is None or context.region_data is None:
        return

    scene = context.scene
    if not scene.hocloth_viewport_overlay_enabled:
        return

    spring_segments = []
    radius_segments = []
    collider_segments = []

    _append_spring_bone_segments(scene, context.region_data, spring_segments, radius_segments)
    _append_collider_segments(scene, context.region_data, collider_segments)

    alpha = scene.hocloth_viewport_overlay_alpha
    _draw_segments(spring_segments, _SPRING_COLOR, alpha)
    _draw_segments(radius_segments, _RADIUS_COLOR, alpha)
    _draw_segments(collider_segments, _COLLIDER_COLOR, alpha)
    gpu.state.line_width_set(1.0)
    gpu.state.blend_set("NONE")


def register():
    global _DRAW_HANDLER
    if _DRAW_HANDLER is None:
        _DRAW_HANDLER = bpy.types.SpaceView3D.draw_handler_add(
            _draw_viewport_overlay,
            (),
            "WINDOW",
            "POST_VIEW",
        )


def unregister():
    global _DRAW_HANDLER
    if _DRAW_HANDLER is not None:
        bpy.types.SpaceView3D.draw_handler_remove(_DRAW_HANDLER, "WINDOW")
        _DRAW_HANDLER = None
