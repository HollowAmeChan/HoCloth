#include "hocloth/manager/transform/transform_manager.hpp"

#include "hocloth/utility/math/math_utility.hpp"
#include "hocloth/virtual_mesh/virtual_mesh_container.hpp"

#include <algorithm>
#include <sstream>

namespace hocloth::mc2 {

Result TransformManager::Initialize()
{
    Dispose();
    data_.Initialize(256);
    initialized_ = true;
    return Result::Ok();
}

void TransformManager::Dispose()
{
    data_.Dispose();
    initialized_ = false;
}

ManagerStatus TransformManager::Status() const
{
    std::ostringstream detail;
    detail << "length=" << data_.flag_array.Length();
    return ManagerStatus{"TransformManager", initialized_, static_cast<std::uint32_t>(Count()), detail.str()};
}

bool TransformManager::IsValid() const
{
    return initialized_;
}

int TransformManager::Count() const
{
    return initialized_ ? data_.Count() : 0;
}

const TransformData& TransformManager::Data() const
{
    return data_;
}

TransformData& TransformManager::Data()
{
    return data_;
}

DataChunk TransformManager::AddTransform(int count, int team_id)
{
    if (!initialized_ || count <= 0) {
        return DataChunk::Empty();
    }

    const DataChunk chunk = data_.flag_array.AddRange(count);
    data_.init_local_position_array.AddRange(count);
    data_.init_local_rotation_array.AddRange(count);
    data_.position_array.AddRange(count);
    data_.rotation_array.AddRange(count);
    data_.inverse_rotation_array.AddRange(count);
    data_.scale_array.AddRange(count);
    data_.local_position_array.AddRange(count);
    data_.local_rotation_array.AddRange(count);
    data_.local_to_world_matrix_array.AddRange(count);
    data_.team_id_array.AddRange(count, static_cast<std::int16_t>(team_id));

    data_.EnsureRecordCapacity(chunk.EndIndex());
    for (int i = 0; i < count; ++i) {
        SetDefaultTransform(chunk.start_index + i, team_id);
    }
    return chunk;
}

DataChunk TransformManager::AddTransform(const VirtualMeshContainer& container, int team_id)
{
    if (!initialized_) {
        return DataChunk::Empty();
    }

    const int count = container.GetTransformCount();
    const DataChunk chunk = AddTransform(count, team_id);
    BitFlag8 flag{FlagRead};
    flag.SetFlag(FlagEnable, true);

    for (int i = 0; i < count; ++i) {
        const TransformRecord record = container.GetTransformRecordFromIndex(i);
        if (record.IsValid()) {
            SetTransform(chunk.start_index + i, record, flag, team_id);
        }
    }
    return chunk;
}

DataChunk TransformManager::AddTransform(const TransformRecord& record, BitFlag8 flag, int team_id)
{
    if (!initialized_ || !record.IsValid()) {
        return DataChunk::Empty();
    }

    const DataChunk chunk = data_.flag_array.Add(flag);
    data_.init_local_position_array.Add(record.local_position);
    data_.init_local_rotation_array.Add(record.local_rotation);
    data_.position_array.Add(record.position);
    data_.rotation_array.Add(record.rotation);
    data_.inverse_rotation_array.Add(Inverse(record.rotation));
    data_.scale_array.Add(record.scale);
    data_.local_position_array.Add(record.local_position);
    data_.local_rotation_array.Add(record.local_rotation);
    data_.local_to_world_matrix_array.Add(record.local_to_world_matrix);
    data_.team_id_array.Add(static_cast<std::int16_t>(team_id));

    data_.EnsureRecordCapacity(chunk.EndIndex());
    SetTransformAccessSlot(chunk.start_index, &record);
    return chunk;
}

void TransformManager::SetTransform(int index, const TransformRecord& record, BitFlag8 flag, int team_id)
{
    if (!initialized_ || index < 0 || index >= data_.flag_array.Length()) {
        return;
    }
    if (!record.IsValid()) {
        ClearTransform(index);
        return;
    }

    data_.flag_array[index] = flag;
    data_.init_local_position_array[index] = record.local_position;
    data_.init_local_rotation_array[index] = record.local_rotation;
    data_.position_array[index] = record.position;
    data_.rotation_array[index] = record.rotation;
    data_.inverse_rotation_array[index] = Inverse(record.rotation);
    data_.scale_array[index] = record.scale;
    data_.local_position_array[index] = record.local_position;
    data_.local_rotation_array[index] = record.local_rotation;
    data_.local_to_world_matrix_array[index] = record.local_to_world_matrix;
    data_.team_id_array[index] = static_cast<std::int16_t>(team_id);

    data_.EnsureRecordCapacity(index + 1);
    SetTransformAccessSlot(index, &record);
}

void TransformManager::ClearTransform(int index)
{
    if (!initialized_ || index < 0 || index >= data_.flag_array.Length()) {
        return;
    }

    data_.flag_array[index] = BitFlag8{};
    data_.team_id_array[index] = 0;
    data_.is_dirty = true;
    data_.EnsureRecordCapacity(index + 1);
    data_.id_array[static_cast<std::size_t>(index)] = 0;
    data_.parent_id_array[static_cast<std::size_t>(index)] = 0;
    data_.name_array[static_cast<std::size_t>(index)].clear();
}

void TransformManager::CopyTransform(int from_index, int to_index)
{
    if (!initialized_ || from_index < 0 || to_index < 0) {
        return;
    }
    if (from_index >= data_.flag_array.Length() || to_index >= data_.flag_array.Length()) {
        return;
    }

    data_.flag_array[to_index] = data_.flag_array[from_index];
    data_.init_local_position_array[to_index] = data_.init_local_position_array[from_index];
    data_.init_local_rotation_array[to_index] = data_.init_local_rotation_array[from_index];
    data_.position_array[to_index] = data_.position_array[from_index];
    data_.rotation_array[to_index] = data_.rotation_array[from_index];
    data_.inverse_rotation_array[to_index] = data_.inverse_rotation_array[from_index];
    data_.scale_array[to_index] = data_.scale_array[from_index];
    data_.local_position_array[to_index] = data_.local_position_array[from_index];
    data_.local_rotation_array[to_index] = data_.local_rotation_array[from_index];
    data_.local_to_world_matrix_array[to_index] = data_.local_to_world_matrix_array[from_index];
    data_.team_id_array[to_index] = data_.team_id_array[from_index];

    data_.EnsureRecordCapacity(std::max(from_index, to_index) + 1);
    data_.id_array[static_cast<std::size_t>(to_index)] = data_.id_array[static_cast<std::size_t>(from_index)];
    data_.parent_id_array[static_cast<std::size_t>(to_index)] =
        data_.parent_id_array[static_cast<std::size_t>(from_index)];
    data_.name_array[static_cast<std::size_t>(to_index)] =
        data_.name_array[static_cast<std::size_t>(from_index)];
    data_.is_dirty = true;
}

void TransformManager::RemoveTransform(DataChunk chunk)
{
    if (!initialized_ || !chunk.IsValid()) {
        return;
    }

    data_.flag_array.RemoveAndFill(chunk);
    data_.init_local_position_array.Remove(chunk);
    data_.init_local_rotation_array.Remove(chunk);
    data_.position_array.Remove(chunk);
    data_.rotation_array.Remove(chunk);
    data_.inverse_rotation_array.Remove(chunk);
    data_.scale_array.Remove(chunk);
    data_.local_position_array.Remove(chunk);
    data_.local_rotation_array.Remove(chunk);
    data_.local_to_world_matrix_array.Remove(chunk);
    data_.team_id_array.RemoveAndFill(chunk, 0);

    data_.EnsureRecordCapacity(chunk.EndIndex());
    for (int i = 0; i < chunk.data_length; ++i) {
        const auto index = static_cast<std::size_t>(chunk.start_index + i);
        data_.id_array[index] = 0;
        data_.parent_id_array[index] = 0;
        data_.name_array[index].clear();
    }
    data_.is_dirty = true;
}

void TransformManager::EnableTransform(DataChunk chunk, bool enabled)
{
    if (!initialized_ || !chunk.IsValid()) {
        return;
    }

    for (int i = 0; i < chunk.data_length; ++i) {
        EnableTransform(chunk.start_index + i, enabled);
    }
}

void TransformManager::EnableTransform(int index, bool enabled)
{
    if (!initialized_ || index < 0 || index >= data_.flag_array.Length()) {
        return;
    }

    BitFlag8 flag = data_.flag_array[index];
    if (flag.Value() == 0) {
        return;
    }
    flag.SetFlag(FlagEnable, enabled);
    data_.flag_array[index] = flag;
    data_.is_dirty = true;
}

DataChunk TransformManager::Expand(DataChunk chunk, int new_length)
{
    if (!initialized_ || !chunk.IsValid()) {
        return DataChunk::Empty();
    }

    const DataChunk new_chunk = data_.flag_array.Expand(chunk, new_length);
    data_.init_local_position_array.Expand(chunk, new_length);
    data_.init_local_rotation_array.Expand(chunk, new_length);
    data_.position_array.Expand(chunk, new_length);
    data_.rotation_array.Expand(chunk, new_length);
    data_.inverse_rotation_array.Expand(chunk, new_length);
    data_.scale_array.Expand(chunk, new_length);
    data_.local_position_array.Expand(chunk, new_length);
    data_.local_rotation_array.Expand(chunk, new_length);
    data_.local_to_world_matrix_array.Expand(chunk, new_length);
    data_.team_id_array.Expand(chunk, new_length);

    data_.EnsureRecordCapacity(new_chunk.EndIndex());
    if (chunk.start_index != new_chunk.start_index) {
        for (int i = 0; i < chunk.data_length; ++i) {
            const auto from = static_cast<std::size_t>(chunk.start_index + i);
            const auto to = static_cast<std::size_t>(new_chunk.start_index + i);
            data_.id_array[to] = data_.id_array[from];
            data_.parent_id_array[to] = data_.parent_id_array[from];
            data_.name_array[to] = data_.name_array[from];
            data_.id_array[from] = 0;
            data_.parent_id_array[from] = 0;
            data_.name_array[from].clear();
        }
    }

    for (int i = chunk.data_length; i < new_chunk.data_length; ++i) {
        SetDefaultTransform(new_chunk.start_index + i, data_.team_id_array[new_chunk.start_index]);
    }
    return new_chunk;
}

TransformRecord TransformManager::GetRecord(int index) const
{
    if (!initialized_ || index < 0 || index >= data_.flag_array.Length()) {
        return TransformRecord{};
    }

    TransformRecord record;
    record.id = index < static_cast<int>(data_.id_array.size()) ? data_.id_array[static_cast<std::size_t>(index)] : 0;
    record.parent_id = index < static_cast<int>(data_.parent_id_array.size())
        ? data_.parent_id_array[static_cast<std::size_t>(index)]
        : 0;
    record.name = index < static_cast<int>(data_.name_array.size()) ? data_.name_array[static_cast<std::size_t>(index)] : "";
    record.local_position = data_.local_position_array[index];
    record.local_rotation = data_.local_rotation_array[index];
    record.position = data_.position_array[index];
    record.rotation = data_.rotation_array[index];
    record.scale = data_.scale_array[index];
    record.local_to_world_matrix = data_.local_to_world_matrix_array[index];
    record.world_to_local_matrix = InverseAffine(record.local_to_world_matrix);
    return record;
}

int TransformManager::GetTransformIndexFromId(int transform_id) const
{
    if (!initialized_ || transform_id == 0) {
        return -1;
    }
    for (std::size_t index = 0; index < data_.id_array.size(); ++index) {
        if (data_.id_array[index] == transform_id) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int TransformManager::GetTransformIdFromIndex(int index) const
{
    if (!initialized_ || index < 0 || index >= static_cast<int>(data_.id_array.size())) {
        return 0;
    }
    return data_.id_array[static_cast<std::size_t>(index)];
}

int TransformManager::GetParentIdFromIndex(int index) const
{
    if (!initialized_ || index < 0 || index >= static_cast<int>(data_.parent_id_array.size())) {
        return 0;
    }
    return data_.parent_id_array[static_cast<std::size_t>(index)];
}

float4x4 TransformManager::GetLocalToWorldMatrix(int index) const
{
    if (!initialized_ || index < 0 || index >= data_.local_to_world_matrix_array.Length()) {
        return float4x4{};
    }
    return data_.local_to_world_matrix_array[index];
}

quaternion TransformManager::GetInverseRotation(int index) const
{
    if (!initialized_ || index < 0 || index >= data_.inverse_rotation_array.Length()) {
        return quaternion{};
    }
    return data_.inverse_rotation_array[index];
}

bool TransformManager::IsDirty() const
{
    return initialized_ && data_.is_dirty;
}

void TransformManager::SetDirty(bool dirty)
{
    if (initialized_) {
        data_.is_dirty = dirty;
    }
}

int TransformManager::RootCount() const
{
    return initialized_ ? data_.RootCount() : 0;
}

void TransformManager::AddRootId(int transform_id)
{
    if (!initialized_ || transform_id == 0) {
        return;
    }
    if (std::find(data_.root_id_list.begin(), data_.root_id_list.end(), transform_id)
        != data_.root_id_list.end()) {
        return;
    }
    data_.root_id_list.push_back(transform_id);
    data_.is_dirty = true;
}

void TransformManager::SetDefaultTransform(int index, int team_id)
{
    TransformRecord record;
    record.id = 0;
    record.parent_id = 0;
    record.scale = float3{1.0f, 1.0f, 1.0f};
    record.local_to_world_matrix = TRS(record.position, record.rotation, record.scale);

    data_.flag_array[index] = BitFlag8{};
    data_.init_local_position_array[index] = record.local_position;
    data_.init_local_rotation_array[index] = record.local_rotation;
    data_.position_array[index] = record.position;
    data_.rotation_array[index] = record.rotation;
    data_.inverse_rotation_array[index] = Inverse(record.rotation);
    data_.scale_array[index] = record.scale;
    data_.local_position_array[index] = record.local_position;
    data_.local_rotation_array[index] = record.local_rotation;
    data_.local_to_world_matrix_array[index] = record.local_to_world_matrix;
    data_.team_id_array[index] = static_cast<std::int16_t>(team_id);
    data_.is_dirty = true;
    SetTransformAccessSlot(index, &record);
}

void TransformManager::SetTransformAccessSlot(int index, const TransformRecord* record)
{
    data_.EnsureRecordCapacity(index + 1);
    const auto slot = static_cast<std::size_t>(index);
    if (record == nullptr) {
        data_.id_array[slot] = 0;
        data_.parent_id_array[slot] = 0;
        data_.name_array[slot].clear();
        return;
    }
    data_.id_array[slot] = record->id;
    data_.parent_id_array[slot] = record->parent_id;
    data_.name_array[slot] = record->name;
}

}  // namespace hocloth::mc2
