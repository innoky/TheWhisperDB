#include <iostream>
#include <string>   
#include <coroutine> 
#include <asio.hpp>
#include <csignal>
#include <atomic>

#include "core/GraphDB.hpp"
#include "server/wserver.hpp"
#include "server/endpoint.hpp"
#include "const/rest_enums.hpp"


std::shared_ptr<GraphDB> db;


void signal_handler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\nSIGINT received, saving database..." << std::endl;
        if (db) {
            db->saveToJson();
        }
        std::exit(0);  
    }
}

int main()
{
    db = std::make_shared<GraphDB>();
    std::signal(SIGINT, signal_handler);

    auto server = std::make_shared<wServer>();
    
    endpoint dump_ep(
        [](const std::string& body) 
        {
        auto json_str = db->serialize();
        return json_str;
        }, 
        HttpRequest::GET, 
        "/dump");
    server->add_endpoint(dump_ep);

    endpoint add_node(
        [](const std::string& body) -> std::string
        {
            std::cout << "Received body: " << body << std::endl;
            try{

                auto json = nlohmann::json::parse(body);
                db->addNode(json);
                return "Node added successfully";
            }
            catch(const std::exception& e){
                std::cerr << e.what() << '\n';
                return "Error adding node";
            }
        },
        HttpRequest::POST,
        "/add_node");
    server->add_endpoint(add_node);

    server->run(8080);
}