#pragma once

#include <boost/asio.hpp>
#include <vector>
#include <iostream>
#include <string>
#include "const/rest_enums.hpp"
#include "server/endpoint.hpp"

class wServer
{
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<endpoint> endpoints_;  // Changed to vector for pattern matching

public:
    wServer();
    void add_endpoint(const endpoint& ep);
    void run(uint16_t port);

private:
    // Parse query string into map
    static std::unordered_map<std::string, std::string> parseQueryString(const std::string& query);

    // URL decode
    static std::string urlDecode(const std::string& str);
};
