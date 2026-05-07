#pragma once

#include <sstream>
#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/Misc/StaticStringBuilder.cs
class StaticStringBuilder {
public:
    [[nodiscard]] static std::string& Instance()
    {
        return buffer_;
    }

    static void Clear()
    {
        buffer_.clear();
    }

    template <typename... Args>
    static std::string& Append(const Args&... args)
    {
        (AppendOne(args), ...);
        return buffer_;
    }

    template <typename... Args>
    static std::string& AppendLine(const Args&... args)
    {
        (AppendOne(args), ...);
        buffer_.push_back('\n');
        return buffer_;
    }

    static std::string& AppendLine()
    {
        buffer_.push_back('\n');
        return buffer_;
    }

    template <typename... Args>
    [[nodiscard]] static std::string AppendToString(const Args&... args)
    {
        Clear();
        (AppendOne(args), ...);
        return buffer_;
    }

    [[nodiscard]] static std::string ToString()
    {
        return buffer_;
    }

private:
    template <typename T>
    static void AppendOne(const T& value)
    {
        std::ostringstream stream;
        stream << value;
        buffer_ += stream.str();
    }

    static inline std::string buffer_;
};

}  // namespace hocloth::mc2
