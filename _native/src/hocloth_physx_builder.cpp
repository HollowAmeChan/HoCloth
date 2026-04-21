#include "hocloth_physx_builder.hpp"

#if defined(HOCLOTH_WITH_PHYSX)
#include "PxPhysicsAPI.h"
#endif

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hocloth {

#if defined(HOCLOTH_WITH_PHYSX)
namespace {

physx::PxDefaultAllocator g_allocator;
physx::PxDefaultErrorCallback g_error_callback;
physx::PxFoundation* g_foundation = nullptr;
physx::PxPhysics* g_physics = nullptr;
physx::PxPvd* g_pvd = nullptr;
physx::PxPvdTransport* g_transport = nullptr;

struct PhysxBoneNode {
    std::string component_id;
    std::string armature_name;
    std::string bone_name;
    std::int32_t parent_index = -1;
    float length = 0.0f;
    physx::PxVec3 rest_head = physx::PxVec3(0.0f);
    physx::PxVec3 rest_tail = physx::PxVec3(0.0f);
    physx::PxQuat rest_local_rotation = physx::PxQuat(physx::PxIdentity);
    physx::PxQuat rest_global_rotation = physx::PxQuat(physx::PxIdentity);
    physx::PxRigidDynamic* tail_actor = nullptr;
    physx::PxDistanceJoint* joint = nullptr;
};

struct PhysxChainResources {
    std::string component_id;
    std::string armature_name;
    std::string root_bone_name;
    physx::PxRigidDynamic* root_anchor = nullptr;
    physx::PxVec3 root_rest_head = physx::PxVec3(0.0f);
    physx::PxQuat root_input_rotation = physx::PxQuat(physx::PxIdentity);
    std::vector<PhysxBoneNode> bones;
};

struct PhysxSceneResources {
    physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
    physx::PxScene* scene = nullptr;
    physx::PxMaterial* material = nullptr;
    std::vector<physx::PxRigidStatic*> colliders;
    std::vector<PhysxChainResources> chains;
    RuntimeInputs last_inputs;
};

std::unordered_map<SceneHandle, PhysxSceneResources> g_physx_scenes;

physx::PxQuat make_quat(const float value[4]) {
    return physx::PxQuat(value[1], value[2], value[3], value[0]);
}

physx::PxVec3 make_vec3(const float value[3]) {
    return physx::PxVec3(value[0], value[1], value[2]);
}

float safe_length(const physx::PxVec3& value) {
    return value.magnitude();
}

physx::PxVec3 normalize_or_default(const physx::PxVec3& value, const physx::PxVec3& fallback) {
    const float length = safe_length(value);
    if (length <= 1.0e-5f) {
        return fallback;
    }
    return value / length;
}

physx::PxQuat normalize_or_identity(const physx::PxQuat& value) {
    physx::PxQuat result = value;
    const float magnitude = std::sqrt(
        result.x * result.x +
        result.y * result.y +
        result.z * result.z +
        result.w * result.w
    );
    if (magnitude <= 1.0e-6f) {
        return physx::PxQuat(physx::PxIdentity);
    }
    const float inv = 1.0f / magnitude;
    result.x *= inv;
    result.y *= inv;
    result.z *= inv;
    result.w *= inv;
    return result;
}

physx::PxQuat quat_between_vectors(const physx::PxVec3& from, const physx::PxVec3& to) {
    const physx::PxVec3 normalized_from = normalize_or_default(from, physx::PxVec3(0.0f, 1.0f, 0.0f));
    const physx::PxVec3 normalized_to = normalize_or_default(to, physx::PxVec3(0.0f, 1.0f, 0.0f));
    const float dot = normalized_from.dot(normalized_to);

    if (dot >= 1.0f - 1.0e-5f) {
        return physx::PxQuat(physx::PxIdentity);
    }

    if (dot <= -1.0f + 1.0e-5f) {
        physx::PxVec3 axis = normalized_from.cross(physx::PxVec3(1.0f, 0.0f, 0.0f));
        if (axis.magnitudeSquared() <= 1.0e-6f) {
            axis = normalized_from.cross(physx::PxVec3(0.0f, 0.0f, 1.0f));
        }
        axis = normalize_or_default(axis, physx::PxVec3(0.0f, 1.0f, 0.0f));
        return physx::PxQuat(physx::PxPi, axis);
    }

    const physx::PxVec3 axis = normalized_from.cross(normalized_to);
    physx::PxQuat result(axis.x, axis.y, axis.z, 1.0f + dot);
    return normalize_or_identity(result);
}

void quat_to_array(const physx::PxQuat& value, float out[4]) {
    const physx::PxQuat normalized = normalize_or_identity(value);
    out[0] = normalized.w;
    out[1] = normalized.x;
    out[2] = normalized.y;
    out[3] = normalized.z;
}

void release_scene_resources(PhysxSceneResources& resources) {
    for (PhysxChainResources& chain : resources.chains) {
        for (PhysxBoneNode& bone : chain.bones) {
            if (bone.joint != nullptr) {
                bone.joint->release();
                bone.joint = nullptr;
            }
            if (bone.tail_actor != nullptr) {
                bone.tail_actor->release();
                bone.tail_actor = nullptr;
            }
        }
        if (chain.root_anchor != nullptr) {
            chain.root_anchor->release();
            chain.root_anchor = nullptr;
        }
    }
    resources.chains.clear();

    for (physx::PxRigidStatic* actor : resources.colliders) {
        if (actor != nullptr) {
            actor->release();
        }
    }
    resources.colliders.clear();

    if (resources.scene != nullptr) {
        resources.scene->release();
        resources.scene = nullptr;
    }
    if (resources.material != nullptr) {
        resources.material->release();
        resources.material = nullptr;
    }
    if (resources.dispatcher != nullptr) {
        resources.dispatcher->release();
        resources.dispatcher = nullptr;
    }
}

physx::PxRigidDynamic* create_anchor_actor(
    PhysxSceneResources& resources,
    const physx::PxVec3& position
) {
    physx::PxRigidDynamic* actor = g_physics->createRigidDynamic(
        physx::PxTransform(position, physx::PxQuat(physx::PxIdentity))
    );
    if (actor == nullptr) {
        return nullptr;
    }

    physx::PxShape* shape = g_physics->createShape(
        physx::PxSphereGeometry(0.01f),
        *resources.material,
        true
    );
    if (shape == nullptr) {
        actor->release();
        return nullptr;
    }

    actor->attachShape(*shape);
    shape->release();
    actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
    actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
    resources.scene->addActor(*actor);
    return actor;
}

physx::PxRigidDynamic* create_tail_actor(
    PhysxSceneResources& resources,
    const BoneChainDescriptor& chain,
    const PhysxBoneNode& bone
) {
    physx::PxRigidDynamic* actor = g_physics->createRigidDynamic(
        physx::PxTransform(bone.rest_tail, physx::PxQuat(physx::PxIdentity))
    );
    if (actor == nullptr) {
        return nullptr;
    }

    const float radius = std::max(0.02f, bone.length * 0.12f);
    physx::PxShape* shape = g_physics->createShape(
        physx::PxSphereGeometry(radius),
        *resources.material,
        true
    );
    if (shape == nullptr) {
        actor->release();
        return nullptr;
    }

    actor->attachShape(*shape);
    shape->release();
    actor->setLinearDamping(0.1f + std::clamp(chain.drag, 0.0f, 1.0f) * 2.5f);
    actor->setAngularDamping(0.2f + std::clamp(chain.damping, 0.0f, 1.0f) * 4.0f);
    actor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true);
    physx::PxRigidBodyExt::updateMassAndInertia(*actor, 0.2f + radius);
    resources.scene->addActor(*actor);
    return actor;
}

PhysxChainResources build_chain_resources(
    PhysxSceneResources& resources,
    const BoneChainDescriptor& chain
) {
    PhysxChainResources chain_resources;
    chain_resources.component_id = chain.component_id;
    chain_resources.armature_name = chain.armature_name;
    chain_resources.root_bone_name = chain.root_bone_name;

    if (chain.bones.empty()) {
        return chain_resources;
    }

    chain_resources.root_rest_head = make_vec3(chain.bones.front().rest_head_local);
    chain_resources.root_anchor = create_anchor_actor(resources, chain_resources.root_rest_head);
    if (chain_resources.root_anchor == nullptr) {
        return chain_resources;
    }

    chain_resources.bones.reserve(chain.bones.size());
    for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
        const BoneDescriptor& source_bone = chain.bones[bone_index];
        PhysxBoneNode bone;
        bone.component_id = chain.component_id;
        bone.armature_name = chain.armature_name;
        bone.bone_name = source_bone.name;
        bone.parent_index = source_bone.parent_index;
        bone.length = std::max(0.01f, source_bone.length);
        bone.rest_head = make_vec3(source_bone.rest_head_local);
        bone.rest_tail = make_vec3(source_bone.rest_tail_local);
        bone.rest_local_rotation = normalize_or_identity(make_quat(source_bone.rest_local_rotation));
        if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_resources.bones.size()) {
            bone.rest_global_rotation = normalize_or_identity(
                chain_resources.bones[static_cast<std::size_t>(bone.parent_index)].rest_global_rotation * bone.rest_local_rotation
            );
        } else {
            bone.rest_global_rotation = bone.rest_local_rotation;
        }

        bone.tail_actor = create_tail_actor(resources, chain, bone);
        if (bone.tail_actor == nullptr) {
            continue;
        }

        physx::PxRigidActor* parent_actor = nullptr;
        physx::PxVec3 parent_anchor(0.0f);
        if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < chain_resources.bones.size()) {
            const PhysxBoneNode& parent_bone = chain_resources.bones[static_cast<std::size_t>(bone.parent_index)];
            parent_actor = parent_bone.tail_actor;
            parent_anchor = bone.rest_head - parent_bone.rest_tail;
        } else {
            parent_actor = chain_resources.root_anchor;
        }

        const physx::PxVec3 child_anchor = bone.rest_head - bone.rest_tail;
        bone.joint = physx::PxDistanceJointCreate(
            *g_physics,
            parent_actor,
            physx::PxTransform(parent_anchor),
            bone.tail_actor,
            physx::PxTransform(child_anchor)
        );
        if (bone.joint != nullptr) {
            bone.joint->setMinDistance(bone.length);
            bone.joint->setMaxDistance(bone.length);
            bone.joint->setStiffness(20.0f + std::clamp(chain.stiffness, 0.0f, 1.0f) * 80.0f);
            bone.joint->setDamping(2.0f + std::clamp(chain.damping, 0.0f, 1.0f) * 18.0f);
            bone.joint->setTolerance(0.01f);
            bone.joint->setDistanceJointFlag(physx::PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, true);
            bone.joint->setDistanceJointFlag(physx::PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, true);
            bone.joint->setDistanceJointFlag(physx::PxDistanceJointFlag::eSPRING_ENABLED, true);
        }

        chain_resources.bones.push_back(std::move(bone));
    }

    return chain_resources;
}

