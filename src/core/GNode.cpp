#include "core/GNode.hpp"


Node::Node(const nlohmann::json& j) {
    this->id = j["id"];
    this->title = j["title"];
    this->description = j["description"];
}



nlohmann::json Node::to_json() const {
    return {
        {"id", id},
        {"title", title},
        {"description", description},
       
    };
}

