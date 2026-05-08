#pragma once

#include <string>

namespace hocloth::inspector {

struct InspectorStatus {
    bool imgui_available = false;
    std::string backend = "none";
    std::string summary;
};

InspectorStatus GetInspectorStatus();

bool DrawInspectorUi();

}  // namespace hocloth::inspector
