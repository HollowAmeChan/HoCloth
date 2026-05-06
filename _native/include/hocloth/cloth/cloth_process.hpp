#pragma once

#include "hocloth/cloth/cloth_serialize_data.hpp"
#include "hocloth/cloth/constraints/distance_constraint.hpp"
#include "hocloth/cloth/constraints/inertia_constraint.hpp"
#include "hocloth/cloth/constraints/triangle_bending_constraint.hpp"
#include "hocloth/core/interface/i_valid.hpp"
#include "hocloth/manager/cloth/prebuild_manager.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/transform/transform_record.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/result_code/result_code.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_container.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

class ClothManager;
class MagicaManager;
class TeamManager;
class TransformManager;
class VirtualMeshManager;

// Port target for Magica Cloth 2: Scripts/Core/Cloth/ClothProcess*.cs
class ClothProcess final : public IValid {
public:
    static constexpr int StateValid = 0;
    static constexpr int StateEnable = 1;
    static constexpr int StateParameterDirty = 2;
    static constexpr int StateInitSuccess = 3;
    static constexpr int StateInitComplete = 4;
    static constexpr int StateBuild = 5;
    static constexpr int StateRunning = 6;
    static constexpr int StateDisableAutoBuild = 7;
    static constexpr int StateCameraCullingInvisible = 8;
    static constexpr int StateCameraCullingKeep = 9;
    static constexpr int StateSkipWriting = 10;
    static constexpr int StateSkipWritingDirty = 11;
    static constexpr int StateUsePreBuild = 12;
    static constexpr int StateDistanceCullingInvisible = 13;

    struct RenderMeshInfo {
        int render_handle = 0;
        VirtualMeshContainer render_mesh_container;
        DataChunk mapping_chunk;
    };

    struct PaintMapData {
        static constexpr std::uint8_t ReadFlagFixed = 0x01;
        static constexpr std::uint8_t ReadFlagMove = 0x02;
        static constexpr std::uint8_t ReadFlagLimit = 0x04;

        std::vector<std::uint32_t> paint_data;
        int paint_map_width = 0;
        int paint_map_height = 0;
        BitFlag8 paint_read_flag;
    };

    struct InitializationInput {
        std::string name;
        ClothSerializeData serialize_data;
        ClothSerializeData2 serialize_data2;
        TransformRecord cloth_transform_record;
        TransformRecord normal_adjustment_transform_record;
        bool enabled = true;
    };

    struct RuntimeBuildResult {
        ResultStatus result = ResultStatus::Empty();
        VirtualMeshContainer proxy_mesh_container;
        std::vector<RenderMeshInfo> render_mesh_info_list;
        InertiaConstraint::ConstraintData inertia_constraint_data;
        DistanceConstraint::ConstraintData distance_constraint_data;
        TriangleBendingConstraint::ConstraintData bending_constraint_data;

        [[nodiscard]] bool IsValid() const;
        void Dispose();
    };

    ClothProcess();
    ~ClothProcess() override = default;

