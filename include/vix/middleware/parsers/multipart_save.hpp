#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/helpers/strings/strings.hpp>

namespace vix::middleware::parsers
{
    struct MultipartFile
    {
        std::string field_name;
        std::string filename;     // original filename (as received)
        std::string content_type; // part content-type
        std::string saved_path;   // final path
        std::size_t bytes{0};
    };

    struct MultipartForm
    {
        std::vector<MultipartFile> files;
        std::unordered_map<std::string, std::string> fields; // last-wins for same key
        std::size_t total_bytes{0};
        std::size_t total_files_bytes{0};
    };

    struct MultipartSaveOptions
    {
        bool require_boundary{true};

        // Hard limits (0 => no limit)
        std::size_t max_bytes{10 * 1024 * 1024};     // whole request (default 10MB)
        std::size_t max_files{16};                   // max number of file parts
        std::size_t max_file_bytes{5 * 1024 * 1024}; // each file (default 5MB)

        // Storage
        std::string upload_dir{"uploads"};
        bool create_upload_dir{true};

        // Filename policy
        bool keep_original_filename{false}; // if false => generate unique base name
        bool keep_extension{true};          // try to keep ".png", ".zip", etc.
        std::string default_basename{"file"};

        // Behavior
        bool store_in_state{true};
    };

    inline std::string trim(std::string s)
    {
        auto is_ws = [](unsigned char c)
        { return std::isspace(c) != 0; };

        while (!s.empty() && is_ws(static_cast<unsigned char>(s.front())))
            s.erase(s.begin());
        while (!s.empty() && is_ws(static_cast<unsigned char>(s.back())))
            s.pop_back();
        return s;
    }

    inline std::string header_value(std::string_view headers, std::string_view key)
    {
        auto lower = [](char c)
        { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; };

        std::string k;
        k.reserve(key.size());
        for (char c : key)
            k.push_back(lower(c));

        std::size_t pos = 0;
        while (pos < headers.size())
        {
            auto eol = headers.find("\r\n", pos);
            if (eol == std::string_view::npos)
                break;

            auto line = headers.substr(pos, eol - pos);
            pos = eol + 2;

            auto colon = line.find(':');
            if (colon == std::string_view::npos)
                continue;

            std::string lk;
            lk.reserve(colon);
            for (std::size_t i = 0; i < colon; ++i)
                lk.push_back(lower(line[i]));

            if (lk != k)
                continue;

            std::string v(line.substr(colon + 1));
            return trim(v);
        }
        return {};
    }

    inline std::string param_from_content_disposition(std::string_view cd, std::string_view name)
    {
        // name="x" or filename="a.png"
        auto pos = cd.find(name);
        if (pos == std::string_view::npos)
            return {};
        pos = cd.find('=', pos);
        if (pos == std::string_view::npos)
            return {};
        pos++;

        while (pos < cd.size() && (cd[pos] == ' ' || cd[pos] == '\t'))
            pos++;
        if (pos >= cd.size())
            return {};

        if (cd[pos] == '"')
        {
            pos++;
            auto end = cd.find('"', pos);
            if (end == std::string_view::npos)
                return {};
            return std::string(cd.substr(pos, end - pos));
        }

        auto end = cd.find(';', pos);
        if (end == std::string_view::npos)
            end = cd.size();
        return trim(std::string(cd.substr(pos, end - pos)));
    }

    inline std::string safe_token(std::string s)
    {
        // keep only [a-zA-Z0-9._-]
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s)
        {
            if ((c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '.' || c == '_' || c == '-')
            {
                out.push_back(static_cast<char>(c));
            }
        }
        if (out.empty())
            out = "file";
        return out;
    }

    inline std::string filename_extension(std::string_view filename)
    {
        // Keep a simple extension like ".png", ".tar.gz" is tricky; we keep last dot extension only.
        auto p = filename.rfind('.');
        if (p == std::string_view::npos || p == 0 || p + 1 >= filename.size())
            return {};
        return std::string(filename.substr(p)); // includes '.'
    }