const BoneChainRuntimeInput* find_chain_input(
    const RuntimeInputs& inputs,
    const PhysxChainResources& chain
) {
    for (const BoneChainRuntimeInput& chain_input : inputs.bone_chains) {
        if (chain_input.component_id == chain.component_id) {
            return &chain_input;
        }
    }
    return nullptr;
}

void reset_chain_actors(
    PhysxChainResources& chain,
    const RuntimeInputs& inputs
) {
    const BoneChainRuntimeInput* chain_input = find_chain_input(inputs, chain);
    physx::PxVec3 root_head = chain.root_rest_head;
    if (chain_input != nullptr) {
        root_head = make_vec3(chain_input->root_translation);
        chain.root_input_rotation = normalize_or_identity(make_quat(chain_input->root_rotation_quaternion));
    } else {
        chain.root_input_rotation = physx::PxQuat(physx::PxIdentity);
    }

    if (chain.root_anchor != nullptr) {
        const physx::PxTransform anchor_pose(root_head, physx::PxQuat(physx::PxIdentity));
        chain.root_anchor->setKinematicTarget(anchor_pose);
        chain.root_anchor->setGlobalPose(anchor_pose);
    }

    for (PhysxBoneNode& bone : chain.bones) {
        const physx::PxVec3 rest_offset = bone.rest_tail - chain.root_rest_head;
        const physx::PxTransform actor_pose(root_head + rest_offset, physx::PxQuat(physx::PxIdentity));
        if (bone.tail_actor != nullptr) {
            bone.tail_actor->setLinearVelocity(physx::PxVec3(0.0f));
            bone.tail_actor->setAngularVelocity(physx::PxVec3(0.0f));
            bone.tail_actor->setGlobalPose(actor_pose);
        }
    }
}

