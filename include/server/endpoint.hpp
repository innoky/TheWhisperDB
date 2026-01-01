#pragma once

#include <string>
#include <functional>
#include <vector>
#include "const/rest_enums.hpp"
#include "http/MultipartParser.hpp"

using whisperdb::http::MultipartPart;

class endpoint
{
    std::function<std::string(const std::vector<MultipartPart>&)> handler;
    HttpRequest rest_type;
    std::string path;

public:
    endpoint(std::function<std::string(const std::vector<MultipartPart>&)> handler,
             HttpRequest rest_type,
             const std::string& path)
        : handler(std::move(handler)), rest_type(rest_type), path(path) {}

    std::string get_path() const { return path; }
    std::function<std::string(const std::vector<MultipartPart>&)> get_handler() const { return handler; }
    HttpRequest get_rest_type() const { return rest_type; }
};
