#include "core/GraphDB.hpp"
#include <filesystem>
#include <fstream>
#include "config.hpp"
#include <iostream>


GraphDB::GraphDB()
{
    this->initGraphDB();
}

GraphDB::~GraphDB()
{
    saveToJson();
}

void GraphDB::initGraphDB()
{
    if(std::filesystem::exists(DB_FILE_PATH)) loadFromJson();
    else createJson();
}

std::string GraphDB::serialize() const
{
    std::vector<nlohmann::json> nodes_json;
        nodes_json.reserve(nodes.size());

        for (const auto& node : nodes) {
            nodes_json.push_back(node->to_json());
        }

        nlohmann::json j;
        j["nodes"] = nodes_json;

        return j.dump();
}

void GraphDB::loadFromJson()
{
    std::ifstream file(DB_FILE_PATH);
    nlohmann::json j;
    try {
        file >> j;
        if (!j.contains("nodes") || !j["nodes"].is_array()) {
            std::cerr << "Invalid JSON structure: 'nodes' array missing" << std::endl;
            createJson();
            return;
        }
        for (const auto& node_json : j["nodes"]) {
            nodes.push_back(new Node(node_json));
        }
    } catch (const nlohmann::json::parse_error& e) {
        // обработка ошибки парсинга
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        createJson();
    }
    
}

void GraphDB::createJson() {
    nodes.clear();
    std::ofstream file(DB_FILE_PATH);
    nlohmann::json j;
    j["nodes"] = nlohmann::json::array();
    file << j.dump(4);
}

void GraphDB::saveToJson()
{
    nlohmann::json j;
    j["nodes"] = nlohmann::json::array();

    for (auto node : this->nodes)
    {
        j["nodes"].push_back(node->to_json()); 
    }

    std::ofstream file(DB_FILE_PATH);
    file << j.dump(4);
}