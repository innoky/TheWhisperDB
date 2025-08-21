#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class Node
{
public:
    // Nodes of knowlage data (each node is a lecture, conspect, etc.)
    
    Node(const nlohmann::json& json, int id);
    Node(const nlohmann::json& json);
   
    
    std::string to_str();
    nlohmann::json to_json() const;

    // Getters
    int getId() const { return id; }
    std::string getTitle() const { return title; }
    std::string getStoragePath() const { return storage_path; }
    
    // Setters
    void setStoragePath(const std::string& path) { storage_path = path; }

private:
    int id; // Unique identifier for the node
    std::string title; // Title of the node (lecture, conspect, etc.)
    int course; // Course ID
    std::string subject; // Subject of the node
    std::string description; // Description of the node
    std::string author; // Author of the node
    std::string date; // Date of creation or last modification
    std::vector<std::string> tags; // Tags associated with the node
    std::string storage_path; // Path to the main file associated with this node

    std::vector<int> LinkedNodes; // List of connected node IDs 
};