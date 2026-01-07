#pragma once

#include <string>
#include <vector>
#include <optional>

class TagClient {
public:
    TagClient(const std::string& apiKey,
              const std::string& baseUrl = "https://api.deepseek.com");

    // Generate tags for content, optionally constrained by existing tag bank
    std::optional<std::vector<std::string>> generateTags(
        const std::string& content,
        const std::vector<std::string>& existingTagBank = {}
    );

    // Set model name (default: deepseek-chat)
    void setModel(const std::string& model) { model_ = model; }

    // Set max new tags allowed per request
    void setMaxNewTags(int max) { maxNewTags_ = max; }

private:
    std::string apiKey_;
    std::string baseUrl_;
    std::string model_ = "deepseek-chat";
    int maxNewTags_ = 3;

    // HTTP POST request using libcurl
    std::string httpPost(const std::string& url, const std::string& body,
                         const std::vector<std::string>& headers);

    // Build the system prompt for tag generation
    std::string buildSystemPrompt(const std::vector<std::string>& existingTagBank) const;

    // Parse JSON array from AI response
    std::vector<std::string> parseTagsFromResponse(const std::string& content) const;

    // Normalize tag (lowercase, trim, replace spaces with hyphens)
    std::string normalizeTag(const std::string& tag) const;
};
