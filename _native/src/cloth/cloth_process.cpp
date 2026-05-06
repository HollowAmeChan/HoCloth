#include "hocloth/cloth/cloth_process.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/cloth/cloth_manager.hpp"
#include "hocloth/manager/magica_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include <algorithm>
#include <utility>

namespace hocloth::mc2 {

ClothProcess::ClothProcess() = default;

bool ClothProcess::RuntimeBuildResult::IsValid() const
{
    const VirtualMesh* proxy_mesh = proxy_mesh_container.SharedVirtualMesh();
    return result.IsSuccess()
        && proxy_mesh != nullptr
        && proxy_mesh->IsValid()
        && proxy_mesh->VertexCount() > 0;
}

void ClothProcess::RuntimeBuildResult::Dispose()
{
    proxy_mesh_container.Dispose();
    for (RenderMeshInfo& info : render_mesh_info_list) {
        info.render_mesh_container.Dispose();
        info.mapping_chunk.Clear();
    }
    render_mesh_info_list.clear();
    inertia_constraint_data = InertiaConstraint::ConstraintData{};
    distance_constraint_data = DistanceConstraint::ConstraintData{};
    bending_constraint_data = TriangleBendingConstraint::ConstraintData{};
    result.Clear();
}

void ClothProcess::Dispose()
{
    destroyed_ = true;
    SetState(StateValid, false);
    result_.Clear();
    DisposeInternal();
}

bool ClothProcess::IsValid() const
{
    return IsState(StateValid);
}

bool ClothProcess::IsState(int state) const
{
    return state_flag_.IsSet(state);
}

void ClothProcess::SetState(int state, bool enabled)
{
    state_flag_.Set(state, enabled);
}

BitFlag64 ClothProcess::StateFlag() const
{
    return state_flag_;
}

bool ClothProcess::IsCameraCullingInvisible() const
{
    return IsState(StateCameraCullingInvisible);
}

bool ClothProcess::IsCameraCullingKeep() const
{
    return IsState(StateCameraCullingKeep);
}

bool ClothProcess::IsDistanceCullingInvisible() const
{
    return IsState(StateDistanceCullingInvisible);
}

bool ClothProcess::IsSkipWriting() const
{
    return IsState(StateSkipWriting);
}

bool ClothProcess::HasProxyMesh() const
{
    const VirtualMesh* proxy_mesh = proxy_mesh_container.SharedVirtualMesh();
    return IsValid() && proxy_mesh != nullptr && proxy_mesh->IsValid() && proxy_mesh->VertexCount() > 0;
}

const std::string& ClothProcess::Name() const
{
    return name_;
}

ResultStatus ClothProcess::Result() const
{
    return result_;
}

int ClothProcess::TeamId() const
{
    return team_id_;
}

void ClothProcess::SetTeamId(int team_id)
{
    team_id_ = team_id;
}

ClothType ClothProcess::Type() const
{
    return cloth_type_;
}

const ClothParameters& ClothProcess::Parameters() const
{
    return parameters_;
}

void ClothProcess::SyncParameters()
{
    ClothSerializeData copy = serialize_data;
    copy.DataValidate();
    parameters_ = copy.GetClothParameters();
}

void ClothProcess::SetSkipWriting(bool enabled)
{
    SetState(StateSkipWriting, enabled);
    SetState(StateSkipWritingDirty, true);
}

ClothUpdateMode ClothProcess::GetClothUpdateMode() const
{
    if (serialize_data.update_mode == ClothUpdateMode::AnimatorLinkage) {
        return ClothUpdateMode::Normal;
    }
    return serialize_data.update_mode;
}

ResultStatus ClothProcess::GenerateStatusCheck(const float3& scale) const
{
    ResultStatus result = ResultStatus::None();
    if (Abs(scale.x) <= define::system::Epsilon
        || Abs(scale.y) <= define::system::Epsilon
        || Abs(scale.z) <= define::system::Epsilon) {
        result.SetError(ResultCode::Init_ScaleIsZero);
        return result;
    }
    if (scale.x < 0.0f || scale.y < 0.0f || scale.z < 0.0f) {
        result.SetError(ResultCode::Init_NegativeScale);
        return result;
    }

    const float diff_xy = Abs(1.0f - scale.x / scale.y);
    const float diff_xz = Abs(1.0f - scale.x / scale.z);
    constexpr float diff_tolerance = 0.01f;
    if (diff_xy > diff_tolerance || diff_xz > diff_tolerance) {
        result.SetWarning(ResultCode::Init_NonUniformScale);
    }
    return result;
}

bool ClothProcess::GenerateInitialization(
    const InitializationInput& input,
    PreBuildManager* prebuild_manager
)
{
    result_.SetProcess();
    if (destroyed_ || IsState(StateInitComplete)) {
        result_.SetCancel();
        return false;
    }

    serialize_data = input.serialize_data;
    serialize_data2 = input.serialize_data2;
    name_ = input.name.empty() ? "(none)" : input.name;
    cloth_transform_record = input.cloth_transform_record;
    normal_adjustment_transform_record = input.normal_adjustment_transform_record;
    SetState(StateEnable, input.enabled);
    SetState(StateValid, false);

    ResultStatus validation = serialize_data.ValidateForBuild();
    if (validation.IsFailed() || !validation.IsSuccess()) {
        result_.SetResult(validation.Code());
        if (result_.IsNone() || result_.IsResult(ResultCode::Empty)) {
            result_.SetError(ResultCode::CreateCloth_InvalidSerializeData);
        }
        return false;
    }

    serialize_data.DataValidate();
    serialize_data2.DataValidate();

    ResultStatus status_result = GenerateStatusCheck(cloth_transform_record.scale);
    result_.Merge(status_result);
    if (status_result.IsError()) {
        return false;
    }

    SetState(StateInitComplete, true);

    const bool use_prebuild_data = serialize_data2.prebuild_data.UsePreBuild();
    PreBuildManager::ShareDeserializationData* share_prebuild_deserialized = nullptr;
    if (use_prebuild_data) {
        SetState(StateUsePreBuild, true);
        ResultStatus prebuild_result = serialize_data2.prebuild_data.DataValidate();
        if (prebuild_result.IsFailed()) {
            result_.Merge(prebuild_result);
            return false;
        }

        SharePreBuildData* share_prebuild_data = serialize_data2.prebuild_data.GetSharePreBuildData();
        if (share_prebuild_data == nullptr) {
            result_.SetError(ResultCode::PreBuildData_Empty);
            return false;
        }
        if (prebuild_manager == nullptr) {
            result_.SetError(ResultCode::PreBuild_InvalidPreBuildData);
            return false;
        }
        prebuild_manager_ = prebuild_manager;
        registered_prebuild_data_ = share_prebuild_data;
        share_prebuild_deserialized =
            prebuild_manager_->RegisterPreBuildData(*share_prebuild_data, true);
        if (share_prebuild_deserialized == nullptr
            || share_prebuild_deserialized->result.IsFailed()) {
            result_.SetError(ResultCode::PreBuild_SetupDeserializationError);
            return false;
        }
        cloth_transform_record.scale = share_prebuild_data->build_scale;
        proxy_mesh_container = share_prebuild_deserialized->GetProxyMeshContainer();
        proxy_mesh_container.SetUniqueTransformRecords(
            VirtualMeshSerializationData::UniqueTransformRecords(
                serialize_data2.prebuild_data.unique_prebuild_data.proxy_mesh
            )
        );
        render_mesh_info_list.clear();
        const int render_mesh_count = share_prebuild_deserialized->RenderMeshCount();
        render_mesh_info_list.reserve(static_cast<std::size_t>(render_mesh_count));
        for (int index = 0; index < render_mesh_count; ++index) {
            RenderMeshInfo info;
            if (index < static_cast<int>(render_handle_list.size())) {
                info.render_handle = render_handle_list[static_cast<std::size_t>(index)];
            }
            info.render_mesh_container = share_prebuild_deserialized->GetRenderMeshContainer(index);
            if (index < static_cast<int>(
                    serialize_data2.prebuild_data.unique_prebuild_data.render_mesh_list.size())) {
                info.render_mesh_container.SetUniqueTransformRecords(
                    VirtualMeshSerializationData::UniqueTransformRecords(
                        serialize_data2.prebuild_data.unique_prebuild_data
                            .render_mesh_list[static_cast<std::size_t>(index)]
                    )
                );
            }
            render_mesh_info_list.push_back(std::move(info));
        }
        distance_constraint_data = share_prebuild_deserialized->distance_constraint_data;
        bending_constraint_data = share_prebuild_deserialized->bending_constraint_data;
        inertia_constraint_data = share_prebuild_deserialized->inertia_constraint_data;
    }

    cloth_type_ = serialize_data.cloth_type;
    parameters_ = serialize_data.GetClothParameters();
    custom_skinning_bone_records.clear();
    custom_skinning_bone_records.reserve(serialize_data.custom_skinning_setting.skinning_bone_ids.size());
    for (int bone_id : serialize_data.custom_skinning_setting.skinning_bone_ids) {
        custom_skinning_bone_records.push_back(TransformRecord{bone_id});
    }

    result_.SetSuccess();
    SetState(StateValid, true);
    SetState(StateInitSuccess, true);
    return true;
}

bool ClothProcess::PreBuildDataConstruction(
    TeamManager& team_manager,
    TransformManager& transform_manager,
    VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager,
    ClothManager& cloth_manager
)
{
    if (!IsState(StateUsePreBuild)) {
        result_.SetError(ResultCode::CreateCloth_CanNotStart);
        return false;
    }
    return RegisterToManagers(
        team_manager,
        transform_manager,
        virtual_mesh_manager,
        simulation_manager,
        cloth_manager
    );
}

bool ClothProcess::PreBuildDataConstruction(MagicaManager& manager)
{
    return PreBuildDataConstruction(
        manager.Team(),
        manager.Transform(),
        manager.VirtualMesh(),
        manager.Simulation(),
        manager.Cloth()
    );
}

bool ClothProcess::RegisterToManagers(
    TeamManager& team_manager,
    TransformManager& transform_manager,
    VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager,
    ClothManager& cloth_manager
)
{
    if (registered_to_managers_) {
        return true;
    }
    if (!IsValid() || !IsState(StateInitSuccess)) {
        result_.SetError(ResultCode::ClothProcess_Invalid);
        return false;
    }

    VirtualMesh* proxy_mesh = proxy_mesh_container.SharedVirtualMesh();
    if (proxy_mesh == nullptr || !proxy_mesh->IsValid() || proxy_mesh->VertexCount() <= 0) {
        result_.SetError(ResultCode::ClothProcess_Invalid);
        return false;
    }

    try {
        if (!team_manager.IsValidTeam(team_id_)) {
            team_id_ = team_manager.CreateTeam(
                parameters_,
                IsState(StateEnable),
                cloth_type_ == ClothType::BoneSpring
            );
        } else {
            team_manager.SetParameters(team_id_, parameters_);
        }
        if (team_id_ <= 0) {
            result_.SetError(ResultCode::ClothProcess_OverflowTeamCount4096);
            return false;
        }

        TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id_);
        team_data.proxy_mesh_type = proxy_mesh->mesh_type;
        team_data.init_scale = cloth_transform_record.scale;
        team_manager.SetUpdateMode(team_id_, GetClothUpdateMode());
        team_manager.SetAnimationPoseRatio(team_id_, serialize_data.animation_pose_ratio);
        team_manager.SetTimeScale(team_id_, 1.0f);
        team_manager.SetSkipWriting(team_id_, IsSkipWriting());

        virtual_mesh_manager.RegisterProxyMesh(
            team_id_,
            proxy_mesh_container,
            team_manager,
            transform_manager
        );
        particle_chunks_ =
            simulation_manager.RegisterParticleRange(team_id_, proxy_mesh->VertexCount());
        team_data.particle_chunk = particle_chunks_.team_id_chunk;

        cloth_manager.Inertia().Register(team_id_, inertia_constraint_data, team_manager);
        cloth_manager.Distance().Register(team_id_, distance_constraint_data, team_manager);
        cloth_manager.TriangleBending().Register(
            team_id_,
            bending_constraint_data,
            team_manager
        );

        for (RenderMeshInfo& info : render_mesh_info_list) {
            VirtualMesh* mapping_mesh = info.render_mesh_container.SharedVirtualMesh();
            if (mapping_mesh != nullptr && mapping_mesh->IsMapping() && mapping_mesh->IsValid()) {
                info.mapping_chunk = virtual_mesh_manager.RegisterMappingMesh(
                    team_id_,
                    info.render_mesh_container,
                    team_manager,
                    transform_manager
                );
            }
        }

        team_manager.SetEnable(team_id_, IsState(StateEnable));
        registered_to_managers_ = true;
        registered_team_manager_ = &team_manager;
        registered_transform_manager_ = &transform_manager;
        registered_virtual_mesh_manager_ = &virtual_mesh_manager;
        registered_simulation_manager_ = &simulation_manager;
        registered_cloth_manager_ = &cloth_manager;
        SetState(StateBuild, true);
        SetState(StateRunning, true);
        result_.SetSuccess();
        return true;
    } catch (...) {
        result_.SetError(ResultCode::ClothProcess_Exception);
        UnregisterFromManagers(
            team_manager,
            transform_manager,
            virtual_mesh_manager,
            simulation_manager,
            cloth_manager
        );
        return false;
    }
}

