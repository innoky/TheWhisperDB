#pragma once

#include <string>
#include <stdexcept>

enum class HttpRequest {
    GET,
    POST,
    PUT,
    PATCH,
    DELETE
};

inline const char* to_string(HttpRequest method) {
    switch(method) {
        case HttpRequest::GET: return "GET";
        case HttpRequest::POST: return "POST";
        case HttpRequest::PUT: return "PUT";
        case HttpRequest::PATCH: return "PATCH";
        case HttpRequest::DELETE: return "DELETE";
        default: return "UNKNOWN";
    }
}

inline HttpRequest from_string(const std::string& method) {
    if (method == "GET") return HttpRequest::GET;
    else if (method == "POST") return HttpRequest::POST;
    else if (method == "PUT") return HttpRequest::PUT;
    else if (method == "PATCH") return HttpRequest::PATCH;
    else if (method == "DELETE") return HttpRequest::DELETE;
    else throw std::invalid_argument("Invalid HTTP method string: " + method);
}
