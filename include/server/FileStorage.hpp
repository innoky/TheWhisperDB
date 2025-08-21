#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

class FileStorage {
public:
    FileStorage(const std::string& basePath = "storage");
    
    // Saves file content to storage and returns the relative path
    std::string saveFile(const std::string& filename, const std::string& content);
    
    // Saves binary file content to storage and returns the relative path
    std::string saveFile(const std::string& filename, const std::vector<uint8_t>& content);
    
    // Reads file content from storage
    std::string readFile(const std::string& filepath) const;
    
    // Deletes a file from storage
    bool deleteFile(const std::string& filepath) const;
    
    // Gets the full path for a stored file
    std::string getFullPath(const std::string& filepath) const;
    
    // Ensures the storage directory exists
    void ensureStorageDirectory() const;
    
private:
    std::string basePath_;
    
    // Generates a unique filename to prevent collisions
    std::string generateUniqueFilename(const std::string& originalName) const;
    
    // Creates a directory structure based on current date
    std::string generateStoragePath() const;
};
