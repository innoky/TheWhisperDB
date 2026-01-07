#include <iostream>
#include <csignal>
#include <cstdlib>
#include <nlohmann/json.hpp>

#include "core/GraphDB.hpp"
#include "server/wserver.hpp"
#include "server/endpoint.hpp"
#include "server/UploadHandler.hpp"
#include "http/MultipartParser.hpp"
#include "http/Request.hpp"
#include "embedding/EmbeddingService.hpp"
#include "embedding/Clustering.hpp"
#include "tagging/TagService.hpp"
#include <algorithm>

using json = nlohmann::json;
using whisperdb::http::Request;
using whisperdb::http::Response;
using whisperdb::http::MultipartPart;

std::shared_ptr<GraphDB> db;
std::unique_ptr<EmbeddingService> embeddingService;
std::unique_ptr<TagService> tagService;
const std::string STORAGE_PATH = "./storage";

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

    // Initialize embedding service if API key is set
    const char* openaiKey = std::getenv("OPENAI_API_KEY");
    if (openaiKey) {
        embeddingService = std::make_unique<EmbeddingService>(*db, openaiKey);
        std::cout << "Embedding service initialized (OpenAI)" << std::endl;
    } else {
        std::cout << "Warning: OPENAI_API_KEY not set, embedding features disabled" << std::endl;
    }

    // Initialize tag service if DeepSeek API key is set
    const char* deepseekKey = std::getenv("DEEPSEEK_API_KEY");
    if (deepseekKey) {
        tagService = std::make_unique<TagService>(*db, deepseekKey);
        std::cout << "Tag service initialized (DeepSeek)" << std::endl;
    } else {
        std::cout << "Warning: DEEPSEEK_API_KEY not set, tag generation disabled" << std::endl;
    }

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

    // ============================================
    // POST /api/cluster - Run clustering batch job
    // Query params: threshold (default: 0.75)
    // ============================================
    endpoint run_clustering(
        [](const Request& req) -> Response {
            if (!embeddingService) {
                return Response::error("Embedding service not initialized. Set OPENAI_API_KEY environment variable.");
            }

            float threshold = 0.75f;
            if (req.hasQuery("threshold")) {
                try {
                    threshold = std::stof(req.getQuery("threshold"));
                } catch (...) {
                    return Response::badRequest("Invalid threshold parameter");
                }
            }

            try {
                std::cout << "Starting clustering with threshold " << threshold << std::endl;
                ClusteringResult result = embeddingService->runClustering(STORAGE_PATH, threshold);

                json response;
                response["status"] = "success";
                response["nodesProcessed"] = result.nodesProcessed;
                response["embeddingsGenerated"] = result.embeddingsGenerated;
                response["linksCreated"] = result.linksCreated;
                response["clustersFound"] = result.clustersFound;
                response["clusters"] = result.clusters;

                return Response::ok(response.dump());
            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::POST,
        "/api/cluster"
    );
    server->add_endpoint(run_clustering);

    // ============================================
    // POST /api/nodes/:id/embedding - Generate embedding for single node
    // ============================================
    endpoint generate_embedding(
        [](const Request& req) -> Response {
            if (!embeddingService) {
                return Response::error("Embedding service not initialized. Set OPENAI_API_KEY environment variable.");
            }

            std::string idStr = req.getParam("id");
            if (!db->exists(idStr)) {
                return Response::notFound("Node not found: " + idStr);
            }

            try {
                int id = std::stoi(idStr);
                bool success = embeddingService->generateEmbedding(id, STORAGE_PATH);

                if (success) {
                    json response;
                    response["status"] = "success";
                    response["message"] = "Embedding generated";
                    response["nodeId"] = id;
                    return Response::ok(response.dump());
                } else {
                    return Response::error("Failed to generate embedding");
                }
            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::POST,
        "/api/nodes/:id/embedding"
    );
    server->add_endpoint(generate_embedding);

    // ============================================
    // GET /api/nodes/:id/similar - Get similar nodes
    // Query params: limit (default: 10)
    // ============================================
    endpoint get_similar_nodes(
        [](const Request& req) -> Response {
            std::string idStr = req.getParam("id");
            if (!db->exists(idStr)) {
                return Response::notFound("Node not found: " + idStr);
            }

            try {
                Node node = db->find(idStr);

                if (!node.hasEmbedding()) {
                    return Response::badRequest("Node has no embedding. Generate embedding first.");
                }

                int limit = 10;
                if (req.hasQuery("limit")) {
                    try {
                        limit = std::stoi(req.getQuery("limit"));
                    } catch (...) {}
                }

                // Get all nodes with embeddings and calculate similarity
                auto allNodes = db->getAllNodes();
                std::vector<std::pair<std::string, float>> similarities;

                for (const auto& otherJson : allNodes) {
                    Node other(otherJson);
                    if (other.getId() != node.getId() && other.hasEmbedding()) {
                        float sim = Clustering::cosineSimilarity(node.getEmbedding(), other.getEmbedding());
                        similarities.emplace_back(std::to_string(other.getId()), sim);
                    }
                }

                // Sort by similarity descending
                std::sort(similarities.begin(), similarities.end(),
                    [](const auto& a, const auto& b) { return a.second > b.second; });

                // Take top N
                json similarNodes = json::array();
                for (int i = 0; i < std::min(limit, (int)similarities.size()); ++i) {
                    Node similarNode = db->find(similarities[i].first);
                    json nodeJson = similarNode.to_json();
                    nodeJson["similarity"] = similarities[i].second;
                    similarNodes.push_back(nodeJson);
                }

                json response;
                response["status"] = "success";
                response["nodeId"] = idStr;
                response["similarNodes"] = similarNodes;

                return Response::ok(response.dump());
            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::GET,
        "/api/nodes/:id/similar"
    );
    server->add_endpoint(get_similar_nodes);

    // ============================================
    // POST /api/nodes/:id/tags - Generate tags for a node using DeepSeek
    // ============================================
    endpoint generate_tags(
        [](const Request& req) -> Response {
            if (!tagService) {
                return Response::error("Tag service not initialized. Set DEEPSEEK_API_KEY environment variable.");
            }

            std::string idStr = req.getParam("id");
            if (!db->exists(idStr)) {
                return Response::notFound("Node not found: " + idStr);
            }

            try {
                int id = std::stoi(idStr);
                TagGenerationResult result = tagService->generateTagsForNode(id, STORAGE_PATH);

                if (result.success) {
                    json response;
                    response["status"] = "success";
                    response["nodeId"] = id;
                    response["tags"] = result.generatedTags;
                    response["newTagsAdded"] = result.newTagsAdded;
                    response["linkedNodes"] = result.linkedNodeIds;
                    return Response::ok(response.dump());
                } else {
                    return Response::error(result.error);
                }
            } catch (const std::exception& e) {
                return Response::error(e.what());
            }
        },
        HttpRequest::POST,
        "/api/nodes/:id/tags"
    );
    server->add_endpoint(generate_tags);

    // ============================================
    // GET /api/tags - Get the tag bank
    // ============================================
    endpoint get_tag_bank(
        [](const Request&) -> Response {
            json response;
            response["status"] = "success";
            response["tagBank"] = db->getTagBank();
            response["count"] = db->getTagBank().size();
            return Response::ok(response.dump());
        },
        HttpRequest::GET,
        "/api/tags"
    );
    server->add_endpoint(get_tag_bank);

    // ============================================
    // GET /api/tags/:tag/nodes - Get nodes with a specific tag
    // ============================================
    endpoint get_nodes_by_tag(
        [](const Request& req) -> Response {
            std::string tag = req.getParam("tag");
            auto nodeIds = db->findNodesByTag(tag);

            json nodes = json::array();
            for (int id : nodeIds) {
                std::string idStr = std::to_string(id);
                if (db->exists(idStr)) {
                    nodes.push_back(db->find(idStr).to_json());
                }
            }

            json response;
            response["status"] = "success";
            response["tag"] = tag;
            response["nodes"] = nodes;
            response["count"] = nodes.size();
            return Response::ok(response.dump());
        },
        HttpRequest::GET,
        "/api/tags/:tag/nodes"
    );
    server->add_endpoint(get_nodes_by_tag);

    // ============================================
    // POST /api/tags/link-all - Batch update all tag-based links
    // ============================================
    endpoint update_tag_links(
        [](const Request&) -> Response {
            if (!tagService) {
                return Response::error("Tag service not initialized. Set DEEPSEEK_API_KEY environment variable.");
            }

            int linksCreated = tagService->updateAllTagBasedLinks();

            json response;
            response["status"] = "success";
            response["linksCreated"] = linksCreated;
            return Response::ok(response.dump());
        },
        HttpRequest::POST,
        "/api/tags/link-all"
    );
    server->add_endpoint(update_tag_links);

    std::cout << "TheWhisperDB REST API" << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  GET    /api/nodes              - List all nodes (supports: ?sort=<field>&order=<asc|desc>&limit=<n>&offset=<n>)" << std::endl;
    std::cout << "  GET    /api/nodes/count        - Count nodes (supports filters)" << std::endl;
    std::cout << "  GET    /api/nodes/:id          - Get node by ID" << std::endl;
    std::cout << "  POST   /api/nodes              - Create new node" << std::endl;
    std::cout << "  PUT    /api/nodes/:id          - Update node" << std::endl;
    std::cout << "  DELETE /api/nodes/:id          - Delete node" << std::endl;
    std::cout << "  GET    /api/nodes/:id/files    - Get node files" << std::endl;
    std::cout << "  POST   /api/nodes/:id/files    - Add file to node" << std::endl;
    std::cout << "  POST   /api/nodes/:id/embedding - Generate embedding for node" << std::endl;
    std::cout << "  GET    /api/nodes/:id/similar  - Get similar nodes" << std::endl;
    std::cout << "  POST   /api/nodes/:id/tags     - Generate tags for node (DeepSeek)" << std::endl;
    std::cout << "  POST   /api/cluster            - Run clustering batch job (?threshold=0.75)" << std::endl;
    std::cout << "  GET    /api/tags               - Get tag bank" << std::endl;
    std::cout << "  GET    /api/tags/:tag/nodes    - Get nodes by tag" << std::endl;
    std::cout << "  POST   /api/tags/link-all      - Update all tag-based links" << std::endl;
    std::cout << "  GET    /health                 - Health check" << std::endl;
    std::cout << std::endl;
    std::cout << "Supported sort fields: id, title, author, subject, course, date" << std::endl;
    std::cout << "Supported filters: subject, author, course, title, tag" << std::endl;
    std::cout << std::endl;
    std::cout << "Embedding: Set OPENAI_API_KEY environment variable to enable" << std::endl;
    std::cout << "Tagging:   Set DEEPSEEK_API_KEY environment variable to enable" << std::endl;
    std::cout << std::endl;

    server->run(8080);
}