std::vector<BoneTransform> build_transforms_from_scene(const PhysxSceneResources& resources) {
    std::vector<BoneTransform> transforms;

    for (const PhysxChainResources& chain : resources.chains) {
        std::vector<physx::PxQuat> current_global_rotations(chain.bones.size(), physx::PxQuat(physx::PxIdentity));
        std::vector<physx::PxVec3> current_tail_positions(chain.bones.size(), physx::PxVec3(0.0f));

        for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
            const PhysxBoneNode& bone = chain.bones[bone_index];
            if (bone.tail_actor != nullptr) {
                current_tail_positions[bone_index] = bone.tail_actor->getGlobalPose().p;
            } else {
                current_tail_positions[bone_index] = bone.rest_tail;
            }
        }

        for (std::size_t bone_index = 0; bone_index < chain.bones.size(); ++bone_index) {
            const PhysxBoneNode& bone = chain.bones[bone_index];
            physx::PxVec3 current_head = chain.root_anchor != nullptr ? chain.root_anchor->getGlobalPose().p : chain.root_rest_head;
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < current_tail_positions.size()) {
                current_head = current_tail_positions[static_cast<std::size_t>(bone.parent_index)];
            }

            const physx::PxVec3 current_tail = current_tail_positions[bone_index];
            const physx::PxVec3 rest_direction = bone.rest_tail - bone.rest_head;
            const physx::PxVec3 current_direction = current_tail - current_head;
            const physx::PxQuat swing = quat_between_vectors(rest_direction, current_direction);
            const physx::PxQuat current_global_rotation = normalize_or_identity(swing * bone.rest_global_rotation);
            current_global_rotations[bone_index] = current_global_rotation;

            physx::PxQuat delta = physx::PxQuat(physx::PxIdentity);
            if (bone.parent_index >= 0 && static_cast<std::size_t>(bone.parent_index) < current_global_rotations.size()) {
                const physx::PxQuat parent_current = current_global_rotations[static_cast<std::size_t>(bone.parent_index)];
                const physx::PxQuat current_local = normalize_or_identity(parent_current.getConjugate() * current_global_rotation);
                delta = normalize_or_identity(current_local * bone.rest_local_rotation.getConjugate());
            } else {
                delta = normalize_or_identity(current_global_rotation * bone.rest_local_rotation.getConjugate());
            }

            BoneTransform transform;
            transform.component_id = bone.component_id;
            transform.armature_name = bone.armature_name;
            transform.bone_name = bone.bone_name;
            quat_to_array(delta, transform.rotation_quaternion);
            transforms.push_back(std::move(transform));
        }
    }

    return transforms;
}

}  // namespace
#endif

