from dataclasses import dataclass, field


@dataclass
class CompiledSpringJoint:
    name: str
    parent_index: int
    depth: int
    length: float
    radius: float
    stiffness: float
    damping: float
    drag: float
    gravity_scale: float
    rest_head_local: tuple[float, float, float]
    rest_tail_local: tuple[float, float, float]
    rest_local_translation: tuple[float, float, float]
    rest_local_rotation: tuple[float, float, float, float]


@dataclass
class CompiledSpringLine:
    start_index: int
    end_index: int


@dataclass
class CompiledSpringBaseline:
    joint_indices: list[int] = field(default_factory=list)


@dataclass
class CompiledSpringBone:
    component_id: str
    armature_name: str
    root_bone_name: str
    center_object_name: str
    center_bone_name: str
    joint_radius: float
    stiffness: float
    damping: float
    drag: float
    use_spring: bool
    spring_power: float
    limit_distance: float
    normal_limit_ratio: float
    spring_noise: float
    collider_friction: float
    collider_limit_distance: float
    gravity_strength: float
    gravity_direction: tuple[float, float, float]
    collider_group_ids: list[str] = field(default_factory=list)
    collision_binding_ids: list[str] = field(default_factory=list)
    armature_scale: tuple[float, float, float] = (1.0, 1.0, 1.0)
    joints: list[CompiledSpringJoint] = field(default_factory=list)
    lines: list[CompiledSpringLine] = field(default_factory=list)
    baselines: list[CompiledSpringBaseline] = field(default_factory=list)
    line_start_indices: list[int] = field(default_factory=list)
    line_counts: list[int] = field(default_factory=list)
    line_data: list[int] = field(default_factory=list)
    baseline_start_indices: list[int] = field(default_factory=list)
    baseline_counts: list[int] = field(default_factory=list)
    baseline_data: list[int] = field(default_factory=list)

    @property
    def bone_names(self) -> list[str]:
        return [joint.name for joint in self.joints]

    @property
    def bones(self) -> list["CompiledSpringJoint"]:
        return self.joints


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
class CompiledColliderGroup:
    component_id: str
    collider_ids: list[str] = field(default_factory=list)


@dataclass
class CompiledCollisionObject:
    collision_object_id: str
    owner_component_id: str
    motion_type: str
    shape_type: str
    world_translation: tuple[float, float, float]
    world_rotation: tuple[float, float, float, float]
    linear_velocity: tuple[float, float, float] = (0.0, 0.0, 0.0)
    angular_velocity: tuple[float, float, float] = (0.0, 0.0, 0.0)
    radius: float = 0.0
    height: float = 0.0
    source_object_name: str = ""
    source_group_ids: list[str] = field(default_factory=list)


