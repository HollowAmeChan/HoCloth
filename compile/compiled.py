from dataclasses import dataclass, field


@dataclass
class CompiledBone:
    name: str
    parent_index: int
    length: float
    rest_head_local: tuple[float, float, float]
    rest_tail_local: tuple[float, float, float]
    rest_local_translation: tuple[float, float, float]
    rest_local_rotation: tuple[float, float, float, float]


@dataclass
class CompiledBoneChain:
    component_id: str
    armature_name: str
    root_bone_name: str
    stiffness: float
    damping: float
    drag: float
    gravity_strength: float
    gravity_direction: tuple[float, float, float]
    bones: list[CompiledBone] = field(default_factory=list)

    @property
    def bone_names(self) -> list[str]:
        return [bone.name for bone in self.bones]


@dataclass
class CompiledCollider:
    component_id: str
    object_name: str
    shape_type: str
    radius: float
    height: float
    world_translation: tuple[float, float, float]
    world_rotation: tuple[float, float, float, float]


@dataclass
class SimulationCacheDescriptor:
    component_id: str
    source_object_name: str
    topology_hash: str
    cache_format: str = "pc2"
    cache_path: str = ""

    def to_dict(self) -> dict:
        return {
            "component_id": self.component_id,
            "source_object_name": self.source_object_name,
            "topology_hash": self.topology_hash,
            "cache_format": self.cache_format,
            "cache_path": self.cache_path,
        }


@dataclass
class CompiledScene:
    bone_chains: list[CompiledBoneChain] = field(default_factory=list)
    colliders: list[CompiledCollider] = field(default_factory=list)
    cache_descriptors: list[SimulationCacheDescriptor] = field(default_factory=list)

    def total_bone_count(self) -> int:
        return sum(len(chain.bones) for chain in self.bone_chains)

    def summary(self) -> str:
        return (
            f"bone_chains={len(self.bone_chains)}, "
            f"bones={self.total_bone_count()}, "
            f"colliders={len(self.colliders)}"
        )

    def to_dict(self) -> dict:
        data = {
            "bone_chains": [
                {
                    "component_id": chain.component_id,
                    "armature_name": chain.armature_name,
                    "root_bone_name": chain.root_bone_name,
                    "stiffness": chain.stiffness,
                    "damping": chain.damping,
                    "drag": chain.drag,
                    "gravity_strength": chain.gravity_strength,
                    "gravity_direction": chain.gravity_direction,
                    "bones": [
                        {
                            "name": bone.name,
                            "parent_index": bone.parent_index,
                            "length": bone.length,
                            "rest_head_local": bone.rest_head_local,
                            "rest_tail_local": bone.rest_tail_local,
                            "rest_local_translation": bone.rest_local_translation,
                            "rest_local_rotation": bone.rest_local_rotation,
                        }
                        for bone in chain.bones
                    ],
                }
                for chain in self.bone_chains
            ],
            "colliders": [
                {
                    "component_id": collider.component_id,
                    "object_name": collider.object_name,
                    "shape_type": collider.shape_type,
                    "radius": collider.radius,
                    "height": collider.height,
                    "world_translation": collider.world_translation,
                    "world_rotation": collider.world_rotation,
                }
                for collider in self.colliders
            ],
        }
        if self.cache_descriptors:
            data["cache_descriptors"] = [descriptor.to_dict() for descriptor in self.cache_descriptors]
        return data

    def to_native_dict(self) -> dict:
        return self.to_dict()
