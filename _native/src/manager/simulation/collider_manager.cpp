#include "hocloth/manager/simulation/collider_manager.hpp"

#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/transform/transform_manager.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <stdexcept>

namespace hocloth::mc2 {

namespace {

float3 Min(const float3& a, const float3& b)
{
    return float3{
        std::min(a.x, b.x),
        std::min(a.y, b.y),
        std::min(a.z, b.z),
    };
}

float3 Max(const float3& a, const float3& b)
{
    return float3{
        std::max(a.x, b.x),
        std::max(a.y, b.y),
        std::max(a.z, b.z),
    };
}

float Sign(float value)
{
    if (value > 0.0f) {
        return 1.0f;
    }
    if (value < 0.0f) {
        return -1.0f;
    }
    return 0.0f;
}

bool IsCapsule(ColliderManager::ColliderType type)
{
    return type >= ColliderManager::ColliderType::CapsuleXCenter
        && type <= ColliderManager::ColliderType::CapsuleZStart;
}

bool IsCenterAlignedCapsule(ColliderManager::ColliderType type)
{
    return type >= ColliderManager::ColliderType::CapsuleXCenter
        && type <= ColliderManager::ColliderType::CapsuleZCenter;
}

float3 CapsuleDirection(ColliderManager::ColliderType type)
{
    switch (type) {
    case ColliderManager::ColliderType::CapsuleXCenter:
    case ColliderManager::ColliderType::CapsuleXStart:
        return float3{1.0f, 0.0f, 0.0f};
    case ColliderManager::ColliderType::CapsuleYCenter:
    case ColliderManager::ColliderType::CapsuleYStart:
        return float3{0.0f, 1.0f, 0.0f};
    case ColliderManager::ColliderType::CapsuleZCenter:
    case ColliderManager::ColliderType::CapsuleZStart:
    default:
        return float3{0.0f, 0.0f, 1.0f};
    }
}

float DotAbsScaleAxis(const float3& scale, const float3& axis)
{
    return scale.x * axis.x + scale.y * axis.y + scale.z * axis.z;
}

}  // namespace

Result ColliderManager::Initialize()
{
    Dispose();
    initialized_ = true;
    constexpr int capacity = 256;
    team_id_array_ = ExNativeArray<short>(capacity);
    flag_array_ = ExNativeArray<BitFlag8>(capacity);
    center_array_ = ExNativeArray<float3>(capacity);
    size_array_ = ExNativeArray<float3>(capacity);
    frame_positions_ = ExNativeArray<float3>(capacity);
    frame_rotations_ = ExNativeArray<quaternion>(capacity);
    frame_scales_ = ExNativeArray<float3>(capacity);
    now_positions_ = ExNativeArray<float3>(capacity);
    now_rotations_ = ExNativeArray<quaternion>(capacity);
    old_frame_positions_ = ExNativeArray<float3>(capacity);
    old_frame_rotations_ = ExNativeArray<quaternion>(capacity);
    old_positions_ = ExNativeArray<float3>(capacity);
    old_rotations_ = ExNativeArray<quaternion>(capacity);
    work_data_array_ = ExNativeArray<WorkData>(capacity);
    return Result::Ok();
}

void ColliderManager::Dispose()
{
    team_id_array_.Dispose();
    flag_array_.Dispose();
    center_array_.Dispose();
    size_array_.Dispose();
    frame_positions_.Dispose();
    frame_rotations_.Dispose();
    frame_scales_.Dispose();
    now_positions_.Dispose();
    now_rotations_.Dispose();
    old_frame_positions_.Dispose();
    old_frame_rotations_.Dispose();
    old_positions_.Dispose();
    old_rotations_.Dispose();
    work_data_array_.Dispose();
    initialized_ = false;
}

ManagerStatus ColliderManager::Status() const
{
    std::ostringstream detail;
    detail << "colliders=" << DataCount()
           << " work_data=" << work_data_array_.Count();
    return ManagerStatus{"ColliderManager", initialized_, static_cast<std::uint32_t>(DataCount()), detail.str()};
}

int ColliderManager::DataCount() const
{
    return team_id_array_.Count();
}

DataChunk ColliderManager::RegisterColliderRange(int team_id, int collider_count)
{
    if (!initialized_) {
        Initialize();
    }
    if (collider_count < 0) {
        throw std::runtime_error("Collider count must be non-negative.");
    }

    const DataChunk chunk =
        team_id_array_.AddRange(collider_count, static_cast<short>(team_id));
    flag_array_.AddRange(collider_count, BitFlag8{});
    center_array_.AddRange(collider_count, float3{});
    size_array_.AddRange(collider_count, float3{});
    frame_positions_.AddRange(collider_count, float3{});
    frame_rotations_.AddRange(collider_count, quaternion{});
    frame_scales_.AddRange(collider_count, float3{1.0f, 1.0f, 1.0f});
    now_positions_.AddRange(collider_count, float3{});
    now_rotations_.AddRange(collider_count, quaternion{});
    old_frame_positions_.AddRange(collider_count, float3{});
    old_frame_rotations_.AddRange(collider_count, quaternion{});
    old_positions_.AddRange(collider_count, float3{});
    old_rotations_.AddRange(collider_count, quaternion{});
    work_data_array_.AddRange(collider_count, WorkData{});
    return chunk;
}

void ColliderManager::RemoveColliderRange(DataChunk chunk)
{
    team_id_array_.RemoveAndFill(chunk, 0);
    flag_array_.RemoveAndFill(chunk, BitFlag8{});
    center_array_.Remove(chunk);
    size_array_.Remove(chunk);
    frame_positions_.Remove(chunk);
    frame_rotations_.Remove(chunk);
    frame_scales_.Remove(chunk);
    now_positions_.Remove(chunk);
    now_rotations_.Remove(chunk);
    old_frame_positions_.Remove(chunk);
    old_frame_rotations_.Remove(chunk);
    old_positions_.Remove(chunk);
    old_rotations_.Remove(chunk);
    work_data_array_.Remove(chunk);
}

void ColliderManager::SetCollider(int collider_index, const ColliderData& data)
{
    if (collider_index < 0 || collider_index >= flag_array_.Length()) {
        return;
    }

    BitFlag8 flag = SetColliderType(BitFlag8{}, data.type);
    flag.SetFlag(FlagValid, data.type != ColliderType::None);
    flag.SetFlag(FlagEnable, data.enabled);
    flag.SetFlag(FlagReverse, data.reverse);
    flag_array_[collider_index] = flag;
    center_array_[collider_index] = data.center;
    size_array_[collider_index] = data.size;
    frame_positions_[collider_index] = data.frame_position;
    frame_rotations_[collider_index] = data.frame_rotation;
    frame_scales_[collider_index] = data.frame_scale;
    now_positions_[collider_index] = data.frame_position;
    now_rotations_[collider_index] = data.frame_rotation;
    old_frame_positions_[collider_index] = data.frame_position;
    old_frame_rotations_[collider_index] = data.frame_rotation;
    old_positions_[collider_index] = data.frame_position;
    old_rotations_[collider_index] = data.frame_rotation;
}

void ColliderManager::PreSimulationUpdate(
    const TeamManager& team_manager,
    const TransformManager& transform_manager
)
{
    // Ported from MC2 ColliderManager.PreSimulationUpdateJob.
    const TransformData& transform_data = transform_manager.Data();
    for (int collider_index = 0; collider_index < DataCount(); ++collider_index) {
        BitFlag8 flag = flag_array_[collider_index];
        if (!flag.IsSet(FlagValid) || !flag.IsSet(FlagEnable)) {
            continue;
        }

        const int team_id = team_id_array_[collider_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess()) {
            continue;
        }

        const int local_index = collider_index - team_data.collider_chunk.start_index;
        const int transform_index = team_data.collider_transform_chunk.start_index + local_index;
        if (transform_index < 0
            || transform_index >= transform_data.position_array.Length()
            || transform_index >= transform_data.rotation_array.Length()
            || transform_index >= transform_data.scale_array.Length()) {
            continue;
        }

        const float3 world_position = transform_data.position_array[transform_index];
        const quaternion world_rotation = transform_data.rotation_array[transform_index];
        const float3 world_scale = transform_data.scale_array[transform_index];
        const float3 rotated_center = Rotate(world_rotation, center_array_[collider_index]);
        const float3 scaled_center{
            rotated_center.x * world_scale.x,
            rotated_center.y * world_scale.y,
            rotated_center.z * world_scale.z,
        };
        const float3 frame_position = Add(world_position, scaled_center);

        frame_positions_[collider_index] = frame_position;
        frame_rotations_[collider_index] = world_rotation;
        frame_scales_[collider_index] = world_scale;

        if (team_data.flag.IsSet(TeamManager::FlagReset) || flag.IsSet(FlagReset)) {
            old_frame_positions_[collider_index] = frame_position;
            old_frame_rotations_[collider_index] = world_rotation;
            now_positions_[collider_index] = frame_position;
            now_rotations_[collider_index] = world_rotation;
            old_positions_[collider_index] = frame_position;
            old_rotations_[collider_index] = world_rotation;

            flag.SetFlag(FlagReset, false);
            flag_array_[collider_index] = flag;
        } else if (
            team_data.flag.IsSet(TeamManager::FlagInertiaShift)
            || team_data.flag.IsSet(TeamManager::FlagNegativeScaleTeleport)
        ) {
            const InertiaCenterData& center_data = team_manager.GetCenterData(team_id);
            float3 old_frame_position = old_frame_positions_[collider_index];
            quaternion old_frame_rotation = old_frame_rotations_[collider_index];
            float3 now_position = now_positions_[collider_index];
            quaternion now_rotation = now_rotations_[collider_index];
            float3 old_position = old_positions_[collider_index];
            quaternion old_rotation = old_rotations_[collider_index];

            if (team_data.flag.IsSet(TeamManager::FlagNegativeScaleTeleport)) {
                const float4x4& negative_matrix = center_data.negative_scale_matrix;
                old_frame_position = TransformPoint(old_frame_position, negative_matrix);
                old_frame_rotation = TransformRotation(
                    old_frame_rotation,
                    negative_matrix,
                    team_data.negative_scale_change
                );
                now_position = TransformPoint(now_position, negative_matrix);
                now_rotation = TransformRotation(
                    now_rotation,
                    negative_matrix,
                    team_data.negative_scale_change
                );
                old_position = TransformPoint(old_position, negative_matrix);
                old_rotation = TransformRotation(
                    old_rotation,
                    negative_matrix,
                    team_data.negative_scale_change
                );
            }

            if (team_data.flag.IsSet(TeamManager::FlagInertiaShift)) {
                old_frame_position = ShiftPosition(
                    old_frame_position,
                    center_data.old_component_world_position,
                    center_data.frame_component_shift_vector,
                    center_data.frame_component_shift_rotation
                );
                old_frame_rotation =
                    Multiply(center_data.frame_component_shift_rotation, old_frame_rotation);
                now_position = ShiftPosition(
                    now_position,
                    center_data.old_component_world_position,
                    center_data.frame_component_shift_vector,
                    center_data.frame_component_shift_rotation
                );
                now_rotation =
                    Multiply(center_data.frame_component_shift_rotation, now_rotation);
                old_position = ShiftPosition(
                    old_position,
                    center_data.old_component_world_position,
                    center_data.frame_component_shift_vector,
                    center_data.frame_component_shift_rotation
                );
                old_rotation =
                    Multiply(center_data.frame_component_shift_rotation, old_rotation);
            }

            old_frame_positions_[collider_index] = old_frame_position;
            old_frame_rotations_[collider_index] = old_frame_rotation;
            now_positions_[collider_index] = now_position;
            now_rotations_[collider_index] = now_rotation;
            old_positions_[collider_index] = old_position;
            old_rotations_[collider_index] = old_rotation;
        }
    }
}

void ColliderManager::CreateUpdateColliderList(
    int update_index,
    const TeamManager& team_manager,
    SimulationManager& simulation_manager
) const
{
    // Ported from MC2 CreateUpdatecolliderListJob.
    for (int team_id = 0; team_id < team_manager.TeamCount(); ++team_id) {
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (!team_data.IsProcess() || update_index >= team_data.update_count) {
            continue;
        }

        for (int offset = 0; offset < team_data.ColliderCount(); ++offset) {
            simulation_manager.MarkStepCollider(team_data.collider_chunk.start_index + offset);
        }
    }
}

void ColliderManager::StartSimulationStep(
    const TeamManager& team_manager,
    const SimulationManager& simulation_manager
)
{
    // Ported from MC2 ColliderManager.StartSimulationStepJob.
    const ExProcessingList<int>& step_colliders = simulation_manager.ProcessingStepColliders();
    const auto& step_buffer = step_colliders.Buffer();
    for (int step_index = 0; step_index < step_colliders.Count(); ++step_index) {
        const int collider_index = step_buffer[static_cast<std::size_t>(step_index)];
        if (collider_index < 0 || collider_index >= flag_array_.Length()) {
            continue;
        }

        const BitFlag8 flag = flag_array_[collider_index];
        if (!flag.IsSet(FlagValid) || !flag.IsSet(FlagEnable)) {
            continue;
        }

        const int team_id = team_id_array_[collider_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const InertiaCenterData& center_data = team_manager.GetCenterData(team_id);
        const float3 position = Lerp(
            old_frame_positions_[collider_index],
            frame_positions_[collider_index],
            team_data.frame_interpolation
        );
        const quaternion rotation = Normalize(Slerp(
            old_frame_rotations_[collider_index],
            frame_rotations_[collider_index],
            team_data.frame_interpolation
        ));
        now_positions_[collider_index] = position;
        now_rotations_[collider_index] = rotation;

        float3 old_position = old_positions_[collider_index];
        quaternion old_rotation = old_rotations_[collider_index];
        old_position = Lerp(old_position, position, center_data.step_move_inertia_ratio);
        old_rotation = Normalize(Slerp(old_rotation, rotation, center_data.step_rotation_inertia_ratio));
        old_positions_[collider_index] = old_position;
        old_rotations_[collider_index] = old_rotation;

        const ColliderType type = TypeFromFlag(flag);
        const float3 size = size_array_[collider_index];
        const float3 scale = frame_scales_[collider_index];
        WorkData work;
        work.inverse_old_rotation = Inverse(old_rotation);
        work.rotation = rotation;

        if (type == ColliderType::Sphere) {
            const float radius = size.x * std::abs(scale.x);
            work.radius = float2{radius, radius};
            AABB bounds{Min(old_position, position), Max(old_position, position)};
            Expand(bounds, radius);
            work.aabb = bounds;
            work.old_positions[0] = old_position;
            work.next_positions[0] = position;
        } else if (IsCapsule(type)) {
            const bool aligned_center = IsCenterAlignedCapsule(type);
            float3 direction = CapsuleDirection(type);
            const float axis_scale = DotAbsScaleAxis(scale, direction);
            direction = Scale(direction, Sign(axis_scale));
            const float scale_value = std::abs(axis_scale);
            if (flag.IsSet(FlagReverse)) {
                direction = Scale(direction, -1.0f);
            }

            const float start_radius = size.x * scale_value;
            const float end_radius = size.y * scale_value;
            const float length = size.z * scale_value;
            float start_length = aligned_center ? length * 0.5f : 0.0f;
            float end_length = aligned_center ? length * 0.5f : (length - start_radius);
            start_length = std::max(start_length - start_radius, 0.0f);
            end_length = std::max(end_length - end_radius, 0.0f);

            const float3 start_old_position =
                Add(old_position, Rotate(old_rotation, Scale(direction, start_length)));
            const float3 end_old_position =
                Subtract(old_position, Rotate(old_rotation, Scale(direction, end_length)));
            const float3 start_position =
                Add(position, Rotate(rotation, Scale(direction, start_length)));
            const float3 end_position =
                Subtract(position, Rotate(rotation, Scale(direction, end_length)));

            AABB bounds{
                Subtract(Min(start_old_position, start_position), float3{start_radius, start_radius, start_radius}),
                Add(Max(start_old_position, start_position), float3{start_radius, start_radius, start_radius}),
            };
            AABB end_bounds{
                Subtract(Min(end_old_position, end_position), float3{end_radius, end_radius, end_radius}),
                Add(Max(end_old_position, end_position), float3{end_radius, end_radius, end_radius}),
            };
            Encapsulate(bounds, end_bounds);

            work.aabb = bounds;
            work.radius = float2{start_radius, end_radius};
            work.old_positions[0] = start_old_position;
            work.old_positions[1] = end_old_position;
            work.next_positions[0] = start_position;
            work.next_positions[1] = end_position;
        } else if (type == ColliderType::Plane) {
            float3 direction{0.0f, 1.0f, 0.0f};
            direction = Scale(direction, Sign(scale.y));
            const float3 normal = Rotate(rotation, direction);
            work.old_positions[0] = normal;
            work.next_positions[0] = position;
        }

        work_data_array_[collider_index] = work;
    }
}

void ColliderManager::EndSimulationStep(const SimulationManager& simulation_manager)
{
    // Ported from MC2 ColliderManager.EndSimulationStepJob.
    const ExProcessingList<int>& step_colliders = simulation_manager.ProcessingStepColliders();
    const auto& step_buffer = step_colliders.Buffer();
    for (int step_index = 0; step_index < step_colliders.Count(); ++step_index) {
        const int collider_index = step_buffer[static_cast<std::size_t>(step_index)];
        if (collider_index < 0 || collider_index >= old_positions_.Length()) {
            continue;
        }
        old_positions_[collider_index] = now_positions_[collider_index];
        old_rotations_[collider_index] = now_rotations_[collider_index];
    }
}

void ColliderManager::PostSimulationUpdate(const TeamManager& team_manager)
{
    // Ported from MC2 ColliderManager.PostSimulationUpdateJob.
    for (int collider_index = 0; collider_index < DataCount(); ++collider_index) {
        const int team_id = team_id_array_[collider_index];
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        if (team_data.IsProcess() && team_data.IsRunning()) {
            old_frame_positions_[collider_index] = frame_positions_[collider_index];
            old_frame_rotations_[collider_index] = frame_rotations_[collider_index];
        }
    }
}

const ExNativeArray<short>& ColliderManager::TeamIds() const
{
    return team_id_array_;
}

const ExNativeArray<BitFlag8>& ColliderManager::Flags() const
{
    return flag_array_;
}

ExNativeArray<BitFlag8>& ColliderManager::Flags()
{
    return flag_array_;
}

const ExNativeArray<float3>& ColliderManager::Centers() const
{
    return center_array_;
}

const ExNativeArray<float3>& ColliderManager::Sizes() const
{
    return size_array_;
}

const ExNativeArray<float3>& ColliderManager::FramePositions() const
{
    return frame_positions_;
}

ExNativeArray<float3>& ColliderManager::FramePositions()
{
    return frame_positions_;
}

const ExNativeArray<quaternion>& ColliderManager::FrameRotations() const
{
    return frame_rotations_;
}

ExNativeArray<quaternion>& ColliderManager::FrameRotations()
{
    return frame_rotations_;
}

const ExNativeArray<float3>& ColliderManager::FrameScales() const
{
    return frame_scales_;
}

ExNativeArray<float3>& ColliderManager::FrameScales()
{
    return frame_scales_;
}

const ExNativeArray<float3>& ColliderManager::OldFramePositions() const
{
    return old_frame_positions_;
}

ExNativeArray<float3>& ColliderManager::OldFramePositions()
{
    return old_frame_positions_;
}

const ExNativeArray<quaternion>& ColliderManager::OldFrameRotations() const
{
    return old_frame_rotations_;
}

ExNativeArray<quaternion>& ColliderManager::OldFrameRotations()
{
    return old_frame_rotations_;
}

const ExNativeArray<float3>& ColliderManager::NowPositions() const
{
    return now_positions_;
}

ExNativeArray<float3>& ColliderManager::NowPositions()
{
    return now_positions_;
}

const ExNativeArray<quaternion>& ColliderManager::NowRotations() const
{
    return now_rotations_;
}

ExNativeArray<quaternion>& ColliderManager::NowRotations()
{
    return now_rotations_;
}

const ExNativeArray<float3>& ColliderManager::OldPositions() const
{
    return old_positions_;
}

ExNativeArray<float3>& ColliderManager::OldPositions()
{
    return old_positions_;
}

const ExNativeArray<quaternion>& ColliderManager::OldRotations() const
{
    return old_rotations_;
}

ExNativeArray<quaternion>& ColliderManager::OldRotations()
{
    return old_rotations_;
}

const ExNativeArray<ColliderManager::WorkData>& ColliderManager::WorkDataArray() const
{
    return work_data_array_;
}

ExNativeArray<ColliderManager::WorkData>& ColliderManager::WorkDataArray()
{
    return work_data_array_;
}

ColliderManager::ColliderType ColliderManager::TypeFromFlag(BitFlag8 flag)
{
    return static_cast<ColliderType>(flag.Value() & 0x0fu);
}

BitFlag8 ColliderManager::SetColliderType(BitFlag8 flag, ColliderType type)
{
    const std::uint8_t value =
        static_cast<std::uint8_t>((flag.Value() & 0xf0u) | static_cast<std::uint8_t>(type));
    return BitFlag8{value};
}

}  // namespace hocloth::mc2
