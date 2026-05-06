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

int MagicaManager::StepFrame(
    float frame_delta_time,
    float fixed_delta_time,
    float unscaled_delta_time,
    float global_time_scale,
    int simulation_frequency
)
{
    if (!initialized_) {
        Initialize();
    }

    time_manager_.FrameUpdate(simulation_frequency);
    wind_manager_.AlwaysWindUpdate();
    const float simulation_delta_time = time_manager_.SimulationDeltaTime();
    const float4 simulation_power = time_manager_.SimulationPower();
    const int max_update_count = team_manager_.AlwaysTeamUpdate(
        frame_delta_time,
        fixed_delta_time,
        unscaled_delta_time,
        global_time_scale,
        simulation_delta_time,
        time_manager_.MaxSimulationCountPerFrame()
    );

    if (max_update_count <= 0) {
        team_manager_.PostTeamUpdate();
        collider_manager_.PostSimulationUpdate(team_manager_);
        return 0;
    }

    team_manager_.UpdateCenterAndInertia(
        simulation_delta_time,
        transform_manager_,
        virtual_mesh_manager_,
        wind_manager_,
        cloth_manager_.Inertia().FixedArray()
    );
    simulation_manager_.PreSimulationUpdate(team_manager_, virtual_mesh_manager_);
    collider_manager_.PreSimulationUpdate(team_manager_, transform_manager_);
    for (int update_index = 0; update_index < max_update_count; ++update_index) {
        simulation_manager_.BeginSimulationStep();
        team_manager_.SimulationStepTeamUpdate(update_index, simulation_delta_time);
        simulation_manager_.CreateStepParticleList(team_manager_);
        collider_manager_.CreateUpdateColliderList(update_index, team_manager_, simulation_manager_);
        collider_manager_.StartSimulationStep(team_manager_, simulation_manager_);
        cloth_manager_.PrepareStepWorkBuffers(simulation_manager_);
        simulation_manager_.StartSimulationStep(
            simulation_power,
            simulation_delta_time,
            team_manager_,
            virtual_mesh_manager_
        );
        simulation_manager_.UpdateStepBasicPosture(team_manager_, virtual_mesh_manager_);
        cloth_manager_.SolveStepConstraints(
            update_index,
            simulation_power,
            team_manager_,
            virtual_mesh_manager_,
            collider_manager_,
            simulation_manager_
        );
        simulation_manager_.EndSimulationStepSolve(
            simulation_delta_time,
            team_manager_,
            virtual_mesh_manager_
        );
        collider_manager_.EndSimulationStep(simulation_manager_);
        simulation_manager_.EndSimulationStep();
    }

    simulation_manager_.CalcDisplayPosition(
        simulation_delta_time,
        team_manager_,
        virtual_mesh_manager_
    );
    collider_manager_.PostSimulationUpdate(team_manager_);
    team_manager_.PostTeamUpdate();
    return max_update_count;
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
