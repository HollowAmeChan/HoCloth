#include "hocloth_runtime_api.hpp"

#include "hocloth/blender/authoring_snapshot_transfer.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <cstddef>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <string>
#include <vector>

namespace nb = nanobind;

namespace hocloth {

namespace {

nb::object GetOptional(const nb::dict& dict, const char* key)
{
    return dict.contains(key) ? nb::borrow<nb::object>(dict[key]) : nb::none();
}

std::string ReadString(const nb::dict& dict, const char* key, const std::string& fallback = {})
{
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return fallback;
    }
    return nb::cast<std::string>(value);
}

int ReadInt(const nb::dict& dict, const char* key, int fallback = 0)
{
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return fallback;
    }
    return nb::cast<int>(value);
}

float ReadFloat(const nb::dict& dict, const char* key, float fallback = 0.0f)
{
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return fallback;
    }
    return nb::cast<float>(value);
}

bool ReadBool(const nb::dict& dict, const char* key, bool fallback = false)
{
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return fallback;
    }
    return nb::cast<bool>(value);
}

nb::sequence ReadSequence(const nb::dict& dict, const char* key)
{
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return nb::steal<nb::sequence>(PyList_New(0));
    }
    return nb::cast<nb::sequence>(value);
}

Vec3 ReadVec3(nb::handle handle)
{
    nb::sequence seq = nb::cast<nb::sequence>(handle);
    const std::size_t size = nb::len(seq);
    Vec3 value;
    if (size > 0) {
        value.x = nb::cast<float>(seq[0]);
    }
    if (size > 1) {
        value.y = nb::cast<float>(seq[1]);
    }
    if (size > 2) {
        value.z = nb::cast<float>(seq[2]);
    }
    return value;
}

Quat ReadQuat(nb::handle handle)
{
    nb::sequence seq = nb::cast<nb::sequence>(handle);
    const std::size_t size = nb::len(seq);
    Quat value;
    if (size > 0) {
        value.w = nb::cast<float>(seq[0]);
    }
    if (size > 1) {
        value.x = nb::cast<float>(seq[1]);
    }
    if (size > 2) {
        value.y = nb::cast<float>(seq[2]);
    }
    if (size > 3) {
        value.z = nb::cast<float>(seq[3]);
    }
    return value;
}

std::vector<std::string> ReadStringArray(const nb::dict& dict, const char* key)
{
    std::vector<std::string> result;
    for (nb::handle item : ReadSequence(dict, key)) {
        result.push_back(nb::cast<std::string>(item));
    }
    return result;
}

std::vector<int> ReadIntArray(const nb::dict& dict, const char* key)
{
    std::vector<int> result;
    for (nb::handle item : ReadSequence(dict, key)) {
        result.push_back(nb::cast<int>(item));
    }
    return result;
}

std::vector<Vec3> ReadVec3Array(const nb::dict& dict, const char* key)
{
    std::vector<Vec3> result;
    nb::sequence values = ReadSequence(dict, key);
    const std::size_t size = nb::len(values);
    if (size % 3 != 0) {
        throw std::runtime_error("Expected a flat float3 array.");
    }

    result.reserve(size / 3);
    for (std::size_t index = 0; index < size; index += 3) {
        result.push_back(Vec3{
            nb::cast<float>(values[index + 0]),
            nb::cast<float>(values[index + 1]),
            nb::cast<float>(values[index + 2]),
        });
    }
    return result;
}

std::vector<Quat> ReadQuatArray(const nb::dict& dict, const char* key)
{
    std::vector<Quat> result;
    nb::sequence values = ReadSequence(dict, key);
    const std::size_t size = nb::len(values);
    if (size % 4 != 0) {
        throw std::runtime_error("Expected a flat quaternion array.");
    }

    result.reserve(size / 4);
    for (std::size_t index = 0; index < size; index += 4) {
        result.push_back(Quat{
            nb::cast<float>(values[index + 0]),
            nb::cast<float>(values[index + 1]),
            nb::cast<float>(values[index + 2]),
            nb::cast<float>(values[index + 3]),
        });
    }
    return result;
}

