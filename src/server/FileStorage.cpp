#include "server/FileStorage.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cstring>

FileStorage::FileStorage(const std::string& basePath) 
    : basePath_(basePath) {
    ensureStorageDirectory();
}

std::string FileStorage::saveFile(const std::string& filename, const std::string& content) {
    std::cout << "Saving file with original name: " << filename << std::endl;
    std::string storagePath = generateStoragePath();
    std::string uniqueFilename = generateUniqueFilename(filename);
    std::string fullPath = storagePath + "/" + uniqueFilename;
    
    std::cout << "Generated unique filename: " << uniqueFilename << std::endl;
    std::cout << "Full storage path: " << fullPath << std::endl;
    
    // Ensure the directory exists
    std::filesystem::path dirPath(basePath_ + "/" + storagePath);
    std::filesystem::create_directories(dirPath);
    
    // Write the file
    std::string fullSystemPath = basePath_ + "/" + fullPath;
    std::cout << "Writing to: " << fullSystemPath << std::endl;
    
    std::ofstream outFile(fullSystemPath, std::ios::binary);
    if (!outFile) {
        std::string error = "Failed to create file: " + fullSystemPath + " (" + strerror(errno) + ")";
        std::cerr << error << std::endl;
        throw std::runtime_error(error);
    }
    outFile.write(content.data(), content.size());
    
    std::cout << "File saved successfully: " << fullPath << " (" << content.size() << " bytes)" << std::endl;
    return fullPath;
}

std::string FileStorage::saveFile(const std::string& filename, const std::vector<uint8_t>& content) {
    std::cout << "Saving binary file with original name: " << filename << " (" << content.size() << " bytes)" << std::endl;
    std::string storagePath = generateStoragePath();
    std::string uniqueFilename = generateUniqueFilename(filename);
    std::string fullPath = storagePath + "/" + uniqueFilename;
    
    std::cout << "Generated unique filename: " << uniqueFilename << std::endl;
    std::cout << "Full storage path: " << fullPath << std::endl;
    
    // Ensure the directory exists
    std::filesystem::path dirPath(basePath_ + "/" + storagePath);
    std::filesystem::create_directories(dirPath);
    
    // Write the binary file
    std::string fullSystemPath = basePath_ + "/" + fullPath;
    std::cout << "Writing binary data to: " << fullSystemPath << std::endl;
    
    std::ofstream outFile(fullSystemPath, std::ios::binary);
    if (!outFile) {
        std::string error = "Failed to create binary file: " + fullSystemPath + " (" + strerror(errno) + ")";
        std::cerr << error << std::endl;
        throw std::runtime_error(error);
    }
    
    outFile.write(reinterpret_cast<const char*>(content.data()), content.size());
    outFile.close();
    
    std::cout << "Binary file saved successfully: " << fullPath << " (" << content.size() << " bytes)" << std::endl;
    return fullPath;
}

std::string FileStorage::readFile(const std::string& filepath) const {
    std::ifstream inFile(basePath_ + "/" + filepath, std::ios::binary);
    if (!inFile) {
        throw std::runtime_error("File not found: " + filepath);
    }
    
    std::string content((std::istreambuf_iterator<char>(inFile)), 
                        std::istreambuf_iterator<char>());
    return content;
}

bool FileStorage::deleteFile(const std::string& filepath) const {
    return std::filesystem::remove(basePath_ + "/" + filepath);
}

std::string FileStorage::getFullPath(const std::string& filepath) const {
    return basePath_ + "/" + filepath;
}

void FileStorage::ensureStorageDirectory() const {
    std::filesystem::create_directories(basePath_);
}

std::string FileStorage::generateUniqueFilename(const std::string& originalName) const {
    std::cout << "Generating unique filename for: " << originalName << std::endl;
    
    // Extract the base name and extension
    std::string baseName = originalName;
    std::string extension = "";
    
    // Find the last dot in the filename
    size_t dotPos = originalName.find_last_of('.');
    if (dotPos != std::string::npos && dotPos < originalName.length() - 1) {
        // Split into base name and extension
        baseName = originalName.substr(0, dotPos);
        extension = originalName.substr(dotPos); // includes the dot
        std::cout << "  Base name: " << baseName << ", Extension: " << extension << std::endl;
    } else {
        std::cout << "  No extension found in filename" << std::endl;
    }
    
    // Generate a timestamp
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    // Generate a random number for additional uniqueness
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    int random = dis(gen);
    
    // Combine components to create the unique filename
    std::ostringstream ss;
    ss << baseName << "_" << millis << "_" << random << extension;
    
    std::string result = ss.str();
    std::cout << "  Generated unique filename: " << result << std::endl;
    return result;
}

std::string FileStorage::generateStoragePath() const {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm = *std::localtime(&in_time_t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y/%m/%d");
    
    return ss.str();
}
