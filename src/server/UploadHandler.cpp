#include "server/UploadHandler.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <cctype>

UploadHandler::UploadHandler(GraphDB& db) : db_(db) {}

std::string UploadHandler::handleUpload(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files,
    const nlohmann::json& metadata
) {
    nlohmann::json response;
    
    // Validate metadata
    std::string error;
    if (!validateMetadata(metadata, error)) {
        response["status"] = "error";
        response["message"] = "Invalid metadata: " + error;
        return response.dump();
    }
    
    try {
        // Process files and create node with metadata
        nlohmann::json nodeData = metadata;
        
        // Convert course to number if it's a string
        if (nodeData.contains("course") && nodeData["course"].is_string()) {
            try {
                int courseNum = std::stoi(nodeData["course"].get<std::string>());
                nodeData["course"] = courseNum;
            } catch (const std::exception& e) {
                // If conversion fails, remove the course field
                nodeData.erase("course");
            }
        }
        
        // Add current timestamp if not provided
        if (!nodeData.contains("date")) {
            auto now = std::time(nullptr);
            char buf[20];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            nodeData["date"] = std::string(buf);
        }
        
        // Add the node to the database
        std::string nodeId = db_.addNode(nodeData);
        
        // Store the files - convert string content back to vector<uint8_t> for binary safety
        for (const auto& file : files) {
            db_.addFileToNode(nodeId, file.first, file.second);
        }
        
        // Get the stored file paths for the response
        auto storedFiles = db_.getNodeFiles(nodeId);
        
        // Prepare success response
        response["status"] = "success";
        response["nodeId"] = nodeId;
        response["files"] = nlohmann::json::array();
        
        // Add file information to the response
        for (size_t i = 0; i < files.size() && i < storedFiles.size(); ++i) {
            response["files"].push_back({
                {"originalName", files[i].first},
                {"storedPath", storedFiles[i]}
            });
        }
        
    } catch (const std::exception& e) {
        response["status"] = "error";
        response["message"] = std::string("Failed to process upload: ") + e.what();
    }
    
    return response.dump();
}

bool UploadHandler::validateMetadata(const nlohmann::json& metadata, std::string& error) const {
    if (!metadata.is_object()) {
        error = "Metadata must be a JSON object";
        return false;
    }
    
    // Required fields
    const std::vector<std::string> requiredFields = {"title", "author", "subject"};
    
    for (const auto& field : requiredFields) {
        if (!metadata.contains(field) || metadata[field].is_null()) {
            error = "Missing required field: " + field;
            return false;
        }
    }
    
    // Validate field types
    if (!metadata["title"].is_string() || metadata["title"].get<std::string>().empty()) {
        error = "Title must be a non-empty string";
        return false;
    }
    
    if (!metadata["author"].is_string() || metadata["author"].get<std::string>().empty()) {
        error = "Author must be a non-empty string";
        return false;
    }
    
    if (!metadata["subject"].is_string() || metadata["subject"].get<std::string>().empty()) {
        error = "Subject must be a non-empty string";
        return false;
    }
    
    // Optional fields validation
    if (metadata.contains("course") && !metadata["course"].is_null()) {
        if (!metadata["course"].is_number_integer() && !metadata["course"].is_string()) {
            error = "Course must be an integer or string";
            return false;
        }
        
        // If it's a string, try to convert it to a number
        if (metadata["course"].is_string()) {
            std::string courseStr = metadata["course"].get<std::string>();
            if (courseStr.empty()) {
                error = "Course cannot be an empty string";
                return false;
            }
            try {
                // This will throw if the string is not a valid number
                std::stoi(courseStr);
            } catch (const std::exception&) {
                error = "Course must be a valid number";
                return false;
            }
        }
    }
    
    if (metadata.contains("tags") && !metadata["tags"].is_array()) {
        error = "Tags must be an array";
        return false;
    }
    
    // Validate tags array contents
    if (metadata.contains("tags")) {
        for (const auto& tag : metadata["tags"]) {
            if (!tag.is_string() || tag.get<std::string>().empty()) {
                error = "Tags must be non-empty strings";
                return false;
            }
        }
    }
    
    return true;
}

std::vector<std::pair<std::string, std::string>> UploadHandler::processFiles(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files
) {
    if (files.empty()) {
        throw std::runtime_error("At least one file is required");
    }
    
    std::vector<std::pair<std::string, std::string>> processedFiles;
    
    for (const auto& file : files) {
        std::string filename = file.first;
        const std::vector<uint8_t>& content = file.second;
        
        if (filename.empty() || content.empty()) {
            throw std::runtime_error("Filename and content cannot be empty");
        }
        
        // Sanitize filename - remove any path components
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }
        
        // Ensure the filename has an extension
        size_t dotPos = filename.find_last_of('.');
        if (dotPos == std::string::npos || dotPos == filename.length() - 1) {
            // No extension found or dot is the last character
            filename += ".dat";
        }
        
        std::cout << "Processing file: " << filename << " (" << content.size() << " bytes)" << std::endl;
        
        // Return the filename and an empty path - the actual storage will be handled by the caller
        processedFiles.emplace_back(filename, "");
    }
    
    return processedFiles;
}
