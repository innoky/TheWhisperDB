#include "embedding/EmbeddingService.hpp"
#include <iostream>
#include <filesystem>

EmbeddingService::EmbeddingService(GraphDB& db, const std::string& apiKey)
    : db_(db), client_(apiKey) {}

std::string EmbeddingService::buildTextForEmbedding(const Node& node, const std::string& storagePath) {
    std::string text;

    // Add metadata
    text += "Title: " + node.getTitle() + "\n";
    text += "Subject: " + node.getSubject() + "\n";
    text += "Author: " + node.getAuthor() + "\n";

    if (!node.getDescription().empty()) {
        text += "Description: " + node.getDescription() + "\n";
    }

    auto tags = node.getTags();
    if (!tags.empty()) {
        text += "Tags: ";
        for (size_t i = 0; i < tags.size(); ++i) {
            text += tags[i];
            if (i < tags.size() - 1) text += ", ";
        }
        text += "\n";
    }

    // Add file content if available
    std::string nodePath = node.getStoragePath();
    if (!nodePath.empty()) {
        std::string fullPath = storagePath + "/" + nodePath;
        if (std::filesystem::exists(fullPath)) {
            auto content = TextExtractor::extractFromFile(fullPath);
            if (content) {
                text += "\nContent:\n" + *content;
            }
        }
    }

    return text;
}

bool EmbeddingService::generateEmbedding(int nodeId, const std::string& storagePath) {
    std::string nodeIdStr = std::to_string(nodeId);
    if (!db_.exists(nodeIdStr)) {
        return false;
    }

    Node node = db_.find(nodeIdStr);
    std::string text = buildTextForEmbedding(node, storagePath);

    auto embedding = client_.getEmbedding(text);
    if (!embedding) {
        return false;
    }

    node.setEmbedding(*embedding);
    db_.updateNode(nodeIdStr, node.to_json());
    return true;
}

int EmbeddingService::generateMissingEmbeddings(const std::string& storagePath) {
    int count = 0;
    auto allNodes = db_.getAllNodes();

    for (const auto& nodeJson : allNodes) {
        Node node(nodeJson);
        if (!node.hasEmbedding()) {
            std::string text = buildTextForEmbedding(node, storagePath);
            auto embedding = client_.getEmbedding(text);

            if (embedding) {
                node.setEmbedding(*embedding);
                db_.updateNode(std::to_string(node.getId()), node.to_json());
                count++;
                std::cout << "Generated embedding for node " << node.getId() << std::endl;
            }
        }
    }

    return count;
}

int EmbeddingService::updateLinks(float threshold) {
    auto allNodes = db_.getAllNodes();

    // Collect embeddings
    std::unordered_map<int, std::vector<float>> embeddings;
    std::vector<int> nodeIds;

    for (const auto& nodeJson : allNodes) {
        Node node(nodeJson);
        if (node.hasEmbedding()) {
            embeddings[node.getId()] = node.getEmbedding();
            nodeIds.push_back(node.getId());
        }
    }

    if (embeddings.size() < 2) {
        return 0;
    }

    // Find similar pairs
    auto pairs = Clustering::findSimilarPairs(embeddings, threshold);
    auto adjacencyList = Clustering::buildAdjacencyList(pairs);

    // Update LinkedNodes for each node
    int linksCreated = 0;
    for (const auto& nodeJson : allNodes) {
        Node node(nodeJson);
        int id = node.getId();
        auto it = adjacencyList.find(id);

        if (it != adjacencyList.end()) {
            std::vector<int> newLinks = it->second;
            std::vector<int> oldLinks = node.getLinkedNodes();

            // Merge old and new links (keep unique)
            for (int oldLink : oldLinks) {
                if (std::find(newLinks.begin(), newLinks.end(), oldLink) == newLinks.end()) {
                    newLinks.push_back(oldLink);
                }
            }

            int newLinksCount = newLinks.size() - oldLinks.size();
            if (newLinksCount > 0) {
                linksCreated += newLinksCount;
            }

            node.setLinkedNodes(newLinks);
            db_.updateNode(std::to_string(id), node.to_json());
        }
    }

    return linksCreated;
}

ClusteringResult EmbeddingService::runClustering(const std::string& storagePath, float threshold) {
    ClusteringResult result;

    auto allNodes = db_.getAllNodes();
    result.nodesProcessed = allNodes.size();

    // Generate missing embeddings
    result.embeddingsGenerated = generateMissingEmbeddings(storagePath);

    // Reload nodes after embedding generation
    allNodes = db_.getAllNodes();

    // Collect embeddings
    std::unordered_map<int, std::vector<float>> embeddings;
    std::vector<int> nodeIds;

    for (const auto& nodeJson : allNodes) {
        Node node(nodeJson);
        if (node.hasEmbedding()) {
            embeddings[node.getId()] = node.getEmbedding();
            nodeIds.push_back(node.getId());
        }
    }

    if (embeddings.size() < 2) {
        return result;
    }

    // Find similar pairs and build links
    auto pairs = Clustering::findSimilarPairs(embeddings, threshold);
    auto adjacencyList = Clustering::buildAdjacencyList(pairs);

    // Find connected components (clusters)
    result.clusters = Clustering::findConnectedComponents(adjacencyList, nodeIds);
    result.clustersFound = result.clusters.size();

    // Update LinkedNodes
    for (const auto& nodeJson : allNodes) {
        Node node(nodeJson);
        int id = node.getId();
        auto it = adjacencyList.find(id);

        if (it != adjacencyList.end()) {
            node.setLinkedNodes(it->second);
            db_.updateNode(std::to_string(id), node.to_json());
            result.linksCreated += it->second.size();
        }
    }

    // Links are bidirectional, so divide by 2
    result.linksCreated /= 2;

    return result;
}
