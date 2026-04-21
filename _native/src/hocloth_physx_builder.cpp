#include "hocloth_physx_builder.hpp"

#if defined(HOCLOTH_WITH_PHYSX)
#include "PxPhysicsAPI.h"
#endif

#include <unordered_map>

namespace hocloth {

#if defined(HOCLOTH_WITH_PHYSX)
namespace {

physx::PxDefaultAllocator g_allocator;
physx::PxDefaultErrorCallback g_error_callback;
physx::PxFoundation* g_foundation = nullptr;
physx::PxPhysics* g_physics = nullptr;
physx::PxPvd* g_pvd = nullptr;
physx::PxDefaultPvdSocketTransport* g_transport = nullptr;

struct PhysxSceneResources {
    physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
    physx::PxScene* scene = nullptr;
    physx::PxMaterial* material = nullptr;
    std::vector<physx::PxRigidStatic*> colliders;
};

std::unordered_map<SceneHandle, PhysxSceneResources> g_physx_scenes;

physx::PxQuat make_quat(const float value[4]) {
    return physx::PxQuat(value[1], value[2], value[3], value[0]);
}

physx::PxVec3 make_vec3(const float value[3]) {
    return physx::PxVec3(value[0], value[1], value[2]);
}

void release_scene_resources(PhysxSceneResources& resources) {
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

    g_foundation = physx::PxCreateFoundation(PX_PHYSICS_VERSION, g_allocator, g_error_callback);
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

    physx::PxTolerancesScale scale;
    g_physics = physx::PxCreatePhysics(PX_PHYSICS_VERSION, *g_foundation, scale, true, g_pvd);
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
    scene_desc.gravity = physx::PxVec3(0.0f, 0.0f, 0.0f);
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
            shape = g_physics->createShape(
                physx::PxSphereGeometry(collider.radius),
                *resources.material,
                true
            );
        } else {
            const float half_height = collider.height * 0.5f;
            shape = g_physics->createShape(
                physx::PxCapsuleGeometry(collider.radius, half_height),
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

    g_physx_scenes.emplace(handle, std::move(resources));
    result.scene_created = true;
    result.backend = "physx";
    result.message = "PhysX scene created";
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
