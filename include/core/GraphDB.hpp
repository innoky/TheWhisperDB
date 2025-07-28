#pragma once

#include <vector>
#include "GNode.hpp"

class GraphDB
{
public:
    GraphDB();
    ~GraphDB();
    std::string serialize() const;
    Node find(int id) const;
    // void addNode(const Node& node) { nodes[node.id] = new Node(node);
    void addNode(nlohmann::json& j);

    int getSize() const { return size; }
    void sizeUpOne() { size++; }
    void sizeDownOne() { size--; }
    void setSize(int new_size) { size = new_size; }

private:
    std::unordered_map<int, Node*> nodes; // Map of nodes by their unique ID
    int size;
    void initGraphDB();
    void createJson();
    void loadFromJson();
    void saveToJson();

};