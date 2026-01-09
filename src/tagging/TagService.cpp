#include "tagging/TagService.hpp"
#include "embedding/TextExtractor.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <unordered_set>
#include <queue>

TagService::TagService(GraphDB& db, const std::string& apiKey)
    : db_(db), client_(apiKey) {}

std::string TagService::buildContentForTagging(const Node& node, const std::string& storagePath) {
    std::string text;

    // Add metadata
    text += "Title: " + node.getTitle() + "\n";
    text += "Subject: " + node.getSubject() + "\n";
    text += "Author: " + node.getAuthor() + "\n";

    if (!node.getDescription().empty()) {
        text += "Description: " + node.getDescription() + "\n";
    }

    // Add file content if available
    std::string nodePath = node.getStoragePath();
    if (!nodePath.empty()) {
        std::string fullPath = storagePath + "/" + nodePath;
        if (std::filesystem::exists(fullPath)) {
            auto content = TextExtractor::extractFromFile(fullPath);
            if (content) {
                // Limit content to ~2000 chars to avoid too long prompts
                std::string fileContent = *content;
                if (fileContent.size() > 2000) {
                    fileContent = fileContent.substr(0, 2000) + "...";
                }
                text += "\nFile content:\n" + fileContent;
            }
        }
    }

    return text;
}

TagGenerationResult TagService::generateTagsForNode(int nodeId, const std::string& storagePath) {
    TagGenerationResult result;

    std::string nodeIdStr = std::to_string(nodeId);
    if (!db_.exists(nodeIdStr)) {
        result.error = "Node not found: " + nodeIdStr;
        return result;
    }

    Node node = db_.find(nodeIdStr);
    std::string content = buildContentForTagging(node, storagePath);

    if (content.empty()) {
        result.error = "No content to generate tags from";
        return result;
    }

    // Get current tag bank
    auto tagBank = db_.getTagBank();

    // Generate tags using DeepSeek
    auto tagsOpt = client_.generateTags(content, tagBank);
    if (!tagsOpt) {
        result.error = "Failed to generate tags from AI";
        return result;
    }

    result.generatedTags = *tagsOpt;

    // Find new tags (not in tag bank)
    for (const auto& tag : result.generatedTags) {
        if (std::find(tagBank.begin(), tagBank.end(), tag) == tagBank.end()) {
            result.newTagsAdded.push_back(tag);
        }
    }

    // Add new tags to the bank
    if (!result.newTagsAdded.empty()) {
        db_.addToTagBank(result.newTagsAdded);
    }

    // Update node's tags
    node.setTags(result.generatedTags);
    db_.updateNode(nodeIdStr, node.to_json());

    // Find and link nodes with Jaccard similarity >= 0.3
    updateLinksForNode(nodeId, 0.3f);
    result.linkedNodeIds = db_.findNodesWithJaccardSimilarity(nodeId, 0.3f);

    result.success = true;
    return result;
}

std::vector<std::string> TagService::getTagBank() const {
    return db_.getTagBank();
}

std::vector<int> TagService::findNodesWithSharedTags(int nodeId) const {
    return db_.findNodesWithSharedTags(nodeId);
}

std::vector<int> TagService::findNodesByTag(const std::string& tag) const {
    return db_.findNodesByTag(tag);
}

void TagService::addBidirectionalLink(int nodeId1, int nodeId2) {
    std::string id1Str = std::to_string(nodeId1);
    std::string id2Str = std::to_string(nodeId2);

    if (!db_.exists(id1Str) || !db_.exists(id2Str)) {
        return;
    }

    // Update node1's links
    Node node1 = db_.find(id1Str);
    auto links1 = node1.getLinkedNodes();
    if (std::find(links1.begin(), links1.end(), nodeId2) == links1.end()) {
        links1.push_back(nodeId2);
        node1.setLinkedNodes(links1);
        db_.updateNode(id1Str, node1.to_json());
    }

    // Update node2's links
    Node node2 = db_.find(id2Str);
    auto links2 = node2.getLinkedNodes();
    if (std::find(links2.begin(), links2.end(), nodeId1) == links2.end()) {
        links2.push_back(nodeId1);
        node2.setLinkedNodes(links2);
        db_.updateNode(id2Str, node2.to_json());
    }
}

