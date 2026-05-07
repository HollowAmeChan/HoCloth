#include "hocloth/virtual_mesh/virtual_mesh_container.hpp"

#include <cstddef>
#include <utility>

namespace hocloth::mc2 {

VirtualMeshContainer::VirtualMeshContainer(std::shared_ptr<VirtualMesh> mesh)
    : share_virtual_mesh_(std::move(mesh))
{
}

void VirtualMeshContainer::Dispose()
{
    if (share_virtual_mesh_) {
        if (!share_virtual_mesh_->is_managed) {
            share_virtual_mesh_->Dispose();
        }
    }
    unique_transform_records_.clear();
    share_virtual_mesh_.reset();
}

bool VirtualMeshContainer::HasUniqueData() const
{
    return !unique_transform_records_.empty();
}

int VirtualMeshContainer::GetTransformCount() const
{
    if (HasUniqueData()) {
        return static_cast<int>(unique_transform_records_.size());
    }
    return share_virtual_mesh_ ? share_virtual_mesh_->TransformCount() : 0;
}

TransformRecord VirtualMeshContainer::GetTransformRecordFromIndex(int index) const
{
    if (index < 0) {
        return TransformRecord{};
    }

    if (HasUniqueData()) {
        if (index >= static_cast<int>(unique_transform_records_.size())) {
            return TransformRecord{};
        }
        return unique_transform_records_[static_cast<std::size_t>(index)];
    }

    if (!share_virtual_mesh_) {
        return TransformRecord{};
    }
    return share_virtual_mesh_->transform_data.Count() > index
        ? TransformRecord{
              share_virtual_mesh_->transform_data.id_array[static_cast<std::size_t>(index)],
              share_virtual_mesh_->transform_data.parent_id_array[static_cast<std::size_t>(index)],
              share_virtual_mesh_->transform_data.name_array[static_cast<std::size_t>(index)],
              share_virtual_mesh_->transform_data.local_position_array[index],
              share_virtual_mesh_->transform_data.local_rotation_array[index],
              share_virtual_mesh_->transform_data.position_array[index],
              share_virtual_mesh_->transform_data.rotation_array[index],
              share_virtual_mesh_->transform_data.scale_array[index],
              share_virtual_mesh_->transform_data.local_to_world_matrix_array[index],
              float4x4{},
          }
        : TransformRecord{};
}

TransformRecord VirtualMeshContainer::GetTransformFromIndex(int index) const
{
    return GetTransformRecordFromIndex(index);
}

TransformRecord VirtualMeshContainer::GetCenterTransformRecord() const
{
    if (!share_virtual_mesh_) {
        return TransformRecord{};
    }
    return GetTransformRecordFromIndex(share_virtual_mesh_->center_transform_index);
}

TransformRecord VirtualMeshContainer::GetCenterTransform() const
{
    return GetCenterTransformRecord();
}

VirtualMesh* VirtualMeshContainer::SharedVirtualMesh()
{
    return share_virtual_mesh_.get();
}

const VirtualMesh* VirtualMeshContainer::SharedVirtualMesh() const
{
    return share_virtual_mesh_.get();
}

void VirtualMeshContainer::SetSharedVirtualMesh(std::shared_ptr<VirtualMesh> mesh)
{
    share_virtual_mesh_ = std::move(mesh);
}

void VirtualMeshContainer::SetUniqueTransformRecords(std::vector<TransformRecord> records)
{
    unique_transform_records_ = std::move(records);
}

}  // namespace hocloth::mc2
