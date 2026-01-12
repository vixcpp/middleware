#ifndef VIX_STRING_HPP
#define VIX_STRING_HPP

#include <cstddef>
#include <string_view>
#include <string>

namespace vix::middleware::helpers::strings
{
    inline bool starts_with_icase(std::string_view s, std::string_view prefix)
    {
        if (s.size() < prefix.size())
            return false;

        for (std::size_t i = 0; i < prefix.size(); ++i)
        {
            unsigned char a = static_cast<unsigned char>(s[i]);
            unsigned char b = static_cast<unsigned char>(prefix[i]);

            if (a >= 'A' && a <= 'Z')
                a = static_cast<unsigned char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z')
                b = static_cast<unsigned char>(b - 'A' + 'a');

            if (a != b)
                return false;
        }
        return true;
    }

    inline std::string extract_boundary(std::string_view ct)
    {
        // "multipart/form-data; boundary=----WebKitFormBoundaryabc"
        auto pos = ct.find("boundary=");
        if (pos == std::string_view::npos)
            return {};

        pos += std::string_view("boundary=").size();
        if (pos >= ct.size())
            return {};

        std::string_view b = ct.substr(pos);

        while (!b.empty() && (b.front() == ' ' || b.front() == '\t'))
            b.remove_prefix(1);

        if (!b.empty() && b.front() == '"')
        {
            b.remove_prefix(1);
            auto endq = b.find('"');
            if (endq != std::string_view::npos)
                b = b.substr(0, endq);
        }
        else
        {
            auto semi = b.find(';');
            if (semi != std::string_view::npos)
                b = b.substr(0, semi);
        }

        while (!b.empty() && (b.back() == ' ' || b.back() == '\t'))
            b.remove_suffix(1);

        return std::string(b);
    }
} // namespace vix::middleware::parsers::detail

#endif
