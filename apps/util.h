#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace util {
// Hopcroft-Karp algorithm for maximum bipartite matching
class HopcroftKarp {
    uint32_t                           l_, r_;
    std::vector<std::vector<uint32_t>> adj_;
    std::vector<uint32_t>              match_l_, match_r_, dist_;

    bool bfs();
    bool dfs(uint32_t u);

public:
    HopcroftKarp(uint32_t l, uint32_t r) : l_(l), r_(r), adj_(l), match_l_(l, ~0U), match_r_(r, ~0U), dist_(l) {}

    void addEdge(uint32_t u, uint32_t v);

    // Runs the algorithm. Returns `num_matches`, `match_l_`, `match_r_`
    [[nodiscard]] std::tuple<uint32_t, std::span<const uint32_t>, std::span<const uint32_t>> match();

    const std::vector<std::vector<uint32_t>>& getAdj() { return adj_; }
};
} // namespace util