@dataclass
class CompiledCollisionBinding:
    binding_id: str
    owner_component_id: str
    source_group_ids: list[str] = field(default_factory=list)
    collision_object_ids: list[str] = field(default_factory=list)


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
    spring_bones: list[CompiledSpringBone] = field(default_factory=list)
    colliders: list[CompiledCollider] = field(default_factory=list)
    collider_groups: list[CompiledColliderGroup] = field(default_factory=list)
    collision_objects: list[CompiledCollisionObject] = field(default_factory=list)
    collision_bindings: list[CompiledCollisionBinding] = field(default_factory=list)
    cache_descriptors: list[SimulationCacheDescriptor] = field(default_factory=list)

    def total_bone_count(self) -> int:
        return sum(len(chain.joints) for chain in self.spring_bones)

    @property
    def bone_chains(self) -> list[CompiledSpringBone]:
        return self.spring_bones

    def summary(self) -> str:
        return (
            f"spring_bones={len(self.spring_bones)}, "
            f"bones={self.total_bone_count()}, "
            f"colliders={len(self.colliders)}, "
            f"collider_groups={len(self.collider_groups)}, "
            f"collision_objects={len(self.collision_objects)}, "
            f"collision_bindings={len(self.collision_bindings)}, "
            f"cache_outputs={len(self.cache_descriptors)}"
        )

    def to_dict(self) -> dict:
        data = {
            "spring_bones": [
                {
                    "component_id": chain.component_id,
                    "armature_name": chain.armature_name,
                    "root_bone_name": chain.root_bone_name,
                    "center_object_name": chain.center_object_name,
                    "center_bone_name": chain.center_bone_name,
                    "joint_radius": chain.joint_radius,
                    "collider_group_ids": list(chain.collider_group_ids),
                    "collision_binding_ids": list(chain.collision_binding_ids),
                    "stiffness": chain.stiffness,
                    "damping": chain.damping,
                    "drag": chain.drag,
                    "use_spring": chain.use_spring,
                    "spring_power": chain.spring_power,
                    "limit_distance": chain.limit_distance,
                    "normal_limit_ratio": chain.normal_limit_ratio,
                    "spring_noise": chain.spring_noise,
                    "collider_friction": chain.collider_friction,
                    "collider_limit_distance": chain.collider_limit_distance,
                    "gravity_strength": chain.gravity_strength,
                    "gravity_direction": chain.gravity_direction,
                    "armature_scale": chain.armature_scale,
                    "joints": [
                        {
                            "name": joint.name,
                            "parent_index": joint.parent_index,
                            "depth": joint.depth,
                            "length": joint.length,
                            "radius": joint.radius,
                            "stiffness": joint.stiffness,
                            "damping": joint.damping,
                            "drag": joint.drag,
                            "gravity_scale": joint.gravity_scale,
                            "rest_head_local": joint.rest_head_local,
                            "rest_tail_local": joint.rest_tail_local,
                            "rest_local_translation": joint.rest_local_translation,
                            "rest_local_rotation": joint.rest_local_rotation,
                        }
                        for joint in chain.joints
                    ],
                    "lines": [
                        {
                            "start_index": line.start_index,
                            "end_index": line.end_index,
                        }
                        for line in chain.lines
                    ],
                    "line_start_indices": list(chain.line_start_indices),
                    "line_counts": list(chain.line_counts),
                    "line_data": list(chain.line_data),
                    "baselines": [
                        {
                            "joint_indices": list(baseline.joint_indices),
                        }
                        for baseline in chain.baselines
                    ],
                    "baseline_start_indices": list(chain.baseline_start_indices),
                    "baseline_counts": list(chain.baseline_counts),
                    "baseline_data": list(chain.baseline_data),
                }
                for chain in self.spring_bones
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
            "collider_groups": [
                {
                    "component_id": group.component_id,
                    "collider_ids": list(group.collider_ids),
                }
                for group in self.collider_groups
            ],
            "collision_objects": [
                {
                    "collision_object_id": collision_object.collision_object_id,
                    "owner_component_id": collision_object.owner_component_id,
                    "motion_type": collision_object.motion_type,
                    "shape_type": collision_object.shape_type,
                    "world_translation": collision_object.world_translation,
                    "world_rotation": collision_object.world_rotation,
                    "linear_velocity": collision_object.linear_velocity,
                    "angular_velocity": collision_object.angular_velocity,
                    "radius": collision_object.radius,
                    "height": collision_object.height,
                    "source_object_name": collision_object.source_object_name,
                    "source_group_ids": list(collision_object.source_group_ids),
                }
                for collision_object in self.collision_objects
            ],
            "collision_bindings": [
                {
                    "binding_id": binding.binding_id,
                    "owner_component_id": binding.owner_component_id,
                    "source_group_ids": list(binding.source_group_ids),
                    "collision_object_ids": list(binding.collision_object_ids),
                }
                for binding in self.collision_bindings
            ],
        }
        data["bone_chains"] = data["spring_bones"]
        if self.cache_descriptors:
            data["cache_descriptors"] = [descriptor.to_dict() for descriptor in self.cache_descriptors]
        return data

    def to_native_dict(self) -> dict:
        return self.to_dict()
