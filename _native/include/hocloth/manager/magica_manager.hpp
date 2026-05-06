#pragma once

#include "hocloth/manager/cloth/cloth_manager.hpp"
#include "hocloth/manager/manager_status.hpp"
#include "hocloth/manager/simulation/collider_manager.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/simulation/time_manager.hpp"
#include "hocloth/manager/simulation/wind_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/result_code/result_code.hpp"

#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/MagicaManager.cs
class MagicaManager final {
public:
    Result Initialize();
    void Dispose();

    [[nodiscard]] bool Initialized() const;
    [[nodiscard]] std::vector<ManagerStatus> Statuses() const;

    [[nodiscard]] TimeManager& Time();
    [[nodiscard]] TeamManager& Team();
    [[nodiscard]] TransformManager& Transform();
    [[nodiscard]] SimulationManager& Simulation();
    [[nodiscard]] ColliderManager& Collider();
    [[nodiscard]] WindManager& Wind();
    [[nodiscard]] ClothManager& Cloth();
    [[nodiscard]] VirtualMeshManager& VirtualMesh();

private:
    bool initialized_ = false;
    TimeManager time_manager_;
    TeamManager team_manager_;
    TransformManager transform_manager_;
    SimulationManager simulation_manager_;
    ColliderManager collider_manager_;
    WindManager wind_manager_;
    ClothManager cloth_manager_;
    VirtualMeshManager virtual_mesh_manager_;
};

}  // namespace hocloth::mc2
