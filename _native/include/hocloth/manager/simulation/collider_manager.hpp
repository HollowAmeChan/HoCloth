#pragma once

#include "hocloth/manager/i_manager.hpp"
#include "hocloth/utility/math/math_types.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

#include <cstdint>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Simulation/ColliderManager.cs
class ColliderManager final : public IManager {
public:
    enum class ColliderType : std::uint8_t {
        None = 0,
        Sphere = 1,
        CapsuleXCenter = 2,
        CapsuleYCenter = 3,
        CapsuleZCenter = 4,
        CapsuleXStart = 5,
        CapsuleYStart = 6,
        CapsuleZStart = 7,
        Plane = 8,
        Box = 9,
    };

    static constexpr std::uint8_t FlagValid = 0x10;
    static constexpr std::uint8_t FlagEnable = 0x20;
    static constexpr std::uint8_t FlagReset = 0x40;
    static constexpr std::uint8_t FlagReverse = 0x80;

    struct WorkData {
        AABB aabb;
        float2 radius;
        float3 old_positions[2]{};
        float3 next_positions[2]{};
        quaternion inverse_old_rotation{};
        quaternion rotation{};
    };

    struct ColliderData {
        ColliderType type = ColliderType::None;
        bool enabled = true;
        bool reverse = false;
        float3 center{};
        float3 size{};
        float3 frame_position{};
        quaternion frame_rotation{};
        float3 frame_scale{1.0f, 1.0f, 1.0f};
    };

    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] int DataCount() const;
    [[nodiscard]] DataChunk RegisterColliderRange(int team_id, int collider_count);
    void RemoveColliderRange(DataChunk chunk);
    void SetCollider(int collider_index, const ColliderData& data);

    [[nodiscard]] const ExNativeArray<short>& TeamIds() const;
    [[nodiscard]] const ExNativeArray<BitFlag8>& Flags() const;
    [[nodiscard]] ExNativeArray<BitFlag8>& Flags();
    [[nodiscard]] const ExNativeArray<float3>& Centers() const;
    [[nodiscard]] const ExNativeArray<float3>& Sizes() const;
    [[nodiscard]] const ExNativeArray<float3>& FramePositions() const;
    [[nodiscard]] const ExNativeArray<quaternion>& FrameRotations() const;
    [[nodiscard]] const ExNativeArray<float3>& FrameScales() const;
    [[nodiscard]] const ExNativeArray<float3>& NowPositions() const;
    [[nodiscard]] ExNativeArray<float3>& NowPositions();
    [[nodiscard]] const ExNativeArray<quaternion>& NowRotations() const;
    [[nodiscard]] ExNativeArray<quaternion>& NowRotations();
    [[nodiscard]] const ExNativeArray<float3>& OldPositions() const;
    [[nodiscard]] ExNativeArray<float3>& OldPositions();
    [[nodiscard]] const ExNativeArray<quaternion>& OldRotations() const;
    [[nodiscard]] ExNativeArray<quaternion>& OldRotations();
    [[nodiscard]] const ExNativeArray<WorkData>& WorkDataArray() const;
    [[nodiscard]] ExNativeArray<WorkData>& WorkDataArray();

    [[nodiscard]] static ColliderType TypeFromFlag(BitFlag8 flag);
    [[nodiscard]] static BitFlag8 SetColliderType(BitFlag8 flag, ColliderType type);

private:
    bool initialized_ = false;
    ExNativeArray<short> team_id_array_;
    ExNativeArray<BitFlag8> flag_array_;
    ExNativeArray<float3> center_array_;
    ExNativeArray<float3> size_array_;
    ExNativeArray<float3> frame_positions_;
    ExNativeArray<quaternion> frame_rotations_;
    ExNativeArray<float3> frame_scales_;
    ExNativeArray<float3> now_positions_;
    ExNativeArray<quaternion> now_rotations_;
    ExNativeArray<float3> old_frame_positions_;
    ExNativeArray<quaternion> old_frame_rotations_;
    ExNativeArray<float3> old_positions_;
    ExNativeArray<quaternion> old_rotations_;
    ExNativeArray<WorkData> work_data_array_;
};

}  // namespace hocloth::mc2
