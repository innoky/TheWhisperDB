#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "core/GraphDB.hpp"

class UploadHandler {
public:
    explicit UploadHandler(GraphDB& db);
    
    // Handle file upload with metadata
    // Returns a JSON response as a string
    std::string handleUpload(
        const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files,
        const nlohmann::json& metadata
    );

private:
    GraphDB& db_;
    
    // Validate the metadata JSON
    bool validateMetadata(const nlohmann::json& metadata, std::string& error) const;
    
    // Process uploaded files and return their storage paths
    std::vector<std::pair<std::string, std::string>> processFiles(
        const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files
    );
};
