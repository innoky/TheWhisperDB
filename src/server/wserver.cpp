#include "server/wserver.hpp"
#include "server/endpoint.hpp"

wServer::wServer(): acceptor_(io_context_){}

void wServer::add_endpoint(const endpoint& ep)
{
    handlers_.insert_or_assign(ep.get_path(), ep);
}

std::string extract_body(const std::string& raw)
{
    auto pos = raw.find('{'); 
    if (pos == std::string::npos) return ""; 
    return raw.substr(pos);
}

void wServer::run(uint16_t port)
{
    tcp::endpoint endpoint(tcp::v4(), port);
    acceptor_ = tcp::acceptor(io_context_, endpoint);
    std::cout << "Server listening on port " << port << std::endl;

    while (true)
    {
        tcp::socket socket(io_context_);
        acceptor_.accept(socket);

        asio::streambuf buf;

        asio::read_until(socket, buf, "\r\n\r\n");

        std::istream request_stream(&buf);

        std::string method, path, version;
        request_stream >> method >> path >> version;
        
        HttpRequest req_type = from_string(method);

        std::string header_line;
        size_t content_length = 0;
        while (std::getline(request_stream, header_line) && header_line != "\r")
        {
            if (header_line.find("Content-Length:") != std::string::npos)
            {
                content_length = std::stoul(header_line.substr(15));
            }
        }
        std::string body;

        if (buf.size() > 0)
        {
            std::ostringstream ss;
            ss << &buf;
            body = ss.str();
        }

        if (content_length > body.size())
        {
            std::vector<char> remaining(content_length - body.size());
            asio::read(socket, asio::buffer(remaining.data(), remaining.size()));
            body.append(remaining.data(), remaining.size());
        }

        std::string response_body;
        body = extract_body(body);
        auto it = handlers_.find(path);
        if (it != handlers_.end())
        {
            if (it->second.get_rest_type() == req_type)
            {
                response_body = it->second.get_handler()(body);
            }
            else
            {
                response_body = "405 Method Not Allowed";
            }
        }
        else
        {
            response_body = "404 Not Found";
        }

        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Length: " << response_body.size() << "\r\n";
        response << "Content-Type: text/plain\r\n";
        response << "Connection: close\r\n\r\n";
        response << response_body;

        asio::write(socket, asio::buffer(response.str()));
        socket.close();
    }
}
