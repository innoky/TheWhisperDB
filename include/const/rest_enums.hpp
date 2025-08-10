#pragma once

#include <string>
#include <stdexcept>

enum class HttpRequest {
    GET ,
    POST,
    PATCH,
    PUT,
};


inline const char* to_string(HttpRequest method) {
    switch(method) {
        case HttpRequest::GET: return "GET";
        case HttpRequest::POST: return "POST";
        case HttpRequest::PATCH: return "PATCH";
        case HttpRequest::PUT: return "PUT";
        default: return "UNKNOWN";
    }
}


inline HttpRequest from_string(const std::string& method) {
    if (method == "GET") return HttpRequest::GET;
    else if (method == "POST") return HttpRequest::POST;
    else if (method == "PATCH") return HttpRequest::PATCH;
    else if (method == "PUT") return HttpRequest::PUT;
    else throw std::invalid_argument("Invalid HTTP method string: " + method);
}
