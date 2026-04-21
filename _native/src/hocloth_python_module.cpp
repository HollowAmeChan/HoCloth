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


void copy_vec3_blender_to_physx(const nb::tuple& tuple_value, float out[3]) {
    float blender_value[3];
    copy_vec3(tuple_value, blender_value);
    blender_to_physx_vector(blender_value, out);
}


void copy_quat_blender_to_physx(const nb::tuple& tuple_value, float out[4]) {
    float blender_value[4];
    copy_quat(tuple_value, blender_value);
    blender_to_physx_quaternion(blender_value, out);
}

SceneDescriptor scene_from_python(const nb::dict& scene_dict) {
    SceneDescriptor scene;

    if (scene_dict.contains("bone_chains")) {
        const nb::list bone_chains = nb::cast<nb::list>(scene_dict["bone_chains"]);
        for (nb::handle item_handle : bone_chains) {
            const nb::dict chain_dict = nb::cast<nb::dict>(item_handle);
            BoneChainDescriptor chain;
            chain.component_id = nb::cast<std::string>(chain_dict["component_id"]);
            chain.armature_name = nb::cast<std::string>(chain_dict["armature_name"]);
            chain.root_bone_name = nb::cast<std::string>(chain_dict["root_bone_name"]);
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
                copy_vec3_blender_to_physx(nb::cast<nb::tuple>(chain_dict["gravity_direction"]), chain.gravity_direction);
            }

            if (chain_dict.contains("bones")) {
                const nb::list bones = nb::cast<nb::list>(chain_dict["bones"]);
                for (nb::handle bone_handle : bones) {
                    const nb::dict bone_dict = nb::cast<nb::dict>(bone_handle);
                    BoneDescriptor bone;
                    bone.name = nb::cast<std::string>(bone_dict["name"]);
                    bone.parent_index = nb::cast<std::int32_t>(bone_dict["parent_index"]);
                    bone.length = nb::cast<float>(bone_dict["length"]);
                    copy_vec3_blender_to_physx(nb::cast<nb::tuple>(bone_dict["rest_head_local"]), bone.rest_head_local);
                    copy_vec3_blender_to_physx(nb::cast<nb::tuple>(bone_dict["rest_tail_local"]), bone.rest_tail_local);
                    copy_vec3_blender_to_physx(nb::cast<nb::tuple>(bone_dict["rest_local_translation"]), bone.rest_local_translation);
                    copy_quat_blender_to_physx(nb::cast<nb::tuple>(bone_dict["rest_local_rotation"]), bone.rest_local_rotation);
                    chain.bones.push_back(bone);
                }
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
                copy_vec3_blender_to_physx(
                    nb::cast<nb::tuple>(collider_dict["world_translation"]),
                    collider.world_translation
                );
            }
            if (collider_dict.contains("world_rotation")) {
                copy_quat_blender_to_physx(
                    nb::cast<nb::tuple>(collider_dict["world_rotation"]),
                    collider.world_rotation
                );
            }
            scene.colliders.push_back(collider);
        }
    }

    return scene;
}


RuntimeInputs runtime_inputs_from_python(float dt, std::int32_t substeps, const nb::dict& input_dict) {
    RuntimeInputs inputs;
    inputs.dt = dt;
    inputs.substeps = substeps;

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
        copy_vec3_blender_to_physx(nb::cast<nb::tuple>(chain_dict["root_translation"]), chain.root_translation);
        copy_quat_blender_to_physx(nb::cast<nb::tuple>(chain_dict["root_rotation_quaternion"]), chain.root_rotation_quaternion);
        inputs.bone_chains.push_back(chain);
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
                        ", colliders=" + std::to_string(info.collider_count);
    result["backend"] = info.backend;
    result["build_message"] = info.build_message;
    result["physics_scene_ready"] = info.physics_scene_ready;
    return result;
}

bool set_runtime_inputs_dict(SceneHandle handle, const nb::dict& input_dict) {
    set_runtime_inputs(handle, runtime_inputs_from_python(1.0f / 60.0f, 1, input_dict));
    return true;
}


nb::dict step_scene_dict(SceneHandle handle, float dt, std::int32_t substeps) {
    step_scene(handle, RuntimeInputs{dt, substeps, {}});

    nb::dict result;
    result["handle"] = handle;
    result["dt"] = dt;
    result["substeps"] = substeps;
    result["steps"] = get_step_count(handle);
    result["summary"] = "native step executed";
    return result;
}

nb::list get_bone_transforms_list(SceneHandle handle) {
    nb::list result;
    for (const BoneTransform& transform : get_bone_transforms(handle)) {
        float blender_translation[3];
        float blender_rotation[4];
        physx_to_blender_vector(transform.translation, blender_translation);
        physx_to_blender_quaternion(transform.rotation_quaternion, blender_rotation);

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