bool ClothProcess::RegisterToManagers(MagicaManager& manager)
{
    return RegisterToManagers(
        manager.Team(),
        manager.Transform(),
        manager.VirtualMesh(),
        manager.Simulation(),
        manager.Cloth()
    );
}

bool ClothProcess::ApplyRuntimeBuildResult(
    RuntimeBuildResult build_result,
    TeamManager& team_manager,
    TransformManager& transform_manager,
    VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager,
    ClothManager& cloth_manager
)
{
    if (!IsValid() || !IsState(StateInitSuccess) || IsState(StateUsePreBuild)) {
        result_.SetError(ResultCode::CreateCloth_CanNotStart);
        build_result.Dispose();
        return false;
    }
    if (!build_result.IsValid()) {
        result_.Merge(build_result.result);
        if (!result_.IsError()) {
            result_.SetError(ResultCode::ClothProcess_Invalid);
        }
        build_result.Dispose();
        return false;
    }

    proxy_mesh_container.Dispose();
    for (RenderMeshInfo& info : render_mesh_info_list) {
        info.render_mesh_container.Dispose();
    }
    proxy_mesh_container = std::move(build_result.proxy_mesh_container);
    render_mesh_info_list = std::move(build_result.render_mesh_info_list);
    inertia_constraint_data = std::move(build_result.inertia_constraint_data);
    distance_constraint_data = std::move(build_result.distance_constraint_data);
    bending_constraint_data = std::move(build_result.bending_constraint_data);
    result_.Merge(build_result.result);

    return RegisterToManagers(
        team_manager,
        transform_manager,
        virtual_mesh_manager,
        simulation_manager,
        cloth_manager
    );
}

