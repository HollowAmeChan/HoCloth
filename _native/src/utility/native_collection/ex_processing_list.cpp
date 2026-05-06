#include "hocloth/utility/native_collection/ex_processing_list.hpp"

#include <sstream>

namespace hocloth::mc2 {

template <typename T>
std::string ExProcessingList<T>::ToString() const
{
    std::ostringstream stream;
    stream << "ExProcessingList BufferLength:" << Capacity()
           << " Counter:" << Count();
    return stream.str();
}

template class ExProcessingList<int>;
template class ExProcessingList<unsigned int>;

}  // namespace hocloth::mc2
