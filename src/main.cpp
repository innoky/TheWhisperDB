#include <iostream>
#include <csignal>
#include <nlohmann/json.hpp>

#include "core/GraphDB.hpp"
#include "server/wserver.hpp"
#include "server/endpoint.hpp"
#include "server/UploadHandler.hpp"
#include "http/MultipartParser.hpp"

using json = nlohmann::json;
using whisperdb::http::MultipartPart;

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
    UploadHandler uploadHandler(*db);

    // Test endpoint
    endpoint test_ep(
        [](const std::vector<MultipartPart>& parts) {
            std::string response = "Test endpoint. Got " + std::to_string(parts.size()) + " parts.\n";
            for (size_t i = 0; i < parts.size(); ++i) {
                response += "Part " + std::to_string(i) + ": ";
                response += "name=\"" + parts[i].name + "\"";
                if (!parts[i].filename.empty()) {
                    response += ", filename=\"" + parts[i].filename + "\"";
                }
                response += ", size=" + std::to_string(parts[i].data.size()) + " bytes\n";
            }
            return response;
        },
        HttpRequest::POST,
        "/test"
    );
    server->add_endpoint(test_ep);

    // File upload endpoint
    endpoint upload_ep(
        [&uploadHandler](const std::vector<MultipartPart>& parts) -> std::string {
            if (parts.empty()) {
                return R"({"status":"error","message":"No data received"})";
            }

            try {
                // Find metadata part (first part named "metadata" or first non-file part)
                json metadata;
                bool found_metadata = false;

                for (const auto& part : parts) {
                    if (part.name == "metadata" || (!part.isFile() && !found_metadata)) {
                        std::string json_str = part.dataAsString();

                        // Find JSON start (skip any preamble)
                        size_t json_start = json_str.find('{');
                        if (json_start != std::string::npos) {
                            json_str = json_str.substr(json_start);
                            metadata = json::parse(json_str);
                            found_metadata = true;
                            if (part.name == "metadata") break;
                        }
                    }
                }

                if (!found_metadata) {
                    return R"({"status":"error","message":"No metadata found in request"})";
                }

                // Collect file parts
                std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
                for (const auto& part : parts) {
                    if (part.isFile()) {
                        files.emplace_back(part.filename, part.data);
                    }
                }

                return uploadHandler.handleUpload(files, metadata);

            } catch (const json::parse_error& e) {
                json error;
                error["status"] = "error";
                error["message"] = std::string("Invalid JSON metadata: ") + e.what();
                return error.dump();
            } catch (const std::exception& e) {
                json error;
                error["status"] = "error";
                error["message"] = std::string("Error processing upload: ") + e.what();
                return error.dump();
            }
        },
        HttpRequest::POST,
        "/api/upload"
    );
    server->add_endpoint(upload_ep);

    server->run(8080);
}
