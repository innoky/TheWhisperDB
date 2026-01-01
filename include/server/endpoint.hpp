#pragma once

#include <string>
#include <functional>
#include <vector>
#include <regex>
#include "const/rest_enums.hpp"
#include "http/Request.hpp"

using whisperdb::http::Request;
using whisperdb::http::Response;

class endpoint
{
    std::function<Response(const Request&)> handler;
    HttpRequest rest_type;
    std::string pathPattern;          // Original pattern (e.g., "/api/nodes/:id")
    std::regex pathRegex;             // Compiled regex
    std::vector<std::string> paramNames;  // Parameter names in order

public:
    endpoint(std::function<Response(const Request&)> handler,
             HttpRequest rest_type,
             const std::string& path)
        : handler(std::move(handler)), rest_type(rest_type), pathPattern(path)
    {
        // Convert path pattern to regex
        // /api/nodes/:id -> /api/nodes/([^/]+)
        std::string regexPattern = "^";
        std::string current;

        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == ':') {
                // Start of parameter
                regexPattern += current;
                current.clear();

                // Extract parameter name
                std::string paramName;
                ++i;
                while (i < path.size() && path[i] != '/') {
                    paramName += path[i];
                    ++i;
                }
                --i;  // Back one step

                paramNames.push_back(paramName);
                regexPattern += "([^/]+)";
            } else {
                // Escape special regex characters
                if (path[i] == '.' || path[i] == '+' || path[i] == '*' ||
                    path[i] == '?' || path[i] == '^' || path[i] == '$' ||
                    path[i] == '{' || path[i] == '}' || path[i] == '[' ||
                    path[i] == ']' || path[i] == '|' || path[i] == '(' ||
                    path[i] == ')' || path[i] == '\\') {
                    current += '\\';
                }
                current += path[i];
            }
        }
        regexPattern += current + "$";

        pathRegex = std::regex(regexPattern);
    }

    std::string get_path() const { return pathPattern; }
    HttpRequest get_rest_type() const { return rest_type; }

    // Check if path matches and extract parameters
    bool matches(const std::string& path, std::unordered_map<std::string, std::string>& params) const {
        std::smatch match;
        if (std::regex_match(path, match, pathRegex)) {
            // Extract captured groups as parameters
            for (size_t i = 0; i < paramNames.size() && i + 1 < match.size(); ++i) {
                params[paramNames[i]] = match[i + 1].str();
            }
            return true;
        }
        return false;
    }

    Response handle(const Request& req) const {
        return handler(req);
    }
};
