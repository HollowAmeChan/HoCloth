#pragma once

#include <stdexcept>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/ResultCode/Exception.cs
class MagicaClothProcessingException : public std::runtime_error {
public:
    MagicaClothProcessingException();
    explicit MagicaClothProcessingException(const std::string& message);
};

class MagicaClothCanceledException : public std::runtime_error {
public:
    MagicaClothCanceledException();
    explicit MagicaClothCanceledException(const std::string& message);
};

}  // namespace hocloth::mc2
