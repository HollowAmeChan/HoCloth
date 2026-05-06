#include "hocloth/manager/cloth/prebuild_manager.hpp"

#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <algorithm>

namespace hocloth::mc2 {

void PreBuildManager::ShareDeserializationData::Dispose()
{
    render_setup_data_list.clear();
    if (proxy_mesh) {
        proxy_mesh->is_managed = false;
        proxy_mesh->Dispose();
        proxy_mesh.reset();
    }
    for (std::shared_ptr<VirtualMesh>& render_mesh : render_mesh_list) {
        if (render_mesh) {
            render_mesh->is_managed = false;
            render_mesh->Dispose();
        }
    }
    render_mesh_list.clear();
    distance_constraint_data = DistanceConstraint::ConstraintData{};
    bending_constraint_data = TriangleBendingConstraint::ConstraintData{};
    inertia_constraint_data = InertiaConstraint::ConstraintData{};
    build_id.clear();
    result.Clear();
    reference_count = 0;
}

void PreBuildManager::ShareDeserializationData::Deserialize(
    const SharePreBuildData& share_prebuild_data
)
{
    result.SetProcess();
    render_setup_data_list.clear();
    render_mesh_list.clear();
    proxy_mesh.reset();

    const ResultStatus validate_result = share_prebuild_data.DataValidate();
    if (validate_result.IsFailed()) {
        result.Merge(validate_result);
        return;
    }

    build_id = share_prebuild_data.build_id;
    render_setup_data_list.reserve(share_prebuild_data.render_setup_data_list.size());
    for (const RenderSetupData::ShareSerializationData& setup_data
         : share_prebuild_data.render_setup_data_list) {
        render_setup_data_list.push_back(RenderSetupData::ShareDeserialize(setup_data));
    }
    proxy_mesh = std::make_shared<VirtualMesh>(
        VirtualMeshSerializationData::ShareDeserialize(share_prebuild_data.proxy_mesh)
    );
    proxy_mesh->is_managed = true;

    render_mesh_list.reserve(share_prebuild_data.render_mesh_list.size());
    for (const VirtualMesh::ShareSerializationData& mesh_data : share_prebuild_data.render_mesh_list) {
        auto mesh = std::make_shared<VirtualMesh>(
            VirtualMeshSerializationData::ShareDeserialize(mesh_data)
        );
        mesh->is_managed = true;
        render_mesh_list.push_back(std::move(mesh));
    }

    distance_constraint_data = share_prebuild_data.distance_constraint_data;
    bending_constraint_data = share_prebuild_data.bending_constraint_data;
    inertia_constraint_data = share_prebuild_data.inertia_constraint_data;
    result.SetSuccess();
}

int PreBuildManager::ShareDeserializationData::RenderMeshCount() const
{
    return static_cast<int>(render_mesh_list.size());
}

VirtualMeshContainer PreBuildManager::ShareDeserializationData::GetProxyMeshContainer() const
{
    return VirtualMeshContainer(proxy_mesh);
}

VirtualMeshContainer PreBuildManager::ShareDeserializationData::GetRenderMeshContainer(int index) const
{
    if (index < 0 || index >= RenderMeshCount()) {
        return VirtualMeshContainer();
    }
    return VirtualMeshContainer(render_mesh_list[static_cast<std::size_t>(index)]);
}

Result PreBuildManager::Initialize()
{
    Dispose();
    initialized_ = true;
    return Result::Ok();
}

void PreBuildManager::Dispose()
{
    for (auto& key_value : deserialization_dict_) {
        key_value.second.Dispose();
    }
    deserialization_dict_.clear();
    initialized_ = false;
}

ManagerStatus PreBuildManager::Status() const
{
    return ManagerStatus{
        "PreBuildManager",
        initialized_,
        static_cast<std::uint32_t>(deserialization_dict_.size()),
        InformationString(),
    };
}

bool PreBuildManager::IsValid() const
{
    return initialized_;
}

PreBuildManager::ShareDeserializationData* PreBuildManager::RegisterPreBuildData(
    const SharePreBuildData& share_data,
    bool reference_increment
)
{
    if (!initialized_ || share_data.build_id.empty()) {
        return nullptr;
    }

    const std::string key = KeyFor(share_data);
    auto iterator = deserialization_dict_.find(key);
    if (iterator == deserialization_dict_.end()) {
        ShareDeserializationData data;
        data.build_id = share_data.build_id;
        data.Deserialize(share_data);
        iterator = deserialization_dict_.emplace(key, std::move(data)).first;
    }

    ShareDeserializationData& data = iterator->second;
    if (reference_increment) {
        ++data.reference_count;
    }
    return &data;
}

PreBuildManager::ShareDeserializationData* PreBuildManager::GetPreBuildData(
    const SharePreBuildData& share_data
)
{
    const auto iterator = deserialization_dict_.find(KeyFor(share_data));
    return iterator == deserialization_dict_.end() ? nullptr : &iterator->second;
}

void PreBuildManager::UnregisterPreBuildData(const SharePreBuildData& share_data)
{
    if (!initialized_) {
        return;
    }
    ShareDeserializationData* data = GetPreBuildData(share_data);
    if (data != nullptr) {
        --data->reference_count;
    }
}

void PreBuildManager::UnloadUnusedData()
{
    for (auto iterator = deserialization_dict_.begin(); iterator != deserialization_dict_.end();) {
        if (iterator->second.reference_count <= 0) {
            iterator->second.Dispose();
            iterator = deserialization_dict_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

std::string PreBuildManager::InformationString() const
{
    std::ostringstream stream;
    stream << "Count:" << deserialization_dict_.size();
    for (const auto& key_value : deserialization_dict_) {
        const ShareDeserializationData& data = key_value.second;
        stream << " [" << data.build_id << "] refcnt:" << data.reference_count
               << " result:" << data.result.GetResultString()
               << " proxyMesh:" << (data.proxy_mesh != nullptr ? "true" : "false");
    }
    return stream.str();
}

std::string PreBuildManager::KeyFor(const SharePreBuildData& share_data)
{
    return share_data.build_id;
}

}  // namespace hocloth::mc2
