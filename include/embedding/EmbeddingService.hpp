#pragma once

#include "embedding/EmbeddingClient.hpp"
#include "embedding/TextExtractor.hpp"
#include "embedding/Clustering.hpp"
#include "core/GraphDB.hpp"
#include <string>
#include <memory>

struct ClusteringResult {
    int nodesProcessed = 0;
    int embeddingsGenerated = 0;
    int linksCreated = 0;
    int clustersFound = 0;
    std::vector<std::vector<int>> clusters;
};

class EmbeddingService {
public:
    EmbeddingService(GraphDB& db, const std::string& apiKey);

    // Generate embedding for a single node (by ID)
    bool generateEmbedding(int nodeId, const std::string& storagePath);

    // Generate embeddings for all nodes without embeddings
    int generateMissingEmbeddings(const std::string& storagePath);

    // Run full clustering: generate embeddings + build links
    ClusteringResult runClustering(const std::string& storagePath, float threshold = 0.75f);

    // Update LinkedNodes based on embeddings
    int updateLinks(float threshold = 0.75f);

    // Set API model
    void setModel(const std::string& model) { client_.setModel(model); }

private:
    GraphDB& db_;
    EmbeddingClient client_;

    // Build text for embedding from node metadata + file content
    std::string buildTextForEmbedding(const Node& node, const std::string& storagePath);
};