PhysxBuildResult probe_physx_backend() {
    PhysxBuildResult result;
#if defined(HOCLOTH_WITH_PHYSX)
    if (g_physics != nullptr) {
        result.available = true;
        result.backend = "physx";
        result.message = "PhysX already initialized";
        return result;
    }

    g_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_allocator, g_error_callback);
    if (g_foundation == nullptr) {
        result.available = false;
        result.backend = "stub";
        result.message = "PxCreateFoundation failed";
        return result;
    }

    g_pvd = physx::PxCreatePvd(*g_foundation);
    if (g_pvd != nullptr) {
        g_transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
        if (g_transport != nullptr) {
            g_pvd->connect(*g_transport, physx::PxPvdInstrumentationFlag::eALL);
        }
    }

    const physx::PxTolerancesScale scale;
    g_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_foundation, scale, true, g_pvd);
    if (g_physics == nullptr) {
        if (g_transport != nullptr) {
            g_transport->release();
            g_transport = nullptr;
        }
        if (g_pvd != nullptr) {
            g_pvd->release();
            g_pvd = nullptr;
        }
        g_foundation->release();
        g_foundation = nullptr;
        result.available = false;
        result.backend = "stub";
        result.message = "PxCreatePhysics failed";
        return result;
    }

    result.available = true;
    result.backend = "physx";
    result.message = "PhysX initialized";
