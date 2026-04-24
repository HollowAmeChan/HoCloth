import uuid

import bpy

from .registry import COMPONENT_DEFINITIONS, get_component_definition


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
    for entry in getattr(component, "joint_overrides", []):
        if getattr(entry, "enabled", False):
            continue
        entry.radius = component.joint_radius
        entry.stiffness = component.stiffness
        entry.damping = component.damping
        entry.drag = component.drag
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


class HoClothSpringBoneComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    armature_object: bpy.props.PointerProperty(
        name="Armature",
        type=bpy.types.Object,
        poll=_poll_armature_object,
        update=_update_armature_object,
    )
    root_bone_name: bpy.props.StringProperty(name="Root Bone", update=_update_root_bone_name)
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
    append_tail_tip: bpy.props.BoolProperty(
        name="Append Tail Tip Joint",
        default=False,
    )
    joint_radius: bpy.props.FloatProperty(name="Particle Radius", default=0.02, min=0.0, update=_update_spring_defaults)
    collider_group_ids: bpy.props.StringProperty(name="Collision Bindings")
    stiffness: bpy.props.FloatProperty(name="Stiffness", default=0.6, min=0.0, soft_max=2.0, update=_update_spring_defaults)
    damping: bpy.props.FloatProperty(name="Damping", default=0.5, min=0.0, max=1.0, update=_update_spring_defaults)
    drag: bpy.props.FloatProperty(name="Drag", default=0.1, min=0.0, max=1.0, update=_update_spring_defaults)
    gravity_strength: bpy.props.FloatProperty(name="Gravity Strength", default=0.3, min=0.0, soft_max=2.0)
    gravity_direction: bpy.props.FloatVectorProperty(
        name="Gravity Direction",
        size=3,
        default=(0.0, 0.0, -1.0),
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


class HoClothColliderGroupComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    collider_ids: bpy.props.StringProperty(name="Collider Sources")


class HoClothCacheOutputComponent(bpy.types.PropertyGroup):
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
    entry.stiffness = component.stiffness
    entry.damping = component.damping
    entry.drag = component.drag
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
            entry.stiffness = component.stiffness
            entry.damping = component.damping
            entry.drag = component.drag
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
    bpy.types.Scene.hocloth_max_simulation_steps_per_frame = bpy.props.IntProperty(
        name="Max Steps Per Frame",
        default=5,
        min=1,
        max=16,
        soft_max=8,
        description="Caps fixed simulation steps executed for one Blender frame.",
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
    bpy.types.Scene.hocloth_runtime_last_fixed_steps = bpy.props.IntProperty(
        name="Last Fixed Steps",
        default=0,
        options={"HIDDEN"},
    )
    bpy.types.Scene.hocloth_runtime_last_skipped_steps = bpy.props.IntProperty(
        name="Last Skipped Steps",
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
    bpy.types.Scene.hocloth_viewport_overlay_enabled = bpy.props.BoolProperty(
        name="Viewport Overlay",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_particle_radius = bpy.props.BoolProperty(
        name="Particle Radius",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_colliders = bpy.props.BoolProperty(
        name="Colliders",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_draw_bones = bpy.props.BoolProperty(
        name="Spring Bones",
        default=True,
    )
    bpy.types.Scene.hocloth_viewport_overlay_alpha = bpy.props.FloatProperty(
        name="Overlay Alpha",
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
    del bpy.types.Scene.hocloth_runtime_transform_count
    del bpy.types.Scene.hocloth_runtime_last_fixed_steps
    del bpy.types.Scene.hocloth_runtime_last_skipped_steps
    del bpy.types.Scene.hocloth_runtime_step_count
    del bpy.types.Scene.hocloth_apply_pose_on_step
    del bpy.types.Scene.hocloth_simulation_frequency
    del bpy.types.Scene.hocloth_max_simulation_steps_per_frame
    del bpy.types.Scene.hocloth_runtime_dt
    del bpy.types.Scene.hocloth_ui_debug_expanded
    del bpy.types.Scene.hocloth_viewport_overlay_alpha
    del bpy.types.Scene.hocloth_viewport_draw_bones
    del bpy.types.Scene.hocloth_viewport_draw_colliders
    del bpy.types.Scene.hocloth_viewport_draw_particle_radius
    del bpy.types.Scene.hocloth_viewport_overlay_enabled
    del bpy.types.Scene.hocloth_compile_summary
    del bpy.types.Scene.hocloth_runtime_status
    del bpy.types.Scene.hocloth_cache_output_components
    del bpy.types.Scene.hocloth_collider_group_components
    del bpy.types.Scene.hocloth_collider_components
    del bpy.types.Scene.hocloth_spring_bone_components
    del bpy.types.Scene.hocloth_component_index
    del bpy.types.Scene.hocloth_components

    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
