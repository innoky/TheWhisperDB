#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "http/MultipartParser.hpp"
#include "const/rest_enums.hpp"

namespace whisperdb {
namespace http {

/**
 * HTTP Request object containing all request data
 */
struct Request {
    HttpRequest method;                                    // GET, POST, PUT, DELETE
    std::string path;                                      // Clean path without query string
    std::unordered_map<std::string, std::string> query;    // Query parameters (?key=value)
    std::unordered_map<std::string, std::string> params;   // Path parameters (/nodes/:id)
    std::vector<MultipartPart> parts;                      // Multipart parts or body
    std::string rawBody;                                   // Raw body for JSON requests

    // Convenience methods
    std::string getParam(const std::string& key, const std::string& defaultValue = "") const {
        auto it = params.find(key);
        return it != params.end() ? it->second : defaultValue;
    }

    std::string getQuery(const std::string& key, const std::string& defaultValue = "") const {
        auto it = query.find(key);
        return it != query.end() ? it->second : defaultValue;
    }

    bool hasQuery(const std::string& key) const {
        return query.find(key) != query.end();
    }
};

/**
 * HTTP Response object
 */
struct Response {
    int status = 200;
    std::string contentType = "application/json";
    std::string body;

    static Response ok(const std::string& body) {
        return {200, "application/json", body};
    }

    static Response created(const std::string& body) {
        return {201, "application/json", body};
    }

    static Response noContent() {
        return {204, "application/json", ""};
    }

    static Response badRequest(const std::string& message) {
        return {400, "application/json", R"({"status":"error","message":")" + message + R"("})"};
    }

    static Response notFound(const std::string& message = "Not found") {
        return {404, "application/json", R"({"status":"error","message":")" + message + R"("})"};
    }

    static Response methodNotAllowed() {
        return {405, "application/json", R"({"status":"error","message":"Method not allowed"})"};
    }

    static Response error(const std::string& message) {
        return {500, "application/json", R"({"status":"error","message":")" + message + R"("})"};
    }
};

} // namespace http
} // namespace whisperdb