bool IsExchangeEnvelope(const nb::dict& root)
{
    if (!root.contains("schema") || !root.contains("payload") || !root.contains("payload_type")) {
        return false;
    }
    return nb::cast<std::string>(root["schema"]) == "hocloth.exchange";
}

nb::dict ResolveExchangePayload(const nb::dict& root, const char* expected_payload_type)
{
    if (!IsExchangeEnvelope(root)) {
        return root;
    }

    const std::string payload_type = nb::cast<std::string>(root["payload_type"]);
    if (payload_type != expected_payload_type) {
        throw std::runtime_error(
            std::string("Expected hocloth.exchange payload '")
            + expected_payload_type
            + "', got '"
            + payload_type
            + "'."
        );
    }

    nb::dict payload = nb::cast<nb::dict>(root["payload"]);
    if (payload_type == "compiled_scene" && payload.contains("scene")) {
        return nb::cast<nb::dict>(payload["scene"]);
    }
    return payload;
}

CompiledScene ParseCompiledScene(const nb::dict& root)
{
    nb::dict data = ResolveExchangePayload(root, "compiled_scene");
    CompiledScene scene;

    nb::sequence chains = ReadSequence(data, "spring_bones");
    if (nb::len(chains) == 0) {
        chains = ReadSequence(data, "bone_chains");
    }

    for (nb::handle item : chains) {
        nb::dict chain_dict = nb::cast<nb::dict>(item);
        CompiledSpringBone chain;
        chain.component_id = ReadString(chain_dict, "component_id");
        chain.component_type = ReadString(chain_dict, "component_type", "SPRING_BONE");
        chain.cloth_type = ReadString(chain_dict, "cloth_type", "BoneSpring");
        chain.armature_name = ReadString(chain_dict, "armature_name");
        chain.root_bone_name = ReadString(chain_dict, "root_bone_name");
        chain.center_object_name = ReadString(chain_dict, "center_object_name");
        chain.center_bone_name = ReadString(chain_dict, "center_bone_name");
        chain.joint_radius = ReadFloat(chain_dict, "joint_radius");
        chain.stiffness = ReadFloat(chain_dict, "stiffness");
        chain.damping = ReadFloat(chain_dict, "damping");
        chain.drag = ReadFloat(chain_dict, "drag");
        chain.damping_curve_value = ReadFloat(chain_dict, "damping_curve_value", chain.damping);
        chain.inertia_world_inertia = ReadFloat(chain_dict, "inertia_world_inertia", 1.0f);
        chain.inertia_movement_inertia_smoothing = ReadFloat(chain_dict, "inertia_movement_inertia_smoothing", 0.4f);
        chain.inertia_movement_speed_limit_enabled = ReadInt(chain_dict, "inertia_movement_speed_limit_enabled", 0) != 0;
        if (chain_dict.contains("inertia_movement_speed_limit_enabled")) {
            chain.inertia_movement_speed_limit_enabled = nb::cast<bool>(chain_dict["inertia_movement_speed_limit_enabled"]);
        }
        chain.inertia_movement_speed_limit = ReadFloat(chain_dict, "inertia_movement_speed_limit", 0.0f);
        chain.inertia_rotation_speed_limit_enabled = ReadInt(chain_dict, "inertia_rotation_speed_limit_enabled", 0) != 0;
        if (chain_dict.contains("inertia_rotation_speed_limit_enabled")) {
            chain.inertia_rotation_speed_limit_enabled = nb::cast<bool>(chain_dict["inertia_rotation_speed_limit_enabled"]);
        }
        chain.inertia_rotation_speed_limit = ReadFloat(chain_dict, "inertia_rotation_speed_limit", 0.0f);
        chain.inertia_local_inertia = ReadFloat(chain_dict, "inertia_local_inertia", 1.0f);
        chain.inertia_local_movement_speed_limit_enabled = ReadInt(chain_dict, "inertia_local_movement_speed_limit_enabled", 0) != 0;
        if (chain_dict.contains("inertia_local_movement_speed_limit_enabled")) {
            chain.inertia_local_movement_speed_limit_enabled = nb::cast<bool>(chain_dict["inertia_local_movement_speed_limit_enabled"]);
        }
        chain.inertia_local_movement_speed_limit = ReadFloat(chain_dict, "inertia_local_movement_speed_limit", 0.0f);
        chain.inertia_local_rotation_speed_limit_enabled = ReadInt(chain_dict, "inertia_local_rotation_speed_limit_enabled", 0) != 0;
        if (chain_dict.contains("inertia_local_rotation_speed_limit_enabled")) {
            chain.inertia_local_rotation_speed_limit_enabled = nb::cast<bool>(chain_dict["inertia_local_rotation_speed_limit_enabled"]);
        }
        chain.inertia_local_rotation_speed_limit = ReadFloat(chain_dict, "inertia_local_rotation_speed_limit", 0.0f);
        chain.inertia_depth_inertia = ReadFloat(chain_dict, "inertia_depth_inertia", 0.0f);
        chain.inertia_centrifugal_acceleration = ReadFloat(chain_dict, "inertia_centrifugal_acceleration", 0.0f);
        chain.inertia_particle_speed_limit_enabled = ReadInt(chain_dict, "inertia_particle_speed_limit_enabled", 0) != 0;
        if (chain_dict.contains("inertia_particle_speed_limit_enabled")) {
            chain.inertia_particle_speed_limit_enabled = nb::cast<bool>(chain_dict["inertia_particle_speed_limit_enabled"]);
        }
        chain.inertia_particle_speed_limit = ReadFloat(chain_dict, "inertia_particle_speed_limit", 0.0f);
        chain.tether_distance_compression = ReadFloat(
            chain_dict,
            "tether_distance_compression",
            chain.tether_distance_compression
        );
        chain.distance_stiffness = ReadFloat(chain_dict, "distance_stiffness", chain.stiffness);
        chain.angle_restoration_enabled = ReadInt(chain_dict, "angle_restoration_enabled", 1) != 0;
        if (chain_dict.contains("angle_restoration_enabled")) {
            chain.angle_restoration_enabled = nb::cast<bool>(chain_dict["angle_restoration_enabled"]);
        }
        chain.angle_restoration_stiffness = ReadFloat(
            chain_dict,
            "angle_restoration_stiffness",
            chain.angle_restoration_stiffness
        );
        chain.angle_restoration_velocity_attenuation = ReadFloat(
            chain_dict,
            "angle_restoration_velocity_attenuation",
            0.6f
        );
        chain.use_spring = ReadInt(chain_dict, "use_spring", 1) != 0;
        if (chain_dict.contains("use_spring")) {
            chain.use_spring = nb::cast<bool>(chain_dict["use_spring"]);
        }
        chain.spring_power = ReadFloat(chain_dict, "spring_power", 0.04f);
        chain.limit_distance = ReadFloat(chain_dict, "limit_distance", 0.1f);
        chain.normal_limit_ratio = ReadFloat(chain_dict, "normal_limit_ratio", 1.0f);
        chain.spring_noise = ReadFloat(chain_dict, "spring_noise", 0.0f);
        chain.collider_friction = ReadFloat(chain_dict, "collider_friction", 0.5f);
        chain.collider_limit_distance = ReadFloat(chain_dict, "collider_limit_distance", 0.05f);
        if (chain_dict.contains("collider_collision_enabled")) {
            chain.collider_collision_enabled = nb::cast<bool>(chain_dict["collider_collision_enabled"]);
        }
        chain.collider_collision_mode = ReadString(chain_dict, "collider_collision_mode", "Point");
        chain.gravity_strength = ReadFloat(chain_dict, "gravity_strength");
        if (chain_dict.contains("gravity_direction")) {
            chain.gravity_direction = ReadVec3(chain_dict["gravity_direction"]);
        }
        if (chain_dict.contains("armature_scale")) {
            chain.armature_scale = ReadVec3(chain_dict["armature_scale"]);
        }
        chain.collider_ids = ReadStringArray(chain_dict, "collider_ids");
        if (!chain.collider_collision_enabled) {
            chain.collider_ids.clear();
        }
        chain.collider_group_ids = ReadStringArray(chain_dict, "collider_group_ids");
        chain.collision_binding_ids = ReadStringArray(chain_dict, "collision_binding_ids");
        chain.collision_bone_indices = ReadIntArray(chain_dict, "collision_bone_indices");

        for (nb::handle joint_item : ReadSequence(chain_dict, "joints")) {
            nb::dict joint_dict = nb::cast<nb::dict>(joint_item);
            CompiledSpringJoint joint;
            joint.name = ReadString(joint_dict, "name");
            joint.parent_index = ReadInt(joint_dict, "parent_index", -1);
            joint.depth = ReadInt(joint_dict, "depth");
            joint.length = ReadFloat(joint_dict, "length");
            joint.radius = ReadFloat(joint_dict, "radius");
            joint.stiffness = ReadFloat(joint_dict, "stiffness");
            joint.damping = ReadFloat(joint_dict, "damping");
            joint.drag = ReadFloat(joint_dict, "drag");
            joint.gravity_scale = ReadFloat(joint_dict, "gravity_scale", 1.0f);
            if (joint_dict.contains("rest_head_local")) {
                joint.rest_head_local = ReadVec3(joint_dict["rest_head_local"]);
            }
            if (joint_dict.contains("rest_tail_local")) {
                joint.rest_tail_local = ReadVec3(joint_dict["rest_tail_local"]);
            }
            if (joint_dict.contains("rest_local_translation")) {
                joint.rest_local_translation = ReadVec3(joint_dict["rest_local_translation"]);
            }
            if (joint_dict.contains("rest_local_rotation")) {
                joint.rest_local_rotation = ReadQuat(joint_dict["rest_local_rotation"]);
            }
            chain.joints.push_back(joint);
        }

        for (nb::handle line_item : ReadSequence(chain_dict, "lines")) {
            nb::dict line_dict = nb::cast<nb::dict>(line_item);
            chain.lines.push_back(CompiledSpringLine{
                ReadInt(line_dict, "start_index", -1),
                ReadInt(line_dict, "end_index", -1),
            });
        }

        for (nb::handle baseline_item : ReadSequence(chain_dict, "baselines")) {
            nb::dict baseline_dict = nb::cast<nb::dict>(baseline_item);
            CompiledSpringBaseline baseline;
            baseline.joint_indices = ReadIntArray(baseline_dict, "joint_indices");
            chain.baselines.push_back(std::move(baseline));
        }

        scene.spring_bones.push_back(std::move(chain));
    }

    for (nb::handle item : ReadSequence(data, "collision_objects")) {
        nb::dict object_dict = nb::cast<nb::dict>(item);
        CompiledCollisionObject collision_object;
        collision_object.collision_object_id = ReadString(object_dict, "collision_object_id");
        collision_object.owner_component_id = ReadString(object_dict, "owner_component_id");
        collision_object.motion_type = ReadString(object_dict, "motion_type");
        collision_object.shape_type = ReadString(object_dict, "shape_type");
        collision_object.radius = ReadFloat(object_dict, "radius");
        collision_object.height = ReadFloat(object_dict, "height");
        collision_object.capsule_direction = ReadString(object_dict, "capsule_direction", "Y");
        collision_object.capsule_aligned_on_center =
            ReadBool(object_dict, "capsule_aligned_on_center", true);
        collision_object.capsule_reverse_direction =
            ReadBool(object_dict, "capsule_reverse_direction", false);
        collision_object.capsule_end_radius =
            ReadFloat(object_dict, "capsule_end_radius", collision_object.radius);
        collision_object.source_object_name = ReadString(object_dict, "source_object_name");
        collision_object.source_group_ids = ReadStringArray(object_dict, "source_group_ids");
        if (object_dict.contains("world_translation")) {
            collision_object.world_translation = ReadVec3(object_dict["world_translation"]);
        }
        if (object_dict.contains("world_rotation")) {
            collision_object.world_rotation = ReadQuat(object_dict["world_rotation"]);
        }
        if (object_dict.contains("linear_velocity")) {
            collision_object.linear_velocity = ReadVec3(object_dict["linear_velocity"]);
        }
        if (object_dict.contains("angular_velocity")) {
            collision_object.angular_velocity = ReadVec3(object_dict["angular_velocity"]);
        }
        scene.collision_objects.push_back(std::move(collision_object));
    }

    for (nb::handle item : ReadSequence(data, "collision_bindings")) {
        nb::dict binding_dict = nb::cast<nb::dict>(item);
        CompiledCollisionBinding binding;
        binding.binding_id = ReadString(binding_dict, "binding_id");
        binding.owner_component_id = ReadString(binding_dict, "owner_component_id");
        binding.source_group_ids = ReadStringArray(binding_dict, "source_group_ids");
        binding.collision_object_ids = ReadStringArray(binding_dict, "collision_object_ids");
        scene.collision_bindings.push_back(std::move(binding));
    }

    for (nb::handle item : ReadSequence(data, "mesh_writeback_targets")) {
        nb::dict target_dict = nb::cast<nb::dict>(item);
        CompiledMeshWritebackTarget target;
        target.component_id = ReadString(target_dict, "component_id");
        target.source_object_name = ReadString(target_dict, "source_object_name");
        target.vertex_count = ReadInt(target_dict, "vertex_count");
        target.edge_count = ReadInt(target_dict, "edge_count");
        target.face_count = ReadInt(target_dict, "face_count");
        target.topology_hash = ReadString(target_dict, "topology_hash");
        target.space = ReadString(target_dict, "space", "object_local");
        scene.mesh_writeback_targets.push_back(std::move(target));
    }

    return scene;
}

