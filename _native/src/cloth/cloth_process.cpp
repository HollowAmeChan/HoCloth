#include "hocloth/cloth/cloth_process.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/cloth/cloth_manager.hpp"
#include "hocloth/manager/magica_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_serialization.hpp"

#include <algorithm>
#include <memory>
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

namespace {

TransformRecord ResolveTransformRecordById(
    int transform_id,
    const std::vector<TransformRecord>& records,
    const RenderSetupData* render_setup = nullptr
)
{
    if (transform_id == 0) {
        return TransformRecord{};
    }
    if (render_setup != nullptr) {
        TransformRecord record = render_setup->GetTransformRecordFromId(transform_id);
        if (record.IsValid()) {
            return record;
        }
    }
    for (const TransformRecord& record : records) {
        if (record.id == transform_id) {
            return record;
        }
    }
    return TransformRecord{transform_id};
}

}  // namespace

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
    ApplyPendingManagerUpdates();
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
        custom_skinning_bone_records.push_back(
            ResolveTransformRecordById(bone_id, input.custom_skinning_bone_records)
        );
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
        team_data.flag.Set(
            TeamManager::FlagSpring,
            cloth_type_ == ClothType::BoneSpring
                && parameters_.spring_constraint.spring_power > 0.0f
        );
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
        cloth_manager.SelfCollision().Register(
            team_id_,
            team_manager,
            virtual_mesh_manager
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

ClothProcess::RuntimeBuildResult ClothProcess::CreateBoneRuntimeBuildResult(
    const RenderSetupData& render_setup
) const
{
    RuntimeBuildResult build_result;
    if (cloth_type_ != ClothType::BoneCloth && cloth_type_ != ClothType::BoneSpring) {
        build_result.result.SetError(ResultCode::RenderSetup_InvalidType);
        return build_result;
    }

    auto proxy_mesh = std::make_shared<VirtualMesh>();
    proxy_mesh->ImportFromRenderSetup(render_setup);
    if (!proxy_mesh->IsValid()) {
        build_result.result.SetError(ResultCode::VirtualMesh_InvalidSetup);
        return build_result;
    }

    if (cloth_type_ == ClothType::BoneCloth) {
        SelectionData selection = serialize_data2.selection_data.IsValid()
            ? serialize_data2.selection_data.Clone()
            : SelectionData::CreateBoneClothDefault(render_setup);
        for (const auto& [transform_id, attribute] : serialize_data2.bone_attribute_dict) {
            const int transform_index = render_setup.GetTransformIndexFromId(transform_id);
            if (transform_index < 0 || transform_index >= selection.Count()) {
                continue;
            }
            selection.attributes[static_cast<std::size_t>(transform_index)] = attribute;
        }
        if (selection.IsValid()) {
            proxy_mesh->ApplySelectionAttribute(selection, true);
        }
    }

    proxy_mesh->Optimization();
    if (!proxy_mesh->IsValid()) {
        build_result.result.SetError(ResultCode::VirtualMesh_ImportError);
        return build_result;
    }
    std::vector<TransformRecord> custom_skinning_records;
    custom_skinning_records.reserve(custom_skinning_bone_records.size());
    for (const TransformRecord& record : custom_skinning_bone_records) {
        custom_skinning_records.push_back(
            ResolveTransformRecordById(record.id, custom_skinning_bone_records, &render_setup)
        );
    }

    TransformRecord normal_adjustment_record = normal_adjustment_transform_record.IsValid()
        ? ResolveTransformRecordById(
              normal_adjustment_transform_record.id,
              {normal_adjustment_transform_record},
              &render_setup
          )
        : TransformRecord{};
    if (!normal_adjustment_record.IsValid()
        && serialize_data.normal_alignment_setting.adjustment_transform_id != 0) {
        normal_adjustment_record = render_setup.GetTransformRecordFromId(
            serialize_data.normal_alignment_setting.adjustment_transform_id
        );
    }
    if (!normal_adjustment_record.IsValid()
        && serialize_data.normal_alignment_setting.alignment_mode
            == NormalAlignmentSettings::AlignmentMode::Transform) {
        normal_adjustment_record = cloth_transform_record;
    }

    proxy_mesh->ConvertProxyMesh(
        serialize_data,
        cloth_transform_record,
        custom_skinning_records,
        normal_adjustment_record
    );
    if (!proxy_mesh->IsValid()) {
        build_result.result.SetError(ResultCode::VirtualMesh_ImportError);
        return build_result;
    }

    build_result.proxy_mesh_container = VirtualMeshContainer(proxy_mesh);
    build_result.distance_constraint_data =
        DistanceConstraint::CreateData(*proxy_mesh, parameters_);
    build_result.bending_constraint_data =
        TriangleBendingConstraint::CreateData(*proxy_mesh, parameters_);
    build_result.inertia_constraint_data =
        InertiaConstraint::CreateData(*proxy_mesh, parameters_);
    build_result.result.SetSuccess();
    return build_result;
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

        team_manager.GetTeamData(team_id_).flag.Set(TeamManager::FlagExit, true);
        cloth_manager.SelfCollision().Exit(
            team_id_,
            team_manager,
            virtual_mesh_manager
        );
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
    ApplyPendingManagerUpdates();
}

void ClothProcess::ApplyPendingManagerUpdates()
{
    if (registered_team_manager_ == nullptr
        || registered_virtual_mesh_manager_ == nullptr
        || registered_cloth_manager_ == nullptr) {
        return;
    }
    ApplyPendingManagerUpdates(
        *registered_team_manager_,
        *registered_virtual_mesh_manager_,
        *registered_cloth_manager_
    );
}

void ClothProcess::ApplyPendingManagerUpdates(
    TeamManager& team_manager,
    VirtualMeshManager& virtual_mesh_manager,
    ClothManager& cloth_manager
)
{
    if (!team_manager.IsValidTeam(team_id_)) {
        SetState(StateParameterDirty, false);
        SetState(StateSkipWritingDirty, false);
        return;
    }

    TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id_);
    if (IsState(StateParameterDirty)) {
        SyncParameters();
        team_manager.SetParameters(team_id_, parameters_);
        team_data.update_mode = GetClothUpdateMode();
        team_data.animation_pose_ratio = Clamp01(serialize_data.animation_pose_ratio);
        team_data.flag.Set(
            TeamManager::FlagSpring,
            cloth_type_ == ClothType::BoneSpring
                && parameters_.spring_constraint.spring_power > 0.0f
        );
        cloth_manager.SelfCollision().Register(
            team_id_,
            team_manager,
            virtual_mesh_manager
        );
        SetState(StateParameterDirty, false);
    }

    if (IsState(StateSkipWritingDirty)) {
        team_data.flag.Set(TeamManager::FlagSkipWriting, IsSkipWriting());
        SetState(StateSkipWritingDirty, false);
    }
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
