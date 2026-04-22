#include "hocloth_collision_world.hpp"

namespace hocloth {

void CollisionWorld::clear() {
    objects_.clear();
    bindings_.clear();
    object_index_by_id_.clear();
}

void CollisionWorld::build_from_scene(const SceneDescriptor& scene) {
    clear();

    objects_.reserve(scene.collision_objects.size());
    for (const CollisionObjectDescriptor& descriptor : scene.collision_objects) {
        CollisionWorldObject object;
        object.collision_object_id = descriptor.collision_object_id;
        object.owner_component_id = descriptor.owner_component_id;
        object.motion_type = descriptor.motion_type;
        object.shape_type = descriptor.shape_type;
        object.world_translation[0] = descriptor.world_translation[0];
        object.world_translation[1] = descriptor.world_translation[1];
        object.world_translation[2] = descriptor.world_translation[2];
        object.world_rotation[0] = descriptor.world_rotation[0];
        object.world_rotation[1] = descriptor.world_rotation[1];
        object.world_rotation[2] = descriptor.world_rotation[2];
        object.world_rotation[3] = descriptor.world_rotation[3];
        object.linear_velocity[0] = descriptor.linear_velocity[0];
        object.linear_velocity[1] = descriptor.linear_velocity[1];
        object.linear_velocity[2] = descriptor.linear_velocity[2];
        object.angular_velocity[0] = descriptor.angular_velocity[0];
        object.angular_velocity[1] = descriptor.angular_velocity[1];
        object.angular_velocity[2] = descriptor.angular_velocity[2];
        object.radius = descriptor.radius;
        object.height = descriptor.height;
        object.source_object_name = descriptor.source_object_name;
        object.source_group_ids = descriptor.source_group_ids;
        object_index_by_id_[object.collision_object_id] = objects_.size();
        objects_.push_back(std::move(object));
    }

    bindings_.reserve(scene.collision_bindings.size());
    for (const CollisionBindingDescriptor& descriptor : scene.collision_bindings) {
        CollisionWorldBinding binding;
        binding.binding_id = descriptor.binding_id;
        binding.owner_component_id = descriptor.owner_component_id;
        binding.source_group_ids = descriptor.source_group_ids;
        for (const std::string& collision_object_id : descriptor.collision_object_ids) {
            const auto found = object_index_by_id_.find(collision_object_id);
            if (found != object_index_by_id_.end()) {
                binding.object_indices.push_back(found->second);
            }
        }
        bindings_.push_back(std::move(binding));
    }
}

std::size_t CollisionWorld::object_count() const {
    return objects_.size();
}

std::size_t CollisionWorld::binding_count() const {
    return bindings_.size();
}

const std::vector<CollisionWorldObject>& CollisionWorld::objects() const {
    return objects_;
}

const std::vector<CollisionWorldBinding>& CollisionWorld::bindings() const {
    return bindings_;
}

}  // namespace hocloth
