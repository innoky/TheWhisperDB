#include "server/wserver.hpp"
#include "server/endpoint.hpp"
#include "http/MultipartParser.hpp"
#include "http/Request.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>

using whisperdb::http::MultipartParser;
using whisperdb::http::MultipartPart;
using whisperdb::http::Request;
using whisperdb::http::Response;

wServer::wServer(): acceptor_(io_context_) {}

using boost::asio::ip::tcp;

void wServer::add_endpoint(const endpoint& ep)
{
    endpoints_.push_back(ep);
}

std::string wServer::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            if (iss >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::unordered_map<std::string, std::string> wServer::parseQueryString(const std::string& query) {
    std::unordered_map<std::string, std::string> result;

    if (query.empty()) return result;

    std::string::size_type start = 0;
    while (start < query.size()) {
        auto end = query.find('&', start);
        if (end == std::string::npos) end = query.size();

        auto eq = query.find('=', start);
        if (eq != std::string::npos && eq < end) {
            std::string key = urlDecode(query.substr(start, eq - start));
            std::string value = urlDecode(query.substr(eq + 1, end - eq - 1));
            result[key] = value;
        }

        start = end + 1;
    }

    return result;
}

void wServer::run(uint16_t port)
{
    boost::asio::ip::tcp::endpoint ep(tcp::v4(), port);
    acceptor_ = boost::asio::ip::tcp::acceptor(io_context_, ep);
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
            case 201: return "Created";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 411: return "Length Required";
            case 413: return "Payload Too Large";
            case 415: return "Unsupported Media Type";
            case 500: return "Internal Server Error";
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

        std::string method, fullPath, version;
        request_stream >> method >> fullPath >> version;

        std::string dummy;
        std::getline(request_stream, dummy);

        // Parse path and query string
        std::string cleanPath = fullPath;
        std::string queryString;
        auto qm = fullPath.find('?');
        if (qm != std::string::npos) {
            cleanPath = fullPath.substr(0, qm);
            queryString = fullPath.substr(qm + 1);
        }

        // Parse HTTP method
        bool method_ok = true;
        HttpRequest req_type = HttpRequest::GET;
        try {
            req_type = from_string(method);
        } catch (...) {
            method_ok = false;
        }

        // Parse headers
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

        // Read body
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

        // Extract media type and boundary
        std::string media_type;
        std::string boundary;
        {
            auto sc = content_type_header.find(';');
            media_type = (sc != std::string::npos) ? content_type_header.substr(0, sc) : content_type_header;
            trim(media_type);
            boundary = MultipartParser::extractBoundary(content_type_header);
        }

        Response response;

        if (!method_ok) {
            response = Response::methodNotAllowed();
        }
        else if (has_content_length && content_length > MAX_BODY_SIZE) {
            response = {413, "application/json", R"({"status":"error","message":"Payload too large"})"};
        }
        else {
            // Find matching endpoint
            endpoint* matched_endpoint = nullptr;
            std::unordered_map<std::string, std::string> path_params;

            for (auto& ep : endpoints_) {
                if (ep.get_rest_type() == req_type && ep.matches(cleanPath, path_params)) {
                    matched_endpoint = &ep;
                    break;
                }
            }

            if (!matched_endpoint) {
                // Check if path exists with different method
                bool path_exists = false;
                for (auto& ep : endpoints_) {
                    std::unordered_map<std::string, std::string> dummy_params;
                    if (ep.matches(cleanPath, dummy_params)) {
                        path_exists = true;
                        break;
                    }
                }

                if (path_exists) {
                    response = Response::methodNotAllowed();
                } else {
                    response = Response::notFound("Endpoint not found");
                }
            }
            else {
                try {
                    // Build Request object
                    Request req;
                    req.method = req_type;
                    req.path = cleanPath;
                    req.params = path_params;
                    req.query = parseQueryString(queryString);
                    req.rawBody = body;

                    // Parse multipart if applicable
                    if (starts_with_icase(media_type, "multipart/form-data")) {
                        if (boundary.empty()) {
                            response = Response::badRequest("Missing multipart boundary");
                        } else {
                            req.parts = MultipartParser::parse(body, boundary);
                            response = matched_endpoint->handle(req);
                        }
                    } else {
                        // For non-multipart, create single part with body
                        if (!body.empty()) {
                            MultipartPart single_part;
                            single_part.name = "body";
                            single_part.content_type = media_type;
                            single_part.data = std::vector<uint8_t>(body.begin(), body.end());
                            req.parts.push_back(std::move(single_part));
                        }
                        response = matched_endpoint->handle(req);
                    }
                }
                catch (const std::exception& e) {
                    response = Response::error(e.what());
                }
                catch (...) {
                    response = Response::error("Unknown error");
                }
            }
        }

        // Send response
        std::ostringstream http_response;
        http_response << "HTTP/1.1 " << response.status << " " << status_text(response.status) << "\r\n";
        http_response << "Content-Length: " << response.body.size() << "\r\n";
        http_response << "Content-Type: " << response.contentType << "\r\n";
        http_response << "Connection: close\r\n\r\n";
        http_response << response.body;

        boost::asio::write(socket, boost::asio::buffer(http_response.str()));
        socket.close();
    }
}
