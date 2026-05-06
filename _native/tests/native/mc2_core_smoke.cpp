#include "hocloth/manager/magica_manager.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void ExpectFinite(float actual, const char* label)
{
    if (!std::isfinite(actual)) {
        throw std::runtime_error(
            std::string(label)
            + " was not finite. actual="
            + std::to_string(actual)
        );
    }
}

void ExpectEqual(int actual, int expected, const char* label)
{
    if (actual != expected) {
        throw std::runtime_error(
            std::string(label)
            + " mismatch. actual="
            + std::to_string(actual)
            + " expected="
            + std::to_string(expected)
        );
    }
}

}  // namespace

int main()
{
    try {
        hocloth::mc2::MagicaManager manager;
        manager.Initialize();

        hocloth::mc2::ClothParameters cloth_parameters;
        cloth_parameters.distance_constraint = hocloth::mc2::DistanceConstraintParams::BoneSpringDefaults();
        cloth_parameters.inertia_constraint.local_inertia = 0.0f;
        cloth_parameters.inertia_constraint.depth_inertia = 0.25f;
        cloth_parameters.spring_constraint = hocloth::mc2::SpringConstraintParams::BoneSpringDefaults();
        cloth_parameters.radius_curve_data = hocloth::mc2::ConstantCurve(0.05f);
        cloth_parameters.damping_curve_data = hocloth::mc2::ConstantCurve(0.1f);

        const int team_id = manager.Team().CreateTeam(cloth_parameters, true, true);
        if (team_id < 0) {
            throw std::runtime_error("Team creation failed.");
        }

        auto chunks = manager.Simulation().RegisterParticleRange(team_id, 2);
        manager.Team().GetTeamData(team_id).particle_chunk = chunks.next_pos_chunk;

        manager.Simulation().NextPositions()[chunks.next_pos_chunk.start_index] =
            hocloth::mc2::float3{0.0f, 0.0f, 0.0f};
        manager.Simulation().NextPositions()[chunks.next_pos_chunk.start_index + 1] =
            hocloth::mc2::float3{1.0f, 0.0f, 0.0f};
        manager.Simulation().OldPositions()[chunks.old_pos_chunk.start_index] =
            hocloth::mc2::float3{0.0f, 0.0f, 0.0f};
        manager.Simulation().OldPositions()[chunks.old_pos_chunk.start_index + 1] =
            hocloth::mc2::float3{1.0f, 0.0f, 0.0f};
        manager.Simulation().BasePositions()[chunks.base_pos_chunk.start_index] =
            hocloth::mc2::float3{0.0f, 0.0f, 0.0f};
        manager.Simulation().BasePositions()[chunks.base_pos_chunk.start_index + 1] =
            hocloth::mc2::float3{1.0f, 0.0f, 0.0f};

        const int scheduled_update_count = manager.Team().AlwaysTeamUpdate(
            1.0f / 60.0f,
            1.0f / 60.0f,
            1.0f / 60.0f,
            1.0f,
            1.0f / 60.0f,
            1
        );

        ExpectEqual(scheduled_update_count, 1, "scheduled update count");
        ExpectEqual(manager.Simulation().ParticleCount(), 2, "particle count");
        ExpectFinite(manager.Simulation().NextPositions()[chunks.next_pos_chunk.start_index + 1].x, "p1.x");

        std::cout << "team_id=" << team_id
                  << " particles=" << manager.Simulation().ParticleCount()
                  << " scheduled=" << scheduled_update_count
                  << " p1.x=" << manager.Simulation().NextPositions()[chunks.next_pos_chunk.start_index + 1].x
                  << "\n";

        manager.Team().PostTeamUpdate();
        manager.Dispose();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "native smoke failed: " << exception.what() << "\n";
        return 1;
    }
}