RuntimeInputs ParseRuntimeInputs(const nb::dict& root)
{
    nb::dict data = ResolveExchangePayload(root, "frame_inputs");
    RuntimeInputs inputs;

    for (nb::handle item : ReadSequence(data, "bone_chains")) {
        nb::dict chain_dict = nb::cast<nb::dict>(item);
        RuntimeChainInput chain;
        chain.component_id = ReadString(chain_dict, "component_id");
        chain.armature_name = ReadString(chain_dict, "armature_name");
        chain.root_bone_name = ReadString(chain_dict, "root_bone_name");
        chain.center_object_name = ReadString(chain_dict, "center_object_name");
        chain.center_bone_name = ReadString(chain_dict, "center_bone_name");
        if (chain_dict.contains("root_translation")) {
            chain.root_translation = ReadVec3(chain_dict["root_translation"]);
        }
        if (chain_dict.contains("root_rotation_quaternion")) {
            chain.root_rotation_quaternion = ReadQuat(chain_dict["root_rotation_quaternion"]);
        }
        if (chain_dict.contains("root_linear_velocity")) {
            chain.root_linear_velocity = ReadVec3(chain_dict["root_linear_velocity"]);
        }
        if (chain_dict.contains("root_scale")) {
            chain.root_scale = ReadVec3(chain_dict["root_scale"]);
        }
        if (chain_dict.contains("center_translation")) {
            chain.center_translation = ReadVec3(chain_dict["center_translation"]);
        }
        if (chain_dict.contains("center_rotation_quaternion")) {
            chain.center_rotation_quaternion = ReadQuat(chain_dict["center_rotation_quaternion"]);
        }
        if (chain_dict.contains("center_linear_velocity")) {
            chain.center_linear_velocity = ReadVec3(chain_dict["center_linear_velocity"]);
        }
        if (chain_dict.contains("center_scale")) {
            chain.center_scale = ReadVec3(chain_dict["center_scale"]);
        }
        chain.basic_head_positions = ReadVec3Array(chain_dict, "basic_head_positions");
        chain.basic_tail_positions = ReadVec3Array(chain_dict, "basic_tail_positions");
        chain.basic_rotations = ReadQuatArray(chain_dict, "basic_rotations");
        inputs.bone_chains.push_back(std::move(chain));
    }

    for (nb::handle item : ReadSequence(data, "collision_objects")) {
        nb::dict object_dict = nb::cast<nb::dict>(item);
        RuntimeCollisionObjectInput collision_object;
        collision_object.collision_object_id = ReadString(object_dict, "collision_object_id");
        if (object_dict.contains("world_translation")) {
            collision_object.world_translation = ReadVec3(object_dict["world_translation"]);
        }
        if (object_dict.contains("world_rotation")) {
            collision_object.world_rotation = ReadQuat(object_dict["world_rotation"]);
        }
        if (object_dict.contains("linear_velocity")) {
            collision_object.linear_velocity = ReadVec3(object_dict["linear_velocity"]);
        }
        inputs.collision_objects.push_back(std::move(collision_object));
    }

    return inputs;
}