bool ClothProcess::ApplyRuntimeBuildResult(
    RuntimeBuildResult build_result,
    MagicaManager& manager
)
{
    return ApplyRuntimeBuildResult(
        std::move(build_result),
        manager.Team(),
        manager.Transform(),
        manager.VirtualMesh(),
        manager.Simulation(),
        manager.Cloth()
    );
}

void ClothProcess::UnregisterFromManagers(
    TeamManager& team_manager,
    TransformManager& transform_manager,
    VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager,
    ClothManager& cloth_manager
)
{
    if (!registered_to_managers_ && !team_manager.IsValidTeam(team_id_)) {
        return;
    }

    if (team_manager.IsValidTeam(team_id_)) {
        for (RenderMeshInfo& info : render_mesh_info_list) {
            VirtualMesh* mapping_mesh = info.render_mesh_container.SharedVirtualMesh();
            const int mapping_index = mapping_mesh != nullptr ? mapping_mesh->mapping_id : -1;
            if (mapping_index >= 0) {
                virtual_mesh_manager.ExitMappingMesh(
                    team_id_,
                    mapping_index,
                    team_manager,
                    transform_manager
                );
                mapping_mesh->mapping_id = -1;
            }
            info.mapping_chunk.Clear();
        }

        cloth_manager.TriangleBending().Exit(team_id_, team_manager);
        cloth_manager.Distance().Exit(team_id_, team_manager);
        cloth_manager.Inertia().Exit(team_id_, team_manager);
        simulation_manager.RemoveParticleRange(particle_chunks_);
        particle_chunks_ = SimulationManager::ParticleChunkSet{};
        virtual_mesh_manager.ExitProxyMesh(team_id_, team_manager, transform_manager);
        team_manager.ReleaseTeam(team_id_);
    }

    team_id_ = 0;
    registered_to_managers_ = false;
    registered_team_manager_ = nullptr;
    registered_transform_manager_ = nullptr;
    registered_virtual_mesh_manager_ = nullptr;
    registered_simulation_manager_ = nullptr;
    registered_cloth_manager_ = nullptr;
    SetState(StateBuild, false);
    SetState(StateRunning, false);
}

