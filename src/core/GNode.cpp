#include "core/GNode.hpp"
#include <climits>


Node::Node(const nlohmann::json& j, int id){
    this->id = id;
    title = j.at("title").get<std::string>();
    course = j.value("course", 0);
    subject = j.value("subject", "");
    description = j.value("description", "");
    author = j.value("author", "");
    date = j.value("date", "");
    tags = j.value("tags", "");
    storage_path = j.value("storage_path", "");
    
    if (j.contains("LinkedNodes") && j["LinkedNodes"].is_array()) {
        LinkedNodes = j["LinkedNodes"].get<std::vector<int>>();
    }
}

Node::Node(const nlohmann::json& j){
    id = j.value("id", INT_MAX);
    title = j.at("title").get<std::string>();
    course = j.value("course", 0);
    subject = j.value("subject", "");
    description = j.value("description", "");
    author = j.value("author", "");
    date = j.value("date", "");
    tags = j.value("tags", "");
    storage_path = j.value("storage_path", "");
    
    if (j.contains("LinkedNodes") && j["LinkedNodes"].is_array()) {
        LinkedNodes = j["LinkedNodes"].get<std::vector<int>>();
    }
}


nlohmann::json Node::to_json() const {
    return {
        {"id", id},
        {"title", title},
        {"course", course},
        {"subject", subject},
        {"description", description},
        {"author", author},
        {"date", date},
        {"tags", tags},
        {"storage_path", storage_path},
        {"LinkedNodes", LinkedNodes}
    };
}


std::string Node::to_str() {
    return "Node ID: " + std::to_string(id) + "\n" +
           "Title: " + title + "\n" +
           "Course: " + std::to_string(course) + "\n" +
           "Subject: " + subject + "\n" +
           "Description: " + description + "\n";
}