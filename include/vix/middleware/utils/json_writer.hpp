#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vix::middleware::utils
{
    inline std::string json_escape(std::string_view s)
    {
        std::string out;
        out.reserve(s.size() + 8);

        for (char c : s)
        {
            switch (c)
            {
            case '\"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                    out += ' ';
                else
                    out += c;
                break;
            }
        }
        return out;
    }

    class JsonWriter final
    {
    public:
        JsonWriter() { s_.reserve(256); }

        std::string str() const { return s_; }

        void begin_obj()
        {
            sep();
            s_ += "{";
            stack_.push_back('}');
            first_.push_back(true);
        }

        void end_obj()
        {
            s_ += "}";
            stack_.pop_back();
            first_.pop_back();
            mark_value_written();
        }

        void begin_arr()
        {
            sep();
            s_ += "[";
            stack_.push_back(']');
            first_.push_back(true);
        }

        void end_arr()
        {
            s_ += "]";
            stack_.pop_back();
            first_.pop_back();
            mark_value_written();
        }

        void key(std::string_view k)
        {
            // inside object
            if (!first_.empty() && !first_.back())
                s_ += ",";
            if (!first_.empty())
                first_.back() = false;

            s_ += "\"";
            s_ += json_escape(k);
            s_ += "\":";
            // next write is a value
            after_key_ = true;
        }

        void string(std::string_view v)
        {
            sep();
            s_ += "\"";
            s_ += json_escape(v);
            s_ += "\"";
            mark_value_written();
        }

        void number(std::int64_t v)
        {
            sep();
            s_ += std::to_string(v);
            mark_value_written();
        }

        void boolean(bool v)
        {
            sep();
            s_ += (v ? "true" : "false");
            mark_value_written();
        }

        void null()
        {
            sep();
            s_ += "null";
            mark_value_written();
        }

        // convenience: object from map<string,string>
        void object_of(const std::unordered_map<std::string, std::string> &m)
        {
            begin_obj();
            for (const auto &kv : m)
            {
                key(kv.first);
                string(kv.second);
            }
            end_obj();
        }

    private:
        void sep()
        {
            if (after_key_)
            {
                after_key_ = false;
                return;
            }

            if (!first_.empty())
            {
                // arrays need commas between values
                // objects are handled by key()
                if (stack_.back() == ']' && !first_.back())
                    s_ += ",";
            }
        }

        void mark_value_written()
        {
            if (!first_.empty() && stack_.back() == ']')
                first_.back() = false;
        }

    private:
        std::string s_;
        std::vector<char> stack_;
        std::vector<bool> first_;
        bool after_key_{false};
    };

} // namespace vix::middleware::utils
