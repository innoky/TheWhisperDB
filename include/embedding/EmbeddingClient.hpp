#pragma once

#include <string>
#include <vector>
#include <optional>

class EmbeddingClient {
public:
    EmbeddingClient(const std::string& apiKey, const std::string& baseUrl = "https://api.deepseek.com");

    // Get embedding for a single text
    std::optional<std::vector<float>> getEmbedding(const std::string& text);

    // Get embeddings for multiple texts (batch)
    std::vector<std::vector<float>> getEmbeddings(const std::vector<std::string>& texts);

    // Set model name (default: deepseek-chat)
    void setModel(const std::string& model) { model_ = model; }

private:
    std::string apiKey_;
    std::string baseUrl_;
    std::string model_ = "deepseek-chat";

    // HTTP POST request using libcurl
    std::string httpPost(const std::string& url, const std::string& body, const std::vector<std::string>& headers);
};
