#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class Node
{
public:
    Node(const nlohmann::json& json);
    Node(int id, std::string name); 

    std::string to_str();
    nlohmann::json to_json() const;

private:
    int id;
    std::string title;
    std::string description;
    std::string storage_path;
    std::vector<Node*> LinkedNodes;
};