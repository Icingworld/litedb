#include "core/vindex/hnsw_index/hnsw_index.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>

#include "core/vindex/vector_distance.hpp"

namespace litedb::core::vindex
{

namespace
{

using Node = hnsw_index::HnswNode;
using NodeId = hnsw_index::HnswNodeId;
using NodeMap = hnsw_index::HnswStore::NodeMap;
constexpr std::size_t MaximumDimension = 1U << 20U;
constexpr std::size_t MaximumNeighbors = 1U << 20U;
constexpr std::size_t MaximumEf = 1U << 24U;

struct Candidate
{
    NodeId node_id;
    double distance;
};

class GraphView
{
public:
    explicit GraphView(const NodeMap & base, const NodeMap * overlay = nullptr) noexcept
        : base_(&base), overlay_(overlay)
    {
    }

    [[nodiscard]] const Node * find(NodeId node_id) const noexcept
    {
        if (overlay_ != nullptr) {
            const auto found = overlay_->find(node_id);
            if (found != overlay_->end()) {
                return &found->second;
            }
        }
        const auto found = base_->find(node_id);
        return found == base_->end() ? nullptr : &found->second;
    }

    [[nodiscard]] const Node & at(NodeId node_id) const
    {
        return *find(node_id);
    }

private:
    const NodeMap * base_;
    const NodeMap * overlay_;
};

[[nodiscard]]
Node & mutable_node(NodeMap & overlay, const NodeMap & base, NodeId node_id)
{
    const auto found = overlay.find(node_id);
    if (found != overlay.end()) {
        return found->second;
    }
    return overlay.emplace(node_id, base.at(node_id)).first->second;
}

[[nodiscard]]
bool candidate_less(const Candidate & left, const Candidate & right) noexcept
{
    if (left.distance != right.distance) {
        return left.distance < right.distance;
    }
    return left.node_id < right.node_id;
}

struct CandidateLess
{
    bool operator()(const Candidate & left, const Candidate & right) const noexcept
    {
        return candidate_less(left, right);
    }
};

struct CandidateGreater
{
    bool operator()(const Candidate & left, const Candidate & right) const noexcept
    {
        return candidate_less(right, left);
    }
};

[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, message};
}

[[nodiscard]]
std::expected<void, VectorIndexError> validate_options(const HnswIndexOptions & options)
{
    if (options.dimension == 0 || options.dimension > MaximumDimension) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidDimension,
            "HNSW dimension is outside the supported range"
        ));
    }
    if (options.max_neighbors == 0 || options.max_neighbors > MaximumNeighbors ||
        options.ef_construction < options.max_neighbors || options.ef_construction > MaximumEf ||
        options.ef_search_default == 0 || options.ef_search_default > MaximumEf) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::ResourceLimitExceeded,
            "HNSW construction or search parameters are outside the supported range"
        ));
    }
    return {};
}

[[nodiscard]]
hnsw_index::HnswStoreDescriptor descriptor(
    common::VIndexId index_id,
    common::CollectionId collection_id,
    common::ColumnId column_id,
    const HnswIndexOptions & options
)
{
    return {
        .index_id = index_id,
        .collection_id = collection_id,
        .column_id = column_id,
        .dimension = options.dimension,
        .metric = options.metric,
        .max_neighbors = options.max_neighbors,
        .ef_construction = options.ef_construction,
        .ef_search_default = options.ef_search_default,
        .random_seed = options.random_seed,
    };
}

[[nodiscard]]
std::expected<double, VectorIndexError> distance_to(
    const common::VectorValue & query,
    const Node & node,
    VectorDistanceMetric metric
)
{
    return vector_distance(query, node.vector, metric);
}

[[nodiscard]]
std::expected<Candidate, VectorIndexError> greedy_search(
    const GraphView & nodes,
    const common::VectorValue & query,
    NodeId entry_id,
    std::size_t layer,
    VectorDistanceMetric metric
)
{
    const auto * found = nodes.find(entry_id);
    if (found == nullptr || found->level < layer) {
        return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "HNSW entry node is missing a search layer"));
    }
    auto current_distance = distance_to(query, *found, metric);
    if (!current_distance) {
        return std::unexpected(std::move(current_distance.error()));
    }
    Candidate current {entry_id, *current_distance};
    bool improved = true;
    while (improved) {
        improved = false;
        const auto & node = nodes.at(current.node_id);
        for (const auto neighbor_id : node.neighbors[layer]) {
            const auto * neighbor = nodes.find(neighbor_id);
            if (neighbor == nullptr) {
                return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "HNSW neighbor node is missing"));
            }
            auto neighbor_distance = distance_to(query, *neighbor, metric);
            if (!neighbor_distance) {
                return std::unexpected(std::move(neighbor_distance.error()));
            }
            Candidate candidate {neighbor_id, *neighbor_distance};
            if (candidate_less(candidate, current)) {
                current = candidate;
                improved = true;
            }
        }
    }
    return current;
}