void ClothProcess::UnregisterFromManagers(MagicaManager& manager)
{
    UnregisterFromManagers(
        manager.Team(),
        manager.Transform(),
        manager.VirtualMesh(),
        manager.Simulation(),
        manager.Cloth()
    );
}

void ClothProcess::StartUse(TeamManager& team_manager)
{
    SetState(StateEnable, true);
    if (team_manager.IsValidTeam(team_id_)) {
        team_manager.SetEnable(team_id_, true);
    }
}

void ClothProcess::StartUse(MagicaManager& manager)
{
    StartUse(manager.Team());
}

void ClothProcess::EndUse(TeamManager& team_manager)
{
    SetState(StateEnable, false);
    if (team_manager.IsValidTeam(team_id_)) {
        team_manager.SetEnable(team_id_, false);
    }
}

void ClothProcess::EndUse(MagicaManager& manager)
{
    EndUse(manager.Team());
}

void ClothProcess::DataUpdate()
{
    serialize_data.DataValidate();
    serialize_data2.DataValidate();
    SyncParameters();
    SetState(StateParameterDirty, true);
}

ClothProcess::RenderMeshInfo* ClothProcess::GetRenderMeshInfo(int index)
{
    if (index < 0 || index >= static_cast<int>(render_mesh_info_list.size())) {
        return nullptr;
    }
    return &render_mesh_info_list[static_cast<std::size_t>(index)];
}

