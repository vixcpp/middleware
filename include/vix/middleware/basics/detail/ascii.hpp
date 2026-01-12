#ifndef ASCII_HPP
#define ASCII_HPP

#include <cctype>
#include <string>
#include <utility>

namespace vix::middleware::basics::detail
{
    inline std::string to_lower_ascii(std::string s)
    {
        for (char &c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
}

#endif