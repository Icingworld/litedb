#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/value.hpp"

namespace litedb::core::vindex::hnsw_index
{

using HnswNodeId = std::uint64_t;

inline constexpr HnswNodeId InvalidHnswNodeId = 0;

struct HnswNode
{
    HnswNodeId node_id {InvalidHnswNodeId};
    common::RecordId record_id {0};
    schema::VectorValue vector;
    std::size_t level {0};
    bool deleted {false};
    std::vector<std::vector<HnswNodeId>> neighbors;
};

struct HnswGraphMetadata
{
    HnswNodeId next_node_id {1};
    HnswNodeId entry_point {InvalidHnswNodeId};
    std::size_t max_level {0};
    std::size_t active_count {0};
    std::uint64_t transaction_id {0};
};

} // namespace litedb::core::vindex::hnsw_index
