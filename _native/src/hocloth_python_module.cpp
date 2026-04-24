#include "hocloth_runtime_api.hpp"
#include "hocloth_coordinate_space.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;

namespace hocloth {

namespace {

void copy_vec3(const nb::tuple& tuple_value, float out[3]) {
    out[0] = nb::cast<float>(tuple_value[0]);
    out[1] = nb::cast<float>(tuple_value[1]);
    out[2] = nb::cast<float>(tuple_value[2]);
}


void copy_quat(const nb::tuple& tuple_value, float out[4]) {
    out[0] = nb::cast<float>(tuple_value[0]);
    out[1] = nb::cast<float>(tuple_value[1]);
    out[2] = nb::cast<float>(tuple_value[2]);
    out[3] = nb::cast<float>(tuple_value[3]);
}


void copy_vec3_blender_to_solver(const nb::tuple& tuple_value, float out[3]) {
    float blender_value[3];
    copy_vec3(tuple_value, blender_value);
    blender_to_solver_vector(blender_value, out);
}


void copy_quat_blender_to_solver(const nb::tuple& tuple_value, float out[4]) {
    float blender_value[4];
    copy_quat(tuple_value, blender_value);
    blender_to_solver_quaternion(blender_value, out);
}

SceneDescriptor scene_from_python(const nb::dict& scene_dict) {
    SceneDescriptor scene;

    if (scene_dict.contains("bone_chains") || scene_dict.contains("spring_bones")) {
        nb::handle chain_source = scene_dict.contains("spring_bones")
            ? scene_dict["spring_bones"]
            : scene_dict["bone_chains"];
        const nb::list bone_chains = nb::cast<nb::list>(chain_source);
        for (nb::handle item_handle : bone_chains) {
            const nb::dict chain_dict = nb::cast<nb::dict>(item_handle);
            BoneChainDescriptor chain;
            chain.component_id = nb::cast<std::string>(chain_dict["component_id"]);
            chain.armature_name = nb::cast<std::string>(chain_dict["armature_name"]);
            chain.root_bone_name = nb::cast<std::string>(chain_dict["root_bone_name"]);
            if (chain_dict.contains("center_object_name")) {
                chain.center_object_name = nb::cast<std::string>(chain_dict["center_object_name"]);
            }
            if (chain_dict.contains("joint_radius")) {
                chain.joint_radius = nb::cast<float>(chain_dict["joint_radius"]);
            }
            if (chain_dict.contains("stiffness")) {
                chain.stiffness = nb::cast<float>(chain_dict["stiffness"]);
            }
            if (chain_dict.contains("damping")) {
                chain.damping = nb::cast<float>(chain_dict["damping"]);
            }
            if (chain_dict.contains("drag")) {
                chain.drag = nb::cast<float>(chain_dict["drag"]);
            }
            if (chain_dict.contains("gravity_strength")) {
                chain.gravity_strength = nb::cast<float>(chain_dict["gravity_strength"]);
            }
            if (chain_dict.contains("gravity_direction")) {
                copy_vec3_blender_to_solver(nb::cast<nb::tuple>(chain_dict["gravity_direction"]), chain.gravity_direction);
            }
            if (chain_dict.contains("collider_group_ids")) {
                chain.collider_group_ids = nb::cast<std::vector<std::string>>(chain_dict["collider_group_ids"]);
            }
            if (chain_dict.contains("collision_binding_ids")) {
                chain.collision_binding_ids = nb::cast<std::vector<std::string>>(chain_dict["collision_binding_ids"]);
            }

            if (chain_dict.contains("bones") || chain_dict.contains("joints")) {
                nb::handle bone_source = chain_dict.contains("joints")
                    ? chain_dict["joints"]
                    : chain_dict["bones"];
                const nb::list bones = nb::cast<nb::list>(bone_source);
                for (nb::handle bone_handle : bones) {
                    const nb::dict bone_dict = nb::cast<nb::dict>(bone_handle);
                    BoneDescriptor bone;
                    bone.name = nb::cast<std::string>(bone_dict["name"]);
                    bone.parent_index = nb::cast<std::int32_t>(bone_dict["parent_index"]);
                    if (bone_dict.contains("depth")) {
                        bone.depth = nb::cast<std::int32_t>(bone_dict["depth"]);
                    }
                    bone.length = nb::cast<float>(bone_dict["length"]);
                    if (bone_dict.contains("radius")) {
                        bone.radius = nb::cast<float>(bone_dict["radius"]);
                    }
                    if (bone_dict.contains("stiffness")) {
                        bone.stiffness = nb::cast<float>(bone_dict["stiffness"]);
                    }
                    if (bone_dict.contains("damping")) {
                        bone.damping = nb::cast<float>(bone_dict["damping"]);
                    }
                    if (bone_dict.contains("drag")) {
                        bone.drag = nb::cast<float>(bone_dict["drag"]);
                    }
                    if (bone_dict.contains("gravity_scale")) {
                        bone.gravity_scale = nb::cast<float>(bone_dict["gravity_scale"]);
                    }
                    copy_vec3_blender_to_solver(nb::cast<nb::tuple>(bone_dict["rest_head_local"]), bone.rest_head_local);
                    copy_vec3_blender_to_solver(nb::cast<nb::tuple>(bone_dict["rest_tail_local"]), bone.rest_tail_local);
                    copy_vec3_blender_to_solver(nb::cast<nb::tuple>(bone_dict["rest_local_translation"]), bone.rest_local_translation);
                    copy_quat_blender_to_solver(nb::cast<nb::tuple>(bone_dict["rest_local_rotation"]), bone.rest_local_rotation);
                    chain.bones.push_back(bone);
                }
            }

            if (chain_dict.contains("lines")) {
                const nb::list lines = nb::cast<nb::list>(chain_dict["lines"]);
                for (nb::handle line_handle : lines) {
                    const nb::dict line_dict = nb::cast<nb::dict>(line_handle);
                    BoneLineDescriptor line;
                    line.start_index = nb::cast<std::int32_t>(line_dict["start_index"]);
                    line.end_index = nb::cast<std::int32_t>(line_dict["end_index"]);
                    chain.lines.push_back(line);
                }
            }
            if (chain_dict.contains("line_start_indices")) {
                chain.line_start_indices = nb::cast<std::vector<std::int32_t>>(chain_dict["line_start_indices"]);
            }
            if (chain_dict.contains("line_counts")) {
                chain.line_counts = nb::cast<std::vector<std::int32_t>>(chain_dict["line_counts"]);
            }
            if (chain_dict.contains("line_data")) {
                chain.line_data = nb::cast<std::vector<std::int32_t>>(chain_dict["line_data"]);
            }

            if (chain_dict.contains("baselines")) {
                const nb::list baselines = nb::cast<nb::list>(chain_dict["baselines"]);
                for (nb::handle baseline_handle : baselines) {
                    const nb::dict baseline_dict = nb::cast<nb::dict>(baseline_handle);
                    BoneBaselineDescriptor baseline;
                    if (baseline_dict.contains("joint_indices")) {
                        baseline.joint_indices = nb::cast<std::vector<std::int32_t>>(baseline_dict["joint_indices"]);
                    }
                    chain.baselines.push_back(baseline);
                }
            }

            if (chain_dict.contains("baseline_start_indices")) {
                chain.baseline_start_indices = nb::cast<std::vector<std::int32_t>>(chain_dict["baseline_start_indices"]);
            }
            if (chain_dict.contains("baseline_counts")) {
                chain.baseline_counts = nb::cast<std::vector<std::int32_t>>(chain_dict["baseline_counts"]);
            }
            if (chain_dict.contains("baseline_data")) {
                chain.baseline_data = nb::cast<std::vector<std::int32_t>>(chain_dict["baseline_data"]);
            }

            scene.bone_chains.push_back(chain);
        }
    }

    if (scene_dict.contains("colliders")) {
        const nb::list colliders = nb::cast<nb::list>(scene_dict["colliders"]);
        for (nb::handle item_handle : colliders) {
            const nb::dict collider_dict = nb::cast<nb::dict>(item_handle);
            ColliderDescriptor collider;
            collider.component_id = nb::cast<std::string>(collider_dict["component_id"]);
            collider.object_name = nb::cast<std::string>(collider_dict["object_name"]);
            if (collider_dict.contains("shape_type")) {
                collider.shape_type = nb::cast<std::string>(collider_dict["shape_type"]);
            }
            if (collider_dict.contains("radius")) {
                collider.radius = nb::cast<float>(collider_dict["radius"]);
            }
            if (collider_dict.contains("height")) {
                collider.height = nb::cast<float>(collider_dict["height"]);
            }
            if (collider_dict.contains("world_translation")) {
                copy_vec3_blender_to_solver(
                    nb::cast<nb::tuple>(collider_dict["world_translation"]),
                    collider.world_translation
                );
            }
            if (collider_dict.contains("world_rotation")) {
                copy_quat_blender_to_solver(
                    nb::cast<nb::tuple>(collider_dict["world_rotation"]),
                    collider.world_rotation
                );
            }
            scene.colliders.push_back(collider);
        }
    }

    if (scene_dict.contains("collider_groups")) {
        const nb::list collider_groups = nb::cast<nb::list>(scene_dict["collider_groups"]);
        for (nb::handle item_handle : collider_groups) {
            const nb::dict group_dict = nb::cast<nb::dict>(item_handle);
            ColliderGroupDescriptor group;
            group.component_id = nb::cast<std::string>(group_dict["component_id"]);
            if (group_dict.contains("collider_ids")) {
                group.collider_ids = nb::cast<std::vector<std::string>>(group_dict["collider_ids"]);
            }
            scene.collider_groups.push_back(group);
        }
    }

    if (scene_dict.contains("cache_descriptors")) {
        const nb::list cache_descriptors = nb::cast<nb::list>(scene_dict["cache_descriptors"]);
        for (nb::handle item_handle : cache_descriptors) {
            const nb::dict cache_dict = nb::cast<nb::dict>(item_handle);
            CacheDescriptor descriptor;
            descriptor.component_id = nb::cast<std::string>(cache_dict["component_id"]);
            if (cache_dict.contains("source_object_name")) {
                descriptor.source_object_name = nb::cast<std::string>(cache_dict["source_object_name"]);
            }
            if (cache_dict.contains("topology_hash")) {
                descriptor.topology_hash = nb::cast<std::string>(cache_dict["topology_hash"]);
            }
            if (cache_dict.contains("cache_format")) {
                descriptor.cache_format = nb::cast<std::string>(cache_dict["cache_format"]);
            }
            if (cache_dict.contains("cache_path")) {
                descriptor.cache_path = nb::cast<std::string>(cache_dict["cache_path"]);
            }
            scene.cache_descriptors.push_back(descriptor);
        }
    }

    if (scene_dict.contains("collision_objects")) {
        const nb::list collision_objects = nb::cast<nb::list>(scene_dict["collision_objects"]);
        for (nb::handle item_handle : collision_objects) {
            const nb::dict object_dict = nb::cast<nb::dict>(item_handle);
            CollisionObjectDescriptor collision_object;
            collision_object.collision_object_id = nb::cast<std::string>(object_dict["collision_object_id"]);
            if (object_dict.contains("owner_component_id")) {
                collision_object.owner_component_id = nb::cast<std::string>(object_dict["owner_component_id"]);
            }
            if (object_dict.contains("motion_type")) {
                collision_object.motion_type = nb::cast<std::string>(object_dict["motion_type"]);
            }
            if (object_dict.contains("shape_type")) {
                collision_object.shape_type = nb::cast<std::string>(object_dict["shape_type"]);
            }
            if (object_dict.contains("world_translation")) {
                copy_vec3_blender_to_solver(
                    nb::cast<nb::tuple>(object_dict["world_translation"]),
                    collision_object.world_translation
                );
            }
            if (object_dict.contains("world_rotation")) {
                copy_quat_blender_to_solver(
                    nb::cast<nb::tuple>(object_dict["world_rotation"]),
                    collision_object.world_rotation
                );
            }
            if (object_dict.contains("linear_velocity")) {
                copy_vec3_blender_to_solver(
                    nb::cast<nb::tuple>(object_dict["linear_velocity"]),
                    collision_object.linear_velocity
                );
            }
            if (object_dict.contains("angular_velocity")) {
                copy_vec3_blender_to_solver(
                    nb::cast<nb::tuple>(object_dict["angular_velocity"]),
                    collision_object.angular_velocity
                );
            }
            if (object_dict.contains("radius")) {
                collision_object.radius = nb::cast<float>(object_dict["radius"]);
            }
            if (object_dict.contains("height")) {
                collision_object.height = nb::cast<float>(object_dict["height"]);
            }
            if (object_dict.contains("source_object_name")) {
                collision_object.source_object_name = nb::cast<std::string>(object_dict["source_object_name"]);
            }
            if (object_dict.contains("source_group_ids")) {
                collision_object.source_group_ids = nb::cast<std::vector<std::string>>(object_dict["source_group_ids"]);
            }
            scene.collision_objects.push_back(collision_object);
        }
    }

    if (scene_dict.contains("collision_bindings")) {
        const nb::list collision_bindings = nb::cast<nb::list>(scene_dict["collision_bindings"]);
        for (nb::handle item_handle : collision_bindings) {
            const nb::dict binding_dict = nb::cast<nb::dict>(item_handle);
            CollisionBindingDescriptor binding;
            binding.binding_id = nb::cast<std::string>(binding_dict["binding_id"]);
            if (binding_dict.contains("owner_component_id")) {
                binding.owner_component_id = nb::cast<std::string>(binding_dict["owner_component_id"]);
            }
            if (binding_dict.contains("source_group_ids")) {
                binding.source_group_ids = nb::cast<std::vector<std::string>>(binding_dict["source_group_ids"]);
            }
            if (binding_dict.contains("collision_object_ids")) {
                binding.collision_object_ids = nb::cast<std::vector<std::string>>(binding_dict["collision_object_ids"]);
            }
            scene.collision_bindings.push_back(binding);
        }
    }

    return scene;
}


RuntimeInputs runtime_inputs_from_python(
    float dt,
    std::int32_t simulation_frequency,
    std::int32_t max_simulation_steps_per_frame,
    const nb::dict& input_dict
) {
    RuntimeInputs inputs;
    inputs.dt = dt;
    inputs.simulation_frequency = simulation_frequency;
    inputs.max_simulation_steps_per_frame = max_simulation_steps_per_frame;

    if (!input_dict.contains("bone_chains")) {
        return inputs;
    }

    const nb::list chains = nb::cast<nb::list>(input_dict["bone_chains"]);
    for (nb::handle item_handle : chains) {
        const nb::dict chain_dict = nb::cast<nb::dict>(item_handle);
        BoneChainRuntimeInput chain;
        chain.component_id = nb::cast<std::string>(chain_dict["component_id"]);
        chain.armature_name = nb::cast<std::string>(chain_dict["armature_name"]);
        chain.root_bone_name = nb::cast<std::string>(chain_dict["root_bone_name"]);
        if (chain_dict.contains("center_object_name")) {
            chain.center_object_name = nb::cast<std::string>(chain_dict["center_object_name"]);
        }
        copy_vec3_blender_to_solver(nb::cast<nb::tuple>(chain_dict["root_translation"]), chain.root_translation);
        copy_quat_blender_to_solver(nb::cast<nb::tuple>(chain_dict["root_rotation_quaternion"]), chain.root_rotation_quaternion);
        if (chain_dict.contains("root_linear_velocity")) {
            copy_vec3_blender_to_solver(nb::cast<nb::tuple>(chain_dict["root_linear_velocity"]), chain.root_linear_velocity);
        }
        if (chain_dict.contains("root_scale")) {
            copy_vec3(nb::cast<nb::tuple>(chain_dict["root_scale"]), chain.root_scale);
        }
        if (chain_dict.contains("center_translation")) {
            copy_vec3_blender_to_solver(nb::cast<nb::tuple>(chain_dict["center_translation"]), chain.center_translation);
        }
        if (chain_dict.contains("center_rotation_quaternion")) {
            copy_quat_blender_to_solver(nb::cast<nb::tuple>(chain_dict["center_rotation_quaternion"]), chain.center_rotation_quaternion);
        }
        if (chain_dict.contains("center_linear_velocity")) {
            copy_vec3_blender_to_solver(nb::cast<nb::tuple>(chain_dict["center_linear_velocity"]), chain.center_linear_velocity);
        }
        if (chain_dict.contains("center_scale")) {
            copy_vec3(nb::cast<nb::tuple>(chain_dict["center_scale"]), chain.center_scale);
        }
        inputs.bone_chains.push_back(chain);
    }

    if (input_dict.contains("collision_objects")) {
        const nb::list collision_objects = nb::cast<nb::list>(input_dict["collision_objects"]);
        for (nb::handle item_handle : collision_objects) {
            const nb::dict object_dict = nb::cast<nb::dict>(item_handle);
            CollisionObjectRuntimeInput collision_object;
            collision_object.collision_object_id = nb::cast<std::string>(object_dict["collision_object_id"]);
            if (object_dict.contains("world_translation")) {
                copy_vec3_blender_to_solver(
                    nb::cast<nb::tuple>(object_dict["world_translation"]),
                    collision_object.world_translation
                );
            }
            if (object_dict.contains("world_rotation")) {
                copy_quat_blender_to_solver(
                    nb::cast<nb::tuple>(object_dict["world_rotation"]),
                    collision_object.world_rotation
                );
            }
            if (object_dict.contains("linear_velocity")) {
                copy_vec3_blender_to_solver(
                    nb::cast<nb::tuple>(object_dict["linear_velocity"]),
                    collision_object.linear_velocity
                );
            }
            if (object_dict.contains("angular_velocity")) {
                copy_vec3_blender_to_solver(
                    nb::cast<nb::tuple>(object_dict["angular_velocity"]),
                    collision_object.angular_velocity
                );
            }
            inputs.collision_objects.push_back(collision_object);
        }
    }

    return inputs;
}

nb::dict build_scene_dict(const nb::dict& scene_dict) {
    const SceneDescriptor scene = scene_from_python(scene_dict);
    const SceneHandle handle = build_scene(scene);
    const RuntimeSceneInfo info = get_scene_info(handle);

    nb::dict result;
    result["handle"] = handle;
    result["summary"] = "bone_chains=" + std::to_string(info.bone_chain_count) +
                        ", bones=" + std::to_string(info.bone_count) +
                        ", colliders=" + std::to_string(info.collider_count) +
                        ", collider_groups=" + std::to_string(info.collider_group_count) +
                        ", collision_objects=" + std::to_string(info.collision_object_count) +
                        ", collision_bindings=" + std::to_string(info.collision_binding_count) +
                        ", cache_outputs=" + std::to_string(info.cache_descriptor_count);
    result["backend"] = info.backend;
    result["build_message"] = info.build_message;
    result["physics_scene_ready"] = info.physics_scene_ready;
    return result;
}

bool set_runtime_inputs_dict(SceneHandle handle, const nb::dict& input_dict) {
    set_runtime_inputs(handle, runtime_inputs_from_python(1.0f / 30.0f, 90, 5, input_dict));
    return true;
}


nb::dict step_scene_dict(
    SceneHandle handle,
    float dt,
    std::int32_t simulation_frequency,
    std::int32_t max_simulation_steps_per_frame
) {
    RuntimeInputs inputs;
    inputs.dt = dt;
    inputs.simulation_frequency = simulation_frequency;
    inputs.max_simulation_steps_per_frame = max_simulation_steps_per_frame;
    const std::int32_t executed_steps = step_scene(handle, inputs);
    const RuntimeStepInfo step_info = get_last_step_info(handle);

    nb::dict result;
    result["handle"] = handle;
    result["dt"] = dt;
    result["simulation_frequency"] = simulation_frequency;
    result["max_simulation_steps_per_frame"] = max_simulation_steps_per_frame;
    result["executed_steps"] = executed_steps;
    result["scheduled_steps"] = step_info.scheduled_steps;
    result["skipped_steps"] = step_info.skipped_steps;
    result["steps"] = get_step_count(handle);
    result["summary"] = "native step executed";
    return result;
}

nb::list get_bone_transforms_list(SceneHandle handle) {
    nb::list result;
    for (const BoneTransform& transform : get_bone_transforms(handle)) {
        float blender_translation[3];
        float blender_rotation[4];
        solver_to_blender_vector(transform.translation, blender_translation);
        solver_to_blender_quaternion(transform.rotation_quaternion, blender_rotation);

        nb::dict item;
        item["component_id"] = transform.component_id;
        item["armature_name"] = transform.armature_name;
        item["bone_name"] = transform.bone_name;
        item["translation"] = nb::make_tuple(
            blender_translation[0],
            blender_translation[1],
            blender_translation[2]
        );
        item["rotation_quaternion"] = nb::make_tuple(
            blender_rotation[0],
            blender_rotation[1],
            blender_rotation[2],
            blender_rotation[3]
        );
        result.append(item);
    }
    return result;
}

}  // namespace

}  // namespace hocloth

NB_MODULE(hocloth_native, m) {
    m.def("build_scene", &hocloth::build_scene_dict);
    m.def("destroy_scene", &hocloth::destroy_scene);
    m.def("reset_scene", &hocloth::reset_scene);
    m.def("set_runtime_inputs", &hocloth::set_runtime_inputs_dict);
    m.def("step_scene", &hocloth::step_scene_dict);
    m.def("get_bone_transforms", &hocloth::get_bone_transforms_list);
}