nb::tuple ToTuple(const Vec3& value)
{
    return nb::make_tuple(value.x, value.y, value.z);
}

nb::tuple ToTuple(const Quat& value)
{
    return nb::make_tuple(value.w, value.x, value.y, value.z);
}

nb::dict ToPython(const BuildSceneOutput& output)
{
    nb::dict dict;
    nb::list particles;
    for (const BuildDrawParticle& particle : output.particles) {
        nb::dict item;
        item["component_id"] = particle.component_id;
        item["bone_name"] = particle.bone_name;
        item["joint_index"] = particle.joint_index;
        item["parent_index"] = particle.parent_index;
        item["rest_head_local"] = ToTuple(particle.rest_head_local);
        item["rest_tail_local"] = ToTuple(particle.rest_tail_local);
        item["radius"] = particle.radius;
        particles.append(item);
    }
    dict["particles"] = particles;

    nb::list lines;
    for (const BuildDrawLine& line : output.lines) {
        nb::dict item;
        item["component_id"] = line.component_id;
        item["start_index"] = line.start_index;
        item["end_index"] = line.end_index;
        lines.append(item);
    }
    dict["lines"] = lines;

    nb::list baselines;
    for (const CompiledSpringBaseline& baseline : output.baselines) {
        nb::dict item;
        nb::list indices;
        for (int joint_index : baseline.joint_indices) {
            indices.append(joint_index);
        }
        item["joint_indices"] = indices;
        baselines.append(item);
    }
    dict["baselines"] = baselines;

    nb::list colliders;
    for (const BuildDrawCollider& collider : output.colliders) {
        nb::dict item;
        item["collision_object_id"] = collider.collision_object_id;
        item["owner_component_id"] = collider.owner_component_id;
        item["shape_type"] = collider.shape_type;
        item["world_translation"] = ToTuple(collider.world_translation);
        item["world_rotation"] = ToTuple(collider.world_rotation);
        item["radius"] = collider.radius;
        item["height"] = collider.height;
        item["capsule_direction"] = collider.capsule_direction;
        item["capsule_aligned_on_center"] = collider.capsule_aligned_on_center;
        item["capsule_reverse_direction"] = collider.capsule_reverse_direction;
        item["capsule_end_radius"] = collider.capsule_end_radius;
        item["source_object_name"] = collider.source_object_name;
        colliders.append(item);
    }
    dict["colliders"] = colliders;

    return dict;
}

