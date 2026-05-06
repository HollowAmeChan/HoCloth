#include "hocloth/manager/magica_manager.hpp"

#include <iostream>

int main()
{
    hocloth::mc2::MagicaManager manager;
    manager.Initialize();

    const int team_id = manager.Team().CreateTeam(true, true);
    auto chunks = manager.Simulation().RegisterParticleRange(team_id, 4);
    manager.Team().GetTeamData(team_id).particle_chunk = chunks.next_pos_chunk;

    manager.Simulation().BeginSimulationStep();
    for (int index = 0; index < chunks.ParticleCount(); ++index) {
        manager.Simulation().MarkStepParticle(chunks.next_pos_chunk.start_index + index);
    }
    manager.Simulation().EndSimulationStep();

    std::cout << "team_id=" << team_id
              << " particles=" << manager.Simulation().ParticleCount()
              << " steps=" << manager.Simulation().SimulationStepCount()
              << "\n";

    manager.Dispose();
    return 0;
}
