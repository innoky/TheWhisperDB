#pragma once
#include <boost/asio.hpp>
#include <unordered_map>
#include <iostream>
#include <string>
#include "const/rest_enums.hpp"
#include "server/endpoint.hpp"

class wServer
{
  boost::asio::io_context io_context_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::unordered_map<std::string, endpoint> handlers_;
public:
    wServer();
    void add_endpoint(const endpoint& ep);

    void run(uint16_t port);

};
