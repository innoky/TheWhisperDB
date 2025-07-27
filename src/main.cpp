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

    app.port(8080).run();
}