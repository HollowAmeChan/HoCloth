#include "hocloth/utility/native_collection/data_chunk.hpp"

#include <sstream>

namespace hocloth::mc2 {

DataChunk::DataChunk(int start_index_, int data_length_)
    : start_index(start_index_)
    , data_length(data_length_)
{
}

DataChunk::DataChunk(int start_index_)
    : start_index(start_index_)
    , data_length(1)
{
}

bool DataChunk::IsValid() const
{
    return data_length > 0;
}

void DataChunk::Clear()
{
    start_index = 0;
    data_length = 0;
}

int DataChunk::EndIndex() const
{
    return start_index + data_length;
}

bool DataChunk::Contains(int index) const
{
    return index >= start_index && index < EndIndex();
}

std::string DataChunk::ToString() const
{
    std::ostringstream stream;
    stream << "[startIndex=" << start_index << ", dataLength=" << data_length << "]";
    return stream.str();
}

DataChunk DataChunk::Empty()
{
    return DataChunk{};
}

}  // namespace hocloth::mc2
