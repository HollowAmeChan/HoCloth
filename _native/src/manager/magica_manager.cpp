#include "hocloth/manager/magica_manager.hpp"

namespace hocloth::mc2 {

Result MagicaManager::Initialize()
{
    time_manager_.Initialize();
    team_manager_.Initialize();
    transform_manager_.Initialize();
    simulation_manager_.Initialize();
    collider_manager_.Initialize();
    wind_manager_.Initialize();
    cloth_manager_.Initialize();
    virtual_mesh_manager_.Initialize();
    initialized_ = true;
    return Result::Ok();
}

void MagicaManager::Dispose()
{
    virtual_mesh_manager_.Dispose();
    cloth_manager_.Dispose();
    wind_manager_.Dispose();
    collider_manager_.Dispose();
    simulation_manager_.Dispose();
    transform_manager_.Dispose();
    team_manager_.Dispose();
    time_manager_.Dispose();
    initialized_ = false;
}

bool MagicaManager::Initialized() const
{
    return initialized_;
}

std::vector<ManagerStatus> MagicaManager::Statuses() const
{
    return {
        time_manager_.Status(),
        team_manager_.Status(),
        transform_manager_.Status(),
        simulation_manager_.Status(),
        collider_manager_.Status(),
        wind_manager_.Status(),
        cloth_manager_.Status(),
        virtual_mesh_manager_.Status(),
    };
}

TimeManager& MagicaManager::Time()
{
    return time_manager_;
}

TeamManager& MagicaManager::Team()
{
    return team_manager_;
}

TransformManager& MagicaManager::Transform()
{
    return transform_manager_;
}

SimulationManager& MagicaManager::Simulation()
{
    return simulation_manager_;
}

ColliderManager& MagicaManager::Collider()
{
    return collider_manager_;
}

WindManager& MagicaManager::Wind()
{
    return wind_manager_;
}

ClothManager& MagicaManager::Cloth()
{
    return cloth_manager_;
}

VirtualMeshManager& MagicaManager::VirtualMesh()
{
    return virtual_mesh_manager_;
}

}  // namespace hocloth::mc2
