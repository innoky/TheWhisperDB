#include "embedding/EmbeddingClient.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <iostream>

namespace {
    size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        size_t totalSize = size * nmemb;
        userp->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }
}

EmbeddingClient::EmbeddingClient(const std::string& apiKey, const std::string& baseUrl)
    : apiKey_(apiKey), baseUrl_(baseUrl) {}

std::string EmbeddingClient::httpPost(const std::string& url, const std::string& body,
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "DEBUG CURL error: " << curl_easy_strerror(res) << std::endl;
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
    }

    std::cerr << "DEBUG HTTP code: " << httpCode << std::endl;
    return response;
}

std::optional<std::vector<float>> EmbeddingClient::getEmbedding(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }

    nlohmann::json requestBody = {
        {"model", model_},
        {"input", text},
        {"encoding_format", "float"}
    };

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + apiKey_
    };

    try {
        std::string url = baseUrl_ + "/v1/embeddings";
        std::cerr << "DEBUG: Calling " << url << " with model " << model_ << std::endl;
        std::string response = httpPost(url, requestBody.dump(), headers);
        std::cerr << "DEBUG: Response (" << response.size() << " bytes): "
                  << response.substr(0, 500) << std::endl;
        nlohmann::json jsonResponse = nlohmann::json::parse(response);

        if (jsonResponse.contains("data") && jsonResponse["data"].is_array() &&
            !jsonResponse["data"].empty()) {
            auto& embeddingData = jsonResponse["data"][0];
            if (embeddingData.contains("embedding") && embeddingData["embedding"].is_array()) {
                return embeddingData["embedding"].get<std::vector<float>>();
            }
        }

        if (jsonResponse.contains("error")) {
            std::cerr << "OpenAI API error: " << jsonResponse["error"].dump() << std::endl;
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error getting embedding: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<std::vector<float>> EmbeddingClient::getEmbeddings(const std::vector<std::string>& texts) {
    std::vector<std::vector<float>> results;
    results.reserve(texts.size());

    // OpenAI API supports batch input
    nlohmann::json requestBody = {
        {"model", model_},
        {"input", texts},
        {"encoding_format", "float"}
    };

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + apiKey_
    };

    try {
        std::string response = httpPost(baseUrl_ + "/v1/embeddings", requestBody.dump(), headers);
        nlohmann::json jsonResponse = nlohmann::json::parse(response);

        if (jsonResponse.contains("data") && jsonResponse["data"].is_array()) {
            for (const auto& item : jsonResponse["data"]) {
                if (item.contains("embedding") && item["embedding"].is_array()) {
                    results.push_back(item["embedding"].get<std::vector<float>>());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting embeddings: " << e.what() << std::endl;
    }

    return results;
}
