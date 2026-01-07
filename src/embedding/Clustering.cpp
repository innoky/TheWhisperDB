#include "embedding/Clustering.hpp"
#include <cmath>
#include <algorithm>

float Clustering::threshold_ = 0.75f;

float Clustering::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) {
        return 0.0f;
    }

    float dotProduct = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;

    for (size_t i = 0; i < a.size(); ++i) {
        dotProduct += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }

    if (normA == 0.0f || normB == 0.0f) {
        return 0.0f;
    }

    return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
}

std::vector<std::tuple<int, int, float>> Clustering::findSimilarPairs(
    const std::unordered_map<int, std::vector<float>>& embeddings,
    float threshold
) {
    std::vector<std::tuple<int, int, float>> pairs;

    // Convert to vector for easier iteration
    std::vector<std::pair<int, const std::vector<float>*>> embList;
    embList.reserve(embeddings.size());
    for (const auto& [id, emb] : embeddings) {
        embList.emplace_back(id, &emb);
    }

    // O(n^2) comparison
    for (size_t i = 0; i < embList.size(); ++i) {
        for (size_t j = i + 1; j < embList.size(); ++j) {
            float sim = cosineSimilarity(*embList[i].second, *embList[j].second);
            if (sim >= threshold) {
                pairs.emplace_back(embList[i].first, embList[j].first, sim);
            }
        }
    }

    return pairs;
}

std::unordered_map<int, std::vector<int>> Clustering::buildAdjacencyList(
    const std::vector<std::tuple<int, int, float>>& pairs
) {
    std::unordered_map<int, std::vector<int>> adj;

    for (const auto& [id1, id2, sim] : pairs) {
        adj[id1].push_back(id2);
        adj[id2].push_back(id1);
    }

    return adj;
}

void Clustering::dfs(int node,
                     const std::unordered_map<int, std::vector<int>>& adj,
                     std::unordered_map<int, bool>& visited,
                     std::vector<int>& component) {
    visited[node] = true;
    component.push_back(node);

    auto it = adj.find(node);
    if (it != adj.end()) {
        for (int neighbor : it->second) {
            if (!visited[neighbor]) {
                dfs(neighbor, adj, visited, component);
            }
        }
    }
}

std::vector<std::vector<int>> Clustering::findConnectedComponents(
    const std::unordered_map<int, std::vector<int>>& adjacencyList,
    const std::vector<int>& allNodeIds
) {
    std::vector<std::vector<int>> components;
    std::unordered_map<int, bool> visited;

    for (int id : allNodeIds) {
        visited[id] = false;
    }

    for (int id : allNodeIds) {
        if (!visited[id]) {
            std::vector<int> component;
            dfs(id, adjacencyList, visited, component);
            if (!component.empty()) {
                components.push_back(std::move(component));
            }
        }
    }

    return components;
}
