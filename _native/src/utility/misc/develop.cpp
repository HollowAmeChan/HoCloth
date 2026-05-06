#include "hocloth/utility/misc/develop.hpp"

#include <cassert>
#include <iostream>

namespace hocloth::mc2::develop {

namespace {

void Print(std::ostream& stream, const char* prefix, const std::string& message)
{
    stream << prefix << message << '\n';
}

}  // namespace

void Log(const std::string& message)
{
    Print(std::cout, "[MC2] ", message);
}

void LogWarning(const std::string& message)
{
    Print(std::cerr, "[MC2 WARNING] ", message);
}

void LogError(const std::string& message)
{
    Print(std::cerr, "[MC2 ERROR] ", message);
}

void DebugLog(const std::string& message)
{
#if defined(MC2_LOG)
    Print(std::cout, "[MC2 DEBUG] ", message);
#else
    (void)message;
#endif
}

void DebugLogWarning(const std::string& message)
{
#if defined(MC2_DEBUG)
    Print(std::cerr, "[MC2 DEBUG WARNING] ", message);
#else
    (void)message;
#endif
}

void DebugLogError(const std::string& message)
{
#if defined(MC2_DEBUG)
    Print(std::cerr, "[MC2 DEBUG ERROR] ", message);
#else
    (void)message;
#endif
}

void Assert(bool condition)
{
#if defined(MC2_DEBUG)
    assert(condition);
#else
    (void)condition;
#endif
}

}  // namespace hocloth::mc2::develop
