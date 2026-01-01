#include "core/GraphDB.hpp"
#include "server/FileStorage.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "config.hpp"
#include <iostream>


GraphDB::GraphDB() : size(0) {
    fileStorage = std::make_unique<FileStorage>("storage");
    this->initGraphDB();
}

GraphDB::~GraphDB() {
    saveToJson();
    // Clean up node pointers
    for (auto& pair : nodes) {
        delete pair.second;
    }
}

void GraphDB::initGraphDB()
{
    if(std::filesystem::exists(DB_FILE_PATH)) loadFromJson();
    else createJson();
}

Node GraphDB::find(const std::string& id) const
{
    auto it = nodes.find(id);
    if (it != nodes.end()) {
        return *(it->second);
    } else {
        throw std::runtime_error("Node not found");
    }
}

std::string GraphDB::serialize() const
{
    std::vector<nlohmann::json> nodes_json;
    nodes_json.reserve(nodes.size());

    for (const auto& node_pair : nodes) {
        nodes_json.push_back(node_pair.second->to_json());
    }

    nlohmann::json j;
    j["nodes"] = nodes_json;

    return j.dump();
}

void GraphDB::loadFromJson() {
    try {
        std::ifstream file(DB_FILE_PATH);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open database file");
        }

        nlohmann::json j;
        file >> j;

        // Clear existing nodes
        for (auto& pair : nodes) {
            delete pair.second;
        }
        nodes.clear();
        nodeFiles.clear();
        size = 0;

        // Load nodes
        if (j.contains("nodes") && j["nodes"].is_array()) {
            for (const auto& nodeJson : j["nodes"]) {
                Node* node = new Node(nodeJson);
                std::string nodeId = std::to_string(node->getId());
                nodes[nodeId] = node;
                size++;
            }
        }
        
        // Load file associations
        if (j.contains("nodeFiles") && j["nodeFiles"].is_object()) {
            for (auto it = j["nodeFiles"].begin(); it != j["nodeFiles"].end(); ++it) {
                std::string nodeId = it.key();
                if (it.value().is_array()) {
                    for (const auto& filePath : it.value()) {
                        nodeFiles[nodeId].push_back(filePath);
                    }
                }
            }
        }
        
        setSize(j.value("size", 0));
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        createJson();
    } catch (const std::exception& e) {
        std::cerr << "Error loading database: " << e.what() << std::endl;
        createJson();
    }
}

void GraphDB::createJson() {
    nodes.clear();
    this->setSize(0);

    // Создаем директорию если не существует
    std::filesystem::path dbPath(DB_FILE_PATH);
    if (dbPath.has_parent_path()) {
        std::filesystem::create_directories(dbPath.parent_path());
    }

    std::ofstream file(DB_FILE_PATH);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create database file: " + DB_FILE_PATH);
    }

    nlohmann::json j;
    j["nodes"] = nlohmann::json::array();
    j["size"] = getSize();
    file << j.dump(4);
}

void GraphDB::saveToJson() {
    // Создаем директорию если не существует
    std::filesystem::path dbPath(DB_FILE_PATH);
    if (dbPath.has_parent_path()) {
        std::filesystem::create_directories(dbPath.parent_path());
    }

    std::ofstream file(DB_FILE_PATH);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open database file for writing: " + DB_FILE_PATH);
    }

    nlohmann::json j;
    j["size"] = size;
    j["nodes"] = nlohmann::json::array();
    
    // Convert node IDs to integers for sorting
    std::vector<std::pair<int, Node*>> sorted_nodes;
    for (const auto& pair : nodes) {
        try {
            int nodeId = std::stoi(pair.first);
            sorted_nodes.emplace_back(nodeId, pair.second);
        } catch (const std::exception& e) {
            // If node ID is not a number, skip sorting for this node
            j["nodes"].push_back(pair.second->to_json());
        }
    }
    
    // Sort nodes by numeric ID
    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
             [](const auto& a, const auto& b) {
                  return a.first < b.first;
              });

    // Add sorted nodes to JSON
    for (const auto& [_, node] : sorted_nodes) {
        j["nodes"].push_back(node->to_json());
    }

    // Save file associations
    for (const auto& [nodeId, files] : nodeFiles) {
        j["nodeFiles"][nodeId] = files;
    }

    file << j.dump(4);
}

std::string GraphDB::addNode(nlohmann::json& j, const std::vector<std::pair<std::string, std::string>>& files) {
    std::string id = generateNodeId();
    j["id"] = std::stoi(id);  // Add the ID to the JSON object
    Node* node = new Node(j);  // Use the JSON constructor
    nodes[id] = node;
    size++;
    
    // Add files to the node
    for (const auto& file : files) {
        addFileToNode(id, file.first, file.second);
    }
    
    saveToJson();
    return id;
}

bool GraphDB::deleteNode(const std::string& id) {
    auto nodeIt = nodes.find(id);
    if (nodeIt == nodes.end()) {
        return false;
    }
    
    // Delete associated files
    auto filesIt = nodeFiles.find(id);
    if (filesIt != nodeFiles.end()) {
        for (const auto& filePath : filesIt->second) {
            fileStorage->deleteFile(filePath);
        }
        nodeFiles.erase(filesIt);
    }
    
    // Delete the node
    delete nodeIt->second;
    nodes.erase(nodeIt);
    
    size--;
    saveToJson();
    return true;
}

std::string GraphDB::addFileToNode(const std::string& nodeId, const std::string& filename, const std::string& content) {
    if (nodes.find(nodeId) == nodes.end()) {
        throw std::runtime_error("Node not found");
    }
    
    std::string filePath = fileStorage->saveFile(filename, content);
    nodeFiles[nodeId].push_back(filePath);
    
    // Update node's storage path if this is the first file
    if (nodeFiles[nodeId].size() == 1) {
        nodes[nodeId]->setStoragePath(filePath);
    }
    
    saveToJson();
    return filePath;
}

std::string GraphDB::addFileToNode(const std::string& nodeId, const std::string& filename, const std::vector<uint8_t>& content) {
    if (nodes.find(nodeId) == nodes.end()) {
        throw std::runtime_error("Node not found");
    }
    
    std::string filePath = fileStorage->saveFile(filename, content);
    nodeFiles[nodeId].push_back(filePath);
    
    // Update node's storage path if this is the first file
    if (nodeFiles[nodeId].size() == 1) {
        nodes[nodeId]->setStoragePath(filePath);
    }
    
    saveToJson();
    return filePath;
}

bool GraphDB::removeFileFromNode(const std::string& nodeId, const std::string& filePath) {
    auto filesIt = nodeFiles.find(nodeId);
    if (filesIt == nodeFiles.end()) {
        return false;
    }
    
    auto& files = filesIt->second;
    auto it = std::find(files.begin(), files.end(), filePath);
    if (it == files.end()) {
        return false;
    }
    
    // Remove the file from storage
    fileStorage->deleteFile(filePath);
    
    // Remove the file path from the node's file list
    files.erase(it);
    
    // If this was the last file, clear the storage path
    if (files.empty()) {
        nodes[nodeId]->setStoragePath("");
    }
    
    saveToJson();
    return true;
}

std::vector<std::string> GraphDB::getNodeFiles(const std::string& nodeId) const {
    auto it = nodeFiles.find(nodeId);
    if (it != nodeFiles.end()) {
        return it->second;
    }
    return {};
}

std::string GraphDB::generateNodeId() {
    static int nextId = 1;
    while (nodes.find(std::to_string(nextId)) != nodes.end()) {
        nextId++;
    }
    return std::to_string(nextId++);
}