#pragma once

#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/ResultCode/ResultCode.cs
enum class ResultCode {
    Success,
    Empty,
    InvalidArgument,
    NotImplemented,
};

struct Result {
    ResultCode code = ResultCode::Success;
    std::string message;

    [[nodiscard]] bool Succeeded() const;
    [[nodiscard]] bool Failed() const;

    static Result Ok();
    static Result Error(ResultCode code, std::string message);
};

[[nodiscard]] const char* ToString(ResultCode code);

}  // namespace hocloth::mc2
