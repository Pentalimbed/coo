#include "util.h"

#include <cstdint>
#include <queue>

namespace util {

void HopcroftKarp::addEdge(uint32_t u, uint32_t v)
{
    adj_[u].push_back(v);
}

bool HopcroftKarp::bfs()
{
    std::queue<uint32_t> q;
    for (uint32_t u = 0; u < l_; u++) {
        if (match_l_[u] == ~0U) {
            dist_[u] = 0;
            q.push(u);
        } else
            dist_[u] = ~0U;
    }
    bool found = false;
    while (!q.empty()) {
        uint32_t u = q.front();
        q.pop();
        for (uint32_t v : adj_[u]) {
            uint32_t w = match_r_[v];
            if (w == ~0U) found = true;
            else if (dist_[w] == ~0U) {
                dist_[w] = dist_[u] + 1;
                q.push(w);
            }
        }
    }
    return found;
}

bool HopcroftKarp::dfs(uint32_t u)
{
    for (uint32_t v : adj_[u]) {
        uint32_t w = match_r_[v];
        if (w == ~0U || (dist_[w] == dist_[u] + 1 && dfs(w))) {
            match_l_[u] = v;
            match_r_[v] = u;
            return true;
        }
    }
    dist_[u] = ~0U;
    return false;
}

std::tuple<uint32_t, std::span<const uint32_t>, std::span<const uint32_t>> HopcroftKarp::match()
{
    uint32_t matched = 0;
    while (bfs())
        for (uint32_t u = 0; u < l_; u++)
            if ((match_l_[u] == ~0U) && dfs(u))
                matched++;
    return {matched, match_l_, match_r_};
}
} // namespace util