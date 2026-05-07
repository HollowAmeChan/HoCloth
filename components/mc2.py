from __future__ import annotations

import uuid

import bpy


HOCLOTH_MC2_PRESET_ITEMS = (
    ("ACCESSORY", "Accessory", "MC2 Accessory preset"),
    ("CAPE", "Cape", "MC2 Cape preset"),
    ("FRONT_HAIR", "FrontHair", "MC2 FrontHair preset"),
    ("LONG_HAIR", "LongHair", "MC2 LongHair preset"),
    ("SHORT_HAIR", "ShortHair", "MC2 ShortHair preset"),
    ("SKIRT", "Skirt", "MC2 Skirt preset"),
    ("SOFT_SKIRT", "SoftSkirt", "MC2 SoftSkirt preset"),
    ("MIDDLE_SPRING", "MiddleSpring", "MC2 MiddleSpring preset"),
    ("SOFT_SPRING", "SoftSpring", "MC2 SoftSpring preset"),
    ("HARD_SPRING", "HardSpring", "MC2 HardSpring preset"),
    ("TAIL", "Tail", "MC2 Tail preset"),
)

HOCLOTH_MC2_BONE_SPRING_PRESETS = {
    "ACCESSORY": {"label": "MC2 Accessory", "gravity": 0.0, "damping": 0.10, "radius": 0.02, "distance": 1.0, "angle": 0.15, "angle_velocity": 0.6, "spring": 0.04, "spring_limit": 0.10, "tether": 0.40, "friction": 0.05, "movement_limit": 5.0, "rotation_limit": 720.0, "particle_limit": 4.0, "curves": {}},
    "CAPE": {"label": "MC2 Cape", "gravity": 7.0, "damping": 0.10, "radius": 0.02, "distance": 1.0, "angle": 0.15, "angle_velocity": 0.8, "spring": 0.04, "spring_limit": 0.10, "tether": 0.50, "friction": 0.10, "movement_limit": 5.0, "rotation_limit": 720.0, "particle_limit": 4.0, "curves": {"angle": (1.0, 0.0967835)}},
    "FRONT_HAIR": {"label": "MC2 FrontHair", "gravity": 4.0, "damping": 0.10, "radius": 0.02, "distance": 1.0, "angle": 0.15, "angle_velocity": 0.7, "spring": 0.04, "spring_limit": 0.10, "tether": 0.10, "friction": 0.05, "movement_limit": 3.0, "rotation_limit": 360.0, "particle_limit": 2.0, "curves": {"angle": (1.0, 0.3)}},
    "LONG_HAIR": {"label": "MC2 LongHair", "gravity": 5.0, "damping": 0.10, "radius": 0.02, "distance": 1.0, "angle": 0.20, "angle_velocity": 0.8, "spring": 0.06, "spring_limit": 0.05, "tether": 0.80, "friction": 0.05, "movement_limit": 5.0, "rotation_limit": 720.0, "particle_limit": 3.0, "curves": {"angle": (1.0, 0.106244)}},
    "SHORT_HAIR": {"label": "MC2 ShortHair", "gravity": 2.0, "damping": 0.10, "radius": 0.02, "distance": 1.0, "angle": 0.15, "angle_velocity": 0.6, "spring": 0.03, "spring_limit": 0.05, "tether": 0.40, "friction": 0.05, "movement_limit": 5.0, "rotation_limit": 720.0, "particle_limit": 4.0, "curves": {"angle": (1.0, 0.3)}},
    "SKIRT": {"label": "MC2 Skirt", "gravity": 5.0, "damping": 0.10, "radius": 0.02, "distance": 1.0, "angle": 0.20, "angle_velocity": 0.7, "spring": 0.03, "spring_limit": 0.05, "tether": 0.70, "friction": 0.05, "movement_limit": 5.0, "rotation_limit": 720.0, "particle_limit": 4.0, "curves": {"distance": (1.0, 0.5), "angle": (1.0, 0.204475)}},
    "SOFT_SKIRT": {"label": "MC2 SoftSkirt", "gravity": 3.0, "damping": 0.10, "radius": 0.02, "distance": 1.0, "angle": 0.14, "angle_velocity": 0.7, "spring": 0.03, "spring_limit": 0.05, "tether": 0.70, "friction": 0.05, "movement_limit": 5.0, "rotation_limit": 720.0, "particle_limit": 4.0, "curves": {"distance": (1.0, 0.0433655), "angle": (1.0, 0.0597)}},
    "MIDDLE_SPRING": {"label": "MC2 MiddleSpring", "gravity": 0.0, "damping": 0.30, "radius": 0.02, "distance": 0.242, "angle": 0.40, "angle_velocity": 0.6, "spring": 0.03, "spring_limit": 0.05, "tether": 0.099, "friction": 0.20, "movement_limit": 1.0, "rotation_limit": 360.0, "particle_limit": 4.0, "curves": {"angle": (1.0, 0.502322)}},
    "SOFT_SPRING": {"label": "MC2 SoftSpring", "gravity": 0.0, "damping": 0.20, "radius": 0.02, "distance": 0.242, "angle": 0.20, "angle_velocity": 0.8, "spring": 0.01, "spring_limit": 0.05, "tether": 0.099, "friction": 0.20, "movement_limit": 1.0, "rotation_limit": 360.0, "particle_limit": 4.0, "curves": {"angle": (1.0, 0.502322)}},
    "HARD_SPRING": {"label": "MC2 HardSpring", "gravity": 0.0, "damping": 0.30, "radius": 0.02, "distance": 0.242, "angle": 0.60, "angle_velocity": 0.4, "spring": 0.06, "spring_limit": 0.05, "tether": 0.099, "friction": 0.20, "movement_limit": 1.0, "rotation_limit": 360.0, "particle_limit": 4.0, "curves": {"angle": (1.0, 0.502322)}},
    "TAIL": {"label": "MC2 Tail", "gravity": 0.0, "damping": 0.05, "radius": 0.02, "distance": 1.0, "angle": 0.10, "angle_velocity": 0.5, "spring": 0.01, "spring_limit": 0.05, "tether": 0.80, "friction": 0.05, "movement_limit": 5.0, "rotation_limit": 720.0, "particle_limit": 4.0, "curves": {"angle": (1.0, 0.0973314)}},
}


