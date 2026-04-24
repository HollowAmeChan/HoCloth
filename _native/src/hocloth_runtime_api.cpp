#include "hocloth_runtime_api.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace hocloth {

namespace {

constexpr const char* kTailTipSuffix = "__hocloth_tail_tip__";

// From MC2: MagicaCloth2/Scripts/Core/Define/SystemDefine.cs
constexpr int kMc2DefaultSimulationFrequency = 90;
constexpr int kMc2SimulationFrequencyLow = 30;
constexpr int kMc2SimulationFrequencyHi = 150;
constexpr int kMc2DefaultMaxSimulationCountPerFrame = 3;
constexpr int kMc2MaxSimulationCountPerFrameLow = 1;
constexpr int kMc2MaxSimulationCountPerFrameHi = 5;
constexpr float kMc2BoneSpringDistanceStiffness = 0.5f;
constexpr float kMc2BoneSpringTetherCompressionLimit = 0.8f;
constexpr float kMc2BoneSpringCollisionFriction = 0.5f;

float Clamp(float value, float low, float high)
{
    return std::clamp(value, low, high);
}

float Clamp01(float value)
{
    return Clamp(value, 0.0f, 1.0f);
}

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Scale(const Vec3& value, float scalar)
{
    return Vec3{value.x * scalar, value.y * scalar, value.z * scalar};
}

float Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length(const Vec3& value)
{
    return std::sqrt(Dot(value, value));
}

Vec3 Normalize(const Vec3& value)
{
    const float length = Length(value);
    if (length <= 1.0e-6f) {
        return Vec3{0.0f, -1.0f, 0.0f};
    }
    return Scale(value, 1.0f / length);
}

Quat Mul(const Quat& a, const Quat& b)
{
    return Quat{
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

Quat QuaternionFromPitchRoll(float pitch, float roll)
{
    const float half_pitch = pitch * 0.5f;
    const float half_roll = roll * 0.5f;
    const Quat quat_x{
        std::cos(half_pitch),
        std::sin(half_pitch),
        0.0f,
        0.0f,
    };
    const Quat quat_z{
        std::cos(half_roll),
        0.0f,
        0.0f,
        std::sin(half_roll),
    };
    return Mul(quat_z, quat_x);
}

bool EndsWith(const std::string& value, const std::string& suffix)
{
    if (suffix.size() > value.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<CompiledSpringLine> BuildLineIndices(const CompiledSpringBone& chain)
{
    if (!chain.lines.empty()) {
        return chain.lines;
    }

    std::vector<CompiledSpringLine> result;
    result.reserve(chain.joints.size());
    for (std::size_t index = 0; index < chain.joints.size(); ++index) {
        const int parent_index = chain.joints[index].parent_index;
        if (parent_index >= 0) {
            result.push_back(CompiledSpringLine{parent_index, static_cast<int>(index)});
        }
    }
    return result;
}

std::vector<std::vector<int>> BuildBaselinePaths(const CompiledSpringBone& chain)
{
    std::vector<std::vector<int>> result;
    if (!chain.baselines.empty()) {
        result.reserve(chain.baselines.size());
        for (const CompiledSpringBaseline& baseline : chain.baselines) {
            if (!baseline.joint_indices.empty()) {
                result.push_back(baseline.joint_indices);
            }
        }
        if (!result.empty()) {
            return result;
        }
    }

    std::unordered_map<int, std::vector<int>> parent_to_children;
    for (std::size_t index = 0; index < chain.joints.size(); ++index) {
        const int parent_index = chain.joints[index].parent_index;
        if (parent_index >= 0) {
            parent_to_children[parent_index].push_back(static_cast<int>(index));
        }
    }

    for (std::size_t index = 0; index < chain.joints.size(); ++index) {
        if (parent_to_children.contains(static_cast<int>(index))) {
            continue;
        }

        std::vector<int> path;
        int current = static_cast<int>(index);
        while (current >= 0) {
            path.push_back(current);
            current = chain.joints[static_cast<std::size_t>(current)].parent_index;
        }
        std::reverse(path.begin(), path.end());
        result.push_back(std::move(path));
    }

    return result;
}

struct JointState {
    float pitch = 0.0f;
    float roll = 0.0f;
    float pitch_velocity = 0.0f;
    float roll_velocity = 0.0f;
};

struct ChainCache {
    std::vector<CompiledSpringLine> line_indices;
    std::vector<std::vector<int>> baseline_paths;
    std::vector<JointState> joint_states;
};

RuntimeChainInput* FindRuntimeChainInput(RuntimeInputs& inputs, const std::string& component_id)
{
    for (RuntimeChainInput& input : inputs.bone_chains) {
        if (input.component_id == component_id) {
            return &input;
        }
    }
    return nullptr;
}

const RuntimeChainInput* FindRuntimeChainInput(const RuntimeInputs& inputs, const std::string& component_id)
{
    for (const RuntimeChainInput& input : inputs.bone_chains) {
        if (input.component_id == component_id) {
            return &input;
        }
    }
    return nullptr;
}

Vec3 ToAnglePoint(const JointState& state)
{
    return Vec3{state.roll, state.pitch, 0.0f};
}

void FromAnglePoint(const Vec3& point, JointState& state)
{
    state.roll = Clamp(point.x, -1.2f, 1.2f);
    state.pitch = Clamp(point.y, -1.2f, 1.2f);
}

}  // namespace

struct RuntimeModule::SceneState {
    CompiledScene compiled_scene;
    RuntimeInputs runtime_inputs;
    std::vector<ChainCache> chain_caches;
    std::unordered_map<std::string, std::size_t> collision_object_lookup;
    std::unordered_map<std::string, std::vector<std::size_t>> collision_binding_lookup;
    SimulationTimeState time_state;
    std::uint64_t steps = 0;
};

std::string CompiledScene::Summary() const
{
    std::size_t total_bones = 0;
    for (const CompiledSpringBone& chain : spring_bones) {
        total_bones += chain.joints.size();
    }

    std::ostringstream stream;
    stream << "spring_bones=" << spring_bones.size()
           << ", bones=" << total_bones
           << ", colliders=0"
           << ", collider_groups=0"
           << ", collision_objects=" << collision_objects.size()
           << ", collision_bindings=" << collision_bindings.size()
           << ", cache_outputs=0";
    return stream.str();
}

void SimulationTimeState::FrameUpdate(int requested_frequency)
{
    // From MC2: MagicaCloth2/Scripts/Core/Manager/Simulation/TimeManager.cs
    simulation_frequency = std::clamp(
        requested_frequency,
        kMc2SimulationFrequencyLow,
        kMc2SimulationFrequencyHi
    );
    max_simulation_count_per_frame = std::clamp(
        max_simulation_count_per_frame,
        kMc2MaxSimulationCountPerFrameLow,
        kMc2MaxSimulationCountPerFrameHi
    );
    global_time_scale = Clamp01(global_time_scale);

    simulation_delta_time = 1.0f / static_cast<float>(simulation_frequency);
    max_delta_time = simulation_delta_time * static_cast<float>(max_simulation_count_per_frame);

    const float t = static_cast<float>(kMc2DefaultSimulationFrequency) / static_cast<float>(simulation_frequency);
    simulation_power[0] = t;
    simulation_power[1] = t > 1.0f ? std::pow(t, 0.5f) : t;
    simulation_power[2] = t > 1.0f ? std::pow(t, 0.3f) : t;
    simulation_power[3] = std::pow(t, 1.8f);
}

namespace {

void BuildSceneCaches(RuntimeModule::SceneState& scene)
{
    scene.chain_caches.clear();
    scene.chain_caches.reserve(scene.compiled_scene.spring_bones.size());
    for (const CompiledSpringBone& chain : scene.compiled_scene.spring_bones) {
        ChainCache cache;
        cache.line_indices = BuildLineIndices(chain);
        cache.baseline_paths = BuildBaselinePaths(chain);
        cache.joint_states.resize(chain.joints.size());
        scene.chain_caches.push_back(std::move(cache));
    }

    scene.collision_object_lookup.clear();
    for (std::size_t index = 0; index < scene.compiled_scene.collision_objects.size(); ++index) {
        const std::string& collision_object_id = scene.compiled_scene.collision_objects[index].collision_object_id;
        scene.collision_object_lookup[collision_object_id] = index;
    }

    scene.collision_binding_lookup.clear();
    for (const CompiledCollisionBinding& binding : scene.compiled_scene.collision_bindings) {
        std::vector<std::size_t> indices;
        indices.reserve(binding.collision_object_ids.size());
        for (const std::string& collision_object_id : binding.collision_object_ids) {
            auto it = scene.collision_object_lookup.find(collision_object_id);
            if (it != scene.collision_object_lookup.end()) {
                indices.push_back(it->second);
            }
        }
        scene.collision_binding_lookup[binding.binding_id] = std::move(indices);
    }
}

void ApplyCollisionObjectInputs(RuntimeModule::SceneState& scene)
{
    for (const RuntimeCollisionObjectInput& collision_input : scene.runtime_inputs.collision_objects) {
        auto it = scene.collision_object_lookup.find(collision_input.collision_object_id);
        if (it == scene.collision_object_lookup.end()) {
            continue;
        }

        CompiledCollisionObject& collision_object = scene.compiled_scene.collision_objects[it->second];
        collision_object.world_translation = collision_input.world_translation;
        collision_object.world_rotation = collision_input.world_rotation;
        collision_object.linear_velocity = collision_input.linear_velocity;
    }
}

void ApplyCollisionResponse(
    const RuntimeModule::SceneState& scene,
    const CompiledSpringBone& chain,
    const CompiledSpringJoint& joint,
    JointState& joint_state
)
{
    Vec3 point = ToAnglePoint(joint_state);
    const float point_scale = std::max(0.05f, joint.length);

    for (const std::string& binding_id : chain.collision_binding_ids) {
        auto binding_it = scene.collision_binding_lookup.find(binding_id);
        if (binding_it == scene.collision_binding_lookup.end()) {
            continue;
        }

        for (std::size_t collision_index : binding_it->second) {
            const CompiledCollisionObject& collision_object = scene.compiled_scene.collision_objects[collision_index];
            const Vec3 collider_center{
                collision_object.world_translation.x * 0.35f,
                collision_object.world_translation.z * 0.35f,
                0.0f,
            };

            Vec3 closest = collider_center;
            if (collision_object.shape_type == "CAPSULE") {
                const float half_height = std::max(0.0f, collision_object.height) * 0.2f;
                const Vec3 a{collider_center.x, collider_center.y - half_height, 0.0f};
                const Vec3 b{collider_center.x, collider_center.y + half_height, 0.0f};
                closest = ClosestPointOnSegment(point, a, b);
            }

            const Vec3 delta = Sub(point, closest);
            const float distance = Length(delta);
            const float influence_radius = std::max(0.01f, collision_object.radius / point_scale);
            if (distance >= influence_radius) {
                continue;
            }

            Vec3 normal = distance <= 1.0e-6f ? Vec3{0.0f, 1.0f, 0.0f} : Scale(delta, 1.0f / distance);
            const float penetration = influence_radius - distance;
            const float push_scale = 1.0f - (kMc2BoneSpringCollisionFriction * 0.3f);
            point = Add(point, Scale(normal, penetration * push_scale));
        }
    }

    FromAnglePoint(point, joint_state);
}

void StepChain(
    RuntimeModule::SceneState& scene,
    const CompiledSpringBone& chain,
    ChainCache& cache
)
{
    if (cache.joint_states.empty()) {
        return;
    }

    const RuntimeChainInput* runtime_input = FindRuntimeChainInput(scene.runtime_inputs, chain.component_id);

    for (const std::vector<int>& path : cache.baseline_paths) {
        if (path.empty()) {
            continue;
        }

        JointState& root_state = cache.joint_states[static_cast<std::size_t>(path.front())];
        root_state = JointState{};
    }

    const Vec3 center_offset = runtime_input
        ? Sub(runtime_input->center_translation, runtime_input->root_translation)
        : Vec3{};
    const Vec3 center_velocity = runtime_input
        ? Sub(runtime_input->center_linear_velocity, runtime_input->root_linear_velocity)
        : Vec3{};
    const Vec3 gravity_direction = Normalize(chain.gravity_direction);
    (void)gravity_direction;

    for (const std::vector<int>& path : cache.baseline_paths) {
        if (path.size() < 2) {
            continue;
        }

        const float path_depth = static_cast<float>(std::max<std::size_t>(1, path.size() - 1));
        for (std::size_t path_index = 1; path_index < path.size(); ++path_index) {
            const int joint_index = path[path_index];
            const CompiledSpringJoint& joint = chain.joints[static_cast<std::size_t>(joint_index)];
            JointState& state = cache.joint_states[static_cast<std::size_t>(joint_index)];
            const JointState& parent_state = cache.joint_states[static_cast<std::size_t>(joint.parent_index)];

            const float stiffness = Clamp01(joint.stiffness > 0.0f ? joint.stiffness : chain.stiffness);
            const float damping = Clamp01(joint.damping > 0.0f ? joint.damping : chain.damping);
            const float drag = Clamp01(joint.drag > 0.0f ? joint.drag : chain.drag);
            const float depth_ratio = static_cast<float>(path_index) / path_depth;

            const float inherit = kMc2BoneSpringTetherCompressionLimit * (0.55f + stiffness * 0.25f);
            const float center_offset_scale = 0.16f * (1.0f - depth_ratio * 0.35f);
            const float inertia_scale = 0.014f * (1.0f - depth_ratio * 0.45f);

            const float target_pitch =
                parent_state.pitch * inherit
                + center_offset.y * center_offset_scale
                + center_velocity.y * inertia_scale;
            const float target_roll =
                parent_state.roll * inherit
                + center_offset.x * center_offset_scale
                + center_velocity.x * inertia_scale;

            const float spring_gain =
                (0.45f + stiffness * 0.55f)
                * kMc2BoneSpringDistanceStiffness
                * (0.85f + scene.time_state.simulation_power[1] * 0.35f);
            const float damping_gain = 0.25f + damping * 0.65f;
            const float drag_factor = std::max(
                0.0f,
                1.0f - ((drag * 0.5f) + (damping * 0.25f)) * scene.time_state.simulation_delta_time * 18.0f
            );

            state.pitch_velocity = Clamp(
                (state.pitch_velocity + (target_pitch - state.pitch) * spring_gain) * drag_factor,
                -8.0f,
                8.0f
            );
            state.roll_velocity = Clamp(
                (state.roll_velocity + (target_roll - state.roll) * spring_gain) * drag_factor,
                -8.0f,
                8.0f
            );

            state.pitch = Clamp(
                state.pitch + state.pitch_velocity * scene.time_state.simulation_delta_time - state.pitch * damping_gain * 0.02f,
                -1.2f,
                1.2f
            );
            state.roll = Clamp(
                state.roll + state.roll_velocity * scene.time_state.simulation_delta_time - state.roll * damping_gain * 0.02f,
                -1.2f,
                1.2f
            );
        }
    }

    for (const CompiledSpringLine& line : cache.line_indices) {
        if (line.start_index < 0 || line.end_index < 0) {
            continue;
        }

        const JointState& parent = cache.joint_states[static_cast<std::size_t>(line.start_index)];
        JointState& child = cache.joint_states[static_cast<std::size_t>(line.end_index)];
        const float relax = 0.08f + kMc2BoneSpringDistanceStiffness * 0.2f;
        child.pitch = Clamp(child.pitch * (1.0f - relax) + parent.pitch * relax, -1.2f, 1.2f);
        child.roll = Clamp(child.roll * (1.0f - relax) + parent.roll * relax, -1.2f, 1.2f);
    }

    for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
        const CompiledSpringJoint& joint = chain.joints[joint_index];
        if (joint.parent_index < 0) {
            continue;
        }
        ApplyCollisionResponse(scene, chain, joint, cache.joint_states[joint_index]);
    }
}

}  // namespace

BuildSceneResult RuntimeModule::BuildScene(CompiledScene compiled_scene)
{
    const SceneHandle handle = next_handle_++;

    auto scene = std::make_unique<SceneState>();
    scene->compiled_scene = std::move(compiled_scene);
    scene->time_state.simulation_frequency = kMc2DefaultSimulationFrequency;
    scene->time_state.max_simulation_count_per_frame = kMc2DefaultMaxSimulationCountPerFrame;
    scene->time_state.FrameUpdate(scene->time_state.simulation_frequency);
    BuildSceneCaches(*scene);

    BuildSceneResult result;
    result.handle = handle;
    result.summary = scene->compiled_scene.Summary();
    result.backend = "native_mc2_bootstrap";
    result.build_message = "HoCloth native MC2 bootstrap active.";

    scenes_[handle] = std::move(scene);
    return result;
}

SceneHandle RuntimeModule::DestroyScene(SceneHandle handle)
{
    scenes_.erase(handle);
    return handle;
}

bool RuntimeModule::ResetScene(SceneHandle handle)
{
    SceneState& scene = RequireScene(handle);
    scene.runtime_inputs = RuntimeInputs{};
    scene.steps = 0;
    scene.time_state.simulation_frequency = kMc2DefaultSimulationFrequency;
    scene.time_state.max_simulation_count_per_frame = kMc2DefaultMaxSimulationCountPerFrame;
    scene.time_state.FrameUpdate(scene.time_state.simulation_frequency);
    BuildSceneCaches(scene);
    return true;
}

bool RuntimeModule::SetRuntimeInputs(SceneHandle handle, RuntimeInputs runtime_inputs)
{
    SceneState& scene = RequireScene(handle);
    scene.runtime_inputs = std::move(runtime_inputs);
    ApplyCollisionObjectInputs(scene);
    return true;
}

StepSceneResult RuntimeModule::StepScene(SceneHandle handle, float dt, int simulation_frequency)
{
    SceneState& scene = RequireScene(handle);
    scene.time_state.FrameUpdate(simulation_frequency);

    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        StepChain(scene, scene.compiled_scene.spring_bones[chain_index], scene.chain_caches[chain_index]);
    }

    scene.steps += 1;

    StepSceneResult result;
    result.handle = handle;
    result.dt = dt;
    result.simulation_frequency = scene.time_state.simulation_frequency;
    result.executed_steps = 1;
    result.steps = scene.steps;
    result.summary = scene.compiled_scene.Summary();
    return result;
}

std::vector<BoneTransform> RuntimeModule::GetBoneTransforms(SceneHandle handle) const
{
    const SceneState& scene = RequireScene(handle);
    std::vector<BoneTransform> transforms;

    for (std::size_t chain_index = 0; chain_index < scene.compiled_scene.spring_bones.size(); ++chain_index) {
        const CompiledSpringBone& chain = scene.compiled_scene.spring_bones[chain_index];
        const ChainCache& cache = scene.chain_caches[chain_index];
        for (std::size_t joint_index = 0; joint_index < chain.joints.size(); ++joint_index) {
            const CompiledSpringJoint& joint = chain.joints[joint_index];
            if (joint.parent_index < 0 || EndsWith(joint.name, kTailTipSuffix)) {
                continue;
            }

            const JointState& state = cache.joint_states[joint_index];
            transforms.push_back(BoneTransform{
                chain.component_id,
                chain.armature_name,
                joint.name,
                Vec3{0.0f, 0.0f, 0.0f},
                QuaternionFromPitchRoll(state.pitch, state.roll),
            });
        }
    }

    return transforms;
}

RuntimeModule::SceneState& RuntimeModule::RequireScene(SceneHandle handle)
{
    auto it = scenes_.find(handle);
    if (it == scenes_.end()) {
        throw std::runtime_error("Unknown runtime handle.");
    }
    return *it->second;
}

const RuntimeModule::SceneState& RuntimeModule::RequireScene(SceneHandle handle) const
{
    auto it = scenes_.find(handle);
    if (it == scenes_.end()) {
        throw std::runtime_error("Unknown runtime handle.");
    }
    return *it->second;
}

RuntimeModule& GetRuntimeModule()
{
    static RuntimeModule module;
    return module;
}

}  // namespace hocloth
