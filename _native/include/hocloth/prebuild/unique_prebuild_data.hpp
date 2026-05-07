#pragma once

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/render/render_setup_data_serialization.hpp"
#include "hocloth/utility/result_code/result_code.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
        std::unordered_set<int> seen;
        const auto append_unique = [&result, &seen](const std::vector<int>& values) {
            for (int transform_id : values) {
                if (transform_id < 0 || !seen.insert(transform_id).second) {
                    continue;
                }
                result.push_back(transform_id);
            }
        };
        for (const RenderSetupData::UniqueSerializationData& render_setup : render_setup_data_list) {
            append_unique(render_setup.GetUsedTransforms());
        }
        append_unique(proxy_mesh.GetUsedTransforms());
        for (const VirtualMeshSerializationData::UniqueSerializationData& render_mesh : render_mesh_list) {
            append_unique(render_mesh.GetUsedTransforms());
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

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "<<<<< UniquePreBuildData >>>>>\n";
        stream << "Version:" << version << '\n';
        stream << "BuildResult:" << build_result.GetResultString() << '\n';
        stream << "renderSetupDataList:" << render_setup_data_list.size() << '\n';
        stream << "renderMeshList:" << render_mesh_list.size() << '\n';
        stream << "usedTransforms:" << GetUsedTransforms().size() << '\n';
        return stream.str();
    }
};

}  // namespace hocloth::mc2
