#include "tagging/TagClient.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {
    size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        size_t totalSize = size * nmemb;
        userp->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }
}

TagClient::TagClient(const std::string& apiKey, const std::string& baseUrl)
    : apiKey_(apiKey), baseUrl_(baseUrl) {}

std::string TagClient::httpPost(const std::string& url, const std::string& body,
                                 const std::vector<std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    struct curl_slist* headerList = nullptr;

    for (const auto& header : headers) {
        headerList = curl_slist_append(headerList, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);  // 60 second timeout for chat

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
    }

    if (httpCode != 200) {
        std::cerr << "HTTP error " << httpCode << ": " << response << std::endl;
        throw std::runtime_error("HTTP error: " + std::to_string(httpCode));
    }

    return response;
}

std::string TagClient::buildSystemPrompt(const std::vector<std::string>& existingTagBank) const {
    std::ostringstream prompt;

    if (existingTagBank.empty()) {
        prompt << "You are a document tagging assistant. This is the first document, "
               << "so you will establish the initial tag vocabulary.\n\n"
               << "RULES:\n"
               << "1. Output ONLY a JSON array of tag strings, nothing else\n"
               << "2. Use lowercase, hyphenated tags (e.g., \"machine-learning\", \"data-structures\")\n"
               << "3. Generate 3-8 relevant tags for categorizing this document\n"
               << "4. Tags should be specific enough to be useful for linking similar documents\n\n"
               << "Example output: [\"algorithms\", \"python\", \"sorting\"]\n";
    } else {
        prompt << "You are a document tagging assistant.\n\n"
               << "EXISTING TAG BANK (prefer using these when applicable):\n[";

        for (size_t i = 0; i < existingTagBank.size(); ++i) {
            prompt << "\"" << existingTagBank[i] << "\"";
            if (i < existingTagBank.size() - 1) prompt << ", ";
        }
        prompt << "]\n\n"
               << "RULES:\n"
               << "1. Output ONLY a JSON array of tag strings, nothing else\n"
               << "2. Use lowercase, hyphenated tags (e.g., \"machine-learning\")\n"
               << "3. PREFER tags from the existing bank when they fit the document\n"
               << "4. You may add up to " << maxNewTags_ << " NEW tags if no existing tags are suitable\n"
               << "5. Generate 3-8 tags total per document\n"
               << "6. Tags should be specific enough to link similar documents\n\n"
               << "Example output: [\"algorithms\", \"python\", \"new-topic\"]\n";
    }

    return prompt.str();
}

std::string TagClient::normalizeTag(const std::string& tag) const {
    std::string result;
    result.reserve(tag.size());

    bool lastWasSpace = false;
    for (char c : tag) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!lastWasSpace && !result.empty()) {
                result += '-';
                lastWasSpace = true;
            }
        } else {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            lastWasSpace = false;
        }
    }

    // Trim trailing hyphens
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }

    return result;
}

std::vector<std::string> TagClient::parseTagsFromResponse(const std::string& content) const {
    std::vector<std::string> tags;

    // Find JSON array in response
    size_t start = content.find('[');
    size_t end = content.rfind(']');

    if (start == std::string::npos || end == std::string::npos || end <= start) {
        std::cerr << "Could not find JSON array in response: " << content << std::endl;
        return tags;
    }

    std::string jsonStr = content.substr(start, end - start + 1);

    try {
        nlohmann::json arr = nlohmann::json::parse(jsonStr);
        if (arr.is_array()) {
            for (const auto& item : arr) {
                if (item.is_string()) {
                    std::string tag = normalizeTag(item.get<std::string>());
                    if (!tag.empty()) {
                        tags.push_back(tag);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse tags JSON: " << e.what() << std::endl;
    }

    return tags;
}

std::optional<std::vector<std::string>> TagClient::generateTags(
    const std::string& content,
    const std::vector<std::string>& existingTagBank) {

    if (content.empty()) {
        return std::nullopt;
    }

    std::string systemPrompt = buildSystemPrompt(existingTagBank);

    nlohmann::json requestBody = {
        {"model", model_},
        {"messages", nlohmann::json::array({
            {{"role", "system"}, {"content", systemPrompt}},
            {{"role", "user"}, {"content", content}}
        })},
        {"temperature", 0.3},
        {"max_tokens", 200}
    };

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + apiKey_
    };

    try {
        std::string url = baseUrl_ + "/v1/chat/completions";
        std::string response = httpPost(url, requestBody.dump(), headers);

        nlohmann::json jsonResponse = nlohmann::json::parse(response);

        if (jsonResponse.contains("choices") && jsonResponse["choices"].is_array() &&
            !jsonResponse["choices"].empty()) {
            auto& choice = jsonResponse["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                std::string aiContent = choice["message"]["content"].get<std::string>();
                auto tags = parseTagsFromResponse(aiContent);
                if (!tags.empty()) {
                    return tags;
                }
            }
        }

        if (jsonResponse.contains("error")) {
            std::cerr << "DeepSeek API error: " << jsonResponse["error"].dump() << std::endl;
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error generating tags: " << e.what() << std::endl;
        return std::nullopt;
    }
}