class HoClothSpringJointOverride(bpy.types.PropertyGroup):
    bone_name: bpy.props.StringProperty(name="Bone Name")
    enabled: bpy.props.BoolProperty(name="Enabled", default=False)
    radius: bpy.props.FloatProperty(name="Radius", default=0.02, min=0.0)
    stiffness: bpy.props.FloatProperty(name="Stiffness", default=0.6, min=0.0, soft_max=2.0)
    damping: bpy.props.FloatProperty(name="Damping", default=0.05, min=0.0, max=1.0)
    drag: bpy.props.FloatProperty(name="Drag", default=0.4, min=0.0, max=1.0)
    gravity_scale: bpy.props.FloatProperty(name="Gravity Scale", default=1.0, min=0.0, soft_max=2.0)


class HoClothCurveParameter(bpy.types.PropertyGroup):
    use_curve: bpy.props.BoolProperty(name="Use Curve", default=False)
    value: bpy.props.FloatProperty(name="Value", default=0.0)
    curve_samples: bpy.props.FloatVectorProperty(name="Curve Samples", size=16, default=(1.0,) * 16)


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
    enabled: bpy.props.BoolProperty(name="Enabled", default=True)
    mode: bpy.props.EnumProperty(
        name="Mode",
        items=(
            ("Point", "Point", "Point collider collision"),
            ("Edge", "Edge", "Edge collider collision"),
        ),
        default="Point",
    )
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

def _poll_armature_object(_self, obj):
    return obj is not None and obj.type == "ARMATURE"


def _poll_mc2_collider_object(_self, obj):
    if obj is None:
        return True
    scene = bpy.context.scene
    if scene is None or not hasattr(scene, "hocloth_mc2_colliders"):
        return False
    return any(collider.collider_object == obj for collider in scene.hocloth_mc2_colliders)


def generate_component_id() -> str:
    return uuid.uuid4().hex


