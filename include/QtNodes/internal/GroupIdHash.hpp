#pragma once

#include <functional>
#include "Definitions.hpp"

namespace std {
template <>
struct hash<QtNodes::GroupId>
{
    size_t operator()(const QtNodes::GroupId& g) const noexcept
    {
        size_t seed = 0;
        // 合并 nodeIds 哈希
        for (auto const& id : g.nodeIds) {
            seed ^= hash<QtNodes::NodeId>()(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        return seed;
    }
};
} // namespace std