#else
    result.available = false;
    result.backend = "stub";
    result.message = "PhysX compile path disabled";
#endif
    return result;
}

PhysxBuildResult build_physx_scene(SceneHandle handle, const SceneDescriptor& scene) {
    PhysxBuildResult result = probe_physx_backend();
    result.collider_count = static_cast<std::uint64_t>(scene.colliders.size());

#if defined(HOCLOTH_WITH_PHYSX)
    if (!result.available || g_physics == nullptr || g_foundation == nullptr) {
        result.scene_created = false;
        if (result.message.empty()) {
            result.message = "PhysX runtime unavailable during scene build";
        }
        return result;
    }

    auto existing = g_physx_scenes.find(handle);
    if (existing != g_physx_scenes.end()) {
        release_scene_resources(existing->second);
        g_physx_scenes.erase(existing);
    }

    PhysxSceneResources resources;
    physx::PxSceneDesc scene_desc(g_physics->getTolerancesScale());
    scene_desc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
    resources.dispatcher = physx::PxDefaultCpuDispatcherCreate(1);
    if (resources.dispatcher == nullptr) {
        result.scene_created = false;
        result.message = "PxDefaultCpuDispatcherCreate failed";
        return result;
    }

    scene_desc.cpuDispatcher = resources.dispatcher;
    scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;
    resources.scene = g_physics->createScene(scene_desc);
    if (resources.scene == nullptr) {
        result.scene_created = false;
        result.message = "PxPhysics::createScene failed";
        release_scene_resources(resources);
        return result;
    }

    resources.material = g_physics->createMaterial(0.5f, 0.5f, 0.1f);
    if (resources.material == nullptr) {
        result.scene_created = false;
        result.message = "PxPhysics::createMaterial failed";
        release_scene_resources(resources);
        return result;
    }

    for (const ColliderDescriptor& collider : scene.colliders) {
        const physx::PxTransform transform(make_vec3(collider.world_translation), make_quat(collider.world_rotation));
        physx::PxRigidStatic* actor = g_physics->createRigidStatic(transform);
        if (actor == nullptr) {
            continue;
        }

        physx::PxShape* shape = nullptr;
        if (collider.shape_type == "SPHERE") {
            shape = g_physics->createShape(physx::PxSphereGeometry(std::max(0.01f, collider.radius)), *resources.material, true);
        } else {
            shape = g_physics->createShape(
                physx::PxCapsuleGeometry(std::max(0.01f, collider.radius), std::max(0.01f, collider.height * 0.5f)),
                *resources.material,
                true
            );
        }

        if (shape == nullptr) {
            actor->release();
            continue;
        }

        actor->attachShape(*shape);
        shape->release();
        resources.scene->addActor(*actor);
        resources.colliders.push_back(actor);
    }

    resources.chains.reserve(scene.bone_chains.size());
    for (const BoneChainDescriptor& chain : scene.bone_chains) {
        resources.chains.push_back(build_chain_resources(resources, chain));
    }
    for (PhysxChainResources& chain : resources.chains) {
        reset_chain_actors(chain, resources.last_inputs);
    }

    std::uint64_t dynamic_bone_count = 0;
    for (const PhysxChainResources& chain : resources.chains) {
        dynamic_bone_count += static_cast<std::uint64_t>(chain.bones.size());
    }

    g_physx_scenes.emplace(handle, std::move(resources));
    result.scene_created = true;
    result.backend = "physx";
    result.message = "PhysX scene created with " + std::to_string(dynamic_bone_count) + " runtime bones";
#else
    (void) handle;
    (void) scene;
    result.scene_created = false;
    if (result.message.empty()) {
        result.message = "PhysX compile path disabled";
    }
#endif

    return result;
}

