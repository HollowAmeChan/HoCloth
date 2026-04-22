#pragma once

#include "hocloth_runtime_api.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace hocloth {

struct CollisionWorldObject {
    std::string collision_object_id;
    std::string owner_component_id;
    std::string motion_type;
    std::string shape_type;
    float world_translation[3] = {0.0f, 0.0f, 0.0f};
    float world_rotation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float linear_velocity[3] = {0.0f, 0.0f, 0.0f};
    float angular_velocity[3] = {0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
    float height = 0.0f;
    std::string source_object_name;
    std::vector<std::string> source_group_ids;
};

struct CollisionWorldBinding {
    std::string binding_id;
    std::string owner_component_id;
    std::vector<std::string> source_group_ids;
    std::vector<std::size_t> object_indices;
};

class CollisionWorld {
public:
    void clear();
    void build_from_scene(const SceneDescriptor& scene);

    [[nodiscard]] std::size_t object_count() const;
    [[nodiscard]] std::size_t binding_count() const;
    [[nodiscard]] const std::vector<CollisionWorldObject>& objects() const;
    [[nodiscard]] const std::vector<CollisionWorldBinding>& bindings() const;

private:
    std::vector<CollisionWorldObject> objects_;
    std::vector<CollisionWorldBinding> bindings_;
    std::unordered_map<std::string, std::size_t> object_index_by_id_;
};

}  // namespace hocloth
