#pragma once

#include "tagging/TagClient.hpp"
#include "core/GraphDB.hpp"
#include <string>
#include <vector>

struct TagGenerationResult {
    bool success = false;
    std::vector<std::string> generatedTags;
    std::vector<std::string> newTagsAdded;
    std::vector<int> linkedNodeIds;
    std::string error;
};

struct ClusterInfo {
    int id;
    std::vector<int> nodeIds;
    std::vector<std::string> sharedTags;
};

class TagService {
public:
    TagService(GraphDB& db, const std::string& apiKey);

    // Generate tags for a specific node
    TagGenerationResult generateTagsForNode(int nodeId, const std::string& storagePath);

    // Get the current tag bank
    std::vector<std::string> getTagBank() const;

    // Find nodes that share any tags with the given node
    std::vector<int> findNodesWithSharedTags(int nodeId) const;

    // Find nodes by specific tag
    std::vector<int> findNodesByTag(const std::string& tag) const;

    // Update LinkedNodes for a node based on tag overlap
    int updateLinksForNode(int nodeId);

    // Batch: update all node links based on tags
    int updateAllTagBasedLinks();

    // Get all connected components (clusters)
    std::vector<ClusterInfo> getClusters() const;

    // Set model name
    void setModel(const std::string& model) { client_.setModel(model); }

private:
    GraphDB& db_;
    TagClient client_;

    // Build text content for tag generation
    std::string buildContentForTagging(const Node& node, const std::string& storagePath);

    // Add bidirectional links between nodes
    void addBidirectionalLink(int nodeId1, int nodeId2);
};