    inline std::string random_hex_8()
    {
        std::random_device rd;
        std::uint32_t x = (std::uint32_t(rd()) << 16) ^ std::uint32_t(rd());
        const char *hex = "0123456789abcdef";
        std::string out;
        out.resize(8);
        for (int i = 7; i >= 0; --i)
        {
            out[i] = hex[x & 0xF];
            x >>= 4;
        }
        return out;
    }

    inline std::string unique_basename()
    {
        using namespace std::chrono;
        const auto now = time_point_cast<milliseconds>(system_clock::now()).time_since_epoch().count();
        // "1700000000000_ab12cd34"
        return std::to_string(now) + "_" + random_hex_8();
    }

    inline bool write_file_atomic(const std::filesystem::path &final_path, std::string_view data)
    {
        // write to tmp then rename
        auto tmp = final_path;
        tmp += ".tmp";

        std::ofstream ofs(tmp, std::ios::binary);
        if (!ofs)
            return false;

        ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
        ofs.flush();
        if (!ofs.good())
            return false;

        ofs.close();

        std::error_code ec;
        std::filesystem::rename(tmp, final_path, ec);
        if (ec)
        {
            // try cleanup tmp
            std::error_code ec2;
            std::filesystem::remove(tmp, ec2);
            return false;
        }
        return true;
    }

    inline std::filesystem::path make_unique_path(
        const MultipartSaveOptions &opt,
        std::string_view original_filename)
    {
        std::string ext;
        if (opt.keep_extension)
            ext = filename_extension(original_filename);

        std::string base;
        if (opt.keep_original_filename)
            base = safe_token(std::string(original_filename));
        else
            base = safe_token(opt.default_basename) + "_" + unique_basename();

        // avoid empty, avoid huge
        if (base.size() > 120)
            base.resize(120);

        std::filesystem::path dir(opt.upload_dir);

        // collision-safe: loop if exists
        for (int i = 0; i < 64; ++i)
        {
            std::filesystem::path p = dir / (base + ext);
            if (!std::filesystem::exists(p))
                return p;

            base = safe_token(opt.default_basename) + "_" + unique_basename();
        }

        // last resort
        return dir / (safe_token(opt.default_basename) + "_" + unique_basename() + ext);
    }

    inline std::size_t parse_content_length(std::string_view s)
    {
        // returns 0 on invalid/empty
        std::size_t n = 0;
        bool any = false;
        for (char c : s)
        {
            if (c >= '0' && c <= '9')
            {
                any = true;
                std::size_t digit = std::size_t(c - '0');
                if (n > (std::numeric_limits<std::size_t>::max() - digit) / 10)
                    return 0; // overflow => treat as invalid
                n = n * 10 + digit;
            }
            else if (c == ' ' || c == '\t')
            {
                continue;
            }
            else
            {
                return 0;
            }
        }
        return any ? n : 0;
    }

