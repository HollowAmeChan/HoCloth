from dataclasses import dataclass, field


@dataclass
class CompiledBoneChain:
    component_id: str
    armature_name: str
    root_bone_name: str
    bone_names: list[str] = field(default_factory=list)


@dataclass
class CompiledScene:
    bone_chains: list[CompiledBoneChain] = field(default_factory=list)

    def summary(self) -> str:
        return f"bone_chains={len(self.bone_chains)}"
