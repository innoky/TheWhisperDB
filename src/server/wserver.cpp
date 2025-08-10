#include "server/wserver.hpp"
#include "server/endpoint.hpp"
#include <unordered_map>

wServer::wServer(): acceptor_(io_context_){}



void wServer::add_endpoint(const endpoint& ep)
{
    handlers_.insert_or_assign(ep.get_path(), ep);
}

#include <string>
#include <vector>

static std::vector<std::string>
extract_multipart_parts(const std::string& body, const std::string& boundary_param)
{
    std::vector<std::string> parts;
    if (boundary_param.empty()) return parts;

    const std::string dash = "--" + boundary_param;

    size_t bline; 
    if (body.rfind(dash, 0) == 0) {
        bline = 0;
    } else {
        size_t m = body.find("\r\n" + dash, 0);
        if (m == std::string::npos) return parts;
        bline = m + 2; 
    }

    for (;;) {
       
        size_t line_end = body.find("\r\n", bline);
        if (line_end == std::string::npos) break;

        const size_t after = bline + dash.size();
        const bool is_final = (after + 2 <= body.size() && body.compare(after, 2, "--") == 0);
        if (is_final) break; 
        size_t headers_start = line_end + 2;
        size_t headers_end = body.find("\r\n\r\n", headers_start);
        if (headers_end == std::string::npos) break;

        size_t content_start = headers_end + 4;

        size_t next_marker = body.find("\r\n" + dash, content_start);
        if (next_marker == std::string::npos) {
           
            parts.emplace_back(body.substr(content_start));
            break;
        }

        size_t content_end = next_marker; 
        parts.emplace_back(body.substr(content_start, content_end - content_start));

        bline = next_marker + 2; 
    }

    return parts;
}


void wServer::run(uint16_t port)
{
    tcp::endpoint endpoint(tcp::v4(), port);
    acceptor_ = tcp::acceptor(io_context_, endpoint);
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
    auto starts_with_icase = [&](const std::string& s, const std::string& p) {
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

    auto count_multipart_parts = [](const std::string& body, const std::string& boundary) -> size_t {
        if (boundary.empty()) return 0;
        const std::string dash = "--" + boundary;
        size_t pos = 0;
        size_t first = body.find(dash, pos);
        if (first == std::string::npos) return 0;
        pos = first + dash.size();

        if (pos + 2 <= body.size() && body.compare(pos, 2, "--") == 0) return 0;
   
        if (pos + 2 <= body.size() && body.compare(pos, 2, "\r\n") == 0) pos += 2;

        size_t count = 0;
        while (true) {
         
            const std::string marker = "\r\n" + dash;
            size_t next = body.find(marker, pos);
            if (next == std::string::npos) break; 
            ++count;
            pos = next + marker.size();
           
            if (pos + 2 <= body.size() && body.compare(pos, 2, "--") == 0) break;
          
            if (pos + 2 <= body.size() && body.compare(pos, 2, "\r\n") == 0) pos += 2;
        }
        return count;
    };

    while (true)
    {
        tcp::socket socket(io_context_);
        acceptor_.accept(socket);

        asio::streambuf buf;
       
        asio::read_until(socket, buf, "\r\n\r\n");
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

        HttpRequest req_type = from_string(method);

        std::string header_line;
        size_t content_length = 0;
        bool has_content_length = false;
        std::string media_type;     
        std::string boundary_param; 

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
              
                std::string v = value;
                trim(v);
                // отделяем до ';'
                std::string mt = v;
                auto sc = v.find(';');
                if (sc != std::string::npos) mt = v.substr(0, sc);
                trim(mt);
                media_type = mt;

               
                if (sc != std::string::npos) {
                    std::string params = v.substr(sc + 1);
                    
                    while (!params.empty()) {
                        auto sc2 = params.find(';');
                        std::string token = (sc2 == std::string::npos) ? params : params.substr(0, sc2);
                        params = (sc2 == std::string::npos) ? "" : params.substr(sc2 + 1);
                        trim(token);
                        if (token.empty()) continue;
                        auto eq = token.find('=');
                        if (eq == std::string::npos) continue;
                        std::string k = token.substr(0, eq);
                        std::string v2 = token.substr(eq + 1);
                        trim(k); trim(v2);
                        tolower_inplace(k);
                        if (!v2.empty() && v2.front() == '"' && v2.back() == '"') {
                            v2 = v2.substr(1, v2.size() - 2);
                        }
                        if (k == "boundary") boundary_param = v2;
                    }
                }
            }
        }

        std::string body;
        {
            
            std::string already(std::istreambuf_iterator<char>(request_stream), {});
            body.swap(already);
        }

        if (has_content_length) {
            if (body.size() < content_length) {
                std::string rest;
                rest.resize(content_length - body.size());
                asio::read(socket, asio::buffer(&rest[0], rest.size()));
                body += rest;
            } else if (body.size() > content_length) {
              
                body.resize(content_length);
            }
        }

    
        if (starts_with_icase(media_type, "multipart/form-data")) {
            if (!boundary_param.empty()) {
                size_t parts_count = count_multipart_parts(body, boundary_param);
                std::cout << "Multipart parts count: " << parts_count << std::endl;
            } else {
                std::cout << "Multipart without boundary: bad request" << std::endl;
            }
        }

        int status = 200;
        std::string response_body;

        auto it = handlers_.find(clean_path);
        if (it == handlers_.end()) {
            status = 404;
            response_body = "Not Found";
        } 
        else if (it->second.get_rest_type() != req_type) {
            status = 405;
            response_body = "Method Not Allowed";
        } 
        else {
            try{
                if (media_type == "multipart/form-data")
                {
                std::vector<std::string> parts = extract_multipart_parts(body, boundary_param);
             
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
        

        std::ostringstream response;
        response << "HTTP/1.1 " << status << " " << status_text(status) << "\r\n";
        response << "Content-Length: " << response_body.size() << "\r\n";
        response << "Content-Type: text/plain\r\n";
        response << "Connection: close\r\n\r\n";
        response << response_body;

        asio::write(socket, asio::buffer(response.str()));
        socket.close();
        }
    }
}