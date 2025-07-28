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

Node GraphDB::find(int id) const
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
        for (const nlohmann::json& node_json : j["nodes"]) {
            nodes.emplace(node_json["id"].get<int>(), new Node(node_json));
        }

        setSize(j["size"].get<int>()); 

    } catch (const nlohmann::json::parse_error& e) {

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
    std::ofstream file(DB_FILE_PATH);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << DB_FILE_PATH << std::endl;
        return;
    }

    nlohmann::json j;
    j["nodes"] = nlohmann::json::array();
    j["size"] = getSize();

    // Копируем пары в вектор для сортировки по ключу (id)
    std::vector<std::pair<int, Node*>> sorted_nodes(nodes.begin(), nodes.end());

    // Сортируем по id (ключу)
    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
              [](const auto& a, const auto& b) {
                  return a.first < b.first;
              });

    // Записываем узлы в порядке возрастания id
    for (const auto& node_pair : sorted_nodes) {
        j["nodes"].push_back(node_pair.second->to_json());
    }

    file << j.dump(4);
}




void GraphDB::addNode(nlohmann::json& j)
{
    int id = this->getSize() + 1; // Generate a new ID based on current size
    if (nodes.find(id) != nodes.end()) {
        throw std::runtime_error("Node with this ID already exists");
    }
    sizeUpOne();
    std::cout << "Adding node with ID: " << id << std::endl;
    nodes.emplace(id, new Node(j, id));
    std::cout << nodes[id]->to_str();
}