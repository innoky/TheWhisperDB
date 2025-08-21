#include "server/upload_handler.hpp"

#include <iostream>
#include <cctype>

static void trim_inline(std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    s = s.substr(a, b - a);
}

static bool parse_meta_and_content(const std::string& block,
                                  std::string& name,
                                  std::string& filename,
                                  std::string& content_type,
                                  std::string& content)
{
    size_t sep = block.find("\n\n");
    std::string meta;
    if (sep != std::string::npos) {
        meta = block.substr(0, sep);
        content = block.substr(sep + 2);
    } else {
        content = block;
    }

    size_t pos = 0;
    while (pos < meta.size()) {
        size_t eol = meta.find('\n', pos);
        std::string line = (eol == std::string::npos) ? meta.substr(pos) : meta.substr(pos, eol - pos);
        pos = (eol == std::string::npos) ? meta.size() : eol + 1;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        trim_inline(key); trim_inline(val);
        for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (key == "name") name = val;
        else if (key == "filename") filename = val;
        else if (key == "content-type") content_type = val;
    }

    return true;
}

#include <fstream>
static void save_file_binary(const std::string& data, const std::string& filename)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open file");
    file.write(data.data(), data.size());
}

std::string handle_upload(const std::vector<std::string>& parts)
{
    size_t saved = 0;
    for (const auto& block : parts) {
        std::string name, filename, ctype, content;
        parse_meta_and_content(block, name, filename, ctype, content);
        if (!filename.empty()) {
            std::cout << "Saving file '" << filename << "' (" << content.size() << " bytes)" << std::endl;
            save_file_binary(content, filename);
            ++saved;
        }
    }
    return std::string("Saved files: ") + std::to_string(saved);
}
