#include "hocloth/manager/simulation/collider_manager.hpp"

#include <sstream>
#include <stdexcept>

namespace hocloth::mc2 {

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

const ExNativeArray<quaternion>& ColliderManager::FrameRotations() const
{
    return frame_rotations_;
}

const ExNativeArray<float3>& ColliderManager::FrameScales() const
{
    return frame_scales_;
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
