#pragma once

#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

#include <cstdint>

namespace hocloth::mc2 {

class SimulationManager;
class VirtualMeshManager;

// Port target for Magica Cloth 2: Scripts/Core/Cloth/Constraints/SelfCollisionConstraint.cs
class SelfCollisionConstraint {
public:
    static constexpr std::uint32_t KindPoint = 0;
    static constexpr std::uint32_t KindEdge = 1;
    static constexpr std::uint32_t KindTriangle = 2;

    static constexpr std::uint32_t FlagKindMask = 0x03000000;
    static constexpr std::uint32_t FlagFix0 = 0x04000000;
    static constexpr std::uint32_t FlagFix1 = 0x08000000;
    static constexpr std::uint32_t FlagFix2 = 0x10000000;
    static constexpr std::uint32_t FlagAllFix = 0x20000000;
    static constexpr std::uint32_t FlagIgnore = 0x40000000;
    static constexpr std::uint32_t FlagEnable = 0x80000000;

    struct float3x3 {
        float3 c0{};
        float3 c1{};
        float3 c2{};

        [[nodiscard]] const float3& operator[](int index) const;
        [[nodiscard]] float3& operator[](int index);
    };

    struct Primitive {
        std::uint32_t flag_and_team_id = 0;
        int sort_index = -1;
        int3 particle_indices{-1, -1, -1};
        float3x3 next_position;
        float3x3 old_position;
        float3 inverse_mass{};
        float thickness = 0.0f;

        [[nodiscard]] bool IsIgnore() const;
        [[nodiscard]] bool HasParticle(int particle_index) const;
        [[nodiscard]] std::uint32_t Kind() const;
        [[nodiscard]] int TeamId() const;
        [[nodiscard]] float SolveThickness(const Primitive& other) const;
        [[nodiscard]] bool AnyParticle(const Primitive& other) const;
    };

    struct SortData {
        std::uint32_t flag_and_team_id = 0;
        int primitive_index = -1;
        float2 first_min_max{};
        float2 second_min_max{};
        float2 third_min_max{};

        [[nodiscard]] std::uint32_t Kind() const;
        [[nodiscard]] bool operator<(const SortData& other) const;
    };

    struct EdgeEdgeContact {
        std::uint32_t flag_and_team_id0 = 0;
        std::uint32_t flag_and_team_id1 = 0;
        float thickness = 0.0f;
        float s = 0.0f;
        float t = 0.0f;
        float3 normal{};
        float2 edge_inverse_mass0{};
        float2 edge_inverse_mass1{};
        int2 edge_particle_index0{-1, -1};
        int2 edge_particle_index1{-1, -1};
    };

    struct PointTriangleContact {
        std::uint32_t flag_and_team_id0 = 0;
        std::uint32_t flag_and_team_id1 = 0;
        float thickness = 0.0f;
        float sign = 0.0f;
        int point_particle_index = -1;
        int3 triangle_particle_index{-1, -1, -1};
        float point_inverse_mass = 0.0f;
        float3 triangle_inverse_mass{};
    };

    void Dispose();
    [[nodiscard]] int PointPrimitiveCount() const;
    [[nodiscard]] int EdgePrimitiveCount() const;
    [[nodiscard]] int TrianglePrimitiveCount() const;
    [[nodiscard]] int IntersectCount() const;
    [[nodiscard]] bool HasPrimitive() const;

