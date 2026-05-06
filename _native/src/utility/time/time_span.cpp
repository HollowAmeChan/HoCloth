#include "hocloth/utility/time/time_span.hpp"

#include <sstream>
#include <utility>

namespace hocloth::mc2 {

TimeSpan::TimeSpan()
{
    Start();
}

TimeSpan::TimeSpan(std::string name)
    : name_(std::move(name))
{
    Start();
}

void TimeSpan::Start()
{
    start_time_ = std::chrono::steady_clock::now();
    end_time_ = start_time_;
}

void TimeSpan::Finish()
{
    end_time_ = std::chrono::steady_clock::now();
}

double TimeSpan::TotalSeconds()
{
    Finish();
    return std::chrono::duration<double>(end_time_ - start_time_).count();
}

double TimeSpan::TotalMilliSeconds()
{
    Finish();
    return std::chrono::duration<double, std::milli>(end_time_ - start_time_).count();
}

std::string TimeSpan::ToString()
{
    std::ostringstream stream;
    stream << "TimeSpan [" << name_ << "] : " << TotalMilliSeconds() << "(ms)";
    return stream.str();
}

}  // namespace hocloth::mc2