nb::dict ToPython(const BuildSceneResult& result)
{
    nb::dict dict;
    dict["handle"] = result.handle;
    dict["summary"] = result.summary;
    dict["backend"] = result.backend;
    dict["build_message"] = result.build_message;
    dict["backend_status"] = result.backend_status;
    dict["build_output"] = ToPython(result.build_output);
    return dict;
}

nb::dict ToPython(const StepSceneResult& result)
{
    nb::dict dict;
    dict["handle"] = result.handle;
    dict["dt"] = result.dt;
    dict["simulation_frequency"] = result.simulation_frequency;
    dict["executed_steps"] = result.executed_steps;
    dict["steps"] = result.steps;
    dict["summary"] = result.summary;
    return dict;
}

nb::list ToPython(const std::vector<BoneTransform>& transforms)
{
    nb::list items;
    for (const BoneTransform& transform : transforms) {
        nb::dict item;
        item["component_id"] = transform.component_id;
        item["armature_name"] = transform.armature_name;
        item["bone_name"] = transform.bone_name;
        item["translation"] = ToTuple(transform.translation);
        item["rotation_quaternion"] = ToTuple(transform.rotation_quaternion);
        item["write_mode"] = transform.write_mode;
        item["transform_flags"] = transform.transform_flags;
        items.append(item);
    }
    return items;
}

