import uuid

import bpy

from .registry import COMPONENT_DEFINITIONS, get_component_definition


HOCLOTH_MC2_PRESET_ITEMS = (
    ("ACCESSORY", "Accessory", "Match MC2 Accessory preset"),
    ("CAPE", "Cape", "Match MC2 Cape preset"),
    ("FRONT_HAIR", "FrontHair", "Match MC2 FrontHair preset"),
    ("LONG_HAIR", "LongHair", "Match MC2 LongHair preset"),
    ("SHORT_HAIR", "ShortHair", "Match MC2 ShortHair preset"),
    ("SKIRT", "Skirt", "Match MC2 Skirt preset"),
    ("SOFT_SKIRT", "SoftSkirt", "Match MC2 SoftSkirt preset"),
    ("MIDDLE_SPRING", "MiddleSpring", "Match MC2 MiddleSpring preset"),
    ("SOFT_SPRING", "SoftSpring", "Match MC2 SoftSpring preset"),
    ("HARD_SPRING", "HardSpring", "Match MC2 HardSpring preset"),
    ("TAIL", "Tail", "Match MC2 Tail preset"),
)


HOCLOTH_MC2_BONE_SPRING_PRESETS = {
    "ACCESSORY": {
        "label": "MC2 Accessory",
        "source": "MC2_Preset_Accessory.json",
        "gravity_strength": 0.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.10000000149011612,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.4000000059604645,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 5.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 720.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": False,
        "local_movement_speed_limit": 5.0,
        "local_rotation_speed_limit_use": False,
        "local_rotation_speed_limit": 720.0,
        "depth_inertia": 0.0,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.4000000059604645,
        "distance_stiffness": 1.0,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.15000000596046449,
        "angle_restoration_stiffness_use_curve": False,
        "angle_restoration_velocity_attenuation": 0.6000000238418579,
        "collider_friction": 0.05000000074505806,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.03999999910593033,
        "limit_distance": 0.10000000149011612,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "CAPE": {
        "label": "MC2 Cape",
        "source": "MC2_Preset_Cape.json",
        "gravity_strength": 7.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.10000000149011612,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.6000000238418579,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 5.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 720.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": False,
        "local_movement_speed_limit": 5.0,
        "local_rotation_speed_limit_use": False,
        "local_rotation_speed_limit": 720.0,
        "depth_inertia": 0.699999988079071,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.5,
        "distance_stiffness": 1.0,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.15000000596046449,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.800000011920929,
        "collider_friction": 0.10000000149011612,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.03999999910593033,
        "limit_distance": 0.10000000149011612,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "FRONT_HAIR": {
        "label": "MC2 FrontHair",
        "source": "MC2_Preset_FrontHair.json",
        "gravity_strength": 4.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.10000000149011612,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.5,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 3.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 360.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": False,
        "local_movement_speed_limit": 5.0,
        "local_rotation_speed_limit_use": False,
        "local_rotation_speed_limit": 720.0,
        "depth_inertia": 0.0,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 2.0,
        "tether_distance_compression": 0.10000000149011612,
        "distance_stiffness": 1.0,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.15000000596046449,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.699999988079071,
        "collider_friction": 0.05000000074505806,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.03999999910593033,
        "limit_distance": 0.10000000149011612,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "LONG_HAIR": {
        "label": "MC2 LongHair",
        "source": "MC2_Preset_LongHair.json",
        "gravity_strength": 5.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.10000000149011612,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.5,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 5.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 720.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": True,
        "local_movement_speed_limit": 3.0,
        "local_rotation_speed_limit_use": True,
        "local_rotation_speed_limit": 360.0,
        "depth_inertia": 0.0,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 3.0,
        "tether_distance_compression": 0.800000011920929,
        "distance_stiffness": 1.0,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.20000000298023225,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.800000011920929,
        "collider_friction": 0.05000000074505806,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.05999999865889549,
        "limit_distance": 0.05000000074505806,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "SHORT_HAIR": {
        "label": "MC2 ShortHair",
        "source": "MC2_Preset_ShortHair.json",
        "gravity_strength": 2.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.10000000149011612,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.5,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 5.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 720.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": True,
        "local_movement_speed_limit": 1.0,
        "local_rotation_speed_limit_use": True,
        "local_rotation_speed_limit": 360.0,
        "depth_inertia": 1.0,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.4000000059604645,
        "distance_stiffness": 1.0,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.15000000596046449,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.6000000238418579,
        "collider_friction": 0.05000000074505806,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.029999999329447748,
        "limit_distance": 0.05000000074505806,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "SKIRT": {
        "label": "MC2 Skirt",
        "source": "MC2_Preset_Skirt.json",
        "gravity_strength": 5.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.10000000149011612,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.5,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 5.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 720.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": True,
        "local_movement_speed_limit": 3.0,
        "local_rotation_speed_limit_use": True,
        "local_rotation_speed_limit": 360.0,
        "depth_inertia": 0.699999988079071,
        "centrifugal_acceleration": 0.10000000149011612,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.699999988079071,
        "distance_stiffness": 1.0,
        "distance_stiffness_use_curve": True,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.20000000298023225,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.699999988079071,
        "collider_friction": 0.05000000074505806,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.029999999329447748,
        "limit_distance": 0.05000000074505806,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "SOFT_SKIRT": {
        "label": "MC2 SoftSkirt",
        "source": "MC2_Preset_SoftSkirt.json",
        "gravity_strength": 3.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.10000000149011612,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.5,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 5.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 720.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": True,
        "local_movement_speed_limit": 3.0,
        "local_rotation_speed_limit_use": True,
        "local_rotation_speed_limit": 360.0,
        "depth_inertia": 0.25,
        "centrifugal_acceleration": 0.10000000149011612,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.699999988079071,
        "distance_stiffness": 1.0,
        "distance_stiffness_use_curve": True,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.14000000059604646,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.699999988079071,
        "collider_friction": 0.05000000074505806,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.029999999329447748,
        "limit_distance": 0.05000000074505806,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "SOFT_SPRING": {
        "label": "MC2 SoftSpring",
        "source": "MC2_Preset_SoftSpring.json",
        "gravity_strength": 0.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.20000000298023225,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.4000000059604645,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 1.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 360.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": True,
        "local_movement_speed_limit": 1.0,
        "local_rotation_speed_limit_use": True,
        "local_rotation_speed_limit": 360.0,
        "depth_inertia": 0.0,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.0989999994635582,
        "distance_stiffness": 0.24199999868869782,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.20000000298023225,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.800000011920929,
        "collider_friction": 0.20000000298023225,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.009999999776482582,
        "limit_distance": 0.05000000074505806,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "MIDDLE_SPRING": {
        "label": "MC2 MiddleSpring",
        "source": "MC2_Preset_MiddleSpring.json",
        "gravity_strength": 0.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.30000001192092898,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.4000000059604645,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 1.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 360.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": True,
        "local_movement_speed_limit": 1.0,
        "local_rotation_speed_limit_use": True,
        "local_rotation_speed_limit": 360.0,
        "depth_inertia": 0.0,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.0989999994635582,
        "distance_stiffness": 0.24199999868869782,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.4000000059604645,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.6000000238418579,
        "collider_friction": 0.20000000298023225,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.029999999329447748,
        "limit_distance": 0.05000000074505806,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "HARD_SPRING": {
        "label": "MC2 HardSpring",
        "source": "MC2_Preset_HardSpring.json",
        "gravity_strength": 0.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.30000001192092898,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.4000000059604645,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 1.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 360.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": True,
        "local_movement_speed_limit": 1.0,
        "local_rotation_speed_limit_use": True,
        "local_rotation_speed_limit": 360.0,
        "depth_inertia": 0.0,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.0989999994635582,
        "distance_stiffness": 0.24199999868869782,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.6000000238418579,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.4000000059604645,
        "collider_friction": 0.20000000298023225,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.05999999865889549,
        "limit_distance": 0.05000000074505806,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
    "TAIL": {
        "label": "MC2 Tail",
        "source": "MC2_Preset_Tail.json",
        "gravity_strength": 0.0,
        "gravity_direction": (0.0, -1.0, 0.0),
        "damping": 0.05000000074505806,
        "damping_use_curve": False,
        "joint_radius": 0.019999999552965165,
        "world_inertia": 1.0,
        "movement_inertia_smoothing": 0.5,
        "movement_speed_limit_use": True,
        "movement_speed_limit": 5.0,
        "rotation_speed_limit_use": True,
        "rotation_speed_limit": 720.0,
        "local_inertia": 1.0,
        "local_movement_speed_limit_use": True,
        "local_movement_speed_limit": 3.0,
        "local_rotation_speed_limit_use": True,
        "local_rotation_speed_limit": 360.0,
        "depth_inertia": 0.0,
        "centrifugal_acceleration": 0.0,
        "particle_speed_limit_use": True,
        "particle_speed_limit": 4.0,
        "tether_distance_compression": 0.800000011920929,
        "distance_stiffness": 1.0,
        "distance_stiffness_use_curve": False,
        "angle_restoration_enabled": True,
        "angle_restoration_stiffness": 0.10000000149011612,
        "angle_restoration_stiffness_use_curve": True,
        "angle_restoration_velocity_attenuation": 0.5,
        "collider_friction": 0.05000000074505806,
        "collider_limit_distance": 0.05000000074505806,
        "collider_limit_distance_use_curve": False,
        "use_spring": True,
        "spring_power": 0.009999999776482582,
        "limit_distance": 0.05000000074505806,
        "normal_limit_ratio": 1.0,
        "spring_noise": 0.0,
    },
}


def _poll_armature_object(self, obj):
    _ = self
    return obj is not None and obj.type == "ARMATURE"


def _validate_root_bone(component) -> None:
    armature_object = component.armature_object
    if armature_object is None or armature_object.type != "ARMATURE" or not armature_object.data:
        component.root_bone_name = ""
        return

    if component.root_bone_name and component.root_bone_name not in armature_object.data.bones:
        component.root_bone_name = ""


def _update_armature_object(self, context):
    _ = context
    _validate_root_bone(self)


def _update_root_bone_name(self, context):
    _ = context
    _validate_root_bone(self)


def _sync_disabled_joint_overrides(component) -> None:
    runtime_stiffness = get_runtime_stiffness(component)
    runtime_damping = get_runtime_damping(component)
    runtime_drag = get_runtime_drag(component)
    for entry in getattr(component, "joint_overrides", []):
        if getattr(entry, "enabled", False):
            continue
        entry.radius = component.joint_radius
        entry.stiffness = runtime_stiffness
        entry.damping = runtime_damping
        entry.drag = runtime_drag
        entry.gravity_scale = 1.0


def _update_spring_defaults(self, context):
    _ = context
    _sync_disabled_joint_overrides(self)


def _validate_center_bone(component) -> None:
    armature_object = component.center_armature_object
    if armature_object is None or armature_object.type != "ARMATURE" or not armature_object.data:
        component.center_bone_name = ""
        return
    if component.center_bone_name and component.center_bone_name not in armature_object.data.bones:
        component.center_bone_name = ""


def _update_center_armature_object(self, context):
    _ = context
    _validate_center_bone(self)


def _update_center_bone_name(self, context):
    _ = context
    _validate_center_bone(self)


class HoClothComponentItem(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    component_type: bpy.props.StringProperty(name="Component Type")
    container_index: bpy.props.IntProperty(name="Container Index", default=-1)
    display_name: bpy.props.StringProperty(name="Display Name")
    enabled: bpy.props.BoolProperty(name="Enabled", default=True)
    ui_expanded: bpy.props.BoolProperty(name="Expanded", default=True)


class HoClothSpringJointOverride(bpy.types.PropertyGroup):
    bone_name: bpy.props.StringProperty(name="Bone Name")
    enabled: bpy.props.BoolProperty(name="Enabled", default=False)
    radius: bpy.props.FloatProperty(name="Radius", default=0.02, min=0.0)
    stiffness: bpy.props.FloatProperty(name="Stiffness", default=0.6, min=0.0, soft_max=2.0)
    damping: bpy.props.FloatProperty(name="Damping", default=0.5, min=0.0, max=1.0)
    drag: bpy.props.FloatProperty(name="Drag", default=0.1, min=0.0, max=1.0)
    gravity_scale: bpy.props.FloatProperty(name="Gravity Scale", default=1.0, min=0.0, soft_max=2.0)


class HoClothCurveParameter(bpy.types.PropertyGroup):
    use_curve: bpy.props.BoolProperty(name="Use Curve", default=False)
    value: bpy.props.FloatProperty(name="Value", default=0.0)


class HoClothCheckSliderParameter(bpy.types.PropertyGroup):
    use: bpy.props.BoolProperty(name="Use", default=False)
    value: bpy.props.FloatProperty(name="Value", default=0.0)


class HoClothBoneSpringSpringConstraint(bpy.types.PropertyGroup):
    use_spring: bpy.props.BoolProperty(name="Use Spring", default=True)
    spring_power: bpy.props.FloatProperty(name="Spring Power", default=0.04, min=0.0, max=1.0)
    limit_distance: bpy.props.FloatProperty(name="Limit Distance", default=0.1, min=0.0, soft_max=0.5)
    normal_limit_ratio: bpy.props.FloatProperty(name="Normal Limit Ratio", default=1.0, min=0.0, max=1.0)
    spring_noise: bpy.props.FloatProperty(name="Spring Noise", default=0.0, min=0.0, max=1.0)


class HoClothBoneSpringCollisionConstraint(bpy.types.PropertyGroup):
    friction: bpy.props.FloatProperty(name="Friction", default=0.05, min=0.0, max=0.5)
    limit_distance: bpy.props.PointerProperty(name="Limit Distance", type=HoClothCurveParameter)


class HoClothBoneSpringInertiaConstraint(bpy.types.PropertyGroup):
    world_inertia: bpy.props.FloatProperty(name="World Inertia", default=1.0, min=0.0, max=1.0)
    movement_inertia_smoothing: bpy.props.FloatProperty(name="Movement Inertia Smoothing", default=0.4, min=0.0, max=1.0)
    movement_speed_limit: bpy.props.PointerProperty(name="Movement Speed Limit", type=HoClothCheckSliderParameter)
    rotation_speed_limit: bpy.props.PointerProperty(name="Rotation Speed Limit", type=HoClothCheckSliderParameter)
    local_inertia: bpy.props.FloatProperty(name="Local Inertia", default=1.0, min=0.0, max=1.0)
    local_movement_speed_limit: bpy.props.PointerProperty(name="Local Movement Speed Limit", type=HoClothCheckSliderParameter)
    local_rotation_speed_limit: bpy.props.PointerProperty(name="Local Rotation Speed Limit", type=HoClothCheckSliderParameter)
    depth_inertia: bpy.props.FloatProperty(name="Depth Inertia", default=0.0, min=0.0, max=1.0)
    centrifugal_acceleration: bpy.props.FloatProperty(name="Centrifugal Acceleration", default=0.0, min=0.0, max=1.0)
    particle_speed_limit: bpy.props.PointerProperty(name="Particle Speed Limit", type=HoClothCheckSliderParameter)


class HoClothBoneSpringDistanceConstraint(bpy.types.PropertyGroup):
    stiffness: bpy.props.PointerProperty(name="Stiffness", type=HoClothCurveParameter)


class HoClothBoneSpringTetherConstraint(bpy.types.PropertyGroup):
    distance_compression: bpy.props.FloatProperty(name="Distance Compression", default=0.099, min=0.0, max=1.0)


class HoClothBoneSpringAngleRestorationConstraint(bpy.types.PropertyGroup):
    use_angle_restoration: bpy.props.BoolProperty(name="Use Angle Restoration", default=True)
    stiffness: bpy.props.PointerProperty(name="Stiffness", type=HoClothCurveParameter)
    velocity_attenuation: bpy.props.FloatProperty(name="Velocity Attenuation", default=0.6, min=0.0, max=1.0)


class HoClothSpringBoneComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    armature_object: bpy.props.PointerProperty(
        name="Armature",
        type=bpy.types.Object,
        poll=_poll_armature_object,
        update=_update_armature_object,
    )
    root_bone_name: bpy.props.StringProperty(name="根骨骼", update=_update_root_bone_name)
    center_source: bpy.props.EnumProperty(
        name="Center Anchor",
        items=(
            ("NONE", "None", "No separate center"),
            ("OBJECT", "Object", "Use an object transform as center"),
            ("BONE", "Bone", "Use a bone transform as center"),
        ),
        default="NONE",
    )
    center_object: bpy.props.PointerProperty(name="Center Object", type=bpy.types.Object)
    center_armature_object: bpy.props.PointerProperty(
        name="Center Armature",
        type=bpy.types.Object,
        poll=_poll_armature_object,
        update=_update_center_armature_object,
    )
    center_bone_name: bpy.props.StringProperty(name="Center Bone", update=_update_center_bone_name)
    preset_profile: bpy.props.EnumProperty(
        name="Preset",
        items=HOCLOTH_MC2_PRESET_ITEMS,
        default="MIDDLE_SPRING",
    )
    append_tail_tip: bpy.props.BoolProperty(
        name="Append Tail Tip Joint",
        default=False,
    )
    joint_radius: bpy.props.FloatProperty(name="粒子半径", default=0.02, min=0.0, update=_update_spring_defaults)
    collider_ids: bpy.props.StringProperty(name="MC2 Collider List")
    collider_group_ids: bpy.props.StringProperty(name="Legacy Collision Bindings")
    stiffness: bpy.props.FloatProperty(name="Stiffness", default=0.6, min=0.0, soft_max=2.0, update=_update_spring_defaults)
    damping: bpy.props.FloatProperty(name="Damping", default=0.5, min=0.0, max=1.0, update=_update_spring_defaults)
    drag: bpy.props.FloatProperty(name="Drag", default=0.1, min=0.0, max=1.0, update=_update_spring_defaults)
    gravity_strength: bpy.props.FloatProperty(name="Gravity Strength", default=0.3, min=0.0, soft_max=2.0)
    gravity_direction: bpy.props.FloatVectorProperty(
        name="Gravity Direction",
        size=3,
        default=(0.0, 0.0, -1.0),
    )
    damping_curve: bpy.props.PointerProperty(name="Damping", type=HoClothCurveParameter)
    inertia_constraint: bpy.props.PointerProperty(name="Inertia Constraint", type=HoClothBoneSpringInertiaConstraint)
    tether_constraint: bpy.props.PointerProperty(name="Tether Constraint", type=HoClothBoneSpringTetherConstraint)
    distance_constraint: bpy.props.PointerProperty(name="Distance Constraint", type=HoClothBoneSpringDistanceConstraint)
    angle_restoration_constraint: bpy.props.PointerProperty(
        name="Angle Restoration Constraint",
        type=HoClothBoneSpringAngleRestorationConstraint,
    )
    spring_constraint: bpy.props.PointerProperty(name="Spring Constraint", type=HoClothBoneSpringSpringConstraint)
    collider_collision_constraint: bpy.props.PointerProperty(
        name="Collider Collision Constraint",
        type=HoClothBoneSpringCollisionConstraint,
    )
    joint_overrides: bpy.props.CollectionProperty(type=HoClothSpringJointOverride)
    joint_override_index: bpy.props.IntProperty(name="Joint Override Index", default=0)


class HoClothColliderComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    collider_object: bpy.props.PointerProperty(name="Object", type=bpy.types.Object)
    shape_type: bpy.props.EnumProperty(
        name="Shape",
        items=(
            ("SPHERE", "Sphere", "Sphere collider"),
            ("CAPSULE", "Capsule", "Capsule collider"),
        ),
        default="CAPSULE",
    )
    radius: bpy.props.FloatProperty(name="Radius", default=0.05, min=0.0)
    height: bpy.props.FloatProperty(name="Height", default=0.1, min=0.0)
    capsule_direction: bpy.props.EnumProperty(
        name="Capsule Direction",
        items=(
            ("X", "X", "Local X axis"),
            ("Y", "Y", "Local Y axis"),
            ("Z", "Z", "Local Z axis"),
        ),
        default="Y",
    )
    capsule_aligned_on_center: bpy.props.BoolProperty(name="Center Aligned", default=True)
    capsule_reverse_direction: bpy.props.BoolProperty(name="Reverse Direction", default=False)
    capsule_end_radius: bpy.props.FloatProperty(name="End Radius", default=0.05, min=0.0)


class HoClothColliderGroupComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    collider_ids: bpy.props.StringProperty(name="Collider Sources")


class HoClothCacheOutputComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    source_object: bpy.props.PointerProperty(name="源对象", type=bpy.types.Object)
    cache_format: bpy.props.EnumProperty(
        name="Cache Format",
        items=(
            ("pc2", "PC2", "Point cache 2"),
            ("mdd", "MDD", "MDD cache"),
        ),
        default="pc2",
    )
    cache_path: bpy.props.StringProperty(name="Cache Path", subtype="FILE_PATH")


def generate_component_id() -> str:
    return uuid.uuid4().hex


def _parse_component_id_list(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def ensure_joint_override_entry(component: HoClothSpringBoneComponent, bone_name: str):
    entry = next((item for item in component.joint_overrides if item.bone_name == bone_name), None)
    if entry is not None:
        return entry

    entry = component.joint_overrides.add()
    entry.bone_name = bone_name
    entry.radius = component.joint_radius
    entry.stiffness = get_runtime_stiffness(component)
    entry.damping = get_runtime_damping(component)
    entry.drag = get_runtime_drag(component)
    entry.gravity_scale = 1.0
    return entry


def sync_joint_override_names(component: HoClothSpringBoneComponent, bone_names: list[str]) -> int:
    ordered_names = [bone_name for bone_name in bone_names if bone_name]
    previous_state = {
        item.bone_name: {
            "enabled": item.enabled,
            "radius": item.radius,
            "stiffness": item.stiffness,
            "damping": item.damping,
            "drag": item.drag,
            "gravity_scale": item.gravity_scale,
        }
        for item in component.joint_overrides
        if item.bone_name
    }

    while component.joint_overrides:
        component.joint_overrides.remove(len(component.joint_overrides) - 1)

    for bone_name in ordered_names:
        entry = component.joint_overrides.add()
        entry.bone_name = bone_name
        state = previous_state.get(bone_name)
        if state is None:
            entry.enabled = False
            entry.radius = component.joint_radius
            entry.stiffness = get_runtime_stiffness(component)
            entry.damping = get_runtime_damping(component)
            entry.drag = get_runtime_drag(component)
            entry.gravity_scale = 1.0
        else:
            entry.enabled = state["enabled"]
            entry.radius = state["radius"]
            entry.stiffness = state["stiffness"]
            entry.damping = state["damping"]
            entry.drag = state["drag"]
            entry.gravity_scale = state["gravity_scale"]

    if component.joint_overrides:
        component.joint_override_index = min(component.joint_override_index, len(component.joint_overrides) - 1)
    else:
        component.joint_override_index = 0

    return len(component.joint_overrides)


def get_runtime_stiffness(component: HoClothSpringBoneComponent) -> float:
    distance_constraint = getattr(component, "distance_constraint", None)
    if distance_constraint is not None and getattr(distance_constraint, "stiffness", None) is not None:
        return float(distance_constraint.stiffness.value)
    return float(component.stiffness)


def get_runtime_damping(component: HoClothSpringBoneComponent) -> float:
    damping_curve = getattr(component, "damping_curve", None)
    if damping_curve is not None:
        return float(damping_curve.value)
    return float(component.damping)


def get_runtime_drag(component: HoClothSpringBoneComponent) -> float:
    angle_restoration = getattr(component, "angle_restoration_constraint", None)
    if angle_restoration is not None:
        return max(0.0, min(1.0, 1.0 - float(angle_restoration.velocity_attenuation)))
    return float(component.drag)


def sync_runtime_compat_fields(component: HoClothSpringBoneComponent) -> None:
    component.stiffness = get_runtime_stiffness(component)
    component.damping = get_runtime_damping(component)
    component.drag = get_runtime_drag(component)
    _sync_disabled_joint_overrides(component)


def apply_mc2_bone_spring_preset(component: HoClothSpringBoneComponent, preset_id: str) -> dict:
    preset = HOCLOTH_MC2_BONE_SPRING_PRESETS[preset_id]
    component.preset_profile = preset_id
    component.joint_radius = preset["joint_radius"]
    component.gravity_strength = preset["gravity_strength"]
    component.gravity_direction = preset["gravity_direction"]
    component.damping_curve.use_curve = preset["damping_use_curve"]
    component.damping_curve.value = preset["damping"]
    component.inertia_constraint.world_inertia = preset["world_inertia"]
    component.inertia_constraint.movement_inertia_smoothing = preset["movement_inertia_smoothing"]
    component.inertia_constraint.movement_speed_limit.use = preset["movement_speed_limit_use"]
    component.inertia_constraint.movement_speed_limit.value = preset["movement_speed_limit"]
    component.inertia_constraint.rotation_speed_limit.use = preset["rotation_speed_limit_use"]
    component.inertia_constraint.rotation_speed_limit.value = preset["rotation_speed_limit"]
    component.inertia_constraint.local_inertia = preset["local_inertia"]
    component.inertia_constraint.local_movement_speed_limit.use = preset["local_movement_speed_limit_use"]
    component.inertia_constraint.local_movement_speed_limit.value = preset["local_movement_speed_limit"]
    component.inertia_constraint.local_rotation_speed_limit.use = preset["local_rotation_speed_limit_use"]
    component.inertia_constraint.local_rotation_speed_limit.value = preset["local_rotation_speed_limit"]
    component.inertia_constraint.depth_inertia = preset["depth_inertia"]
    component.inertia_constraint.centrifugal_acceleration = preset["centrifugal_acceleration"]
    component.inertia_constraint.particle_speed_limit.use = preset["particle_speed_limit_use"]
    component.inertia_constraint.particle_speed_limit.value = preset["particle_speed_limit"]
    component.tether_constraint.distance_compression = preset["tether_distance_compression"]
    component.distance_constraint.stiffness.use_curve = preset["distance_stiffness_use_curve"]
    component.distance_constraint.stiffness.value = preset["distance_stiffness"]
    component.angle_restoration_constraint.use_angle_restoration = preset["angle_restoration_enabled"]
    component.angle_restoration_constraint.stiffness.use_curve = preset["angle_restoration_stiffness_use_curve"]
    component.angle_restoration_constraint.stiffness.value = preset["angle_restoration_stiffness"]
    component.angle_restoration_constraint.velocity_attenuation = preset["angle_restoration_velocity_attenuation"]
    component.spring_constraint.use_spring = preset["use_spring"]
    component.spring_constraint.spring_power = preset["spring_power"]
    component.spring_constraint.limit_distance = preset["limit_distance"]
    component.spring_constraint.normal_limit_ratio = preset["normal_limit_ratio"]
    component.spring_constraint.spring_noise = preset["spring_noise"]
    component.collider_collision_constraint.friction = preset["collider_friction"]
    component.collider_collision_constraint.limit_distance.use_curve = preset["collider_limit_distance_use_curve"]
    component.collider_collision_constraint.limit_distance.value = preset["collider_limit_distance"]
    sync_runtime_compat_fields(component)
    return preset


def initialize_mc2_middle_spring_defaults(component: HoClothSpringBoneComponent) -> None:
    apply_mc2_bone_spring_preset(component, "MIDDLE_SPRING")


def list_component_display_names(scene: bpy.types.Scene, component_ids: list[str]) -> list[str]:
    if not component_ids:
        return []
    name_by_id = {item.component_id: item.display_name for item in scene.hocloth_components}
    return [name_by_id[component_id] for component_id in component_ids if component_id in name_by_id]


def find_component_by_id(container, component_id: str):
    if not component_id:
        return None
    return next((item for item in container if item.component_id == component_id), None)


def rebuild_component_indices(scene: bpy.types.Scene) -> None:
    for item in scene.hocloth_components:
        definition = get_component_definition(item.component_type)
        container = getattr(scene, definition.container_name)
        item.container_index = next(
            (index for index, entry in enumerate(container) if entry.component_id == item.component_id),
            -1,
        )


def create_component(scene: bpy.types.Scene, component_type: str, display_name: str = ""):
    definition = get_component_definition(component_type)
    component_id = generate_component_id()

    typed_item = getattr(scene, definition.container_name).add()
    typed_item.component_id = component_id
    if component_type in {"BONE_CLOTH", "SPRING_BONE", "BONE_CHAIN"}:
        initialize_mc2_middle_spring_defaults(typed_item)

    main_item = scene.hocloth_components.add()
    main_item.component_id = component_id
    main_item.component_type = component_type
    main_item.display_name = display_name or definition.label
    main_item.enabled = True

    rebuild_component_indices(scene)
    return main_item, typed_item


def delete_component(scene: bpy.types.Scene, component_id: str) -> bool:
    main_index = next(
        (index for index, item in enumerate(scene.hocloth_components) if item.component_id == component_id),
        -1,
    )
    if main_index < 0:
        return False

    main_item = scene.hocloth_components[main_index]
    definition = get_component_definition(main_item.component_type)
    typed_container = getattr(scene, definition.container_name)
    typed_index = next(
        (index for index, item in enumerate(typed_container) if item.component_id == component_id),
        -1,
    )
    if typed_index >= 0:
        typed_container.remove(typed_index)

    scene.hocloth_components.remove(main_index)
    rebuild_component_indices(scene)
    if scene.hocloth_components:
        scene.hocloth_component_index = min(scene.hocloth_component_index, len(scene.hocloth_components) - 1)
    else:
        scene.hocloth_component_index = 0
    return True


CLASSES = (
    HoClothComponentItem,
    HoClothSpringJointOverride,
    HoClothCurveParameter,
    HoClothCheckSliderParameter,
    HoClothBoneSpringSpringConstraint,
    HoClothBoneSpringCollisionConstraint,
    HoClothBoneSpringInertiaConstraint,
    HoClothBoneSpringDistanceConstraint,
    HoClothBoneSpringTetherConstraint,
    HoClothBoneSpringAngleRestorationConstraint,
    HoClothSpringBoneComponent,
    HoClothColliderComponent,
    HoClothColliderGroupComponent,
    HoClothCacheOutputComponent,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)

    bpy.types.Scene.hocloth_components = bpy.props.CollectionProperty(type=HoClothComponentItem)
    bpy.types.Scene.hocloth_component_index = bpy.props.IntProperty(name="Component Index", default=0)
    bpy.types.Scene.hocloth_spring_bone_components = bpy.props.CollectionProperty(type=HoClothSpringBoneComponent)
    bpy.types.Scene.hocloth_collider_components = bpy.props.CollectionProperty(type=HoClothColliderComponent)
    bpy.types.Scene.hocloth_collider_group_components = bpy.props.CollectionProperty(type=HoClothColliderGroupComponent)
    bpy.types.Scene.hocloth_cache_output_components = bpy.props.CollectionProperty(type=HoClothCacheOutputComponent)
    bpy.types.Scene.hocloth_runtime_status = bpy.props.StringProperty(
        name="Runtime Status",
        default="Idle",
    )
    bpy.types.Scene.hocloth_runtime_backend = bpy.props.StringProperty(
        name="Runtime Backend",
        default="none",
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_compile_summary = bpy.props.StringProperty(
        name="Compile Summary",
        default="Not compiled",
    )
    bpy.types.Scene.hocloth_runtime_dt = bpy.props.FloatProperty(
        name="Runtime dt",
        default=1.0 / 30.0,
        min=0.0001,
    )
    bpy.types.Scene.hocloth_simulation_frequency = bpy.props.IntProperty(
        name="Simulation Frequency",
        default=90,
        min=30,
        max=150,
        soft_min=30,
        soft_max=150,
        description="Fixed simulation steps per second. Default matches MagicaCloth2.",
    )
    bpy.types.Scene.hocloth_apply_pose_on_step = bpy.props.BoolProperty(
        name="Apply Pose On Step",
        default=True,
    )
    bpy.types.Scene.hocloth_runtime_step_count = bpy.props.IntProperty(
        name="Runtime Step Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_transform_count = bpy.props.IntProperty(
        name="Runtime Transform Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_applied_count = bpy.props.IntProperty(
        name="Runtime Applied Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_mesh_output_count = bpy.props.IntProperty(
        name="Runtime Mesh Output Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_mesh_applied_count = bpy.props.IntProperty(
        name="Runtime Mesh Applied Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_mesh_vertex_count = bpy.props.IntProperty(
        name="Runtime Mesh Vertex Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_mesh_missing_object_count = bpy.props.IntProperty(
        name="Runtime Mesh Missing Object Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_mesh_topology_mismatch_count = bpy.props.IntProperty(
        name="Runtime Mesh Topology Mismatch Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_missing_bone_count = bpy.props.IntProperty(
        name="Runtime Missing Bone Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_missing_armature_count = bpy.props.IntProperty(
        name="Runtime Missing Armature Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_apply_armature_count = bpy.props.IntProperty(
        name="Runtime Apply Armature Count",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_last_fixed_steps = bpy.props.IntProperty(
        name="Last Fixed Steps",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_live_running = bpy.props.BoolProperty(
        name="Runtime Live Running",
        default=False,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_ui_debug_expanded = bpy.props.BoolProperty(
        name="Show Debug Tools",
        default=False,
    )
    bpy.types.Scene.hocloth_ui_details_expanded = bpy.props.BoolProperty(
        name="Show Details",
        default=False,
    )
    bpy.types.Scene.hocloth_viewport_overlay_enabled = bpy.props.BoolProperty(
        name="Viewport Overlay",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_particle_radius = bpy.props.BoolProperty(
        name="粒子半径",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_colliders = bpy.props.BoolProperty(
        name="碰撞体",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_bones = bpy.props.BoolProperty(
        name="弹簧骨骼",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_overlay_alpha = bpy.props.FloatProperty(
        name="覆盖透明度",
        default=0.65,
        min=0.1,
        max=1.0,
    )
    bpy.types.Scene.hocloth_runtime_handle = bpy.props.IntProperty(
        name="Runtime Handle",
        default=0,
        options={"HIDDEN"},
    )


def unregister():
    del bpy.types.Scene.hocloth_runtime_handle
    del bpy.types.Scene.hocloth_runtime_live_running
    del bpy.types.Scene.hocloth_runtime_apply_armature_count
    del bpy.types.Scene.hocloth_runtime_missing_armature_count
    del bpy.types.Scene.hocloth_runtime_missing_bone_count
    del bpy.types.Scene.hocloth_runtime_mesh_topology_mismatch_count
    del bpy.types.Scene.hocloth_runtime_mesh_missing_object_count
    del bpy.types.Scene.hocloth_runtime_mesh_vertex_count
    del bpy.types.Scene.hocloth_runtime_mesh_applied_count
    del bpy.types.Scene.hocloth_runtime_mesh_output_count
    del bpy.types.Scene.hocloth_runtime_applied_count
    del bpy.types.Scene.hocloth_runtime_transform_count
    del bpy.types.Scene.hocloth_runtime_last_fixed_steps
    del bpy.types.Scene.hocloth_runtime_step_count
    del bpy.types.Scene.hocloth_apply_pose_on_step
    del bpy.types.Scene.hocloth_simulation_frequency
    del bpy.types.Scene.hocloth_runtime_dt
    del bpy.types.Scene.hocloth_ui_details_expanded
    del bpy.types.Scene.hocloth_ui_debug_expanded
    del bpy.types.Scene.hocloth_viewport_overlay_alpha
    del bpy.types.Scene.hocloth_viewport_draw_bones
    del bpy.types.Scene.hocloth_viewport_draw_colliders
    del bpy.types.Scene.hocloth_viewport_draw_particle_radius
    del bpy.types.Scene.hocloth_viewport_overlay_enabled
    del bpy.types.Scene.hocloth_compile_summary
    del bpy.types.Scene.hocloth_runtime_backend
    del bpy.types.Scene.hocloth_runtime_status
    del bpy.types.Scene.hocloth_cache_output_components
    del bpy.types.Scene.hocloth_collider_group_components
    del bpy.types.Scene.hocloth_collider_components
    del bpy.types.Scene.hocloth_spring_bone_components
    del bpy.types.Scene.hocloth_component_index
    del bpy.types.Scene.hocloth_components

    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
