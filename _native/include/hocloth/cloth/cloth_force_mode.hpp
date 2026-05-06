#pragma once

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Cloth/ClothForceMode.cs
enum class ClothForceMode {
    None = 0,
    VelocityAdd = 1,
    VelocityChange = 2,
    VelocityAddWithoutDepth = 10,
    VelocityChangeWithoutDepth = 11,
};

}  // namespace hocloth::mc2