nb::list ToPython(const std::vector<MeshOutput>& outputs)
{
    nb::list items;
    for (const MeshOutput& output : outputs) {
        nb::dict item;
        item["component_id"] = output.component_id;
        item["object_name"] = output.object_name;
        item["source_object_name"] = output.source_object_name;
        item["space"] = output.space;
        nb::list positions;
        for (const Vec3& position : output.positions) {
            positions.append(ToTuple(position));
        }
        item["positions"] = positions;
        items.append(item);
    }
    return items;
}

}  // namespace

}  // namespace hocloth

NB_MODULE(hocloth_native, module)
{
    module.def("build_scene", [](const nb::dict& compiled_scene) {
        return hocloth::ToPython(hocloth::GetRuntimeModule().BuildScene(hocloth::ParseCompiledScene(compiled_scene)));
    });
    module.def("build_authoring_snapshot", [](const nb::dict& authoring_snapshot) {
        return hocloth::ToPython(
            hocloth::GetRuntimeModule().BuildScene(hocloth::blender::ParseAuthoringSnapshot(authoring_snapshot))
        );
    });
    module.def("destroy_scene", [](hocloth::SceneHandle handle) {
        return hocloth::GetRuntimeModule().DestroyScene(handle);
    });
    module.def("reset_scene", [](hocloth::SceneHandle handle) {
        return hocloth::GetRuntimeModule().ResetScene(handle);
    });
    module.def("set_runtime_inputs", [](hocloth::SceneHandle handle, const nb::dict& runtime_inputs) {
        return hocloth::GetRuntimeModule().SetRuntimeInputs(handle, hocloth::ParseRuntimeInputs(runtime_inputs));
    });
    module.def("step_scene", [](hocloth::SceneHandle handle, float dt, int simulation_frequency) {
        return hocloth::ToPython(hocloth::GetRuntimeModule().StepScene(handle, dt, simulation_frequency));
    });
    module.def("get_bone_transforms", [](hocloth::SceneHandle handle) {
        return hocloth::ToPython(hocloth::GetRuntimeModule().GetBoneTransforms(handle));
    });
    module.def("get_mesh_outputs", [](hocloth::SceneHandle handle) {
        return hocloth::ToPython(hocloth::GetRuntimeModule().GetMeshOutputs(handle));
    });
}
