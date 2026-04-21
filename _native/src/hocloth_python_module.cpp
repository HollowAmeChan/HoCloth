#include "hocloth_runtime_api.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;

namespace hocloth {

namespace {

SceneDescriptor scene_from_python(const nb::dict& scene_dict) {
    SceneDescriptor scene;

    if (!scene_dict.contains("bone_chains")) {
        return scene;
    }

    const nb::list bone_chains = nb::cast<nb::list>(scene_dict["bone_chains"]);
    for (nb::handle item_handle : bone_chains) {
        const nb::dict chain_dict = nb::cast<nb::dict>(item_handle);
        BoneChainDescriptor chain;
        chain.component_id = nb::cast<std::string>(chain_dict["component_id"]);
        chain.armature_name = nb::cast<std::string>(chain_dict["armature_name"]);
        chain.root_bone_name = nb::cast<std::string>(chain_dict["root_bone_name"]);

        if (chain_dict.contains("bones")) {
            const nb::list bones = nb::cast<nb::list>(chain_dict["bones"]);
            for (nb::handle bone_handle : bones) {
                const nb::dict bone_dict = nb::cast<nb::dict>(bone_handle);
                BoneDescriptor bone;
                bone.name = nb::cast<std::string>(bone_dict["name"]);
                bone.parent_index = nb::cast<std::int32_t>(bone_dict["parent_index"]);
                chain.bones.push_back(bone);
            }
        }

        scene.bone_chains.push_back(chain);
    }

    return scene;
}

nb::dict build_scene_dict(const nb::dict& scene_dict) {
    const SceneDescriptor scene = scene_from_python(scene_dict);
    const SceneHandle handle = build_scene(scene);

    std::size_t bone_count = 0;
    for (const BoneChainDescriptor& chain : scene.bone_chains) {
        bone_count += chain.bones.size();
    }

    nb::dict result;
    result["handle"] = handle;
    result["summary"] = "bone_chains=" + std::to_string(scene.bone_chains.size()) +
                        ", bones=" + std::to_string(bone_count);
    result["backend"] = "native";
    return result;
}

nb::dict step_scene_dict(SceneHandle handle, float dt, std::int32_t substeps) {
    step_scene(handle, RuntimeInputs{dt, substeps});

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
        nb::dict item;
        item["component_id"] = transform.component_id;
        item["bone_name"] = transform.bone_name;
        item["translation"] = nb::make_tuple(
            transform.translation[0],
            transform.translation[1],
            transform.translation[2]
        );
        item["rotation_quaternion"] = nb::make_tuple(
            transform.rotation_quaternion[0],
            transform.rotation_quaternion[1],
            transform.rotation_quaternion[2],
            transform.rotation_quaternion[3]
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
    m.def("step_scene", &hocloth::step_scene_dict);
    m.def("get_bone_transforms", &hocloth::get_bone_transforms_list);
}