const ClothProcess::RenderMeshInfo* ClothProcess::GetRenderMeshInfo(int index) const
{
    if (index < 0 || index >= static_cast<int>(render_mesh_info_list.size())) {
        return nullptr;
    }
    return &render_mesh_info_list[static_cast<std::size_t>(index)];
}

std::vector<int> ClothProcess::GetUsedTransforms() const
{
    std::vector<int> result = serialize_data2.GetUsedTransforms();
    if (cloth_transform_record.IsValid()) {
        result.push_back(cloth_transform_record.id);
    }
    if (normal_adjustment_transform_record.IsValid()) {
        result.push_back(normal_adjustment_transform_record.id);
    }
    for (const TransformRecord& record : custom_skinning_bone_records) {
        if (record.IsValid()) {
            result.push_back(record.id);
        }
    }
    return result;
}

void ClothProcess::ReplaceTransform(const std::unordered_map<int, int>& replace_dict)
{
    serialize_data2.ReplaceTransform(replace_dict);
    const auto replace_record = [&replace_dict](TransformRecord& record) {
        const auto iterator = replace_dict.find(record.id);
        if (iterator != replace_dict.end()) {
            record.id = iterator->second;
        }
    };
    replace_record(cloth_transform_record);
    replace_record(normal_adjustment_transform_record);
    for (TransformRecord& record : custom_skinning_bone_records) {
        replace_record(record);
    }
}

void ClothProcess::DisposeInternal()
{
    if (destroyed_internal_ || building_) {
        return;
    }
    if (registered_team_manager_ != nullptr
        && registered_transform_manager_ != nullptr
        && registered_virtual_mesh_manager_ != nullptr
        && registered_simulation_manager_ != nullptr
        && registered_cloth_manager_ != nullptr) {
        UnregisterFromManagers(
            *registered_team_manager_,
            *registered_transform_manager_,
            *registered_virtual_mesh_manager_,
            *registered_simulation_manager_,
            *registered_cloth_manager_
        );
    }
    if (prebuild_manager_ != nullptr && registered_prebuild_data_ != nullptr) {
        prebuild_manager_->UnregisterPreBuildData(*registered_prebuild_data_);
    }
    render_mesh_info_list.clear();
    render_handle_list.clear();
    custom_skinning_bone_records.clear();
    proxy_mesh_container.Dispose();
    registered_prebuild_data_ = nullptr;
    prebuild_manager_ = nullptr;
    team_id_ = 0;
    destroyed_internal_ = true;
}

}  // namespace hocloth::mc2
