#pragma once 

#include <string>
#include <functional>
#include "const/rest_enums.hpp"

class endpoint
{
    std::function<std::string(const std::vector<std::string>&)> handler;
    HttpRequest rest_type;
    std::string path;

public:
    endpoint(std::function<std::string(const std::vector<std::string>&)> handler, 
             HttpRequest rest_type, 
             const std::string& path)
        : handler(std::move(handler)), rest_type(rest_type), path(path) {}

    std::string get_path() const { return path; }
    std::function<std::string(const std::vector<std::string>&)> get_handler() const { return handler; }
    HttpRequest get_rest_type() const { return rest_type; }
};
