from dataclasses import dataclass, field


@dataclass
class CompiledBone:
    name: str
    parent_index: int


@dataclass
class CompiledBoneChain:
    component_id: str
    armature_name: str
    root_bone_name: str
    bones: list[CompiledBone] = field(default_factory=list)

    @property
    def bone_names(self) -> list[str]:
        return [bone.name for bone in self.bones]


@dataclass
class SimulationCacheDescriptor:
    component_id: str
    source_object_name: str
    topology_hash: str
    cache_format: str = "pc2"
    cache_path: str = ""


@dataclass
class CompiledScene:
    bone_chains: list[CompiledBoneChain] = field(default_factory=list)
    cache_descriptors: list[SimulationCacheDescriptor] = field(default_factory=list)

    def total_bone_count(self) -> int:
        return sum(len(chain.bones) for chain in self.bone_chains)

    def summary(self) -> str:
        return f"bone_chains={len(self.bone_chains)}, bones={self.total_bone_count()}"

    def to_native_dict(self) -> dict:
        return {
            "bone_chains": [
                {
                    "component_id": chain.component_id,
                    "armature_name": chain.armature_name,
                    "root_bone_name": chain.root_bone_name,
                    "bones": [
                        {
                            "name": bone.name,
                            "parent_index": bone.parent_index,
                        }
                        for bone in chain.bones
                    ],
                }
                for chain in self.bone_chains
            ],
            "cache_descriptors": [
                {
                    "component_id": descriptor.component_id,
                    "source_object_name": descriptor.source_object_name,
                    "topology_hash": descriptor.topology_hash,
                    "cache_format": descriptor.cache_format,
                    "cache_path": descriptor.cache_path,
                }
                for descriptor in self.cache_descriptors
            ],
        }
