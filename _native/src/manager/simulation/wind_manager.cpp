#include "hocloth/manager/simulation/wind_manager.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace hocloth::mc2 {

namespace {

float CalculateZoneVolume(const WindManager::WindData& wind)
{
    constexpr float pi = 3.14159265358979323846f;
    switch (wind.mode) {
    case WindZoneMode::GlobalDirection:
        return std::numeric_limits<float>::max();
    case WindZoneMode::BoxDirection: {
        const float3 global_size{
            wind.size.x * wind.world_scale.x,
            wind.size.y * wind.world_scale.y,
            wind.size.z * wind.world_scale.z,
        };
        return std::abs(global_size.x * global_size.y * global_size.z);
    }
    case WindZoneMode::SphereDirection:
    case WindZoneMode::SphereRadial: {
        const float radius = std::abs(wind.size.x * wind.world_scale.x);
        return (4.0f / 3.0f) * radius * radius * radius * pi;
    }
    }
    return 0.0f;
}

}  // namespace

bool WindManager::WindData::IsValid() const
{
    return flag.IsSet(FlagValid);
}

bool WindManager::WindData::IsEnabled() const
{
    return flag.IsSet(FlagEnable);
}

bool WindManager::WindData::IsAddition() const
{
    return flag.IsSet(FlagAddition);
}

Result WindManager::Initialize()
{
    Dispose();
    initialized_ = true;
    wind_data_array_ = ExNativeArray<WindData>(64);
    wind_zone_array_ = ExNativeArray<MagicaWindZone>(64);
    return Result::Ok();
}

void WindManager::Dispose()
{
    wind_data_array_.Dispose();
    wind_zone_array_.Dispose();
    initialized_ = false;
}

ManagerStatus WindManager::Status() const
{
    int valid_count = 0;
    for (int index = 0; index < wind_data_array_.Length(); ++index) {
        if (wind_data_array_[index].IsValid()) {
            ++valid_count;
        }
    }
    return ManagerStatus{
        "WindManager",
        initialized_,
        static_cast<std::uint32_t>(valid_count),
        "count=" + std::to_string(WindCount()),
    };
}

int WindManager::WindCount() const
{
    return wind_data_array_.Count();
}

bool WindManager::IsValidWind(int wind_id) const
{
    return initialized_
        && wind_id >= 0
        && wind_id < wind_data_array_.Length()
        && wind_data_array_[wind_id].IsValid();
}

const WindManager::WindData& WindManager::GetWindData(int wind_id) const
{
    if (!IsValidWind(wind_id)) {
        throw std::runtime_error("Invalid MC2 wind id.");
    }
    return wind_data_array_[wind_id];
}

WindManager::WindData& WindManager::GetWindData(int wind_id)
{
    if (!IsValidWind(wind_id)) {
        throw std::runtime_error("Invalid MC2 wind id.");
    }
    return wind_data_array_[wind_id];
}

const ExNativeArray<WindManager::WindData>& WindManager::WindDataArray() const
{
    return wind_data_array_;
}

ExNativeArray<WindManager::WindData>& WindManager::WindDataArray()
{
    return wind_data_array_;
}

int WindManager::AddWind()
{
    if (!initialized_) {
        Initialize();
    }

    WindData wind;
    wind.flag.Set(FlagValid, true);
    const DataChunk chunk = wind_data_array_.Add(wind);
    wind_zone_array_.Add(MagicaWindZone{});
    return chunk.start_index;
}

int WindManager::AddWind(const MagicaWindZone& zone, bool enabled)
{
    const int wind_id = AddWind();
    UpdateWind(wind_id, zone);
    SetEnable(wind_id, enabled);
    return wind_id;
}

void WindManager::RemoveWind(int wind_id)
{
    if (!IsValidWind(wind_id)) {
        return;
    }
    const DataChunk chunk{wind_id, 1};
    wind_data_array_.RemoveAndFill(chunk, WindData{});
    if (wind_id < wind_zone_array_.Length()) {
        wind_zone_array_.RemoveAndFill(chunk, MagicaWindZone{});
    }
}

void WindManager::SetEnable(int wind_id, bool enabled)
{
    if (IsValidWind(wind_id)) {
        wind_data_array_[wind_id].flag.Set(FlagEnable, enabled);
    }
}

void WindManager::UpdateWind(int wind_id, const MagicaWindZone& zone)
{
    if (!IsValidWind(wind_id)) {
        return;
    }
    if (wind_id < wind_zone_array_.Length()) {
        wind_zone_array_[wind_id] = zone;
    }

    WindData& wind = wind_data_array_[wind_id];
    wind.mode = zone.mode;
    switch (zone.mode) {
    case WindZoneMode::BoxDirection:
        wind.size = zone.size;
        break;
    case WindZoneMode::SphereDirection:
    case WindZoneMode::SphereRadial:
        wind.size = float3{zone.radius, zone.radius, zone.radius};
        break;
    case WindZoneMode::GlobalDirection:
        wind.size = float3{};
        break;
    }
    wind.main = zone.main;
    wind.turbulence = zone.turbulence;
    wind.flag.Set(FlagAddition, zone.IsAddition());
    wind.world_position = zone.world_position;
    wind.world_rotation = zone.world_rotation;
    wind.world_scale = zone.world_scale;
    wind.world_to_local_matrix = zone.world_to_local_matrix;
    wind.zone_volume = CalculateZoneVolume(wind);
    if (zone.IsDirection()) {
        wind.world_wind_direction = Normalize(zone.GetWindDirection(), float3{0.0f, 0.0f, 1.0f});
    } else {
        wind.world_wind_direction = float3{};
        wind.attenuation = zone.attenuation;
    }
}

void WindManager::AlwaysWindUpdate()
{
    if (!initialized_) {
        return;
    }

    for (int wind_id = 0; wind_id < wind_data_array_.Length(); ++wind_id) {
        if (!wind_data_array_[wind_id].IsValid()
            || wind_id >= wind_zone_array_.Length()) {
            continue;
        }
        UpdateWind(wind_id, wind_zone_array_[wind_id]);
    }
}

}  // namespace hocloth::mc2
