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
    int getCourse() const { return course; }
    std::string getSubject() const { return subject; }
    std::string getDescription() const { return description; }
    std::string getAuthor() const { return author; }
    std::string getDate() const { return date; }
    std::vector<std::string> getTags() const { return tags; }
    std::string getStoragePath() const { return storage_path; }
    std::vector<int> getLinkedNodes() const { return LinkedNodes; }
    std::vector<float> getEmbedding() const { return embedding; }
    bool hasEmbedding() const { return !embedding.empty(); }

    // Setters
    void setTitle(const std::string& t) { title = t; }
    void setCourse(int c) { course = c; }
    void setSubject(const std::string& s) { subject = s; }
    void setDescription(const std::string& d) { description = d; }
    void setAuthor(const std::string& a) { author = a; }
    void setDate(const std::string& d) { date = d; }
    void setTags(const std::vector<std::string>& t) { tags = t; }
    void setStoragePath(const std::string& path) { storage_path = path; }
    void setLinkedNodes(const std::vector<int>& nodes) { LinkedNodes = nodes; }
    void setEmbedding(const std::vector<float>& emb) { embedding = emb; }

    // Update from JSON (partial update)
    void updateFromJson(const nlohmann::json& j);

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
    std::vector<float> embedding; // Vector embedding for semantic similarity
};