#pragma once

#include "hocloth/core/interface/i_valid.hpp"
#include "hocloth/utility/math/math_types.hpp"

#include <array>
#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Native port of Magica Cloth 2: Scripts/Core/Manager/Team/TeamWindData.cs.
struct TeamWindInfo final : public IValid {
    int wind_id = -1;
    float time = 0.0f;
    float main = 0.0f;
    float3 direction{};

    [[nodiscard]] bool IsValid() const override
    {
        return main > 1.0e-6f;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream stream;
        stream << "windId:" << wind_id
               << ", time:" << time
               << ", main:" << main
               << ", direction:(" << direction.x << "," << direction.y << "," << direction.z << ")";
        return stream.str();
    }
};

struct TeamWindData {
    static constexpr int WindZoneCapacity = 7;

    std::array<TeamWindInfo, WindZoneCapacity> wind_zone_list{};
    int zone_count = 0;
    TeamWindInfo moving_wind;

    [[nodiscard]] int ZoneCount() const
    {
        return zone_count;
    }

    [[nodiscard]] int IndexOf(int wind_id) const
    {
        for (int index = 0; index < zone_count; ++index) {
            if (wind_zone_list[static_cast<std::size_t>(index)].wind_id == wind_id) {
                return index;
            }
        }
        return -1;
    }

    void ClearZoneList()
    {
        wind_zone_list = {};
        zone_count = 0;
    }

    bool AddOrReplaceWindZone(TeamWindInfo wind_info, const TeamWindData& old_wind_data)
    {
        if (!wind_info.IsValid()) {
            return false;
        }

        const int old_index = old_wind_data.IndexOf(wind_info.wind_id);
        if (old_index >= 0) {
            wind_info.time =
                old_wind_data.wind_zone_list[static_cast<std::size_t>(old_index)].time;
        }

        const int existing_index = IndexOf(wind_info.wind_id);
        if (existing_index >= 0) {
            wind_zone_list[static_cast<std::size_t>(existing_index)] = wind_info;
            return true;
        }

        if (zone_count >= WindZoneCapacity) {
            return false;
        }
        wind_zone_list[static_cast<std::size_t>(zone_count)] = wind_info;
        ++zone_count;
        return true;
    }

    bool RemoveWindZone(int wind_id)
    {
        const int index = IndexOf(wind_id);
        if (index < 0) {
            return false;
        }
        --zone_count;
        wind_zone_list[static_cast<std::size_t>(index)] =
            wind_zone_list[static_cast<std::size_t>(zone_count)];
        wind_zone_list[static_cast<std::size_t>(zone_count)] = TeamWindInfo{};
        return true;
    }
};

}  // namespace hocloth::mc2