    // Middleware
    inline MiddlewareFn multipart_save(MultipartSaveOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            auto &req = ctx.req();

            const std::string ct = req.header("content-type");
            if (ct.empty() || !helpers::strings::starts_with_icase(ct, "multipart/form-data"))
            {
                Error e;
                e.status = 415;
                e.code = "unsupported_media_type";
                e.message = "Content-Type must be multipart/form-data";
                if (!ct.empty())
                    e.details["content_type"] = ct;
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            const std::string boundary = helpers::strings::extract_boundary(ct);
            if (opt.require_boundary && boundary.empty())
            {
                Error e;
                e.status = 400;
                e.code = "missing_boundary";
                e.message = "multipart/form-data boundary is missing";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            // Early reject if Content-Length is present (prevents reading huge bodies in many servers)
            if (opt.max_bytes > 0)
            {
                const std::string cl = req.header("content-length");
                if (!cl.empty())
                {
                    const std::size_t declared = parse_content_length(cl);
                    if (declared > 0 && declared > opt.max_bytes)
                    {
                        Error e;
                        e.status = 413;
                        e.code = "payload_too_large";
                        e.message = "Request body exceeds multipart limit (Content-Length)";
                        e.details["max_bytes"] = std::to_string(opt.max_bytes);
                        e.details["content_length"] = std::to_string(declared);
                        ctx.send_error(normalize(std::move(e)));
                        return;
                    }
                }
            }

            // NOTE: still in-memory due to string_body
            const std::string body = req.body();

            if (opt.max_bytes > 0 && body.size() > opt.max_bytes)
            {
                Error e;
                e.status = 413;
                e.code = "payload_too_large";
                e.message = "Request body exceeds multipart limit";
                e.details["max_bytes"] = std::to_string(opt.max_bytes);
                e.details["got_bytes"] = std::to_string(body.size());
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            MultipartForm form;
            form.total_bytes = body.size();

            // Ensure upload dir
            std::error_code ec;
            if (opt.create_upload_dir)
            {
                std::filesystem::create_directories(opt.upload_dir, ec);
                if (ec)
                {
                    Error e;
                    e.status = 500;
                    e.code = "upload_dir_error";
                    e.message = "Failed to create upload directory";
                    e.details["upload_dir"] = opt.upload_dir;
                    ctx.send_error(normalize(std::move(e)));
                    return;
                }
            }

            const std::string sep = "--" + boundary;
            std::size_t pos = 0;

            while (true)
            {
                // find next boundary
                std::size_t b = body.find(sep, pos);
                if (b == std::string::npos)
                    break;

                b += sep.size();

                // end marker?
                if (b + 2 <= body.size() && body.compare(b, 2, "--") == 0)
                    break;

                // optional CRLF after boundary
                if (b + 2 <= body.size() && body.compare(b, 2, "\r\n") == 0)
                    b += 2;

                // headers end
                std::size_t h_end = body.find("\r\n\r\n", b);
                if (h_end == std::string::npos)
                    break;

                std::string_view headers(body.data() + b, h_end - b);
                std::size_t data_start = h_end + 4;

                // next boundary marks data end
                std::size_t nb = body.find(sep, data_start);
                if (nb == std::string::npos)
                    break;

                std::size_t data_end = nb;
                if (data_end >= 2 && body[data_end - 2] == '\r' && body[data_end - 1] == '\n')
                    data_end -= 2;

                if (data_end < data_start)
                {
                    pos = nb;
                    continue;
                }

                std::string_view data(body.data() + data_start, data_end - data_start);

                const std::string cd = header_value(headers, "Content-Disposition");
                if (cd.empty())
                {
                    pos = nb;
                    continue;
                }

                const std::string part_ct = header_value(headers, "Content-Type");
                const std::string field = param_from_content_disposition(cd, "name");
                const std::string filename = param_from_content_disposition(cd, "filename");

                if (!filename.empty())
                {
                    if (opt.max_files > 0 && form.files.size() >= opt.max_files)
                    {
                        Error e;
                        e.status = 413;
                        e.code = "too_many_files";
                        e.message = "Too many files in multipart request";
                        e.details["max_files"] = std::to_string(opt.max_files);
                        ctx.send_error(normalize(std::move(e)));
                        return;
                    }

                    if (opt.max_file_bytes > 0 && data.size() > opt.max_file_bytes)
                    {
                        Error e;
                        e.status = 413;
                        e.code = "file_too_large";
                        e.message = "A multipart file exceeds max_file_bytes";
                        e.details["max_file_bytes"] = std::to_string(opt.max_file_bytes);
                        e.details["file_bytes"] = std::to_string(data.size());
                        e.details["filename"] = filename;
                        ctx.send_error(normalize(std::move(e)));
                        return;
                    }

                    MultipartFile f;
                    f.field_name = field;
                    f.filename = filename;
                    f.content_type = part_ct;
                    f.bytes = data.size();

                    const std::filesystem::path outp = make_unique_path(opt, filename);

                    // write atomic
                    if (!write_file_atomic(outp, data))
                    {
                        Error e;
                        e.status = 500;
                        e.code = "file_write_error";
                        e.message = "Failed to save uploaded file";
                        e.details["path"] = outp.string();
                        e.details["filename"] = filename;
                        ctx.send_error(normalize(std::move(e)));
                        return;
                    }

                    f.saved_path = outp.string();
                    form.total_files_bytes += f.bytes;
                    form.files.push_back(std::move(f));
                }
                else
                {
                    // text field
                    if (!field.empty())
                        form.fields[field] = std::string(data);
                }

                pos = nb;
            }

            if (opt.store_in_state)
                ctx.set_state<MultipartForm>(std::move(form));

            next();
        };
    }

} // namespace vix::middleware::parsers
