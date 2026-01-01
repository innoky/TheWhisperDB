#include "server/wserver.hpp"
#include "server/endpoint.hpp"
#include "http/MultipartParser.hpp"
#include <unordered_map>
#include <sstream>
#include <cctype>

using whisperdb::http::MultipartParser;
using whisperdb::http::MultipartPart;

wServer::wServer(): acceptor_(io_context_) {}

using boost::asio::ip::tcp;

void wServer::add_endpoint(const endpoint& ep)
{
    handlers_.insert_or_assign(ep.get_path(), ep);
}

void wServer::run(uint16_t port)
{
    boost::asio::ip::tcp::endpoint endpoint(tcp::v4(), port);
    acceptor_ = boost::asio::ip::tcp::acceptor(io_context_, endpoint);
    std::cout << "Server listening on port " << port << std::endl;

    auto trim = [](std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
        s = s.substr(a, b - a);
    };

    auto tolower_inplace = [](std::string& s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };

    auto starts_with_icase = [](const std::string& s, const std::string& p) {
        if (s.size() < p.size()) return false;
        for (size_t i = 0; i < p.size(); ++i) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(p[i])));
            if (a != b) return false;
        }
        return true;
    };

    auto status_text = [](int s) {
        switch (s) {
            case 200: return "OK";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 411: return "Length Required";
            case 413: return "Payload Too Large";
            case 415: return "Unsupported Media Type";
            default:  return "OK";
        }
    };

    while (true)
    {
        tcp::socket socket(io_context_);
        acceptor_.accept(socket);

        boost::asio::streambuf buf;
        boost::asio::read_until(socket, buf, "\r\n\r\n");
        std::istream request_stream(&buf);

        std::string method, path, version;
        request_stream >> method >> path >> version;

        std::string dummy;
        std::getline(request_stream, dummy);

        std::string clean_path = path;
        if (!path.empty()) {
            auto qm = path.find('?');
            if (qm != std::string::npos) clean_path = path.substr(0, qm);
        }

        bool method_ok = true;
        HttpRequest req_type = HttpRequest::GET;
        try {
            req_type = from_string(method);
        } catch (...) {
            method_ok = false;
        }

        std::string header_line;
        size_t content_length = 0;
        bool has_content_length = false;
        std::string content_type_header;

        while (std::getline(request_stream, header_line) && header_line != "\r")
        {
            if (!header_line.empty() && header_line.back() == '\r') header_line.pop_back();
            if (header_line.empty()) break;

            auto colon = header_line.find(':');
            if (colon == std::string::npos) continue;

            std::string name = header_line.substr(0, colon);
            std::string value = header_line.substr(colon + 1);
            trim(name); trim(value);
            std::string name_lc = name;
            tolower_inplace(name_lc);

            if (name_lc == "content-length") {
                try {
                    trim(value);
                    content_length = static_cast<size_t>(std::stoull(value));
                    has_content_length = true;
                } catch (...) {
                    has_content_length = false;
                }
            } else if (name_lc == "content-type") {
                content_type_header = value;
            }
        }

        std::string body;
        {
            std::string already(std::istreambuf_iterator<char>(request_stream), {});
            body.swap(already);
        }

        const size_t MAX_BODY_SIZE = 10 * 1024 * 1024; // 10 MB
        if (has_content_length) {
            if (body.size() < content_length) {
                std::string rest;
                rest.resize(content_length - body.size());
                boost::asio::read(socket, boost::asio::buffer(&rest[0], rest.size()));
                body += rest;
            } else if (body.size() > content_length) {
                body.resize(content_length);
            }
        }

        // Extract media type and boundary using MultipartParser
        std::string media_type;
        std::string boundary;
        {
            auto sc = content_type_header.find(';');
            media_type = (sc != std::string::npos) ? content_type_header.substr(0, sc) : content_type_header;
            trim(media_type);
            boundary = MultipartParser::extractBoundary(content_type_header);
        }

        int status = 200;
        std::string response_body;

        auto it = handlers_.find(clean_path);
        if (!method_ok) {
            status = 405;
            response_body = "Method Not Allowed";
        }
        else if (has_content_length && content_length > MAX_BODY_SIZE) {
            status = 413;
            response_body = "Payload Too Large";
        }
        else if (it == handlers_.end()) {
            status = 404;
            response_body = "Not Found";
        }
        else if (it->second.get_rest_type() != req_type) {
            status = 405;
            response_body = "Method Not Allowed";
        }
        else {
            try {
                std::vector<MultipartPart> parts;

                if (starts_with_icase(media_type, "multipart/form-data")) {
                    if (boundary.empty()) {
                        status = 400;
                        response_body = "Bad Request: missing multipart boundary";
                    } else {
                        parts = MultipartParser::parse(body, boundary);
                        response_body = it->second.get_handler()(parts);
                    }
                } else {
                    // Non-multipart: create single part with raw body
                    MultipartPart single_part;
                    single_part.name = "body";
                    single_part.content_type = media_type;
                    single_part.data = std::vector<uint8_t>(body.begin(), body.end());
                    parts.push_back(std::move(single_part));
                    response_body = it->second.get_handler()(parts);
                }
            }
            catch (const std::exception& e) {
                status = 400;
                response_body = std::string("Bad Request: ") + e.what();
            }
            catch (...) {
                status = 400;
                response_body = "Bad Request";
            }
        }

        std::ostringstream response;
        response << "HTTP/1.1 " << status << " " << status_text(status) << "\r\n";
        response << "Content-Length: " << response_body.size() << "\r\n";
        response << "Content-Type: text/plain\r\n";
        response << "Connection: close\r\n\r\n";
        response << response_body;

        boost::asio::write(socket, boost::asio::buffer(response.str()));
        socket.close();
    }
}
