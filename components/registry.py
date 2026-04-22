from dataclasses import dataclass


@dataclass(frozen=True)
class ComponentDefinition:
    type_name: str
    label: str
    container_name: str


COMPONENT_DEFINITIONS = (
    ComponentDefinition("SPRING_BONE", "Spring Bone", "hocloth_spring_bone_components"),
    ComponentDefinition("BONE_CHAIN", "Bone Chain (Legacy)", "hocloth_spring_bone_components"),
    ComponentDefinition("COLLIDER", "Collider", "hocloth_collider_components"),
    ComponentDefinition("COLLIDER_GROUP", "Collider Group", "hocloth_collider_group_components"),
    ComponentDefinition("CACHE_OUTPUT", "Cache Output", "hocloth_cache_output_components"),
)

COMPONENT_BY_TYPE = {item.type_name: item for item in COMPONENT_DEFINITIONS}


def get_component_definition(type_name: str) -> ComponentDefinition:
    return COMPONENT_BY_TYPE[type_name]


def list_component_definitions():
    return COMPONENT_DEFINITIONS


def register():
    return None


def unregister():
    return None
