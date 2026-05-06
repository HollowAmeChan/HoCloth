#pragma once

#include "hocloth/manager/i_manager.hpp"
#include "hocloth/manager/transform/transform_data.hpp"

#include <cstdint>

namespace hocloth::mc2 {

class VirtualMeshContainer;

// Port target for Magica Cloth 2: Scripts/Core/Manager/TransformManager/TransformManager.cs
class TransformManager final : public IManager {
public:
    static constexpr std::uint8_t FlagRead = 0x01;
    static constexpr std::uint8_t FlagWorldRotWrite = 0x02;
    static constexpr std::uint8_t FlagLocalPosRotWrite = 0x04;
    static constexpr std::uint8_t FlagRestore = 0x08;
    static constexpr std::uint8_t FlagEnable = 0x10;

    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] int Count() const;
    [[nodiscard]] const TransformData& Data() const;
    [[nodiscard]] TransformData& Data();

    [[nodiscard]] DataChunk AddTransform(int count, int team_id);
    [[nodiscard]] DataChunk AddTransform(const TransformRecord& record, BitFlag8 flag, int team_id);
    [[nodiscard]] DataChunk AddTransform(const VirtualMeshContainer& container, int team_id);
    void SetTransform(int index, const TransformRecord& record, BitFlag8 flag, int team_id);
    void ClearTransform(int index);
    void CopyTransform(int from_index, int to_index);
    void RemoveTransform(DataChunk chunk);
    void EnableTransform(DataChunk chunk, bool enabled);
    void EnableTransform(int index, bool enabled);
    [[nodiscard]] DataChunk Expand(DataChunk chunk, int new_length);

    [[nodiscard]] TransformRecord GetRecord(int index) const;
    [[nodiscard]] int GetTransformIndexFromId(int transform_id) const;
    [[nodiscard]] int GetTransformIdFromIndex(int index) const;
    [[nodiscard]] int GetParentIdFromIndex(int index) const;
    [[nodiscard]] float4x4 GetLocalToWorldMatrix(int index) const;
    [[nodiscard]] quaternion GetInverseRotation(int index) const;
    [[nodiscard]] bool IsDirty() const;
    void SetDirty(bool dirty);
    [[nodiscard]] int RootCount() const;
    void AddRootId(int transform_id);

private:
    bool initialized_ = false;
    TransformData data_;

    void SetDefaultTransform(int index, int team_id);
    void SetTransformAccessSlot(int index, const TransformRecord* record);
};

}  // namespace hocloth::mc2
