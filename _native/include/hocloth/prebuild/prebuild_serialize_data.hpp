#pragma once

#include "hocloth/prebuild/share_prebuild_data.hpp"
#include "hocloth/prebuild/unique_prebuild_data.hpp"
#include "hocloth/utility/result_code/result_code.hpp"

#include <algorithm>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

struct PreBuildDataLibrary {
    std::vector<SharePreBuildData> share_prebuild_data_list;

    [[nodiscard]] int Count() const
    {
        return static_cast<int>(share_prebuild_data_list.size());
    }

    [[nodiscard]] bool Empty() const
    {
        return share_prebuild_data_list.empty();
    }

    [[nodiscard]] bool HasPreBuildData(const std::string& build_id) const
    {
        return GetPreBuildData(build_id) != nullptr;
    }

    [[nodiscard]] const SharePreBuildData* GetPreBuildData(const std::string& build_id) const
    {
        const auto iterator = std::find_if(
            share_prebuild_data_list.begin(),
            share_prebuild_data_list.end(),
            [&build_id](const SharePreBuildData& data) { return data.CheckBuildId(build_id); }
        );
        return iterator == share_prebuild_data_list.end() ? nullptr : &(*iterator);
    }

    [[nodiscard]] SharePreBuildData* GetPreBuildData(const std::string& build_id)
    {
        return const_cast<SharePreBuildData*>(
            static_cast<const PreBuildDataLibrary*>(this)->GetPreBuildData(build_id)
        );
    }

    void AddPreBuildData(const SharePreBuildData& share_data)
    {
        const auto iterator = std::find_if(
            share_prebuild_data_list.begin(),
            share_prebuild_data_list.end(),
            [&share_data](const SharePreBuildData& data) { return data.build_id == share_data.build_id; }
        );
        if (iterator != share_prebuild_data_list.end()) {
            *iterator = share_data;
        } else {
            share_prebuild_data_list.push_back(share_data);
        }
    }

    [[nodiscard]] bool RemovePreBuildData(const std::string& build_id)
    {
        const auto iterator = std::find_if(
            share_prebuild_data_list.begin(),
            share_prebuild_data_list.end(),
            [&build_id](const SharePreBuildData& data) { return data.CheckBuildId(build_id); }
        );
        if (iterator == share_prebuild_data_list.end()) {
            return false;
        }
        share_prebuild_data_list.erase(iterator);
        return true;
    }

    void Clear()
    {
        share_prebuild_data_list.clear();
    }

    [[nodiscard]] ResultStatus DataValidate() const
    {
        ResultStatus result = ResultStatus::Success();
        for (const SharePreBuildData& data : share_prebuild_data_list) {
            result.Merge(data.DataValidate());
            if (result.IsFailed()) {
                return result;
            }
        }
        return result;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "<<<<< PreBuildDataLibrary >>>>>\n";
        stream << "Count:" << Count() << '\n';
        for (const SharePreBuildData& data : share_prebuild_data_list) {
            stream << "BuildID:" << data.build_id
                   << " Result:" << data.build_result.GetResultString()
                   << " RenderMeshes:" << data.render_mesh_list.size()
                   << " RenderSetups:" << data.render_setup_data_list.size()
                   << '\n';
        }
        return stream.str();
    }
};

// Port target for Magica Cloth 2: Scripts/Core/PreBuild/PreBuildSerializeData.cs
struct PreBuildSerializeData {
    bool enabled = false;
    std::string build_id;
    PreBuildDataLibrary prebuild_library;
    UniquePreBuildData unique_prebuild_data;

    [[nodiscard]] bool UsePreBuild() const
    {
        return enabled;
    }

    [[nodiscard]] ResultStatus DataValidate() const
    {
        const SharePreBuildData* share_data = GetSharePreBuildData();
        if (share_data == nullptr) {
            return ResultStatus(ResultCode::PreBuildData_Empty);
        }

        ResultStatus result = share_data->DataValidate();
        if (result.IsFailed()) {
            return result;
        }

        result = unique_prebuild_data.DataValidate();
        if (result.IsFailed()) {
            return result;
        }

        return ResultStatus::Success();
    }

    [[nodiscard]] const SharePreBuildData* GetSharePreBuildData() const
    {
        if (build_id.empty()) {
            return nullptr;
        }
        return prebuild_library.GetPreBuildData(build_id);
    }

    [[nodiscard]] SharePreBuildData* GetSharePreBuildData()
    {
        return const_cast<SharePreBuildData*>(
            static_cast<const PreBuildSerializeData*>(this)->GetSharePreBuildData()
        );
    }

    [[nodiscard]] static std::string GenerateBuildID()
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::random_device random_device;
        std::uniform_int_distribution<int> distribution(0, 15);
        std::string id;
        id.reserve(8);
        for (int index = 0; index < 8; ++index) {
            id.push_back(digits[distribution(random_device)]);
        }
        return id;
    }

    [[nodiscard]] std::vector<int> GetUsedTransforms() const
    {
        std::vector<int> transforms = unique_prebuild_data.GetUsedTransforms();
        std::vector<int> unique_transforms;
        std::unordered_set<int> seen;
        unique_transforms.reserve(transforms.size());
        for (int transform_id : transforms) {
            if (transform_id < 0 || !seen.insert(transform_id).second) {
                continue;
            }
            unique_transforms.push_back(transform_id);
        }
        return unique_transforms;
    }

    void ReplaceTransform(const std::unordered_map<int, int>& replace_dict)
    {
        unique_prebuild_data.ReplaceTransform(replace_dict);
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        const SharePreBuildData* share_data = GetSharePreBuildData();
        stream << "<<<<< PreBuildSerializeData >>>>>\n";
        stream << "Enabled:" << (enabled ? "true" : "false") << '\n';
        stream << "BuildID:" << build_id << '\n';
        stream << "ShareData:" << (share_data != nullptr ? "found" : "missing") << '\n';
        stream << "LibraryCount:" << prebuild_library.Count() << '\n';
        stream << "UsedTransforms:" << GetUsedTransforms().size() << '\n';
        stream << unique_prebuild_data.ToString();
        return stream.str();
    }
};

}  // namespace hocloth::mc2
