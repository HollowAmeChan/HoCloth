#include "hocloth/utility/result_code/exception.hpp"

namespace hocloth::mc2 {

MagicaClothProcessingException::MagicaClothProcessingException()
    : std::runtime_error("")
{
}

MagicaClothProcessingException::MagicaClothProcessingException(const std::string& message)
    : std::runtime_error(message)
{
}

MagicaClothCanceledException::MagicaClothCanceledException()
    : std::runtime_error("")
{
}

MagicaClothCanceledException::MagicaClothCanceledException(const std::string& message)
    : std::runtime_error(message)
{
}

}  // namespace hocloth::mc2
