#include "hocloth_physx_builder.hpp"

#if defined(HOCLOTH_WITH_PHYSX)
#include "PxPhysicsAPI.h"
#endif

namespace hocloth {

#if defined(HOCLOTH_WITH_PHYSX)
namespace {

physx::PxDefaultAllocator g_allocator;
physx::PxDefaultErrorCallback g_error_callback;
physx::PxFoundation* g_foundation = nullptr;
physx::PxPhysics* g_physics = nullptr;
physx::PxPvd* g_pvd = nullptr;
physx::PxDefaultPvdSocketTransport* g_transport = nullptr;

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

}  // namespace hocloth
