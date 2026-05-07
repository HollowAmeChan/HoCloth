#pragma once

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/VirtualMesh/VirtualMesh.cs
enum class VirtualMeshType {
    NormalMesh = 0,
    NormalBoneMesh = 1,
    ProxyMesh = 2,
    ProxyBoneMesh = 3,
    Mapping = 4,
};

}  // namespace hocloth::mc2
