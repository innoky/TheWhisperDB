#pragma once

#include <string>
#include <optional>

class TextExtractor {
public:
    // Extract text from file based on extension
    static std::optional<std::string> extractFromFile(const std::string& filePath);

    // Extract text from PDF using pdftotext (poppler-utils)
    static std::optional<std::string> extractFromPdf(const std::string& filePath);

    // Extract text from plain text file
    static std::optional<std::string> extractFromTxt(const std::string& filePath);

    // Set maximum text length (for API limits)
    static void setMaxLength(size_t maxLen) { maxLength_ = maxLen; }

private:
    static size_t maxLength_;

    // Truncate text if needed
    static std::string truncate(const std::string& text);

    // Get file extension (lowercase)
    static std::string getExtension(const std::string& filePath);
};
