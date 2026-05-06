#pragma once

#include "hocloth/cloth/constraints/distance_constraint.hpp"
#include "hocloth/cloth/constraints/inertia_constraint.hpp"
#include "hocloth/cloth/constraints/triangle_bending_constraint.hpp"
#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/render/render_setup_data_serialization.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/result_code/result_code.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/PreBuild/SharePreBuildData.cs
struct SharePreBuildData {
    int version = define::system::LatestPreBuildVersion;
    std::string build_id;
    ResultStatus build_result = ResultStatus::Success();
    float3 build_scale{1.0f, 1.0f, 1.0f};

    std::vector<RenderSetupData::ShareSerializationData> render_setup_data_list;
    VirtualMeshSerializationData::ShareSerializationData proxy_mesh;
    std::vector<VirtualMeshSerializationData::ShareSerializationData> render_mesh_list;
    DistanceConstraint::ConstraintData distance_constraint_data;
    TriangleBendingConstraint::ConstraintData bending_constraint_data;
    InertiaConstraint::ConstraintData inertia_constraint_data;

    [[nodiscard]] ResultStatus DataValidate() const
    {
        if (version != define::system::LatestPreBuildVersion) {
            return ResultStatus(ResultCode::PreBuildData_VersionMismatch);
        }
        if (build_scale.x < define::system::Epsilon) {
            return ResultStatus(ResultCode::PreBuildData_InvalidScale);
        }
        if (build_result.IsFailed()) {
            return build_result;
        }
        return ResultStatus::Success();
    }

    [[nodiscard]] bool CheckBuildId(const std::string& target_build_id) const
    {
        return !target_build_id.empty() && build_id == target_build_id;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "<<<<< PreBuildData >>>>>\n";
        stream << "Version:" << version << '\n';
        stream << "BuildID:" << build_id << '\n';
        stream << "BuildResult:" << build_result.GetResultString() << '\n';
        stream << "BuildScale:(" << build_scale.x << ", " << build_scale.y << ", " << build_scale.z << ")\n";
        stream << proxy_mesh.ToString();
        stream << "renderMeshList:" << render_mesh_list.size() << '\n';
        stream << "renderSetupDataList:" << render_setup_data_list.size() << '\n';
        return stream.str();
    }
};

}  // namespace hocloth::mc2
