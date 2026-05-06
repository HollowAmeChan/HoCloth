#include "hocloth/api/mc2_backend.hpp"
#include "hocloth/manager/magica_manager.hpp"

#include <sstream>

namespace hocloth::mc2 {

std::string BackendStatus::Summary() const
{
    std::ostringstream stream;
    stream << name << ":" << phase << " managers=" << managers.size();
    for (const ManagerStatus& manager : managers) {
        stream << " [" << manager.name
               << " initialized=" << (manager.initialized ? "true" : "false")
               << " count=" << manager.item_count
               << "]";
    }
    return stream.str();
}

BackendStatus CreateBootstrapBackendStatus()
{
    MagicaManager manager;
    manager.Initialize();
    BackendStatus status{
        "hocloth_mc2_core",
        "phase_a_manager_skeleton",
        manager.Statuses(),
    };
    manager.Dispose();
    return status;
}

}  // namespace hocloth::mc2
