#include <crow.h>
#include "core/GraphDB.hpp"
#include <iostream>
#include <string>   
#include <coroutine> 
#include <asio.hpp>

int main()
{
    crow::SimpleApp app; 
    auto db = std::make_shared<GraphDB>(); 

    CROW_ROUTE(app, "/dump")
    ([db]() -> crow::response {
        auto json_str = db->serialize();
        return crow::response(json_str);
    });

    CROW_ROUTE(app, "/get/<int>")
    ([db](int id) -> crow::response {
        try {
            Node node = db->find(id);
            return crow::response(node.to_json().dump());
        } catch (const std::runtime_error& e) {
            return crow::response(404, e.what());
        }
    });
    app.port(8080).run();
}