[[nodiscard]]
std::expected<std::vector<Candidate>, VectorIndexError> search_layer(
    const GraphView & nodes,
    const common::VectorValue & query,
    NodeId entry_id,
    std::size_t ef,
    std::size_t layer,
    VectorDistanceMetric metric
)
{
    const auto * entry = nodes.find(entry_id);
    if (entry == nullptr || entry->level < layer || ef == 0) {
        return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "Invalid HNSW layer search entry"));
    }
    auto entry_distance = distance_to(query, *entry, metric);
    if (!entry_distance) {
        return std::unexpected(std::move(entry_distance.error()));
    }

    std::priority_queue<Candidate, std::vector<Candidate>, CandidateGreater> candidates;
    std::priority_queue<Candidate, std::vector<Candidate>, CandidateLess> nearest;
    std::unordered_set<NodeId> visited;
    Candidate initial {entry_id, *entry_distance};
    candidates.push(initial);
    nearest.push(initial);
    visited.insert(entry_id);

    while (!candidates.empty()) {
        const auto current = candidates.top();
        candidates.pop();
        if (nearest.size() >= ef && candidate_less(nearest.top(), current)) {
            break;
        }
        const auto & node = nodes.at(current.node_id);
        for (const auto neighbor_id : node.neighbors[layer]) {
            if (!visited.insert(neighbor_id).second) {
                continue;
            }
            const auto * neighbor = nodes.find(neighbor_id);
            if (neighbor == nullptr) {
                return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "HNSW neighbor node is missing"));
            }
            auto neighbor_distance = distance_to(query, *neighbor, metric);
            if (!neighbor_distance) {
                return std::unexpected(std::move(neighbor_distance.error()));
            }
            Candidate candidate {neighbor_id, *neighbor_distance};
            if (nearest.size() < ef || candidate_less(candidate, nearest.top())) {
                candidates.push(candidate);
                nearest.push(candidate);
                if (nearest.size() > ef) {
                    nearest.pop();
                }
            }
        }
    }

    std::vector<Candidate> result;
    result.reserve(nearest.size());
    while (!nearest.empty()) {
        result.push_back(nearest.top());
        nearest.pop();
    }
    std::sort(result.begin(), result.end(), candidate_less);
    return result;
}

[[nodiscard]]
std::expected<void, VectorIndexError> prune_neighbors(
    const GraphView & nodes,
    Node & node,
    std::size_t layer,
    std::size_t limit,
    VectorDistanceMetric metric
)
{
    auto & neighbors = node.neighbors[layer];
    std::vector<Candidate> ranked;
    ranked.reserve(neighbors.size());
    for (const auto neighbor_id : neighbors) {
        const auto * neighbor = nodes.find(neighbor_id);
        if (neighbor == nullptr) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "Cannot prune a missing HNSW neighbor"));
        }
        auto distance = vector_distance(node.vector, neighbor->vector, metric);
        if (!distance) {
            return std::unexpected(std::move(distance.error()));
        }
        ranked.push_back({neighbor_id, *distance});
    }
    std::sort(ranked.begin(), ranked.end(), candidate_less);
    if (ranked.size() > limit) {
        ranked.resize(limit);
    }
    neighbors.clear();
    neighbors.reserve(ranked.size());
    for (const auto & candidate : ranked) {
        neighbors.push_back(candidate.node_id);
    }
    return {};
}

} // namespace

HnswIndex::HnswIndex(hnsw_index::HnswStore store) noexcept
    : options_ {
        .dimension = store.descriptor().dimension,
        .metric = store.descriptor().metric,
        .max_neighbors = store.descriptor().max_neighbors,
        .ef_construction = store.descriptor().ef_construction,
        .ef_search_default = store.descriptor().ef_search_default,
        .random_seed = store.descriptor().random_seed,
    }
    , store_(std::move(store))
{
}

std::expected<HnswIndex, VectorIndexError> HnswIndex::create(
    std::filesystem::path path,
    common::VIndexId index_id,
    common::CollectionId collection_id,
    common::ColumnId column_id,
    HnswIndexOptions options,
    filesystem::FileSystem & filesystem
)
{
    auto validation = validate_options(options);
    if (!validation) {
        return std::unexpected(std::move(validation.error()));
    }
    auto store = hnsw_index::HnswStore::create(
        std::move(path), descriptor(index_id, collection_id, column_id, options), filesystem
    );
    if (!store) {
        return std::unexpected(std::move(store.error()));
    }
    return HnswIndex {std::move(*store)};
}