int TagService::updateLinksForNode(int nodeId, float jaccardThreshold) {
    auto similarNodes = db_.findNodesWithJaccardSimilarity(nodeId, jaccardThreshold);
    int linksCreated = 0;

    for (int otherId : similarNodes) {
        // Check if link already exists
        std::string nodeIdStr = std::to_string(nodeId);
        Node node = db_.find(nodeIdStr);
        auto links = node.getLinkedNodes();

        if (std::find(links.begin(), links.end(), otherId) == links.end()) {
            addBidirectionalLink(nodeId, otherId);
            linksCreated++;
        }
    }

    return linksCreated;
}

int TagService::updateAllTagBasedLinks(float jaccardThreshold) {
    int totalLinks = 0;
    auto allNodes = db_.getAllNodes();

    for (const auto& nodeJson : allNodes) {
        Node node(nodeJson);
        if (!node.getTags().empty()) {
            totalLinks += updateLinksForNode(node.getId(), jaccardThreshold);
        }
    }

    return totalLinks;
}

std::vector<ClusterInfo> TagService::getClusters() const {
    std::vector<ClusterInfo> clusters;
    auto allNodes = db_.getAllNodes();

    // Build adjacency map and collect all node IDs
    std::unordered_map<int, std::vector<int>> adjacency;
    std::unordered_set<int> allNodeIds;

    for (const auto& nodeJson : allNodes) {
        Node node(nodeJson);
        int nodeId = node.getId();
        allNodeIds.insert(nodeId);
        adjacency[nodeId] = node.getLinkedNodes();
    }

    // Find connected components using BFS
    std::unordered_set<int> visited;
    int clusterId = 1;

    for (int startId : allNodeIds) {
        if (visited.count(startId)) continue;

        ClusterInfo cluster;
        cluster.id = clusterId++;

        // BFS
        std::queue<int> q;
        q.push(startId);
        visited.insert(startId);

        std::unordered_map<std::string, int> tagCounts;

        while (!q.empty()) {
            int current = q.front();
            q.pop();
            cluster.nodeIds.push_back(current);

            // Get tags for this node
            std::string currentIdStr = std::to_string(current);
            if (db_.exists(currentIdStr)) {
                Node currentNode = db_.find(currentIdStr);
                for (const auto& tag : currentNode.getTags()) {
                    tagCounts[tag]++;
                }
            }

            // Visit neighbors
            for (int neighbor : adjacency[current]) {
                if (!visited.count(neighbor)) {
                    visited.insert(neighbor);
                    q.push(neighbor);
                }
            }
        }

        // Find shared tags (appearing in more than one node, or all tags if single node)
        if (cluster.nodeIds.size() == 1) {
            // Single node - show its tags
            std::string idStr = std::to_string(cluster.nodeIds[0]);
            if (db_.exists(idStr)) {
                cluster.sharedTags = db_.find(idStr).getTags();
            }
        } else {
            // Multiple nodes - show tags that appear in at least 2 nodes
            for (const auto& [tag, count] : tagCounts) {
                if (count >= 2) {
                    cluster.sharedTags.push_back(tag);
                }
            }
        }

        clusters.push_back(cluster);
    }

    // Sort clusters by size (largest first)
    std::sort(clusters.begin(), clusters.end(),
        [](const ClusterInfo& a, const ClusterInfo& b) {
            return a.nodeIds.size() > b.nodeIds.size();
        });

    // Reassign IDs after sorting
    for (size_t i = 0; i < clusters.size(); ++i) {
        clusters[i].id = static_cast<int>(i + 1);
    }

    return clusters;
}
