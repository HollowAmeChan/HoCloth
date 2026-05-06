#pragma once

#include "hocloth/manager/i_manager.hpp"
#include "hocloth/prebuild/share_prebuild_data.hpp"
#include "hocloth/utility/result_code/result_code.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_container.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Cloth/PreBuildManager.cs
class PreBuildManager final : public IManager {
public:
    struct ShareDeserializationData {
        std::string build_id;
        ResultStatus result = ResultStatus::None();
        int reference_count = 0;

        std::vector<RenderSetupData> render_setup_data_list;
        std::shared_ptr<VirtualMesh> proxy_mesh;
        std::vector<std::shared_ptr<VirtualMesh>> render_mesh_list;
        DistanceConstraint::ConstraintData distance_constraint_data;
        TriangleBendingConstraint::ConstraintData bending_constraint_data;
        InertiaConstraint::ConstraintData inertia_constraint_data;

        void Dispose();
        void Deserialize(const SharePreBuildData& share_prebuild_data);

        [[nodiscard]] int RenderMeshCount() const;
        [[nodiscard]] VirtualMeshContainer GetProxyMeshContainer() const;
        [[nodiscard]] VirtualMeshContainer GetRenderMeshContainer(int index) const;
    };

    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;
    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] ShareDeserializationData* RegisterPreBuildData(
        const SharePreBuildData& share_data,
        bool reference_increment
    );
    [[nodiscard]] ShareDeserializationData* GetPreBuildData(const SharePreBuildData& share_data);
    void UnregisterPreBuildData(const SharePreBuildData& share_data);
    void UnloadUnusedData();
    [[nodiscard]] std::string InformationString() const;

private:
    [[nodiscard]] static std::string KeyFor(const SharePreBuildData& share_data);

    bool initialized_ = false;
    std::unordered_map<std::string, ShareDeserializationData> deserialization_dict_;
};

}  // namespace hocloth::mc2
