#pragma once

#include "hocloth/cloth/wind/magica_wind_zone.hpp"
#include "hocloth/manager/i_manager.hpp"
#include "hocloth/utility/native_collection/bit_flag.hpp"
#include "hocloth/utility/native_collection/data_chunk.hpp"
#include "hocloth/utility/native_collection/ex_native_array.hpp"

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Manager/Simulation/WindManager.cs
class WindManager final : public IManager {
public:
    static constexpr int FlagValid = 0;
    static constexpr int FlagEnable = 1;
    static constexpr int FlagAddition = 2;

    struct WindData {
        BitFlag64 flag;
        WindZoneMode mode = WindZoneMode::GlobalDirection;
        float3 size{};
        float main = 0.0f;
        float turbulence = 0.0f;
        float zone_volume = 0.0f;
        float3 world_wind_direction{};
        float3 world_position{};
        quaternion world_rotation{};
        float3 world_scale{1.0f, 1.0f, 1.0f};
        float4x4 world_to_local_matrix{};
        float4x4 attenuation{};

        [[nodiscard]] bool IsValid() const;
        [[nodiscard]] bool IsEnabled() const;
        [[nodiscard]] bool IsAddition() const;
    };

    Result Initialize() override;
    void Dispose() override;
    [[nodiscard]] ManagerStatus Status() const override;

    [[nodiscard]] int WindCount() const;
    [[nodiscard]] bool IsValidWind(int wind_id) const;
    [[nodiscard]] const WindData& GetWindData(int wind_id) const;
    [[nodiscard]] WindData& GetWindData(int wind_id);
    [[nodiscard]] const ExNativeArray<WindData>& WindDataArray() const;
    [[nodiscard]] ExNativeArray<WindData>& WindDataArray();

    [[nodiscard]] int AddWind();
    [[nodiscard]] int AddWind(const MagicaWindZone& zone, bool enabled = false);
    void RemoveWind(int wind_id);
    void SetEnable(int wind_id, bool enabled);
    void UpdateWind(int wind_id, const MagicaWindZone& zone);
    void AlwaysWindUpdate();

private:
    bool initialized_ = false;
    ExNativeArray<WindData> wind_data_array_;
    ExNativeArray<MagicaWindZone> wind_zone_array_;
};

}  // namespace hocloth::mc2