    void Dispose();
    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] bool IsState(int state) const;
    void SetState(int state, bool enabled);
    [[nodiscard]] BitFlag64 StateFlag() const;

    [[nodiscard]] bool IsCameraCullingInvisible() const;
    [[nodiscard]] bool IsCameraCullingKeep() const;
    [[nodiscard]] bool IsDistanceCullingInvisible() const;
    [[nodiscard]] bool IsSkipWriting() const;
    [[nodiscard]] bool HasProxyMesh() const;
    [[nodiscard]] const std::string& Name() const;
    [[nodiscard]] ResultStatus Result() const;
    [[nodiscard]] int TeamId() const;
    void SetTeamId(int team_id);

    [[nodiscard]] ClothType Type() const;
    [[nodiscard]] const ClothParameters& Parameters() const;
    void SyncParameters();
    void SetSkipWriting(bool enabled);
    [[nodiscard]] ClothUpdateMode GetClothUpdateMode() const;

    [[nodiscard]] ResultStatus GenerateStatusCheck(const float3& scale) const;
    [[nodiscard]] bool GenerateInitialization(
        const InitializationInput& input,
        PreBuildManager* prebuild_manager
    );
    [[nodiscard]] bool PreBuildDataConstruction(
        TeamManager& team_manager,
        TransformManager& transform_manager,
        VirtualMeshManager& virtual_mesh_manager,
        SimulationManager& simulation_manager,
        ClothManager& cloth_manager
    );
    [[nodiscard]] bool PreBuildDataConstruction(MagicaManager& manager);
    [[nodiscard]] bool RegisterToManagers(
        TeamManager& team_manager,
        TransformManager& transform_manager,
        VirtualMeshManager& virtual_mesh_manager,
        SimulationManager& simulation_manager,
        ClothManager& cloth_manager
    );
    [[nodiscard]] bool RegisterToManagers(MagicaManager& manager);
    [[nodiscard]] bool ApplyRuntimeBuildResult(
        RuntimeBuildResult build_result,
        TeamManager& team_manager,
        TransformManager& transform_manager,
        VirtualMeshManager& virtual_mesh_manager,
        SimulationManager& simulation_manager,
        ClothManager& cloth_manager
    );
    [[nodiscard]] bool ApplyRuntimeBuildResult(
        RuntimeBuildResult build_result,
        MagicaManager& manager
    );
    void UnregisterFromManagers(
        TeamManager& team_manager,
        TransformManager& transform_manager,
        VirtualMeshManager& virtual_mesh_manager,
        SimulationManager& simulation_manager,
        ClothManager& cloth_manager
    );
    void UnregisterFromManagers(MagicaManager& manager);
    void StartUse(TeamManager& team_manager);
    void StartUse(MagicaManager& manager);
    void EndUse(TeamManager& team_manager);
    void EndUse(MagicaManager& manager);
    void DataUpdate();

    [[nodiscard]] RenderMeshInfo* GetRenderMeshInfo(int index);
    [[nodiscard]] const RenderMeshInfo* GetRenderMeshInfo(int index) const;
    [[nodiscard]] std::vector<int> GetUsedTransforms() const;
    void ReplaceTransform(const std::unordered_map<int, int>& replace_dict);

    ClothSerializeData serialize_data;
    ClothSerializeData2 serialize_data2;
    TransformRecord cloth_transform_record;
    TransformRecord normal_adjustment_transform_record;
    std::vector<int> render_handle_list;
    std::vector<RenderMeshInfo> render_mesh_info_list;
    std::vector<TransformRecord> custom_skinning_bone_records;
    VirtualMeshContainer proxy_mesh_container;
    InertiaConstraint::ConstraintData inertia_constraint_data;
    DistanceConstraint::ConstraintData distance_constraint_data;
    TriangleBendingConstraint::ConstraintData bending_constraint_data;

private:
    void DisposeInternal();

    std::string name_ = "(none)";
    BitFlag64 state_flag_;
    ResultStatus result_ = ResultStatus::Empty();
    ClothType cloth_type_ = ClothType::MeshCloth;
    ClothParameters parameters_;
    int team_id_ = 0;
    bool destroyed_ = false;
    bool destroyed_internal_ = false;
    bool building_ = false;
    bool registered_to_managers_ = false;
    SimulationManager::ParticleChunkSet particle_chunks_;
    TeamManager* registered_team_manager_ = nullptr;
    TransformManager* registered_transform_manager_ = nullptr;
    VirtualMeshManager* registered_virtual_mesh_manager_ = nullptr;
    SimulationManager* registered_simulation_manager_ = nullptr;
    ClothManager* registered_cloth_manager_ = nullptr;
    SharePreBuildData* registered_prebuild_data_ = nullptr;
    PreBuildManager* prebuild_manager_ = nullptr;
};

}  // namespace hocloth::mc2
