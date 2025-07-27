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

private:
    std::unordered_map<int, Node*> nodes; // Map of nodes by their unique ID

    void initGraphDB();
    void createJson();
    void loadFromJson();
    void saveToJson();

};