    void Clear();
    void WorkBufferUpdate(int particle_count);
    void RegisterTeamPrimitives(
        int team_id,
        bool use_point_primitive,
        bool use_edge_primitive,
        bool use_triangle_primitive,
        TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager
    );
    void RemoveTeamPrimitives(int team_id, TeamManager& team_manager);
    void UpdatePrimitives(
        std::uint32_t kind,
        const TeamManager& team_manager,
        const VirtualMeshManager& virtual_mesh_manager,
        const SimulationManager& simulation_manager
    );
    void IntersectUpdatePrimitives(
        std::uint32_t kind,
        const TeamManager& team_manager,
        const SimulationManager& simulation_manager
    );
    void SortTeamPrimitives(const TeamManager& team_manager);
    void ClearContacts();
    void GenerateBroadPhaseContacts(
        const TeamManager& team_manager,
        const SimulationManager& simulation_manager
    );
    void UpdateBroadPhaseContacts(const SimulationManager& simulation_manager);
    void SolveContacts(SimulationManager& simulation_manager);
    void SolveRuntimeSelfCollision(
        int update_index,
        const TeamManager& team_manager,
        SimulationManager& simulation_manager
    );
    void SolveIntersect(
        const TeamManager& team_manager,
        SimulationManager& simulation_manager
    );

    [[nodiscard]] const ExNativeArray<Primitive>& Primitives() const;
    [[nodiscard]] ExNativeArray<Primitive>& Primitives();
    [[nodiscard]] const ExNativeArray<SortData>& SortAndSweep() const;
    [[nodiscard]] ExNativeArray<SortData>& SortAndSweep();
    [[nodiscard]] const ExNativeArray<EdgeEdgeContact>& EdgeEdgeContacts() const;
    [[nodiscard]] const ExNativeArray<PointTriangleContact>& PointTriangleContacts() const;

private:
    ExNativeArray<Primitive> primitive_array_;
    ExNativeArray<SortData> sort_and_sweep_array_;
    ExNativeArray<EdgeEdgeContact> edge_edge_contact_array_;
    ExNativeArray<PointTriangleContact> point_triangle_contact_array_;
    ExNativeArray<std::uint8_t> intersect_flag_array_;
    int point_primitive_count_ = 0;
    int edge_primitive_count_ = 0;
    int triangle_primitive_count_ = 0;
    int intersect_count_ = 0;

    void InitPrimitive(
        int team_id,
        const TeamManager::TeamData& team_data,
        std::uint32_t kind,
        int start_primitive,
        int start_sort,
        int length,
        const VirtualMeshManager& virtual_mesh_manager
    );
    [[nodiscard]] int BinarySearchSortAndSweep(const SortData& sort_data, const DataChunk& chunk) const;
    void SweepEdgeEdge(
        const TeamManager::TeamData& team_data,
        const Primitive& primitive0,
        const SortData& sort_data0,
        int sort_index,
        const DataChunk& sub_chunk,
        bool connection_check
    );
    void SweepPointTriangle(
        const TeamManager::TeamData& team_data,
        std::uint32_t main_kind,
        const Primitive& primitive0,
        const SortData& sort_data0,
        int sort_index,
        const DataChunk& sub_chunk,
        bool connection_check
    );
    void BroadEdgeEdge(const Primitive& primitive0, const Primitive& primitive1, float thickness, float scr);
    void BroadPointTriangle(
        const Primitive& point_primitive,
        const Primitive& triangle_primitive,
        float thickness,
        float scr,
        float angle_cos
    );
    void UpdateEdgeEdgeBroadPhaseContact(
        EdgeEdgeContact& contact,
        const SimulationManager& simulation_manager
    ) const;
    void UpdatePointTriangleBroadPhaseContact(
        PointTriangleContact& contact,
        const SimulationManager& simulation_manager
    ) const;
    void SolveEdgeEdgeContact(
        const EdgeEdgeContact& contact,
        SimulationManager& simulation_manager
    ) const;
    void SolvePointTriangleContact(
        const PointTriangleContact& contact,
        SimulationManager& simulation_manager
    ) const;
    void SolveAggregateBufferAndClear(SimulationManager& simulation_manager) const;
    void SweepIntersectEdgeTriangle(
        const Primitive& primitive0,
        const SortData& sort_data0,
        std::uint32_t main_kind,
        const DataChunk& sub_chunk,
        bool connection_check
    );
    void IntersectEdgeTriangle(const Primitive& edge_primitive, const Primitive& triangle_primitive);
    void RecalculateIntersectCount(const TeamManager& team_manager);
};

}  // namespace hocloth::mc2
