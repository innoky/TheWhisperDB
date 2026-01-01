#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace whisperdb {
namespace http {

/**
 * Represents a single part of a multipart/form-data request
 */
struct MultipartPart {
    std::string name;           // form field name
    std::string filename;       // original filename (empty if not a file)
    std::string content_type;   // MIME type of the content
    std::vector<uint8_t> data;  // binary content

    // Convenience methods
    bool isFile() const { return !filename.empty(); }
    std::string dataAsString() const {
        return std::string(data.begin(), data.end());
    }
};

/**
 * Parser for multipart/form-data HTTP requests
 */
class MultipartParser {
public:
    /**
     * Parse multipart body into structured parts
     * @param body Raw HTTP body
     * @param boundary Multipart boundary string (without --)
     * @return Vector of parsed parts
     */
    static std::vector<MultipartPart> parse(const std::string& body, const std::string& boundary);

    /**
     * Extract boundary from Content-Type header value
     * @param content_type Full Content-Type header value
     * @return Boundary string or empty if not found
     */
    static std::string extractBoundary(const std::string& content_type);

    /**
     * Count number of parts without full parsing (for validation)
     * @param body Raw HTTP body
     * @param boundary Multipart boundary string
     * @return Number of parts
     */
    static size_t countParts(const std::string& body, const std::string& boundary);

private:
    static void trim(std::string& s);
    static void toLower(std::string& s);
    static void parseContentDisposition(const std::string& value, std::string& name, std::string& filename);
};

} // namespace http
} // namespace whisperdb
