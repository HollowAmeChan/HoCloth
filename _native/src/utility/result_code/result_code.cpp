#include "hocloth/utility/result_code/result_code.hpp"

#include <utility>

namespace hocloth::mc2 {

bool Result::Succeeded() const
{
    return code == ResultCode::Success;
}

bool Result::Failed() const
{
    return !Succeeded();
}

Result Result::Ok()
{
    return Result{};
}

Result Result::Error(ResultCode code, std::string message)
{
    return Result{code, std::move(message)};
}

const char* ToString(ResultCode code)
{
    switch (code) {
    case ResultCode::Success:
        return "Success";
    case ResultCode::Empty:
        return "Empty";
    case ResultCode::InvalidArgument:
        return "InvalidArgument";
    case ResultCode::NotImplemented:
        return "NotImplemented";
    }
    return "Unknown";
}

}  // namespace hocloth::mc2
