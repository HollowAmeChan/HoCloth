#pragma once

#include "hocloth/manager/transform/transform_data.hpp"
#include "hocloth/manager/transform/transform_record.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/math/math_types.hpp"

#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2:
// Scripts/Core/Manager/TransformManager/TransformDataSerialization.cs
struct TransformDataSerialization {
    template <typename T>
    struct SimpleArrayData {
        std::vector<T> data;
        int count = 0;
        int length = 0;

        [[nodiscard]] bool IsValid() const
        {
            return count > 0 && count <= static_cast<int>(data.size());
        }
    };

    struct ShareSerializationData {
        SimpleArrayData<BitFlag8> flag_array;
        SimpleArrayData<float3> init_local_position_array;
        SimpleArrayData<quaternion> init_local_rotation_array;
    };

    struct UniqueSerializationData {
        std::vector<TransformRecord> transform_array;

        [[nodiscard]] std::vector<int> GetUsedTransforms() const
        {
            std::vector<int> result;
            result.reserve(transform_array.size());
            for (const TransformRecord& record : transform_array) {
                if (record.IsValid()) {
                    result.push_back(record.id);
                }
            }
            return result;
        }

        void ReplaceTransform(const std::unordered_map<int, int>& replace_dict)
        {
            for (TransformRecord& record : transform_array) {
                const auto iterator = replace_dict.find(record.id);
                if (iterator != replace_dict.end()) {
                    record.id = iterator->second;
                }
            }
        }
    };

    [[nodiscard]] static ShareSerializationData ShareSerialize(const TransformData& data);
    [[nodiscard]] static TransformData ShareDeserialize(const ShareSerializationData& data);
    [[nodiscard]] static UniqueSerializationData UniqueSerialize(const TransformData& data);
};

}  // namespace hocloth::mc2
