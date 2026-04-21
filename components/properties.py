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


class HoClothComponentItem(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    component_type: bpy.props.StringProperty(name="Component Type")
    container_index: bpy.props.IntProperty(name="Container Index", default=-1)
    display_name: bpy.props.StringProperty(name="Display Name")
    enabled: bpy.props.BoolProperty(name="Enabled", default=True)
    ui_expanded: bpy.props.BoolProperty(name="Expanded", default=True)


class HoClothBoneChainComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    armature_object: bpy.props.PointerProperty(
        name="Armature",
        type=bpy.types.Object,
        poll=_poll_armature_object,
        update=_update_armature_object,
    )
    root_bone_name: bpy.props.StringProperty(name="Root Bone", update=_update_root_bone_name)


class HoClothColliderComponent(bpy.types.PropertyGroup):
    component_id: bpy.props.StringProperty(name="Component ID")
    collider_object: bpy.props.PointerProperty(name="Object", type=bpy.types.Object)
    radius: bpy.props.FloatProperty(name="Radius", default=0.05, min=0.0)
    height: bpy.props.FloatProperty(name="Height", default=0.1, min=0.0)


def generate_component_id() -> str:
    return uuid.uuid4().hex


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
    HoClothBoneChainComponent,
    HoClothColliderComponent,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)

    bpy.types.Scene.hocloth_components = bpy.props.CollectionProperty(type=HoClothComponentItem)
    bpy.types.Scene.hocloth_component_index = bpy.props.IntProperty(name="Component Index", default=0)
    bpy.types.Scene.hocloth_bone_chain_components = bpy.props.CollectionProperty(type=HoClothBoneChainComponent)
    bpy.types.Scene.hocloth_collider_components = bpy.props.CollectionProperty(type=HoClothColliderComponent)
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
        default=1.0 / 60.0,
        min=0.0001,
    )
    bpy.types.Scene.hocloth_runtime_substeps = bpy.props.IntProperty(
        name="Runtime Substeps",
        default=1,
        min=1,
        soft_max=8,
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
    bpy.types.Scene.hocloth_runtime_handle = bpy.props.IntProperty(
        name="Runtime Handle",
        default=0,
        options={"HIDDEN"},
    )


def unregister():
    del bpy.types.Scene.hocloth_runtime_handle
    del bpy.types.Scene.hocloth_runtime_transform_count
    del bpy.types.Scene.hocloth_runtime_step_count
    del bpy.types.Scene.hocloth_apply_pose_on_step
    del bpy.types.Scene.hocloth_runtime_substeps
    del bpy.types.Scene.hocloth_runtime_dt
    del bpy.types.Scene.hocloth_compile_summary
    del bpy.types.Scene.hocloth_runtime_status
    del bpy.types.Scene.hocloth_collider_components
    del bpy.types.Scene.hocloth_bone_chain_components
    del bpy.types.Scene.hocloth_component_index
    del bpy.types.Scene.hocloth_components

    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
