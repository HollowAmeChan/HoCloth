#pragma once

#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/result_code/result_code.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Render/RenderSetupData*.cs
struct RenderSetupData {
    enum class SetupType {
        MeshCloth = 0,
        BoneCloth = 1,
        BoneSpring = 2,
    };

    enum class BoneConnectionMode {
        Line = 0,
        AutomaticMesh = 1,
        SequentialLoopMesh = 2,
        SequentialNonLoopMesh = 3,
    };

    struct ShareSerializationData {
        ResultStatus result = ResultStatus::None();
        std::string name;
        SetupType setup_type = SetupType::MeshCloth;

        int original_mesh_id = 0;
        int vertex_count = 0;
        bool has_skinned_mesh = false;
        bool has_bone_weight = false;
        int skin_root_bone_index = -1;
        int skin_bone_count = 0;
        std::vector<float4x4> bind_pose_list;
        std::vector<std::uint8_t> bones_per_vertex_array;
        std::vector<std::uint8_t> bone_weight_array;
        std::vector<float3> local_positions;
        std::vector<float3> local_normals;

        BoneConnectionMode bone_connection_mode = BoneConnectionMode::Line;
        int render_transform_index = -1;
    };

    struct UniqueSerializationData {
        ResultStatus result = ResultStatus::None();

        int renderer_id = 0;
        int skin_renderer_id = 0;
        int mesh_filter_id = 0;
        int original_mesh_id = 0;
        std::vector<int> transform_ids;

        [[nodiscard]] std::vector<int> GetUsedTransforms() const
        {
            return transform_ids;
        }

        void ReplaceTransform(const std::unordered_map<int, int>& replace_dict)
        {
            for (int& transform_id : transform_ids) {
                const auto iterator = replace_dict.find(transform_id);
                if (iterator != replace_dict.end()) {
                    transform_id = iterator->second;
                }
            }
        }
    };

    ResultStatus result = ResultStatus::None();
    std::string name;
    bool is_managed = false;
    SetupType setup_type = SetupType::MeshCloth;

    int original_mesh_id = 0;
    int vertex_count = 0;
    bool has_skinned_mesh = false;
    bool has_bone_weight = false;
    int skin_root_bone_index = -1;
    int skin_bone_count = 0;
    std::vector<float4x4> bind_pose_list;
    std::vector<std::uint8_t> bones_per_vertex_array;
    std::vector<std::uint8_t> bone_weight_array;
    std::vector<float3> local_positions;
    std::vector<float3> local_normals;

    BoneConnectionMode bone_connection_mode = BoneConnectionMode::Line;
    int render_transform_index = -1;

    [[nodiscard]] bool IsSuccess() const;
    [[nodiscard]] bool IsFailed() const;
    [[nodiscard]] bool HasLocalPositions() const;
    void Dispose();

    [[nodiscard]] static RenderSetupData ShareDeserialize(
        const ShareSerializationData& data
    );
};

}  // namespace hocloth::mc2
