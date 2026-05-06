#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/render/render_setup_data_serialization.hpp"
#include "hocloth/utility/result_code/result_code.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/PreBuild/UniquePreBuildData.cs
struct UniquePreBuildData {
    int version = define::system::LatestPreBuildVersion;
    ResultStatus build_result = ResultStatus::Success();

    std::vector<RenderSetupData::UniqueSerializationData> render_setup_data_list;
    VirtualMeshSerializationData::UniqueSerializationData proxy_mesh;
    std::vector<VirtualMeshSerializationData::UniqueSerializationData> render_mesh_list;

    [[nodiscard]] ResultStatus DataValidate() const
    {
        if (version != define::system::LatestPreBuildVersion) {
            return ResultStatus(ResultCode::PreBuildData_VersionMismatch);
        }
        if (build_result.IsFailed()) {
            return build_result;
        }
        return ResultStatus::Success();
    }

    [[nodiscard]] std::vector<int> GetUsedTransforms() const
    {
        std::vector<int> result;
        for (const RenderSetupData::UniqueSerializationData& render_setup : render_setup_data_list) {
            const std::vector<int> used = render_setup.GetUsedTransforms();
            result.insert(result.end(), used.begin(), used.end());
        }
        const std::vector<int> proxy_used = proxy_mesh.GetUsedTransforms();
        result.insert(result.end(), proxy_used.begin(), proxy_used.end());
        for (const VirtualMeshSerializationData::UniqueSerializationData& render_mesh : render_mesh_list) {
            const std::vector<int> used = render_mesh.GetUsedTransforms();
            result.insert(result.end(), used.begin(), used.end());
        }
        return result;
    }

    void ReplaceTransform(const std::unordered_map<int, int>& replace_dict)
    {
        for (RenderSetupData::UniqueSerializationData& render_setup : render_setup_data_list) {
            render_setup.ReplaceTransform(replace_dict);
        }
        proxy_mesh.ReplaceTransform(replace_dict);
        for (VirtualMeshSerializationData::UniqueSerializationData& render_mesh : render_mesh_list) {
            render_mesh.ReplaceTransform(replace_dict);
        }
    }
};

}  // namespace hocloth::mc2
