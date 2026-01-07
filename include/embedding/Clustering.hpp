#pragma once

#include <vector>
#include <unordered_map>
#include <utility>

class Clustering {
public:
    // Cosine similarity between two vectors
    static float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

    // Find all pairs with similarity above threshold
    // Returns vector of (id1, id2, similarity)
    static std::vector<std::tuple<int, int, float>> findSimilarPairs(
        const std::unordered_map<int, std::vector<float>>& embeddings,
        float threshold = 0.75f
    );

    // Build adjacency list from similar pairs
    static std::unordered_map<int, std::vector<int>> buildAdjacencyList(
        const std::vector<std::tuple<int, int, float>>& pairs
    );

    // Find connected components (clusters)
    static std::vector<std::vector<int>> findConnectedComponents(
        const std::unordered_map<int, std::vector<int>>& adjacencyList,
        const std::vector<int>& allNodeIds
    );

    // Set similarity threshold
    static void setThreshold(float t) { threshold_ = t; }
    static float getThreshold() { return threshold_; }

private:
    static float threshold_;

    // DFS helper for finding connected components
    static void dfs(int node,
                   const std::unordered_map<int, std::vector<int>>& adj,
                   std::unordered_map<int, bool>& visited,
                   std::vector<int>& component);
};
