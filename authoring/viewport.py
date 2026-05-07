import math

import bpy
import gpu
from gpu_extras.batch import batch_for_shader
from mathutils import Vector

from ..runtime.session import get_last_build_output


_DRAW_HANDLER = None
_LINE_SHADER = None

_BONE_COLOR = (0.24, 0.78, 1.0, 1.0)
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
    return [
        center + (axis_a * math.cos((math.tau * index) / segments) + axis_b * math.sin((math.tau * index) / segments)) * radius
        for index in range(segments + 1)
    ]


def _append_polyline(segments: list[tuple[Vector, Vector]], points: list[Vector]):
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


def _to_vector(value) -> Vector:
    return Vector((float(value[0]), float(value[1]), float(value[2])))


def _append_sphere_segments(segments, center, radius, region_data):
    if radius <= 0.0:
        return
    right, up, forward = _viewport_basis(region_data)
    _append_polyline(segments, _circle_points(center, right, up, radius))
    _append_polyline(segments, _circle_points(center, right, forward, radius))
    _append_polyline(segments, _circle_points(center, up, forward, radius))


def _build_component_particle_positions(scene, build_output):
    positions: dict[str, list[Vector]] = {}
    for item in build_output.get("particles", []):
        component_id = item.get("component_id", "")
        bone_name = item.get("bone_name", "")
        if not component_id or not bone_name:
            continue
        armature_name = ""
        for cloth in scene.hocloth_mc2_magica_cloths:
            if cloth.component_id == component_id and cloth.armature_object is not None:
                armature_name = cloth.armature_object.name
                break
        armature_object = scene.objects.get(armature_name) if armature_name else None
        pose_bone = armature_object.pose.bones.get(bone_name) if armature_object is not None and armature_object.pose is not None else None
        if pose_bone is not None:
            position = (armature_object.matrix_world @ pose_bone.matrix).translation.copy()
        else:
            position = _to_vector(item.get("rest_head_local", (0.0, 0.0, 0.0)))
        positions.setdefault(component_id, []).append(position)
    return positions


def _append_build_output_segments(scene, region_data, bone_segments, radius_segments, collider_segments):
    build_output = get_last_build_output()
    positions = _build_component_particle_positions(scene, build_output)

    if scene.hocloth_viewport_draw_bones:
        for component_id, component_positions in positions.items():
            for position in component_positions:
                _append_cross(bone_segments, position, 0.01)
            for line in build_output.get("lines", []):
                if line.get("component_id") != component_id:
                    continue
                start_index = int(line.get("start_index", -1))
                end_index = int(line.get("end_index", -1))
                if 0 <= start_index < len(component_positions) and 0 <= end_index < len(component_positions):
                    bone_segments.append((component_positions[start_index], component_positions[end_index]))

    if scene.hocloth_viewport_draw_particle_radius:
        for particle in build_output.get("particles", []):
            component_id = particle.get("component_id", "")
            joint_index = int(particle.get("joint_index", -1))
            component_positions = positions.get(component_id, [])
            if 0 <= joint_index < len(component_positions):
                _append_sphere_segments(radius_segments, component_positions[joint_index], float(particle.get("radius", 0.0)), region_data)

    if scene.hocloth_viewport_draw_colliders:
        for collider in build_output.get("colliders", []):
            center = _to_vector(collider.get("world_translation", (0.0, 0.0, 0.0)))
            _append_sphere_segments(collider_segments, center, float(collider.get("radius", 0.0)), region_data)


def _draw_viewport_overlay():
    context = bpy.context
    if context is None or context.scene is None or context.region_data is None:
        return

    scene = context.scene
    if not scene.hocloth_viewport_overlay_enabled:
        return

    bone_segments = []
    radius_segments = []
    collider_segments = []
    _append_build_output_segments(scene, context.region_data, bone_segments, radius_segments, collider_segments)

    alpha = scene.hocloth_viewport_overlay_alpha
    _draw_segments(bone_segments, _BONE_COLOR, alpha)
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
