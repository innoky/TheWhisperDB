#include <vector>
#include "GNode.hpp"

class GraphDB
{
public:
    GraphDB();
    ~GraphDB();
    std::string serialize() const;

private:
    std::vector<Node*> nodes;

    void initGraphDB();
    void createJson();
    void loadFromJson();
    void saveToJson();

};