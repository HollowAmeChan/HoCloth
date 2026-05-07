#pragma once

#include "hocloth/virtual_mesh/virtual_mesh.hpp"

#include <memory>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VirtualMeshContainer.cs
class VirtualMeshContainer {
public:
    VirtualMeshContainer() = default;
    explicit VirtualMeshContainer(std::shared_ptr<VirtualMesh> mesh);

    void Dispose();

    [[nodiscard]] bool HasUniqueData() const;
    [[nodiscard]] int GetTransformCount() const;
    [[nodiscard]] TransformRecord GetTransformRecordFromIndex(int index) const;
    [[nodiscard]] TransformRecord GetTransformFromIndex(int index) const;
    [[nodiscard]] TransformRecord GetCenterTransformRecord() const;
    [[nodiscard]] TransformRecord GetCenterTransform() const;
    [[nodiscard]] VirtualMesh* SharedVirtualMesh();
    [[nodiscard]] const VirtualMesh* SharedVirtualMesh() const;

    void SetSharedVirtualMesh(std::shared_ptr<VirtualMesh> mesh);
    void SetUniqueTransformRecords(std::vector<TransformRecord> records);

private:
    std::shared_ptr<VirtualMesh> share_virtual_mesh_;
    std::vector<TransformRecord> unique_transform_records_;
};

}  // namespace hocloth::mc2