std::expected<HnswIndex, VectorIndexError> HnswIndex::open(
    std::filesystem::path path,
    common::VIndexId expected_index_id,
    common::CollectionId expected_collection_id,
    common::ColumnId expected_column_id,
    HnswIndexOptions expected_options,
    filesystem::FileSystem & filesystem
)
{
    auto validation = validate_options(expected_options);
    if (!validation) {
        return std::unexpected(std::move(validation.error()));
    }
    auto store = hnsw_index::HnswStore::open(
        std::move(path),
        descriptor(expected_index_id, expected_collection_id, expected_column_id, expected_options),
        filesystem
    );
    if (!store) {
        return std::unexpected(std::move(store.error()));
    }
    return HnswIndex {std::move(*store)};
}

VectorIndexKind HnswIndex::kind() const noexcept
{
    return VectorIndexKind::Hnsw;
}

VectorDistanceMetric HnswIndex::metric() const noexcept
{
    return options_.metric;
}

std::size_t HnswIndex::dimension() const noexcept
{
    return options_.dimension;
}

std::expected<void, VectorIndexError> HnswIndex::insert(
    const VectorIndexKey & key,
    common::RecordId record_id
)
{
    auto validation = validate_key(key);
    if (!validation) {
        return validation;
    }
    if (record_id == 0) {
        return std::unexpected(make_error(VectorIndexErrorCode::RecordNotFound, "HNSW record id must not be zero"));
    }
    if (store_.find_active_node(record_id)) {
        return std::unexpected(make_error(VectorIndexErrorCode::RecordAlreadyExists, "HNSW record already exists"));
    }
    const auto current_metadata = store_.metadata();
    if (current_metadata.next_node_id == std::numeric_limits<NodeId>::max()) {
        return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "HNSW node id is exhausted"));
    }

    NodeMap mutations;
    const auto node_id = current_metadata.next_node_id;
    const auto level = random_level(node_id);
    Node new_node {
        .node_id = node_id,
        .record_id = record_id,
        .vector = key.value(),
        .level = level,
        .deleted = false,
        .neighbors = std::vector<std::vector<NodeId>>(level + 1),
    };
    mutations.emplace(node_id, std::move(new_node));
    GraphView working {store_.nodes(), &mutations};

    auto next_metadata = current_metadata;
    next_metadata.next_node_id = node_id + 1;
    ++next_metadata.active_count;
    if (store_.nodes().empty()) {
        next_metadata.entry_point = node_id;
        next_metadata.max_level = level;
    } else {
        auto current = next_metadata.entry_point;
        for (std::size_t layer = next_metadata.max_level; layer > level; --layer) {
            auto greedy = greedy_search(working, key.value(), current, layer, options_.metric);
            if (!greedy) {
                return std::unexpected(std::move(greedy.error()));
            }
            current = greedy->node_id;
        }

        auto layer = std::min(level, next_metadata.max_level);
        while (true) {
            auto candidates = search_layer(
                working, key.value(), current, options_.ef_construction, layer, options_.metric
            );
            if (!candidates) {
                return std::unexpected(std::move(candidates.error()));
            }
            const auto limit = layer == 0 ? options_.max_neighbors * 2 : options_.max_neighbors;
            auto & new_neighbors = mutations.at(node_id).neighbors[layer];
            for (const auto & candidate : *candidates) {
                if (candidate.node_id == node_id) {
                    continue;
                }
                new_neighbors.push_back(candidate.node_id);
                if (new_neighbors.size() == limit) {
                    break;
                }
            }
            for (const auto neighbor_id : new_neighbors) {
                auto & neighbor = mutable_node(mutations, store_.nodes(), neighbor_id);
                auto & reverse = neighbor.neighbors[layer];
                if (std::find(reverse.begin(), reverse.end(), node_id) == reverse.end()) {
                    reverse.push_back(node_id);
                    auto pruned = prune_neighbors(working, neighbor, layer, limit, options_.metric);
                    if (!pruned) {
                        return pruned;
                    }
                }
            }
            if (!candidates->empty()) {
                current = candidates->front().node_id;
            }
            if (layer == 0) {
                break;
            }
            --layer;
        }
        if (level > next_metadata.max_level) {
            next_metadata.entry_point = node_id;
            next_metadata.max_level = level;
        }
    }

    std::vector<Node> upserts;
    upserts.reserve(mutations.size());
    for (auto & [touched_id, node] : mutations) {
        upserts.push_back(std::move(node));
    }
    auto committed = store_.commit(next_metadata, std::move(upserts));
    if (!committed) {
        return std::unexpected(std::move(committed.error()));
    }
    return {};
}

