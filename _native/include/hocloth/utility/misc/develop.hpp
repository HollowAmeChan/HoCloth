#pragma once

#include <string>

namespace hocloth::mc2::develop {

// Native port of Magica Cloth 2: Scripts/Core/Utility/Misc/Develop.cs.
void Log(const std::string& message);
void LogWarning(const std::string& message);
void LogError(const std::string& message);
void DebugLog(const std::string& message);
void DebugLogWarning(const std::string& message);
void DebugLogError(const std::string& message);
void Assert(bool condition);

}  // namespace hocloth::mc2::develop
