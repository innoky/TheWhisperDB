#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class Node
{
public:
    Node(const nlohmann::json& json);
    Node(int id, std::string name); 

    std::string to_str();

private:
    int id;
    std::string name;
    std::string storage_path;
    std::vector<Node*> LinkedNodes;
};