void reset_physx_scene(SceneHandle handle) {
#if defined(HOCLOTH_WITH_PHYSX)
    auto it = g_physx_scenes.find(handle);
    if (it == g_physx_scenes.end()) {
        return;
    }
    for (PhysxChainResources& chain : it->second.chains) {
        reset_chain_actors(chain, it->second.last_inputs);
    }
#else
    (void) handle;
#endif
}

void set_physx_runtime_inputs(SceneHandle handle, const RuntimeInputs& inputs) {
#if defined(HOCLOTH_WITH_PHYSX)
    auto it = g_physx_scenes.find(handle);
    if (it == g_physx_scenes.end()) {
        return;
    }
    it->second.last_inputs = inputs;
    for (PhysxChainResources& chain : it->second.chains) {
        const BoneChainRuntimeInput* chain_input = find_chain_input(inputs, chain);
        if (chain_input == nullptr || chain.root_anchor == nullptr) {
            continue;
        }
        const physx::PxTransform anchor_pose(
            make_vec3(chain_input->root_translation),
            physx::PxQuat(physx::PxIdentity)
        );
        chain.root_input_rotation = normalize_or_identity(make_quat(chain_input->root_rotation_quaternion));
        chain.root_anchor->setKinematicTarget(anchor_pose);
    }
#else
    (void) handle;
    (void) inputs;
#endif
}

void step_physx_scene(SceneHandle handle, const RuntimeInputs& inputs) {
#if defined(HOCLOTH_WITH_PHYSX)
    auto it = g_physx_scenes.find(handle);
    if (it == g_physx_scenes.end() || it->second.scene == nullptr) {
        return;
    }

    set_physx_runtime_inputs(handle, inputs);

    const std::int32_t substeps = std::max<std::int32_t>(1, inputs.substeps);
    const float dt = inputs.dt > 0.0f ? inputs.dt : (1.0f / 60.0f);
    const float substep_dt = dt / static_cast<float>(substeps);

    for (std::int32_t substep = 0; substep < substeps; ++substep) {
        it->second.scene->simulate(substep_dt);
        it->second.scene->fetchResults(true);
    }
#else
    (void) handle;
    (void) inputs;
#endif
}

std::vector<BoneTransform> get_physx_bone_transforms(SceneHandle handle) {
#if defined(HOCLOTH_WITH_PHYSX)
    auto it = g_physx_scenes.find(handle);
    if (it == g_physx_scenes.end()) {
        return {};
    }
    return build_transforms_from_scene(it->second);
#else
    (void) handle;
    return {};
#endif
}

void destroy_physx_scene(SceneHandle handle) {
#if defined(HOCLOTH_WITH_PHYSX)
    auto it = g_physx_scenes.find(handle);
    if (it == g_physx_scenes.end()) {
        return;
    }
    release_scene_resources(it->second);
    g_physx_scenes.erase(it);
#else
    (void) handle;
#endif
}

}  // namespace hocloth
