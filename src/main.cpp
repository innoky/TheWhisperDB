#include <iostream>
#include <csignal>
#include <nlohmann/json.hpp>

#include "core/GraphDB.hpp"
#include "server/wserver.hpp"
#include "server/endpoint.hpp"
#include "server/UploadHandler.hpp"
#include "http/MultipartParser.hpp"
#include "http/Request.hpp"

using json = nlohmann::json;
using whisperdb::http::Request;
using whisperdb::http::Response;
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

    // ============================================
    // GET /api/nodes - List all nodes with optional filters, sorting, and pagination
    // Query params: subject, author, course, title, tag, sort, order, limit, offset
    // ============================================
    endpoint get_nodes(
        [](const Request& req) -> Response {
            json response;

            // Extract filters
            std::unordered_map<std::string, std::string> filters;
            for (const auto& [key, value] : req.query) {
                if (key == "subject" || key == "author" || key == "course" ||
                    key == "title" || key == "tag") {
                    filters[key] = value;
                }
            }

            // Extract sorting parameters
            std::string sortBy = req.hasQuery("sort") ? req.getQuery("sort") : "id";
            std::string order = req.hasQuery("order") ? req.getQuery("order") : "asc";

            // Extract pagination parameters
            int limit = -1;  // -1 means no limit
            int offset = 0;

            if (req.hasQuery("limit")) {
                try {
                    limit = std::stoi(req.getQuery("limit"));
                } catch (...) {
                    return Response::badRequest("Invalid limit parameter");
                }
            }

            if (req.hasQuery("offset")) {
                try {
                    offset = std::stoi(req.getQuery("offset"));
                } catch (...) {
                    return Response::badRequest("Invalid offset parameter");
                }
            }

            // Get nodes with filtering, sorting, and pagination
            json nodes;
            if (filters.empty()) {
                nodes = db->getAllNodes(sortBy, order, limit, offset);
            } else {
                nodes = db->findNodes(filters, sortBy, order, limit, offset);
            }

            response["status"] = "success";
            response["count"] = nodes.size();
            response["nodes"] = nodes;

            // Add metadata for pagination
            if (limit > 0) {
                response["limit"] = limit;
                response["offset"] = offset;
            }

            return Response::ok(response.dump());
        },
        HttpRequest::GET,
        "/api/nodes"
    );
    server->add_endpoint(get_nodes);

    // ============================================
    // GET /api/nodes/count - Count nodes with optional filters
    // Query params: subject, author, course, title, tag (same as /api/nodes)
    // ============================================
    endpoint count_nodes(
        [](const Request& req) -> Response {
            // Extract filters (same logic as get_nodes)
            std::unordered_map<std::string, std::string> filters;
            for (const auto& [key, value] : req.query) {
                if (key == "subject" || key == "author" || key == "course" ||
                    key == "title" || key == "tag") {
                    filters[key] = value;
                }
            }

            int count = db->countNodes(filters);

            json response;
            response["status"] = "success";
            response["count"] = count;

            return Response::ok(response.dump());
        },
        HttpRequest::GET,
        "/api/nodes/count"
    );
    server->add_endpoint(count_nodes);

    // ============================================
    // GET /api/nodes/:id - Get single node by ID
    // ============================================
    endpoint get_node_by_id(
        [](const Request& req) -> Response {
            std::string id = req.getParam("id");

            if (!db->exists(id)) {
                return Response::notFound("Node not found: " + id);
            }

            try {
                Node node = db->find(id);
                json response;
                response["status"] = "success";
                response["node"] = node.to_json();
                response["files"] = db->getNodeFiles(id);

                return Response::ok(response.dump());
            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::GET,
        "/api/nodes/:id"
    );
    server->add_endpoint(get_node_by_id);

    // ============================================
    // POST /api/nodes - Create new node (with optional files)
    // Supports both JSON body and multipart/form-data
    // ============================================
    endpoint create_node(
        [&uploadHandler](const Request& req) -> Response {
            try {
                json metadata;
                std::vector<std::pair<std::string, std::vector<uint8_t>>> files;

                if (req.parts.empty()) {
                    return Response::badRequest("No data received");
                }

                // Check if it's multipart or JSON
                bool foundMetadata = false;
                for (const auto& part : req.parts) {
                    if (part.name == "metadata" || part.name == "body") {
                        std::string jsonStr = part.dataAsString();
                        size_t jsonStart = jsonStr.find('{');
                        if (jsonStart != std::string::npos) {
                            jsonStr = jsonStr.substr(jsonStart);
                            metadata = json::parse(jsonStr);
                            foundMetadata = true;
                        }
                    }
                    if (part.isFile()) {
                        files.emplace_back(part.filename, part.data);
                    }
                }

                if (!foundMetadata) {
                    // Try parsing first part as JSON
                    std::string jsonStr = req.parts[0].dataAsString();
                    size_t jsonStart = jsonStr.find('{');
                    if (jsonStart != std::string::npos) {
                        jsonStr = jsonStr.substr(jsonStart);
                        metadata = json::parse(jsonStr);
                        foundMetadata = true;
                    }
                }

                if (!foundMetadata) {
                    return Response::badRequest("No metadata found in request");
                }

                std::string result = uploadHandler.handleUpload(files, metadata);
                return Response::created(result);

            } catch (const json::parse_error& e) {
                return Response::badRequest(std::string("Invalid JSON: ") + e.what());
            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::POST,
        "/api/nodes"
    );
    server->add_endpoint(create_node);

    // ============================================
    // PUT /api/nodes/:id - Update existing node
    // Body: JSON with fields to update
    // ============================================
    endpoint update_node(
        [](const Request& req) -> Response {
            std::string id = req.getParam("id");

            if (!db->exists(id)) {
                return Response::notFound("Node not found: " + id);
            }

            try {
                json updates;

                if (req.parts.empty()) {
                    return Response::badRequest("No data received");
                }

                // Parse JSON body
                std::string jsonStr = req.parts[0].dataAsString();
                size_t jsonStart = jsonStr.find('{');
                if (jsonStart != std::string::npos) {
                    jsonStr = jsonStr.substr(jsonStart);
                    updates = json::parse(jsonStr);
                } else {
                    return Response::badRequest("Invalid JSON body");
                }

                // Prevent changing ID
                updates.erase("id");

                if (db->updateNode(id, updates)) {
                    Node node = db->find(id);
                    json response;
                    response["status"] = "success";
                    response["message"] = "Node updated";
                    response["node"] = node.to_json();

                    return Response::ok(response.dump());
                } else {
                    return Response::error("Failed to update node");
                }

            } catch (const json::parse_error& e) {
                return Response::badRequest(std::string("Invalid JSON: ") + e.what());
            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::PUT,
        "/api/nodes/:id"
    );
    server->add_endpoint(update_node);

    // ============================================
    // DELETE /api/nodes/:id - Delete node and its files
    // ============================================
    endpoint delete_node(
        [](const Request& req) -> Response {
            std::string id = req.getParam("id");

            if (!db->exists(id)) {
                return Response::notFound("Node not found: " + id);
            }

            if (db->deleteNode(id)) {
                json response;
                response["status"] = "success";
                response["message"] = "Node deleted";
                response["deletedId"] = id;

                return Response::ok(response.dump());
            } else {
                return Response::error("Failed to delete node");
            }
        },
        HttpRequest::DELETE,
        "/api/nodes/:id"
    );
    server->add_endpoint(delete_node);

    // ============================================
    // GET /api/nodes/:id/files - Get files for a node
    // ============================================
    endpoint get_node_files(
        [](const Request& req) -> Response {
            std::string id = req.getParam("id");

            if (!db->exists(id)) {
                return Response::notFound("Node not found: " + id);
            }

            json response;
            response["status"] = "success";
            response["nodeId"] = id;
            response["files"] = db->getNodeFiles(id);

            return Response::ok(response.dump());
        },
        HttpRequest::GET,
        "/api/nodes/:id/files"
    );
    server->add_endpoint(get_node_files);

    // ============================================
    // POST /api/nodes/:id/files - Add file to existing node
    // ============================================
    endpoint add_file_to_node(
        [](const Request& req) -> Response {
            std::string id = req.getParam("id");

            if (!db->exists(id)) {
                return Response::notFound("Node not found: " + id);
            }

            try {
                std::vector<std::string> addedFiles;

                for (const auto& part : req.parts) {
                    if (part.isFile()) {
                        std::string path = db->addFileToNode(id, part.filename, part.data);
                        addedFiles.push_back(path);
                    }
                }

                if (addedFiles.empty()) {
                    return Response::badRequest("No files provided");
                }

                json response;
                response["status"] = "success";
                response["nodeId"] = id;
                response["addedFiles"] = addedFiles;

                return Response::created(response.dump());

            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::POST,
        "/api/nodes/:id/files"
    );
    server->add_endpoint(add_file_to_node);

    // ============================================
    // Legacy endpoint: POST /api/upload
    // (kept for backward compatibility)
    // ============================================
    endpoint upload_ep(
        [&uploadHandler](const Request& req) -> Response {
            if (req.parts.empty()) {
                return Response::badRequest("No data received");
            }

            try {
                json metadata;
                bool foundMetadata = false;

                for (const auto& part : req.parts) {
                    if (part.name == "metadata" || (!part.isFile() && !foundMetadata)) {
                        std::string jsonStr = part.dataAsString();
                        size_t jsonStart = jsonStr.find('{');
                        if (jsonStart != std::string::npos) {
                            jsonStr = jsonStr.substr(jsonStart);
                            metadata = json::parse(jsonStr);
                            foundMetadata = true;
                            if (part.name == "metadata") break;
                        }
                    }
                }

                if (!foundMetadata) {
                    return Response::badRequest("No metadata found in request");
                }

                std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
                for (const auto& part : req.parts) {
                    if (part.isFile()) {
                        files.emplace_back(part.filename, part.data);
                    }
                }

                std::string result = uploadHandler.handleUpload(files, metadata);
                return Response::created(result);

            } catch (const json::parse_error& e) {
                return Response::badRequest(std::string("Invalid JSON: ") + e.what());
            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::POST,
        "/api/upload"
    );
    server->add_endpoint(upload_ep);

    // ============================================
    // GET /health - Health check endpoint
    // ============================================
    endpoint health_check(
        [](const Request&) -> Response {
            json response;
            response["status"] = "ok";
            response["service"] = "TheWhisperDB";
            response["nodes_count"] = db->getSize();

            return Response::ok(response.dump());
        },
        HttpRequest::GET,
        "/health"
    );
    server->add_endpoint(health_check);

    // ============================================
    // POST /test - Test endpoint
    // ============================================
    endpoint test_ep(
        [](const Request& req) -> Response {
            std::string response = "Test endpoint. Got " + std::to_string(req.parts.size()) + " parts.\n";
            for (size_t i = 0; i < req.parts.size(); ++i) {
                response += "Part " + std::to_string(i) + ": ";
                response += "name=\"" + req.parts[i].name + "\"";
                if (!req.parts[i].filename.empty()) {
                    response += ", filename=\"" + req.parts[i].filename + "\"";
                }
                response += ", size=" + std::to_string(req.parts[i].data.size()) + " bytes\n";
            }

            if (!req.query.empty()) {
                response += "Query params:\n";
                for (const auto& [key, value] : req.query) {
                    response += "  " + key + "=" + value + "\n";
                }
            }

            return {200, "text/plain", response};
        },
        HttpRequest::POST,
        "/test"
    );
    server->add_endpoint(test_ep);

    std::cout << "TheWhisperDB REST API" << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  GET    /api/nodes          - List all nodes (supports: ?sort=<field>&order=<asc|desc>&limit=<n>&offset=<n>)" << std::endl;
    std::cout << "  GET    /api/nodes/count    - Count nodes (supports filters)" << std::endl;
    std::cout << "  GET    /api/nodes/:id      - Get node by ID" << std::endl;
    std::cout << "  POST   /api/nodes          - Create new node" << std::endl;
    std::cout << "  PUT    /api/nodes/:id      - Update node" << std::endl;
    std::cout << "  DELETE /api/nodes/:id      - Delete node" << std::endl;
    std::cout << "  GET    /api/nodes/:id/files - Get node files" << std::endl;
    std::cout << "  POST   /api/nodes/:id/files - Add file to node" << std::endl;
    std::cout << "  POST   /api/upload          - Legacy upload" << std::endl;
    std::cout << "  GET    /health              - Health check" << std::endl;
    std::cout << std::endl;
    std::cout << "Supported sort fields: id, title, author, subject, course, date" << std::endl;
    std::cout << "Supported filters: subject, author, course, title, tag" << std::endl;
    std::cout << std::endl;

    server->run(8080);
}
