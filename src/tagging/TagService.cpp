#include "tagging/TagService.hpp"
#include "embedding/TextExtractor.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>

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

    // Find and link nodes with shared tags
    result.linkedNodeIds = updateLinksForNode(nodeId);

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

int TagService::updateLinksForNode(int nodeId) {
    auto sharedNodes = db_.findNodesWithSharedTags(nodeId);
    int linksCreated = 0;

    for (int otherId : sharedNodes) {
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

int TagService::updateAllTagBasedLinks() {
    int totalLinks = 0;
    auto allNodes = db_.getAllNodes();

    for (const auto& nodeJson : allNodes) {
        Node node(nodeJson);
        if (!node.getTags().empty()) {
            totalLinks += updateLinksForNode(node.getId());
        }
    }

    return totalLinks;
}