def parse_component_id_list(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def join_component_id_list(component_ids: list[str]) -> str:
    ordered: list[str] = []
    for component_id in component_ids:
        if component_id and component_id not in ordered:
            ordered.append(component_id)
    return ", ".join(ordered)


class HoClothMC2ComponentItem(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    component_type: bpy.props.EnumProperty(
        name="MC2 Component",
        items=(
            ("MAGICA_CLOTH", "MagicaCloth", "MC2 MagicaCloth component"),
            ("SPHERE_COLLIDER", "MagicaSphereCollider", "MC2 sphere collider"),
            ("CAPSULE_COLLIDER", "MagicaCapsuleCollider", "MC2 capsule collider"),
            ("PLANE_COLLIDER", "MagicaPlaneCollider", "MC2 plane collider"),
            ("CACHE_OUTPUT", "Blender Cache Output", "Blender mesh/cache writeback target"),
        ),
        default="MAGICA_CLOTH",
    )
    display_name: bpy.props.StringProperty(name="Display Name")
    enabled: bpy.props.BoolProperty(name="Enabled", default=True)
    ui_expanded: bpy.props.BoolProperty(name="Expanded", default=True)


class HoClothMC2ColliderReference(bpy.types.PropertyGroup):
    collider_object: bpy.props.PointerProperty(
        name="Collider",
        type=bpy.types.Object,
        poll=_poll_mc2_collider_object,
    )


class HoClothMC2MagicaClothComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    authoring_mode: bpy.props.EnumProperty(
        name="Authoring Mode",
        items=(
            ("BONE_CLOTH", "BoneCloth", "MC2 BoneCloth root-bone authoring"),
            ("BONE_SPRING", "BoneSpring", "MC2 BoneSpring authoring"),
        ),
        default="BONE_CLOTH",
    )
    armature_object: bpy.props.PointerProperty(
        name="Armature",
        type=bpy.types.Object,
        poll=_poll_armature_object,
    )
    root_bone_name: bpy.props.StringProperty(name="Root Bone")
    center_source: bpy.props.EnumProperty(
        name="Center",
        items=(
            ("NONE", "None", "Use root as center"),
            ("OBJECT", "Object", "Use object transform as center"),
            ("BONE", "Bone", "Use bone transform as center"),
        ),
        default="NONE",
    )
    center_object: bpy.props.PointerProperty(name="Center Object", type=bpy.types.Object)
    center_armature_object: bpy.props.PointerProperty(
        name="Center Armature",
        type=bpy.types.Object,
        poll=_poll_armature_object,
    )
    center_bone_name: bpy.props.StringProperty(name="Center Bone")
    preset_profile: bpy.props.EnumProperty(
        name="Preset",
        items=HOCLOTH_MC2_PRESET_ITEMS,
        default="MIDDLE_SPRING",
    )
    collider_ids: bpy.props.StringProperty(name="Collider List")
    collider_references: bpy.props.CollectionProperty(type=HoClothMC2ColliderReference)
    collider_reference_index: bpy.props.IntProperty(name="Collider Index", default=0, min=0)
    joint_radius: bpy.props.FloatProperty(name="Radius", default=0.02, min=0.0)
    gravity_strength: bpy.props.FloatProperty(name="Gravity", default=5.0, min=0.0, soft_max=10.0)
    gravity_direction: bpy.props.FloatVectorProperty(
        name="Gravity Direction",
        size=3,
        default=(0.0, -1.0, 0.0),
    )
    damping_curve: bpy.props.PointerProperty(name="Damping", type=HoClothCurveParameter)
    inertia_constraint: bpy.props.PointerProperty(name="Inertia", type=HoClothBoneSpringInertiaConstraint)
    tether_constraint: bpy.props.PointerProperty(name="Tether", type=HoClothBoneSpringTetherConstraint)
    distance_constraint: bpy.props.PointerProperty(name="Distance", type=HoClothBoneSpringDistanceConstraint)
    angle_restoration_constraint: bpy.props.PointerProperty(
        name="Angle Restoration",
        type=HoClothBoneSpringAngleRestorationConstraint,
    )
    spring_constraint: bpy.props.PointerProperty(name="Spring", type=HoClothBoneSpringSpringConstraint)
    collider_collision_constraint: bpy.props.PointerProperty(
        name="Collider Collision",
        type=HoClothBoneSpringCollisionConstraint,
    )
    joint_overrides: bpy.props.CollectionProperty(type=HoClothSpringJointOverride)
    joint_override_index: bpy.props.IntProperty(name="Joint Override Index", default=0)

    @property
    def collider_group_ids(self) -> str:
        return ""


class HoClothMC2ColliderComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    collider_type: bpy.props.EnumProperty(
        name="Collider Type",
        items=(
            ("SPHERE", "MagicaSphereCollider", "Sphere collider"),
            ("CAPSULE", "MagicaCapsuleCollider", "Capsule collider"),
            ("PLANE", "MagicaPlaneCollider", "Plane collider"),
        ),
        default="CAPSULE",
    )
    collider_object: bpy.props.PointerProperty(name="Object", type=bpy.types.Object)
    center: bpy.props.FloatVectorProperty(name="Center", size=3, default=(0.0, 0.0, 0.0))
    radius: bpy.props.FloatProperty(name="Radius", default=0.05, min=0.001)
    end_radius: bpy.props.FloatProperty(name="End Radius", default=0.05, min=0.001)
    length: bpy.props.FloatProperty(name="Length", default=0.1, min=0.001)
    direction: bpy.props.EnumProperty(
        name="Direction",
        items=(
            ("X", "X-Axis", "Local X axis"),
            ("Y", "Y-Axis", "Local Y axis"),
            ("Z", "Z-Axis", "Local Z axis"),
        ),
        default="X",
    )
    reverse_direction: bpy.props.BoolProperty(name="Reverse Direction", default=False)
    radius_separation: bpy.props.BoolProperty(name="Radius Separation", default=False)
    aligned_on_center: bpy.props.BoolProperty(name="Aligned On Center", default=True)


class HoClothMC2CacheOutputComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    source_object: bpy.props.PointerProperty(name="Source Object", type=bpy.types.Object)
    cache_format: bpy.props.EnumProperty(
        name="Cache Format",
        items=(
            ("pc2", "PC2", "Point cache 2"),
            ("mdd", "MDD", "MDD cache"),
        ),
        default="pc2",
    )
    cache_path: bpy.props.StringProperty(name="Cache Path", subtype="FILE_PATH")


CLASSES = (
    HoClothSpringJointOverride,
    HoClothCurveParameter,
    HoClothCheckSliderParameter,
    HoClothBoneSpringSpringConstraint,
    HoClothBoneSpringCollisionConstraint,
    HoClothBoneSpringInertiaConstraint,
    HoClothBoneSpringDistanceConstraint,
    HoClothBoneSpringTetherConstraint,
    HoClothBoneSpringAngleRestorationConstraint,
    HoClothMC2ComponentItem,
    HoClothMC2ColliderReference,
    HoClothMC2MagicaClothComponent,
    HoClothMC2ColliderComponent,
    HoClothMC2CacheOutputComponent,
)


def _add_main_item(scene, component_id: str, component_type: str, display_name: str):
    item = scene.hocloth_mc2_components.add()
    item.component_id = component_id
    item.component_type = component_type
    item.display_name = display_name
    item.enabled = True
    item.ui_expanded = True
    return item


def create_magica_cloth(scene, authoring_mode: str, display_name: str = ""):
    component_id = generate_component_id()
    cloth = scene.hocloth_mc2_magica_cloths.add()
    cloth.component_id = component_id
    cloth.authoring_mode = authoring_mode
    apply_preset(cloth, "MIDDLE_SPRING")
    if authoring_mode == "BONE_CLOTH":
        cloth.spring_constraint.use_spring = False
    _add_main_item(
        scene,
        component_id,
        "MAGICA_CLOTH",
        display_name or f"MagicaCloth: {authoring_mode}",
    )
    return cloth


def create_collider(scene, collider_type: str, display_name: str = ""):
    component_id = generate_component_id()
    collider = scene.hocloth_mc2_colliders.add()
    collider.component_id = component_id
    collider.collider_type = collider_type
    _add_main_item(
        scene,
        component_id,
        f"{collider_type}_COLLIDER",
        display_name or f"Magica{collider_type.title()}Collider",
    )
    return collider


def create_cache_output(scene, display_name: str = ""):
    component_id = generate_component_id()
    cache = scene.hocloth_mc2_cache_outputs.add()
    cache.component_id = component_id
    _add_main_item(scene, component_id, "CACHE_OUTPUT", display_name or "Blender Cache Output")
    return cache


def delete_component(scene, component_id: str) -> bool:
    removed = False
    for collection_name in (
        "hocloth_mc2_magica_cloths",
        "hocloth_mc2_colliders",
        "hocloth_mc2_cache_outputs",
    ):
        collection = getattr(scene, collection_name)
        index = next((i for i, item in enumerate(collection) if item.component_id == component_id), -1)
        if index >= 0:
            collection.remove(index)
            removed = True

    main_index = next((i for i, item in enumerate(scene.hocloth_mc2_components) if item.component_id == component_id), -1)
    if main_index >= 0:
        scene.hocloth_mc2_components.remove(main_index)
        removed = True
    return removed


def find_magica_cloth(scene, component_id: str):
    return next((item for item in scene.hocloth_mc2_magica_cloths if item.component_id == component_id), None)


def find_collider(scene, component_id: str):
    return next((item for item in scene.hocloth_mc2_colliders if item.component_id == component_id), None)


def find_collider_by_object(scene, collider_object):
    if collider_object is None:
        return None
    return next(
        (
            item
            for item in scene.hocloth_mc2_colliders
            if item.collider_object == collider_object
        ),
        None,
    )


def resolve_cloth_collider_ids(scene, cloth) -> list[str]:
    component_ids: list[str] = []
    for reference in getattr(cloth, "collider_references", []):
        collider = find_collider_by_object(scene, reference.collider_object)
        if collider is not None and collider.component_id not in component_ids:
            component_ids.append(collider.component_id)
    for component_id in parse_component_id_list(getattr(cloth, "collider_ids", "")):
        if component_id not in component_ids:
            component_ids.append(component_id)
    return component_ids


def find_cache_output(scene, component_id: str):
    return next((item for item in scene.hocloth_mc2_cache_outputs if item.component_id == component_id), None)


def cloth_runtime_defaults(cloth) -> tuple[float, float, float]:
    stiffness = float(cloth.distance_constraint.stiffness.value)
    damping = float(cloth.damping_curve.value)
    drag = max(0.0, min(1.0, 1.0 - float(cloth.angle_restoration_constraint.velocity_attenuation)))
    return stiffness, damping, drag


def _sync_joint_override_defaults(cloth) -> None:
    stiffness, damping, drag = cloth_runtime_defaults(cloth)
    for entry in cloth.joint_overrides:
        if entry.enabled:
            continue
        entry.radius = cloth.joint_radius
        entry.stiffness = stiffness
        entry.damping = damping
        entry.drag = drag


def _linear_curve_samples(start: float = 1.0, end: float = 1.0) -> tuple[float, ...]:
    return tuple(float(start) + (float(end) - float(start)) * (index / 15.0) for index in range(16))


def _set_curve_parameter(parameter, value: float, curve: tuple[float, float] | None = None) -> None:
    parameter.value = float(value)
    parameter.use_curve = curve is not None
    if curve is None:
        parameter.curve_samples = _linear_curve_samples(1.0, 1.0)
    else:
        parameter.curve_samples = _linear_curve_samples(curve[0], curve[1])


def apply_preset(cloth, preset_id: str | None = None) -> None:
    preset_id = preset_id or cloth.preset_profile
    preset = HOCLOTH_MC2_BONE_SPRING_PRESETS[preset_id]
    curves = preset.get("curves", {})
    cloth.preset_profile = preset_id
    cloth.joint_radius = preset["radius"]
    cloth.gravity_strength = preset["gravity"]
    cloth.gravity_direction = (0.0, -1.0, 0.0)
    _set_curve_parameter(cloth.damping_curve, preset["damping"], curves.get("damping"))
    _set_curve_parameter(cloth.distance_constraint.stiffness, preset["distance"], curves.get("distance"))
    cloth.angle_restoration_constraint.use_angle_restoration = True
    _set_curve_parameter(cloth.angle_restoration_constraint.stiffness, preset["angle"], curves.get("angle"))
    cloth.angle_restoration_constraint.velocity_attenuation = preset["angle_velocity"]
    cloth.spring_constraint.use_spring = True
    cloth.spring_constraint.spring_power = preset["spring"]
    cloth.spring_constraint.limit_distance = preset["spring_limit"]
    cloth.spring_constraint.normal_limit_ratio = 1.0
    cloth.spring_constraint.spring_noise = 0.0
    cloth.tether_constraint.distance_compression = preset["tether"]
    cloth.inertia_constraint.movement_speed_limit.use = True
    cloth.inertia_constraint.movement_speed_limit.value = preset["movement_limit"]
    cloth.inertia_constraint.rotation_speed_limit.use = True
    cloth.inertia_constraint.rotation_speed_limit.value = preset["rotation_limit"]
    cloth.inertia_constraint.local_movement_speed_limit.use = True
    cloth.inertia_constraint.local_movement_speed_limit.value = preset["movement_limit"]
    cloth.inertia_constraint.local_rotation_speed_limit.use = True
    cloth.inertia_constraint.local_rotation_speed_limit.value = preset["rotation_limit"]
    cloth.inertia_constraint.particle_speed_limit.use = True
    cloth.inertia_constraint.particle_speed_limit.value = preset["particle_limit"]
    cloth.collider_collision_constraint.enabled = True
    cloth.collider_collision_constraint.mode = "Point"
    cloth.collider_collision_constraint.friction = preset["friction"]
    _set_curve_parameter(cloth.collider_collision_constraint.limit_distance, 0.05, curves.get("collider_limit"))
    _sync_joint_override_defaults(cloth)


def sync_joint_override_names(cloth, bone_names: list[str]) -> int:
    previous = {
        item.bone_name: {
            "enabled": item.enabled,
            "radius": item.radius,
            "stiffness": item.stiffness,
            "damping": item.damping,
            "drag": item.drag,
            "gravity_scale": item.gravity_scale,
        }
        for item in cloth.joint_overrides
        if item.bone_name
    }
    while cloth.joint_overrides:
        cloth.joint_overrides.remove(len(cloth.joint_overrides) - 1)
    default_stiffness, default_damping, default_drag = cloth_runtime_defaults(cloth)
    for bone_name in [name for name in bone_names if name]:
        entry = cloth.joint_overrides.add()
        entry.bone_name = bone_name
        state = previous.get(bone_name)
        if state is None:
            entry.enabled = False
            entry.radius = cloth.joint_radius
            entry.stiffness = default_stiffness
            entry.damping = default_damping
            entry.drag = default_drag
            entry.gravity_scale = 1.0
        else:
            entry.enabled = state["enabled"]
            entry.radius = state["radius"]
            entry.stiffness = state["stiffness"]
            entry.damping = state["damping"]
            entry.drag = state["drag"]
            entry.gravity_scale = state["gravity_scale"]
    cloth.joint_override_index = min(cloth.joint_override_index, max(len(cloth.joint_overrides) - 1, 0))
    return len(cloth.joint_overrides)


def has_mc2_components(scene) -> bool:
    return hasattr(scene, "hocloth_mc2_components") and len(scene.hocloth_mc2_components) > 0


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)

    bpy.types.Scene.hocloth_mc2_components = bpy.props.CollectionProperty(type=HoClothMC2ComponentItem)
    bpy.types.Scene.hocloth_mc2_magica_cloths = bpy.props.CollectionProperty(type=HoClothMC2MagicaClothComponent)
    bpy.types.Scene.hocloth_mc2_colliders = bpy.props.CollectionProperty(type=HoClothMC2ColliderComponent)
    bpy.types.Scene.hocloth_mc2_cache_outputs = bpy.props.CollectionProperty(type=HoClothMC2CacheOutputComponent)


def unregister():
    del bpy.types.Scene.hocloth_mc2_cache_outputs
    del bpy.types.Scene.hocloth_mc2_colliders
    del bpy.types.Scene.hocloth_mc2_magica_cloths
    del bpy.types.Scene.hocloth_mc2_components

    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
