#include "core/GNode.hpp"
#include <climits>


Node::Node(const nlohmann::json& j, int id){
    this->id = id;
    title = j.at("title").get<std::string>();
    
    // Handle course as either string or integer
    if (j.contains("course")) {
        if (j["course"].is_number_integer()) {
            course = j["course"].get<int>();
        } else if (j["course"].is_string()) {
            try {
                course = std::stoi(j["course"].get<std::string>());
            } catch (const std::exception&) {
                course = 0; // Default value if conversion fails
            }
        } else {
            course = 0; // Default value if not a string or integer
        }
    } else {
        course = 0; // Default value if course is not provided
    }
    
    subject = j.value("subject", "");
    description = j.value("description", "");
    author = j.value("author", "");
    date = j.value("date", "");
    
    // Handle tags as either array or string (same as in the other constructor)
    if (j.contains("tags")) {
        if (j["tags"].is_array()) {
            tags = j["tags"].get<std::vector<std::string>>();
        } else if (j["tags"].is_string()) {
            std::string tags_str = j["tags"].get<std::string>();
            std::istringstream iss(tags_str);
            std::string tag;
            while (std::getline(iss, tag, ',')) {
                // Trim whitespace
                tag.erase(0, tag.find_first_not_of(" \t"));
                tag.erase(tag.find_last_not_of(" \t") + 1);
                if (!tag.empty()) {
                    tags.push_back(tag);
                }
            }
        }
    }
    
    storage_path = j.value("storage_path", "");
    
    if (j.contains("LinkedNodes") && j["LinkedNodes"].is_array()) {
        LinkedNodes = j["LinkedNodes"].get<std::vector<int>>();
    }
}

Node::Node(const nlohmann::json& j){
    id = j.value("id", INT_MAX);
    title = j.at("title").get<std::string>();
    
    // Handle course as either string or integer
    if (j.contains("course")) {
        if (j["course"].is_number_integer()) {
            course = j["course"].get<int>();
        } else if (j["course"].is_string()) {
            try {
                course = std::stoi(j["course"].get<std::string>());
            } catch (const std::exception&) {
                course = 0; // Default value if conversion fails
            }
        } else {
            course = 0; // Default value if not a string or integer
        }
    } else {
        course = 0; // Default value if course is not provided
    }
    
    subject = j.value("subject", "");
    description = j.value("description", "");
    author = j.value("author", "");
    date = j.value("date", "");
    
    // Handle tags as either array or string
    if (j.contains("tags")) {
        if (j["tags"].is_array()) {
            tags = j["tags"].get<std::vector<std::string>>();
        } else if (j["tags"].is_string()) {
            std::string tags_str = j["tags"].get<std::string>();
            std::istringstream iss(tags_str);
            std::string tag;
            while (std::getline(iss, tag, ',')) {
                // Trim whitespace
                tag.erase(0, tag.find_first_not_of(" \t"));
                tag.erase(tag.find_last_not_of(" \t") + 1);
                if (!tag.empty()) {
                    tags.push_back(tag);
                }
            }
        }
    }
    
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