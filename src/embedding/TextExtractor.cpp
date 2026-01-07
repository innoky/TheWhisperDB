#include "embedding/TextExtractor.hpp"
#include <fstream>
#include <sstream>
#include <array>
#include <memory>
#include <algorithm>
#include <cstdio>
#include <filesystem>

size_t TextExtractor::maxLength_ = 8000; // Default: ~8k chars for API limits

std::string TextExtractor::getExtension(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

std::string TextExtractor::truncate(const std::string& text) {
    if (text.length() <= maxLength_) {
        return text;
    }
    return text.substr(0, maxLength_);
}

std::optional<std::string> TextExtractor::extractFromFile(const std::string& filePath) {
    std::string ext = getExtension(filePath);

    if (ext == ".pdf") {
        return extractFromPdf(filePath);
    } else if (ext == ".txt" || ext == ".md" || ext == ".text") {
        return extractFromTxt(filePath);
    }

    // Unsupported format
    return std::nullopt;
}

std::optional<std::string> TextExtractor::extractFromPdf(const std::string& filePath) {
    // Use pdftotext from poppler-utils
    std::string command = "pdftotext -q \"" + filePath + "\" -";

    std::array<char, 4096> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return std::nullopt;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
        // Early exit if we have enough text
        if (result.length() > maxLength_) {
            break;
        }
    }

    if (result.empty()) {
        return std::nullopt;
    }

    return truncate(result);
}

std::optional<std::string> TextExtractor::extractFromTxt(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    if (content.empty()) {
        return std::nullopt;
    }

    return truncate(content);
}
