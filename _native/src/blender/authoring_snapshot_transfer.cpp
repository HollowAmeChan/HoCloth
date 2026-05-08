#include "hocloth/blender/authoring_snapshot_transfer.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <algorithm>
#include <cstddef>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nb = nanobind;

namespace hocloth::blender {

namespace {

nb::object GetOptional(const nb::dict& dict, const char* key)
{
    return dict.contains(key) ? nb::borrow<nb::object>(dict[key]) : nb::none();
}

nb::dict ReadDict(const nb::dict& dict, const char* key)
{
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return nb::dict();
    }
    return nb::cast<nb::dict>(value);
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

float ReadCurveValue(const nb::dict& dict, const char* key, float fallback = 0.0f)
{
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return fallback;
    }
    if (PyDict_Check(value.ptr())) {
        return ReadFloat(nb::cast<nb::dict>(value), "value", fallback);
    }
    return nb::cast<float>(value);
}

nb::sequence ReadSequence(const nb::dict& dict, const char* key)
{
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return nb::steal<nb::sequence>(PyList_New(0));
    }
    return nb::cast<nb::sequence>(value);
}

CompiledCurve ReadCurve(const nb::dict& dict, const char* key, float fallback = 0.0f)
{
    CompiledCurve curve;
    curve.value = fallback;
    nb::object value = GetOptional(dict, key);
    if (value.is_none()) {
        return curve;
    }
    if (!PyDict_Check(value.ptr())) {
        curve.value = nb::cast<float>(value);
        curve.has_value = true;
        return curve;
    }

    nb::dict curve_dict = nb::cast<nb::dict>(value);
    curve.value = ReadFloat(curve_dict, "value", fallback);
    curve.has_value = !GetOptional(curve_dict, "value").is_none();
    curve.use_curve = ReadBool(curve_dict, "useCurve", false);
    for (nb::handle item : ReadSequence(curve_dict, "samples")) {
        curve.samples.push_back(nb::cast<float>(item));
    }
    if (curve.samples.size() != 16) {
        curve.samples.clear();
    }
    return curve;
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

std::string NormalizeBoneConnectionMode(std::string mode)
{
    mode.erase(
        std::remove_if(mode.begin(), mode.end(), [](unsigned char c) {
            return c == '_' || c == '-' || std::isspace(c) != 0;
        }),
        mode.end()
    );
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (mode == "automaticmesh" || mode == "automatic") {
        return "AutomaticMesh";
    }
    if (mode == "sequentialloopmesh" || mode == "sequentialloop") {
        return "SequentialLoopMesh";
    }
    if (mode == "sequentialnonloopmesh" || mode == "sequentialnonloop") {
        return "SequentialNonLoopMesh";
    }
    return "Line";
}

std::vector<int> ReadIntArray(const nb::dict& dict, const char* key)
{
    std::vector<int> result;
    for (nb::handle item : ReadSequence(dict, key)) {
        result.push_back(nb::cast<int>(item));
    }
    return result;
}

std::vector<CompiledBoneAttributeOverride> ReadBoneAttributeOverrides(const nb::dict& dict)
{
    std::vector<CompiledBoneAttributeOverride> result;
    for (nb::handle item : ReadSequence(dict, "bone_attribute_overrides")) {
        nb::dict override_dict = nb::cast<nb::dict>(item);
        CompiledBoneAttributeOverride override;
        override.bone_name = ReadString(override_dict, "bone_name");
        override.attribute = ReadString(override_dict, "attribute", "DEFAULT");
        if (!override.bone_name.empty() && override.attribute != "DEFAULT") {
            result.push_back(std::move(override));
        }
    }
    return result;
}

void AppendUnique(std::vector<std::string>& values, const std::string& value)
{
    if (value.empty()) {
        return;
    }
    for (const std::string& existing : values) {
        if (existing == value) {
            return;
        }
    }
    values.push_back(value);
}

void BuildTopologyFromJoints(CompiledSpringBone& chain)
{
    chain.lines.clear();
    chain.baselines.clear();

    std::vector<std::vector<int>> children(chain.joints.size());
    for (std::size_t index = 0; index < chain.joints.size(); ++index) {
        const int parent_index = chain.joints[index].parent_index;
        if (parent_index >= 0 && parent_index < static_cast<int>(chain.joints.size())) {
            chain.lines.push_back(CompiledSpringLine{parent_index, static_cast<int>(index)});
            children[static_cast<std::size_t>(parent_index)].push_back(static_cast<int>(index));
        }
    }

    for (std::size_t index = 0; index < chain.joints.size(); ++index) {
        if (!children[index].empty()) {
            continue;
        }

        CompiledSpringBaseline baseline;
        int current_index = static_cast<int>(index);
        while (current_index >= 0 && current_index < static_cast<int>(chain.joints.size())) {
            baseline.joint_indices.insert(baseline.joint_indices.begin(), current_index);
            current_index = chain.joints[static_cast<std::size_t>(current_index)].parent_index;
        }
        if (!baseline.joint_indices.empty()) {
            chain.baselines.push_back(std::move(baseline));
        }
    }
}

void AppendJointFromAuthoringBone(
    CompiledSpringBone& chain,
    const nb::dict& bone_dict,
    const std::unordered_map<std::string, int>& index_by_name
)
{
    CompiledSpringJoint joint;
    joint.name = ReadString(bone_dict, "name");
    const std::string parent_name = ReadString(bone_dict, "parent_name");
    joint.parent_index = ReadInt(bone_dict, "parent_index", -1);
    if (!parent_name.empty()) {
        auto parent_it = index_by_name.find(parent_name);
        joint.parent_index = parent_it != index_by_name.end() ? parent_it->second : -1;
    }
    joint.depth = ReadInt(bone_dict, "depth");
    joint.length = ReadFloat(bone_dict, "length");
    joint.radius = ReadFloat(bone_dict, "radius", chain.joint_radius);
    joint.stiffness = ReadFloat(bone_dict, "stiffness", chain.stiffness);
    joint.damping = ReadFloat(bone_dict, "damping", chain.damping);
    joint.drag = ReadFloat(bone_dict, "drag", chain.drag);
    joint.gravity_scale = ReadFloat(bone_dict, "gravity_scale", 1.0f);
    if (bone_dict.contains("rest_head_local")) {
        joint.rest_head_local = ReadVec3(bone_dict["rest_head_local"]);
    }
    if (bone_dict.contains("rest_tail_local")) {
        joint.rest_tail_local = ReadVec3(bone_dict["rest_tail_local"]);
    }
    if (bone_dict.contains("rest_local_translation")) {
        joint.rest_local_translation = ReadVec3(bone_dict["rest_local_translation"]);
    }
    if (bone_dict.contains("rest_local_rotation")) {
        joint.rest_local_rotation = ReadQuat(bone_dict["rest_local_rotation"]);
    }
    if (bone_dict.contains("rest_world_rotation")) {
        joint.rest_world_rotation = ReadQuat(bone_dict["rest_world_rotation"]);
        joint.has_rest_world_rotation = true;
    } else {
        joint.rest_world_rotation = joint.rest_local_rotation;
    }
    chain.joints.push_back(std::move(joint));
}

void BuildJointsFromAuthoringBones(CompiledSpringBone& chain, const nb::dict& chain_dict)
{
    std::unordered_map<std::string, int> index_by_name;
    for (nb::handle bone_item : ReadSequence(chain_dict, "bones")) {
        nb::dict bone_dict = nb::cast<nb::dict>(bone_item);
        const std::string bone_name = ReadString(bone_dict, "name");
        if (bone_name.empty()) {
            continue;
        }
        index_by_name[bone_name] = static_cast<int>(chain.joints.size());
        AppendJointFromAuthoringBone(chain, bone_dict, index_by_name);
    }
}

void ReadLegacyJoints(CompiledSpringBone& chain, const nb::dict& chain_dict)
{
    for (nb::handle joint_item : ReadSequence(chain_dict, "joints")) {
        nb::dict joint_dict = nb::cast<nb::dict>(joint_item);
        CompiledSpringJoint joint;
        joint.name = ReadString(joint_dict, "name");
        joint.parent_index = ReadInt(joint_dict, "parent_index", -1);
        joint.depth = ReadInt(joint_dict, "depth");
        joint.length = ReadFloat(joint_dict, "length");
        joint.radius = ReadFloat(joint_dict, "radius", chain.joint_radius);
        joint.stiffness = ReadFloat(joint_dict, "stiffness", chain.stiffness);
        joint.damping = ReadFloat(joint_dict, "damping", chain.damping);
        joint.drag = ReadFloat(joint_dict, "drag", chain.drag);
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
        if (joint_dict.contains("rest_world_rotation")) {
            joint.rest_world_rotation = ReadQuat(joint_dict["rest_world_rotation"]);
            joint.has_rest_world_rotation = true;
        } else {
            joint.rest_world_rotation = joint.rest_local_rotation;
        }
        chain.joints.push_back(std::move(joint));
    }
}

void ApplyClothSerializeData(CompiledSpringBone& chain, const nb::dict& serialize_data)
{
    if (serialize_data.empty()) {
        return;
    }

    chain.cloth_type = ReadString(serialize_data, "clothType", chain.cloth_type);
    chain.component_type = chain.cloth_type == "BoneSpring" ? "SPRING_BONE" : "BONE_CLOTH";
    chain.bone_connection_mode = NormalizeBoneConnectionMode(
        ReadString(serialize_data, "connectionMode", chain.bone_connection_mode)
    );
    chain.gravity_strength = ReadFloat(serialize_data, "gravity", chain.gravity_strength);
    chain.gravity_falloff = ReadFloat(serialize_data, "gravityFalloff", chain.gravity_falloff);
    chain.stabilization_time_after_reset = ReadFloat(
        serialize_data,
        "stablizationTimeAfterReset",
        chain.stabilization_time_after_reset
    );
    chain.blend_weight = ReadFloat(serialize_data, "blendWeight", chain.blend_weight);
    if (serialize_data.contains("gravityDirection")) {
        chain.gravity_direction = ReadVec3(serialize_data["gravityDirection"]);
    }
    chain.radius_curve = ReadCurve(serialize_data, "radius", chain.joint_radius);
    chain.joint_radius = chain.radius_curve.value;
    chain.damping_curve = ReadCurve(serialize_data, "damping", chain.damping);
    chain.damping = chain.damping_curve.value;
    chain.damping_curve_value = chain.damping;

    nb::dict inertia = ReadDict(serialize_data, "inertiaConstraint");
    if (!inertia.empty()) {
        chain.inertia_world_inertia = ReadFloat(inertia, "worldInertia", chain.inertia_world_inertia);
        chain.inertia_movement_inertia_smoothing =
            ReadFloat(inertia, "movementInertiaSmoothing", chain.inertia_movement_inertia_smoothing);
        nb::dict movement_limit = ReadDict(inertia, "movementSpeedLimit");
        chain.inertia_movement_speed_limit_enabled =
            ReadBool(movement_limit, "use", chain.inertia_movement_speed_limit_enabled);
        chain.inertia_movement_speed_limit =
            ReadFloat(movement_limit, "value", chain.inertia_movement_speed_limit);
        nb::dict rotation_limit = ReadDict(inertia, "rotationSpeedLimit");
        chain.inertia_rotation_speed_limit_enabled =
            ReadBool(rotation_limit, "use", chain.inertia_rotation_speed_limit_enabled);
        chain.inertia_rotation_speed_limit =
            ReadFloat(rotation_limit, "value", chain.inertia_rotation_speed_limit);
        chain.inertia_local_inertia = ReadFloat(inertia, "localInertia", chain.inertia_local_inertia);
        nb::dict local_movement_limit = ReadDict(inertia, "localMovementSpeedLimit");
        chain.inertia_local_movement_speed_limit_enabled =
            ReadBool(local_movement_limit, "use", chain.inertia_local_movement_speed_limit_enabled);
        chain.inertia_local_movement_speed_limit =
            ReadFloat(local_movement_limit, "value", chain.inertia_local_movement_speed_limit);
        nb::dict local_rotation_limit = ReadDict(inertia, "localRotationSpeedLimit");
        chain.inertia_local_rotation_speed_limit_enabled =
            ReadBool(local_rotation_limit, "use", chain.inertia_local_rotation_speed_limit_enabled);
        chain.inertia_local_rotation_speed_limit =
            ReadFloat(local_rotation_limit, "value", chain.inertia_local_rotation_speed_limit);
        chain.inertia_depth_inertia = ReadFloat(inertia, "depthInertia", chain.inertia_depth_inertia);
        chain.inertia_centrifugal_acceleration =
            ReadFloat(inertia, "centrifugalAcceleration", chain.inertia_centrifugal_acceleration);
        nb::dict particle_limit = ReadDict(inertia, "particleSpeedLimit");
        chain.inertia_particle_speed_limit_enabled =
            ReadBool(particle_limit, "use", chain.inertia_particle_speed_limit_enabled);
        chain.inertia_particle_speed_limit =
            ReadFloat(particle_limit, "value", chain.inertia_particle_speed_limit);
    }

    nb::dict tether = ReadDict(serialize_data, "tetherConstraint");
    chain.tether_distance_compression =
        ReadFloat(tether, "distanceCompression", chain.tether_distance_compression);

    nb::dict distance = ReadDict(serialize_data, "distanceConstraint");
    chain.distance_stiffness_curve = ReadCurve(distance, "stiffness", chain.distance_stiffness);
    chain.distance_stiffness = chain.distance_stiffness_curve.value;
    chain.stiffness = chain.distance_stiffness;

    nb::dict triangle = ReadDict(serialize_data, "triangleBendingConstraint");
    chain.triangle_bending_stiffness =
        ReadFloat(triangle, "stiffness", chain.triangle_bending_stiffness);

    nb::dict angle = ReadDict(serialize_data, "angleRestorationConstraint");
    chain.angle_restoration_enabled =
        ReadBool(angle, "useAngleRestoration", chain.angle_restoration_enabled);
    chain.angle_restoration_stiffness_curve =
        ReadCurve(angle, "stiffness", chain.angle_restoration_stiffness);
    chain.angle_restoration_stiffness = chain.angle_restoration_stiffness_curve.value;
    chain.angle_restoration_velocity_attenuation =
        ReadFloat(angle, "velocityAttenuation", chain.angle_restoration_velocity_attenuation);
    chain.angle_restoration_gravity_falloff =
        ReadFloat(angle, "gravityFalloff", chain.angle_restoration_gravity_falloff);
    chain.drag = std::max(0.0f, std::min(1.0f, 1.0f - chain.angle_restoration_velocity_attenuation));

    nb::dict angle_limit = ReadDict(serialize_data, "angleLimitConstraint");
    chain.angle_limit_enabled =
        ReadBool(angle_limit, "useAngleLimit", chain.angle_limit_enabled);
    chain.angle_limit_angle_curve =
        ReadCurve(angle_limit, "limitAngle", chain.angle_limit_angle);
    chain.angle_limit_angle = chain.angle_limit_angle_curve.value;
    chain.angle_limit_stiffness =
        ReadFloat(angle_limit, "stiffness", chain.angle_limit_stiffness);

    nb::dict spring = ReadDict(serialize_data, "springConstraint");
    chain.use_spring = ReadBool(spring, "useSpring", chain.use_spring);
    chain.spring_power = ReadFloat(spring, "springPower", chain.spring_power);
    chain.limit_distance = ReadFloat(spring, "limitDistance", chain.limit_distance);
    chain.normal_limit_ratio = ReadFloat(spring, "normalLimitRatio", chain.normal_limit_ratio);
    chain.spring_noise = ReadFloat(spring, "springNoise", chain.spring_noise);

    nb::dict collider = ReadDict(serialize_data, "colliderCollisionConstraint");
    const std::string collider_mode = ReadString(collider, "mode", chain.collider_collision_mode);
    chain.collider_collision_mode = collider_mode;
    chain.collider_collision_enabled = collider_mode != "None";
    chain.collider_friction = ReadFloat(collider, "friction", chain.collider_friction);
    chain.collider_limit_distance_curve = ReadCurve(collider, "limitDistance", chain.collider_limit_distance);
    chain.collider_limit_distance = chain.collider_limit_distance_curve.value;
    if (collider.contains("colliderList")) {
        chain.collider_ids = ReadStringArray(collider, "colliderList");
    }
    if (collider.contains("collisionBones")) {
        chain.collision_bone_indices = ReadIntArray(collider, "collisionBones");
    }
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

    return nb::cast<nb::dict>(root["payload"]);
}

}  // namespace

