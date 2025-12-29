#include <iostream>
#include <string>    
#include <boost/asio.hpp>
#include <csignal>
#include <atomic>
#include <fstream>
#include <cctype>


#include "core/GraphDB.hpp"
#include "server/wserver.hpp"
#include "server/endpoint.hpp"
#include "server/upload_handler.hpp"
#include "server/UploadHandler.hpp"
#include "const/rest_enums.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::shared_ptr<GraphDB> db;

void save_png_from_string(const std::string& data, const std::string& filename)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open file");
    file.write(data.data(), data.size());
}



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

    endpoint test_ep(
        [](const std::vector<std::string>& parts) 
        {
            std::string response = "Test endpoint. Got " + std::to_string(parts.size()) + " parts.\n";
            for (size_t i = 0; i < parts.size(); ++i) {
                response += "Part " + std::to_string(i) + " size: " + 
                           std::to_string(parts[i].size()) + " bytes\n";
            }
            return response;
        },
        HttpRequest::POST,
        "/test"
    );
    server->add_endpoint(test_ep);
    // File upload endpoint with metadata
    endpoint upload_ep(
        [&uploadHandler](const std::vector<std::string>& parts) -> std::string {
            std::cout << "=== Received upload request ===\n";
            std::cout << "Number of parts: " << parts.size() << "\n";
            
            if (parts.empty()) {
                std::cerr << "Error: No parts received in the request\n";
                return "{\"status\":\"error\",\"message\":\"No data received\"}";
            }

            for (size_t i = 0; i < parts.size(); ++i) {
                std::string preview = parts[i].substr(0, 200);
                std::cout << "Part " << i << " (first 200 chars): " << preview << "...\n";
            }

            try {
                // The first part should be the metadata
                json metadata;
                try {
                    std::cout << "\nAttempting to parse metadata...\n";
                    std::string metadata_str = parts[0];
                    
                    // Find the JSON part after the headers
                    size_t json_start = metadata_str.find("{");
                    if (json_start == std::string::npos) {
                        throw std::runtime_error("Could not find JSON data in the metadata part");
                    }
                    
                    // Extract just the JSON part
                    std::string json_str = metadata_str.substr(json_start);
                    
                    // Clean up any trailing whitespace or control characters
                    json_str.erase(std::remove_if(json_str.begin(), json_str.end(), 
                        [](char c) { 
                            return c == '\r' || c == '\n' || c == '\t'; 
                        }), json_str.end());
                    
                    std::cout << "Extracted JSON string: " << json_str << "\n";
                    
                    // Parse the JSON
                    metadata = json::parse(json_str);
                    std::cout << "Successfully parsed metadata: " << metadata.dump(2) << "\n";
                } catch (const json::parse_error& e) {
                    std::cerr << "JSON parse error: " << e.what() << "\n";
                    std::cerr << "Problematic JSON: " << parts[0] << "\n";
                    json error;
                    error["status"] = "error";
                    error["message"] = std::string("Invalid JSON metadata: ") + e.what();
                    error["received_data"] = parts[0];
                    return error.dump();
                }

                // The rest are files (if any)
                std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
                std::cout << "Processing " << parts.size() - 1 << " file parts\n";
                
                // Debug: Print request parts
                std::cout << "\n==== REQUEST PARTS ====\n";
                for (size_t i = 0; i < parts.size(); i++) {
                    std::cout << "Part " << i << " (first 100 chars): " 
                              << parts[i].substr(0, std::min(parts[i].size(), 100UL)) 
                              << "\n\n";
                }
                std::cout << "======================\n\n";
                
                for (size_t i = 1; i < parts.size(); ++i) {
                    std::cout << "Processing part " << i << " (size: " << parts[i].size() << " bytes)\n";
                    
                    // Print first 500 chars of the part for debugging
                    std::string debug_part = parts[i].substr(0, std::min(parts[i].size(), 500UL));
                    std::cout << "\n==== PART CONTENT (first 500 chars) ====\n" << debug_part << "\n==============================\n\n";
                    
                    std::string name, filename, content_type;
                    std::vector<uint8_t> content;
                    
                    // Parse the multipart headers to get filename and content type
                    // First, normalize line endings to \n for consistent processing
                    std::string part = parts[i];
                    size_t pos = 0;
                    while ((pos = part.find("\r\n", pos)) != std::string::npos) {
                        part.replace(pos, 2, "\n");
                        pos += 1;
                    }
                    
                    // Find the end of headers (double newline)
                    size_t header_end = part.find("\n\n");
                    std::cout << "Header end position: " << header_end << "\n";
                    
                    if (header_end != std::string::npos) {
                        std::string headers = part.substr(0, header_end);
                        // Store binary content as bytes
                        const char* content_start = part.data() + header_end + 2;
                        size_t content_length = part.size() - (header_end + 2);
                        content = std::vector<uint8_t>(content_start, content_start + content_length);
                        
                        std::cout << "Headers: " << headers << "\n";
                        std::cout << "Content size: " << content.size() << " bytes\n";
                        
                        // Simple approach: Search for "filename: " in the headers
                        size_t filename_pos = headers.find("filename: ");
                        if (filename_pos != std::string::npos) {
                            size_t start = filename_pos + 10; // length of "filename: "
                            size_t end = headers.find_first_of("\r\n", start);
                            if (end == std::string::npos) end = headers.length();
                            
                            filename = headers.substr(start, end - start);
                            
                            // Trim whitespace and quotes
                            filename.erase(0, filename.find_first_not_of(" \t\"'"));
                            filename.erase(filename.find_last_not_of(" \t\"'") + 1);
                            
                            std::cout << "Extracted filename: " << filename << "\n";
                        }
                            
                        // If we still don't have a filename, try to get it from the 'name' header
                        if (filename.empty()) {
                            size_t name_pos = headers.find("name: ");
                            if (name_pos != std::string::npos) {
                                size_t start = name_pos + 6; // length of "name: "
                                size_t end = headers.find_first_of("\r\n", start);
                                if (end == std::string::npos) end = headers.length();
                                
                                filename = headers.substr(start, end - start);
                                
                                // Trim whitespace and quotes
                                filename.erase(0, filename.find_first_not_of(" \t\"'"));
                                filename.erase(filename.find_last_not_of(" \t\"'") + 1);
                                
                                std::cout << "Using name header as filename: " << filename << "\n";
                            }
                        }
                        
                        // If we still don't have a filename, generate one
                        if (filename.empty()) {
                            filename = "file" + std::to_string(i) + ".dat";
                            std::cout << "Generated filename: " << filename << "\n";
                        }
                        
                        files.emplace_back(filename, content);
                        std::cout << "Added file: " << filename << " (" << content.size() << " bytes)\n";
                    } else {
                        std::cerr << "Error: Could not find end of headers in part " << i << "\n";
                        std::cerr << "First 100 chars of part: " << parts[i].substr(0, std::min(parts[i].size(), 100UL)) << "\n";
                    }
                }

                std::cout << "Processing upload with " << files.size() << " files\n";
                
                // Process the files directly as binary data
                std::string result = uploadHandler.handleUpload(files, metadata);
                std::cout << "Upload processed successfully\n";
                return result;
                
            } catch (const std::exception& e) {
                std::cerr << "Exception in upload handler: " << e.what() << "\n";
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