std::expected<void, VectorIndexError> HnswIndex::erase(common::RecordId record_id)
{
    const auto node_id = store_.find_active_node(record_id);
    if (!node_id) {
        return std::unexpected(make_error(VectorIndexErrorCode::RecordNotFound, "HNSW record was not found"));
    }
    auto node = *store_.find_node(*node_id);
    node.deleted = true;
    auto metadata = store_.metadata();
    --metadata.active_count;
    auto committed = store_.commit(metadata, {std::move(node)});
    if (!committed) {
        return std::unexpected(std::move(committed.error()));
    }
    return {};
}

std::expected<std::vector<VectorSearchResult>, VectorIndexError> HnswIndex::search(
    const VectorIndexKey & query,
    VectorSearchRequest request
) const
{
    auto validation = validate_key(query);
    if (!validation) {
        return std::unexpected(std::move(validation.error()));
    }
    if (request.top_k == 0 || store_.metadata().active_count == 0) {
        return std::vector<VectorSearchResult> {};
    }

    const GraphView graph {store_.nodes()};
    auto current = store_.metadata().entry_point;
    for (std::size_t layer = store_.metadata().max_level; layer > 0; --layer) {
        auto greedy = greedy_search(graph, query.value(), current, layer, options_.metric);
        if (!greedy) {
            return std::unexpected(std::move(greedy.error()));
        }
        current = greedy->node_id;
    }
    const auto tombstones = store_.nodes().size() - store_.metadata().active_count;
    auto ef = std::max(request.top_k, options_.ef_search_default);
    ef = std::min(store_.nodes().size(), ef > store_.nodes().size() - std::min(tombstones, store_.nodes().size())
        ? store_.nodes().size()
        : ef + tombstones);
    auto candidates = search_layer(graph, query.value(), current, ef, 0, options_.metric);
    if (!candidates) {
        return std::unexpected(std::move(candidates.error()));
    }

    std::vector<VectorSearchResult> results;
    results.reserve(std::min(request.top_k, store_.metadata().active_count));
    for (const auto & candidate : *candidates) {
        const auto & node = store_.nodes().at(candidate.node_id);
        if (node.deleted) {
            continue;
        }
        results.push_back({.record_id = node.record_id, .distance = candidate.distance});
        if (results.size() == request.top_k) {
            break;
        }
    }
    std::sort(results.begin(), results.end(), [](const auto & left, const auto & right) {
        return left.distance != right.distance ? left.distance < right.distance : left.record_id < right.record_id;
    });
    return results;
}

std::size_t HnswIndex::size() const noexcept
{
    return store_.metadata().active_count;
}

const std::filesystem::path & HnswIndex::path() const noexcept
{
    return store_.path();
}

const HnswIndexOptions & HnswIndex::options() const noexcept
{
    return options_;
}

hnsw_index::HnswStoreStats HnswIndex::stats() const noexcept
{
    return store_.stats();
}

std::expected<void, VectorIndexError> HnswIndex::close()
{
    return store_.close();
}

std::expected<void, VectorIndexError> HnswIndex::validate_key(const VectorIndexKey & key) const
{
    if (key.dimension() != options_.dimension) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "Vector dimension does not match HNSW index"));
    }
    for (const auto value : key.value()) {
        if (!std::isfinite(value)) {
            return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "HNSW vectors must contain finite values"));
        }
    }
    return {};
}

bool HnswIndex::matches_record(common::RecordId record_id, const VectorIndexKey & key) const noexcept
{
    const auto node_id = store_.find_active_node(record_id);
    if (!node_id) {
        return false;
    }
    const auto * node = store_.find_node(*node_id);
    return node != nullptr && node->vector == key.value();
}

std::size_t HnswIndex::random_level(hnsw_index::HnswNodeId node_id) const noexcept
{
    std::uint64_t value = static_cast<std::uint64_t>(options_.random_seed) ^
        (node_id + 0x9e3779b97f4a7c15ULL);
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    const auto uniform = (static_cast<double>(value >> 11U) + 1.0) / 9007199254740993.0;
    const auto multiplier = 1.0 / std::log(static_cast<double>(std::max<std::size_t>(2, options_.max_neighbors)));
    return std::min<std::size_t>(63, static_cast<std::size_t>(-std::log(uniform) * multiplier));
}

} // namespace litedb::core::vindex
