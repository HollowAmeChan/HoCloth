#include "hocloth/utility/native_collection/data_chunk.hpp"

#include <sstream>

namespace hocloth::mc2 {

std::string DataChunk::ToString() const
{
    std::ostringstream stream;
    stream << "[startIndex=" << start_index << ", dataLength=" << data_length << "]";
    return stream.str();
}

}  // namespace hocloth::mc2