CompiledScene ParseAuthoringSnapshot(const nb::dict& root)
{
    nb::dict data = ResolveExchangePayload(root, "authoring_snapshot");
    CompiledScene scene;
    std::unordered_map<std::string, std::string> collision_object_id_by_collider_id;
    std::unordered_set<std::string> valid_collider_ids;

    for (nb::handle item : ReadSequence(data, "bone_chains")) {
        nb::dict chain_dict = nb::cast<nb::dict>(item);
        CompiledSpringBone chain;
        chain.component_id = ReadString(chain_dict, "component_id");
        chain.component_type = ReadString(chain_dict, "component_type", "BONE_CLOTH");
        chain.cloth_type = ReadString(
            chain_dict,
            "cloth_type",
            chain.component_type == "BONE_CLOTH" ? "BoneCloth" : "BoneSpring"
        );
        chain.armature_name = ReadString(chain_dict, "armature_name");
        chain.root_bone_name = ReadString(chain_dict, "root_bone_name");
        chain.root_bone_names = ReadStringArray(chain_dict, "root_bone_names");
        if (chain.root_bone_names.empty() && !chain.root_bone_name.empty()) {
            chain.root_bone_names.push_back(chain.root_bone_name);
        }
        chain.bone_connection_mode = NormalizeBoneConnectionMode(
            ReadString(chain_dict, "bone_connection_mode", chain.bone_connection_mode)
        );
        chain.pose_space = ReadString(chain_dict, "pose_space", chain.pose_space);
        chain.center_object_name = ReadString(chain_dict, "center_object_name");
        chain.center_bone_name = ReadString(chain_dict, "center_bone_name");
        chain.joint_radius = ReadFloat(chain_dict, "joint_radius");
        chain.stiffness = ReadFloat(chain_dict, "stiffness");
        chain.damping = ReadFloat(chain_dict, "damping");
        chain.drag = ReadFloat(chain_dict, "drag");
        chain.damping_curve_value = ReadFloat(chain_dict, "damping_curve_value", chain.damping);
        chain.gravity_falloff = ReadFloat(chain_dict, "gravity_falloff", chain.gravity_falloff);
        chain.stabilization_time_after_reset = ReadFloat(
            chain_dict,
            "stabilization_time_after_reset",
            chain.stabilization_time_after_reset
        );
        chain.blend_weight = ReadFloat(chain_dict, "blend_weight", chain.blend_weight);
        chain.inertia_world_inertia = ReadFloat(chain_dict, "inertia_world_inertia", 1.0f);
        chain.inertia_movement_inertia_smoothing = ReadFloat(chain_dict, "inertia_movement_inertia_smoothing", 0.4f);
        chain.inertia_movement_speed_limit_enabled = ReadBool(chain_dict, "inertia_movement_speed_limit_enabled", false);
        chain.inertia_movement_speed_limit = ReadFloat(chain_dict, "inertia_movement_speed_limit", 0.0f);
        chain.inertia_rotation_speed_limit_enabled = ReadBool(chain_dict, "inertia_rotation_speed_limit_enabled", false);
        chain.inertia_rotation_speed_limit = ReadFloat(chain_dict, "inertia_rotation_speed_limit", 0.0f);
        chain.inertia_local_inertia = ReadFloat(chain_dict, "inertia_local_inertia", 1.0f);
        chain.inertia_local_movement_speed_limit_enabled = ReadBool(chain_dict, "inertia_local_movement_speed_limit_enabled", false);
        chain.inertia_local_movement_speed_limit = ReadFloat(chain_dict, "inertia_local_movement_speed_limit", 0.0f);
        chain.inertia_local_rotation_speed_limit_enabled = ReadBool(chain_dict, "inertia_local_rotation_speed_limit_enabled", false);
        chain.inertia_local_rotation_speed_limit = ReadFloat(chain_dict, "inertia_local_rotation_speed_limit", 0.0f);
        chain.inertia_depth_inertia = ReadFloat(chain_dict, "inertia_depth_inertia", 0.0f);
        chain.inertia_centrifugal_acceleration = ReadFloat(chain_dict, "inertia_centrifugal_acceleration", 0.0f);
        chain.inertia_particle_speed_limit_enabled = ReadBool(chain_dict, "inertia_particle_speed_limit_enabled", false);
        chain.inertia_particle_speed_limit = ReadFloat(chain_dict, "inertia_particle_speed_limit", 0.0f);
        chain.tether_distance_compression = ReadFloat(chain_dict, "tether_distance_compression", chain.tether_distance_compression);
        chain.distance_stiffness = ReadFloat(chain_dict, "distance_stiffness", chain.stiffness);
        chain.triangle_bending_stiffness = ReadFloat(
            chain_dict,
            "triangle_bending_stiffness",
            chain.triangle_bending_stiffness
        );
        chain.angle_restoration_enabled = ReadBool(chain_dict, "angle_restoration_enabled", true);
        chain.angle_restoration_stiffness = ReadFloat(
            chain_dict,
            "angle_restoration_stiffness",
            chain.angle_restoration_stiffness
        );
        chain.angle_restoration_velocity_attenuation = ReadFloat(chain_dict, "angle_restoration_velocity_attenuation", 0.6f);
        chain.angle_restoration_gravity_falloff = ReadFloat(
            chain_dict,
            "angle_restoration_gravity_falloff",
            chain.angle_restoration_gravity_falloff
        );
        chain.angle_limit_enabled = ReadBool(chain_dict, "angle_limit_enabled", false);
        chain.angle_limit_angle = ReadFloat(chain_dict, "angle_limit_angle", chain.angle_limit_angle);
        chain.angle_limit_stiffness = ReadFloat(
            chain_dict,
            "angle_limit_stiffness",
            chain.angle_limit_stiffness
        );
        chain.use_spring = ReadBool(chain_dict, "use_spring", true);
        chain.spring_power = ReadFloat(chain_dict, "spring_power", 0.04f);
        chain.limit_distance = ReadFloat(chain_dict, "limit_distance", 0.1f);
        chain.normal_limit_ratio = ReadFloat(chain_dict, "normal_limit_ratio", 1.0f);
        chain.spring_noise = ReadFloat(chain_dict, "spring_noise", 0.0f);
        chain.collider_friction = ReadFloat(chain_dict, "collider_friction", 0.5f);
        chain.collider_limit_distance = ReadFloat(chain_dict, "collider_limit_distance", 0.05f);
        chain.collider_collision_enabled = ReadBool(chain_dict, "collider_collision_enabled", true);
        chain.collider_collision_mode = ReadString(chain_dict, "collider_collision_mode", "Point");
        chain.gravity_strength = ReadFloat(chain_dict, "gravity_strength");
        if (chain_dict.contains("gravity_direction")) {
            chain.gravity_direction = ReadVec3(chain_dict["gravity_direction"]);
        }
        if (chain_dict.contains("armature_scale")) {
            chain.armature_scale = ReadVec3(chain_dict["armature_scale"]);
        }
        chain.collider_ids = ReadStringArray(chain_dict, "collider_ids");
        chain.collider_group_ids = ReadStringArray(chain_dict, "collider_group_ids");
        chain.collision_binding_ids = ReadStringArray(chain_dict, "collision_binding_ids");
        chain.collision_bone_indices = ReadIntArray(chain_dict, "collision_bone_indices");
        chain.bone_attribute_overrides = ReadBoneAttributeOverrides(chain_dict);

        ApplyClothSerializeData(chain, ReadDict(chain_dict, "serialize_data"));
        if (!chain.collider_collision_enabled) {
            chain.collider_ids.clear();
            chain.collider_group_ids.clear();
            chain.collision_binding_ids.clear();
        }

        if (chain_dict.contains("bones")) {
            BuildJointsFromAuthoringBones(chain, chain_dict);
        } else {
            ReadLegacyJoints(chain, chain_dict);
        }

        BuildTopologyFromJoints(chain);
        scene.spring_bones.push_back(std::move(chain));
    }

    for (nb::handle item : ReadSequence(data, "colliders")) {
        nb::dict collider_dict = nb::cast<nb::dict>(item);
        const std::string component_id = ReadString(collider_dict, "component_id");
        if (component_id.empty()) {
            continue;
        }

        CompiledCollisionObject collision_object;
        collision_object.collision_object_id = std::string("collision::") + component_id;
        collision_object.owner_component_id = component_id;
        collision_object.motion_type = "KINEMATIC";
        collision_object.shape_type = ReadString(collider_dict, "shape_type", "SPHERE");
        collision_object.radius = ReadFloat(collider_dict, "radius");
        collision_object.height = ReadFloat(collider_dict, "height");
        collision_object.capsule_direction = ReadString(collider_dict, "capsule_direction", "Y");
        collision_object.capsule_aligned_on_center = ReadBool(collider_dict, "capsule_aligned_on_center", true);
        collision_object.capsule_reverse_direction = ReadBool(collider_dict, "capsule_reverse_direction", false);
        collision_object.capsule_end_radius = ReadFloat(collider_dict, "capsule_end_radius", collision_object.radius);
        collision_object.source_object_name = ReadString(collider_dict, "object_name");
        if (collider_dict.contains("world_translation")) {
            collision_object.world_translation = ReadVec3(collider_dict["world_translation"]);
        }
        if (collider_dict.contains("world_rotation")) {
            collision_object.world_rotation = ReadQuat(collider_dict["world_rotation"]);
        }
        valid_collider_ids.insert(component_id);
        collision_object_id_by_collider_id[component_id] = collision_object.collision_object_id;
        scene.collision_objects.push_back(std::move(collision_object));
    }

    for (nb::handle item : ReadSequence(data, "collider_groups")) {
        nb::dict group_dict = nb::cast<nb::dict>(item);
        const std::string group_id = ReadString(group_dict, "component_id");
        if (group_id.empty()) {
            continue;
        }

        CompiledCollisionBinding binding;
        binding.binding_id = group_id;
        binding.owner_component_id = group_id;
        binding.source_group_ids.push_back(group_id);
        for (const std::string& collider_id : ReadStringArray(group_dict, "collider_ids")) {
            auto it = collision_object_id_by_collider_id.find(collider_id);
            if (it != collision_object_id_by_collider_id.end()) {
                AppendUnique(binding.collision_object_ids, it->second);
            }
        }
        scene.collision_bindings.push_back(std::move(binding));
    }

    std::unordered_set<std::string> binding_ids;
    for (const CompiledCollisionBinding& binding : scene.collision_bindings) {
        binding_ids.insert(binding.binding_id);
    }

    for (CompiledSpringBone& chain : scene.spring_bones) {
        std::vector<std::string> resolved_group_bindings;
        for (const std::string& group_id : chain.collider_group_ids) {
            if (binding_ids.count(group_id) > 0) {
                AppendUnique(resolved_group_bindings, group_id);
            }
        }

        std::vector<std::string> direct_collider_ids;
        for (const std::string& collider_id : chain.collider_ids) {
            if (valid_collider_ids.count(collider_id) > 0) {
                AppendUnique(direct_collider_ids, collider_id);
            }
        }
        chain.collider_ids = std::move(direct_collider_ids);
        chain.collision_binding_ids = std::move(resolved_group_bindings);

        if (!chain.collider_ids.empty()) {
            const std::string binding_id = std::string("chain::") + chain.component_id + "::colliders";
            CompiledCollisionBinding binding;
            binding.binding_id = binding_id;
            binding.owner_component_id = chain.component_id;
            for (const std::string& collider_id : chain.collider_ids) {
                auto it = collision_object_id_by_collider_id.find(collider_id);
                if (it != collision_object_id_by_collider_id.end()) {
                    AppendUnique(binding.collision_object_ids, it->second);
                }
            }
            if (!binding.collision_object_ids.empty()) {
                scene.collision_bindings.push_back(std::move(binding));
                AppendUnique(chain.collision_binding_ids, binding_id);
            }
        }
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

}  // namespace hocloth::blender
