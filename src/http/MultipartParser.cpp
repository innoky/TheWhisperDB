#include "http/MultipartParser.hpp"
#include <cctype>
#include <algorithm>

namespace whisperdb {
namespace http {

void MultipartParser::trim(std::string& s) {
    size_t start = 0;
    size_t end = s.size();
    while (start < end && (s[start] == ' ' || s[start] == '\t')) ++start;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
                            s[end - 1] == '\r' || s[end - 1] == '\n')) --end;
    s = s.substr(start, end - start);
}

void MultipartParser::toLower(std::string& s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
}

void MultipartParser::parseContentDisposition(const std::string& value,
                                               std::string& name,
                                               std::string& filename) {
    size_t pos = 0;
    while (pos < value.size()) {
        size_t next = value.find(';', pos);
        std::string token = value.substr(pos, (next == std::string::npos ? value.size() : next) - pos);
        pos = (next == std::string::npos ? value.size() : next + 1);

        trim(token);
        if (token.empty()) continue;

        auto eq = token.find('=');
        if (eq == std::string::npos) continue;

        std::string key = token.substr(0, eq);
        std::string val = token.substr(eq + 1);
        trim(key);
        trim(val);
        toLower(key);

        // Remove surrounding quotes
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }

        if (key == "name") {
            name = val;
        } else if (key == "filename") {
            filename = val;
        }
    }
}

std::string MultipartParser::extractBoundary(const std::string& content_type) {
    std::string boundary;

    auto semicolon = content_type.find(';');
    if (semicolon == std::string::npos) {
        return boundary;
    }

    std::string params = content_type.substr(semicolon + 1);

    while (!params.empty()) {
        auto next_semi = params.find(';');
        std::string token = (next_semi == std::string::npos) ? params : params.substr(0, next_semi);
        params = (next_semi == std::string::npos) ? "" : params.substr(next_semi + 1);

        trim(token);
        if (token.empty()) continue;

        auto eq = token.find('=');
        if (eq == std::string::npos) continue;

        std::string key = token.substr(0, eq);
        std::string val = token.substr(eq + 1);
        trim(key);
        trim(val);
        toLower(key);

        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }

        if (key == "boundary") {
            boundary = val;
            break;
        }
    }

    return boundary;
}

size_t MultipartParser::countParts(const std::string& body, const std::string& boundary) {
    if (boundary.empty()) return 0;

    const std::string dash = "--" + boundary;
    size_t pos = 0;
    size_t first = body.find(dash, pos);
    if (first == std::string::npos) return 0;

    pos = first + dash.size();

    // Check for final boundary
    if (pos + 2 <= body.size() && body.compare(pos, 2, "--") == 0) return 0;

    // Skip CRLF after boundary
    if (pos + 2 <= body.size() && body.compare(pos, 2, "\r\n") == 0) pos += 2;

    size_t count = 0;
    while (true) {
        const std::string marker = "\r\n" + dash;
        size_t next = body.find(marker, pos);
        if (next == std::string::npos) break;

        ++count;
        pos = next + marker.size();

        // Check for final boundary
        if (pos + 2 <= body.size() && body.compare(pos, 2, "--") == 0) break;

        // Skip CRLF
        if (pos + 2 <= body.size() && body.compare(pos, 2, "\r\n") == 0) pos += 2;
    }

    return count;
}

std::vector<MultipartPart> MultipartParser::parse(const std::string& body,
                                                   const std::string& boundary) {
    std::vector<MultipartPart> parts;
    if (boundary.empty()) return parts;

    const std::string dash = "--" + boundary;

    // Find first boundary
    size_t bline;
    if (body.rfind(dash, 0) == 0) {
        bline = 0;
    } else {
        size_t m = body.find("\r\n" + dash, 0);
        if (m == std::string::npos) return parts;
        bline = m + 2;
    }

    while (true) {
        // Boundary line ends with CRLF
        size_t line_end = body.find("\r\n", bline);
        if (line_end == std::string::npos) break;

        // Check for final boundary (--)
        const size_t after = bline + dash.size();
        if (after + 2 <= body.size() && body.compare(after, 2, "--") == 0) break;

        // Parse headers
        size_t headers_start = line_end + 2;
        size_t headers_end = body.find("\r\n\r\n", headers_start);
        if (headers_end == std::string::npos) break;

        MultipartPart part;

        // Extract headers
        size_t hpos = headers_start;
        while (hpos < headers_end) {
            size_t eol = body.find("\r\n", hpos);
            if (eol == std::string::npos || eol > headers_end) break;

            std::string hline = body.substr(hpos, eol - hpos);
            hpos = eol + 2;

            auto colon = hline.find(':');
            if (colon == std::string::npos) continue;

            std::string hname = hline.substr(0, colon);
            std::string hvalue = hline.substr(colon + 1);
            trim(hname);
            trim(hvalue);

            std::string hname_lc = hname;
            toLower(hname_lc);

            if (hname_lc == "content-disposition") {
                parseContentDisposition(hvalue, part.name, part.filename);
            } else if (hname_lc == "content-type") {
                part.content_type = hvalue;
            }
        }

        // Extract content
        size_t content_start = headers_end + 4;
        size_t next_marker = body.find("\r\n" + dash, content_start);
        size_t content_end = (next_marker == std::string::npos) ? body.size() : next_marker;

        // Store as binary data
        const char* data_ptr = body.data() + content_start;
        size_t data_len = content_end - content_start;
        part.data = std::vector<uint8_t>(data_ptr, data_ptr + data_len);

        parts.push_back(std::move(part));

        if (next_marker == std::string::npos) break;
        bline = next_marker + 2;
    }

    return parts;
}

} // namespace http
} // namespace whisperdb
