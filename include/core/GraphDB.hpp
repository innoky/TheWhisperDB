#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "GNode.hpp"

// Forward declaration
class FileStorage;

class GraphDB
{
public:
    GraphDB();
    ~GraphDB();
    
    // Node operations
    std::string serialize() const;
    Node find(const std::string& id) const;
    bool exists(const std::string& id) const;
    std::string addNode(nlohmann::json& j, const std::vector<std::pair<std::string, std::string>>& files = {});
    bool updateNode(const std::string& id, const nlohmann::json& updates);
    bool deleteNode(const std::string& id);

    // Query operations
    nlohmann::json getAllNodes(
        const std::string& sortBy = "id",
        const std::string& order = "asc",
        int limit = -1,
        int offset = 0
    ) const;

    nlohmann::json findNodes(
        const std::unordered_map<std::string, std::string>& filters,
        const std::string& sortBy = "id",
        const std::string& order = "asc",
        int limit = -1,
        int offset = 0
    ) const;

    // Count nodes (with optional filters)
    int countNodes(const std::unordered_map<std::string, std::string>& filters = {}) const;
    
    // File operations
    std::string addFileToNode(const std::string& nodeId, const std::string& filename, const std::string& content);
    std::string addFileToNode(const std::string& nodeId, const std::string& filename, const std::vector<uint8_t>& content);
    bool removeFileFromNode(const std::string& nodeId, const std::string& filePath);
    std::vector<std::string> getNodeFiles(const std::string& nodeId) const;
    
    // Getters and setters
    int getSize() const { return size; }
    void setSize(int new_size) { size = new_size; }
    
    // Persistence
    void saveToJson();
    
private:
    std::unordered_map<std::string, Node*> nodes; // Map of nodes by their unique ID
    std::unordered_map<std::string, std::vector<std::string>> nodeFiles; // Maps node ID to list of file paths
    int size;
    std::unique_ptr<FileStorage> fileStorage;
    
    void initGraphDB();
    void createJson();
    void loadFromJson();
    std::string generateNodeId();
};