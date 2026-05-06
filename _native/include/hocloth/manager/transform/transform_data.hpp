#pragma once

#include "hocloth/manager/transform/transform_record.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/TransformManager/TransformData.cs
struct TransformData {
    ExNativeArray<BitFlag8> flag_array;
    ExNativeArray<float3> init_local_position_array;
    ExNativeArray<quaternion> init_local_rotation_array;
    ExNativeArray<float3> position_array;
    ExNativeArray<quaternion> rotation_array;
    ExNativeArray<quaternion> inverse_rotation_array;
    ExNativeArray<float3> scale_array;
    ExNativeArray<float3> local_position_array;
    ExNativeArray<quaternion> local_rotation_array;
    ExNativeArray<float4x4> local_to_world_matrix_array;
    ExNativeArray<std::int16_t> team_id_array;

    std::vector<int> id_array;
    std::vector<int> parent_id_array;
    std::vector<std::string> name_array;
    std::vector<int> root_id_list;
    bool is_dirty = false;

    void Initialize(int capacity);
    void Dispose();
    void EnsureRecordCapacity(int count);

    [[nodiscard]] int Count() const;
    [[nodiscard]] int RootCount() const;
    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] bool IsEmpty() const;
};

}  // namespace hocloth::mc2
