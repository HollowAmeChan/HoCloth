#pragma once

#include "hocloth/cloth/cloth_parameters.hpp"
#include "hocloth/cloth/custom_skinning_settings.hpp"
#include "hocloth/cloth/normal_alignment_settings.hpp"
#include "hocloth/cloth/parameters/culling_settings.hpp"
#include "hocloth/cloth/parameters/curve_serialize_data.hpp"
#include "hocloth/cloth/selection_data.hpp"
#include "hocloth/cloth/wind/wind_settings.hpp"
#include "hocloth/core/define/system_define.hpp"
#include "hocloth/core/interface/i_data_validate.hpp"
#include "hocloth/core/interface/i_valid.hpp"
#include "hocloth/manager/render/render_setup_data_serialization.hpp"
#include "hocloth/prebuild/prebuild_serialize_data.hpp"
#include "hocloth/reduction/reduction_settings.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/utility/result_code/result_code.hpp"
#include "hocloth/virtual_mesh/vertex_attribute.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Cloth/ClothSerializeData.cs
struct ClothSerializeData final : public IValid, public IDataValidate {
    enum class PaintMode {
        Manual = 0,
        TextureFixedMove = 1,
        TextureFixedMoveLimit = 2,
    };

    ClothType cloth_type = ClothType::MeshCloth;
    std::vector<int> source_renderer_ids;
    PaintMode paint_mode = PaintMode::Manual;
    std::vector<int> paint_map_ids;
    std::vector<int> root_bone_ids;
    RenderSetupData::BoneConnectionMode connection_mode =
        RenderSetupData::BoneConnectionMode::Line;
    float rotational_interpolation = 0.5f;
    float root_rotation = 0.5f;
    ClothUpdateMode update_mode = ClothUpdateMode::AnimatorLinkage;
    float animation_pose_ratio = 0.0f;
    ReductionSettings reduction_setting;
    CustomSkinningSettings custom_skinning_setting;
    NormalAlignmentSettings normal_alignment_setting;
    CullingSettings culling_settings;
    ClothNormalAxis normal_axis = ClothNormalAxis::Up;
    float gravity = 5.0f;
    float3 gravity_direction{0.0f, -1.0f, 0.0f};
    float gravity_falloff = 0.0f;
    float stabilization_time_after_reset = 0.1f;
    float blend_weight = 1.0f;
    CurveSerializeData damping{0.05f};
    CurveSerializeData radius{0.02f};

    InertiaConstraintParams inertia_constraint;
    TetherConstraintParams tether_constraint;
    DistanceConstraintParams distance_constraint;
    TriangleBendingConstraintParams triangle_bending_constraint;
    AngleConstraintParams angle_constraint;
    MotionConstraintParams motion_constraint;
    ColliderCollisionConstraintParams collider_collision_constraint;
    SelfCollisionConstraintParams self_collision_constraint;
    WindSettings wind;
    SpringConstraintParams spring_constraint;

    ResultStatus verification_result = ResultStatus::Empty();

    [[nodiscard]] bool IsValid() const override
    {
        switch (cloth_type) {
        case ClothType::BoneCloth:
        case ClothType::BoneSpring:
            return std::any_of(root_bone_ids.begin(), root_bone_ids.end(), [](int id) {
                return id != 0;
            });
        case ClothType::MeshCloth:
            if (source_renderer_ids.size() > define::system::MaxRendererCount) {
                return false;
            }
            return std::any_of(
                source_renderer_ids.begin(),
                source_renderer_ids.end(),
                [](int id) { return id != 0; }
            );
        default:
            return false;
        }
    }

    [[nodiscard]] ResultStatus ValidateForBuild() const
    {
        ResultStatus result = ResultStatus::Empty();
        switch (cloth_type) {
        case ClothType::BoneCloth:
        case ClothType::BoneSpring:
            if (std::any_of(root_bone_ids.begin(), root_bone_ids.end(), [](int id) {
                    return id != 0;
                })) {
                result.SetSuccess();
            }
            return result;
        case ClothType::MeshCloth:
            if (source_renderer_ids.size() > define::system::MaxRendererCount) {
                return ResultStatus(ResultCode::SerializeData_Over31Renderers);
            }
            if (std::any_of(source_renderer_ids.begin(), source_renderer_ids.end(), [](int id) {
                    return id != 0;
                })) {
                result.SetSuccess();
            }
            return result;
        default:
            return result;
        }
    }

    void DataValidate() override
    {
        rotational_interpolation = Clamp01(rotational_interpolation);
        root_rotation = Clamp01(root_rotation);
        animation_pose_ratio = Clamp01(animation_pose_ratio);
        reduction_setting.DataValidate();
        custom_skinning_setting.DataValidate();
        normal_alignment_setting.DataValidate();
        culling_settings.DataValidate();
        gravity = Clamp(gravity, 0.0f, 20.0f);
        if (Length(gravity_direction) > define::system::Epsilon) {
            gravity_direction = Normalize(gravity_direction);
        } else {
            gravity_direction = float3{};
        }
        gravity_falloff = Clamp01(gravity_falloff);
        stabilization_time_after_reset = Clamp01(stabilization_time_after_reset);
        blend_weight = Clamp01(blend_weight);
        damping.DataValidate(0.0f, 1.0f);
        radius.DataValidate(0.001f, 1.0f);
        wind.DataValidate();
    }

    [[nodiscard]] ClothParameters GetClothParameters() const
    {
        ClothParameters parameters;
        parameters.gravity = cloth_type == ClothType::BoneSpring ? 0.0f : gravity;
        parameters.world_gravity_direction = gravity_direction;
        parameters.gravity_falloff = gravity_falloff;
        parameters.stabilization_time_after_reset = stabilization_time_after_reset;
        parameters.blend_weight = blend_weight;
        parameters.damping_curve_data = damping.ConvertFloatArray();
        for (int index = 0; index < 16; ++index) {
            MC2SetValue(
                parameters.damping_curve_data,
                index,
                MC2GetValue(parameters.damping_curve_data, index) * 0.2f
            );
        }
        parameters.radius_curve_data = radius.ConvertFloatArray();
        parameters.normal_axis = normal_axis;
        parameters.wind.Convert(wind, cloth_type);
        parameters.inertia_constraint = inertia_constraint;
        parameters.tether_constraint = tether_constraint;
        parameters.distance_constraint = distance_constraint;
        parameters.triangle_bending_constraint = triangle_bending_constraint;
        parameters.angle_constraint = angle_constraint;
        parameters.motion_constraint = motion_constraint;
        parameters.collider_collision_constraint = collider_collision_constraint;
        parameters.self_collision_constraint = self_collision_constraint;
        parameters.spring_constraint = spring_constraint;
        return parameters;
    }
};

// Port target for Magica Cloth 2: Scripts/Core/Cloth/ClothSerializeData2.cs
struct ClothSerializeData2 final : public IValid, public IDataValidate {
    SelectionData selection_data;
    std::unordered_map<int, VertexAttribute> bone_attribute_dict;
    std::vector<std::vector<VertexAttribute>> vertex_attribute_list;
    PreBuildSerializeData prebuild_data;

    [[nodiscard]] bool IsValid() const override
    {
        return true;
    }

    void DataValidate() override
    {
    }

    [[nodiscard]] std::vector<int> GetUsedTransforms() const
    {
        return prebuild_data.GetUsedTransforms();
    }

    void ReplaceTransform(const std::unordered_map<int, int>& replace_dict)
    {
        prebuild_data.ReplaceTransform(replace_dict);
    }
};

}  // namespace hocloth::mc2
