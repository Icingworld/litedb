#include "core/index/btree_index/btree_index.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace litedb::core::index
{

namespace
{

[[nodiscard]]
IndexError storage_error(btree_index::BTreePageStoreError error)
{
    return IndexError {
        IndexErrorCode::StorageError,
        "BTreeIndex page store error: " + std::move(error.message),
    };
}

[[nodiscard]]
IndexError corrupted_tree(std::string message)
{
    return IndexError {
        IndexErrorCode::StorageError,
        "BTreeIndex is corrupted: " + std::move(message),
    };
}

[[nodiscard]]
IndexError codec_error(btree_index::BTreePageCodecError error)
{
    switch (error.code) {
    case btree_index::BTreePageCodecErrorCode::UnsupportedKeyType:
        return IndexError {IndexErrorCode::UnsupportedKeyType, std::move(error.message)};
    case btree_index::BTreePageCodecErrorCode::KeyTypeMismatch:
        return IndexError {IndexErrorCode::KeyTypeMismatch, std::move(error.message)};
    case btree_index::BTreePageCodecErrorCode::InvalidPage:
    case btree_index::BTreePageCodecErrorCode::PageTooLarge:
    case btree_index::BTreePageCodecErrorCode::InvalidFormat:
    case btree_index::BTreePageCodecErrorCode::UnsupportedVersion:
    case btree_index::BTreePageCodecErrorCode::ChecksumMismatch:
    case btree_index::BTreePageCodecErrorCode::CorruptedPage:
        return IndexError {
            IndexErrorCode::StorageError,
            "BTreeIndex page codec error: " + std::move(error.message),
        };
    }
    return IndexError {IndexErrorCode::StorageError, "Unknown BTreeIndex page codec error"};
}

class BTreeInserter final
{
private:
    using PageId = btree_index::BTreePageId;
    using SearchPath = std::vector<PageId>;

public:
    explicit BTreeInserter(btree_index::BTreePageStore & store) noexcept
        : store_(store)
    {
    }

    [[nodiscard]]
    std::expected<void, IndexError> insert(btree_index::BTreeEntryKey entry)
    {
        auto entry_fits = single_leaf_entry_fits(entry);
        if (!entry_fits.has_value()) {
            return std::unexpected(std::move(entry_fits.error()));
        }
        if (!*entry_fits) {
            return std::unexpected(IndexError {
                IndexErrorCode::InvalidKeyValue,
                "BTreeIndex entry is too large to fit in a leaf page",
            });
        }
        if (store_.entry_count() == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(IndexError {
                IndexErrorCode::StorageError,
                "BTreeIndex entry count is exhausted",
            });
        }

        if (store_.root_page_id() == btree_index::InvalidBTreePageId) {
            auto created = create_root_leaf(std::move(entry));
            if (!created.has_value()) {
                return std::unexpected(std::move(created.error()));
            }
            return increment_entry_count();
        }

        SearchPath path;
        auto leaf = find_leaf(entry, path);
        if (!leaf.has_value()) {
            return std::unexpected(std::move(leaf.error()));
        }
        if (leaf->contains(entry)) {
            return std::unexpected(IndexError {
                IndexErrorCode::DuplicateEntry,
                "BTreeIndex entry already exists",
            });
        }
        if (!leaf->insert(std::move(entry))) {
            return std::unexpected(corrupted_tree("leaf rejected a non-duplicate entry"));
        }

        auto fits = page_fits(btree_index::BTreePage {*leaf});
        if (!fits.has_value()) {
            return std::unexpected(std::move(fits.error()));
        }
        if (*fits) {
            auto written = store_.write_page(btree_index::BTreePage {*leaf});
            if (!written.has_value()) {
                return std::unexpected(storage_error(std::move(written.error())));
            }
        } else {
            auto split = split_leaf(*leaf, path);
            if (!split.has_value()) {
                return std::unexpected(std::move(split.error()));
            }
        }
        return increment_entry_count();
    }

private:
    [[nodiscard]]
    std::expected<bool, IndexError> page_fits(const btree_index::BTreePage & page) const
    {
        auto fits = btree_index::BTreePageCodec::can_fit(page, store_.key_type());
        if (!fits.has_value()) {
            return std::unexpected(codec_error(std::move(fits.error())));
        }
        return *fits;
    }

    [[nodiscard]]
    std::expected<std::size_t, IndexError> page_size(
        const btree_index::BTreePage & page
    ) const
    {
        auto size = btree_index::BTreePageCodec::encoded_size(page, store_.key_type());
        if (!size.has_value()) {
            return std::unexpected(codec_error(std::move(size.error())));
        }
        return *size;
    }

    [[nodiscard]]
    std::expected<bool, IndexError> single_leaf_entry_fits(
        const btree_index::BTreeEntryKey & entry
    ) const
    {
        btree_index::BTreeLeafPage leaf {1};
        if (!leaf.insert(entry)) {
            return std::unexpected(corrupted_tree("failed to build a single-entry leaf page"));
        }
        return page_fits(btree_index::BTreePage {std::move(leaf)});
    }

    [[nodiscard]]
    std::expected<void, IndexError> increment_entry_count()
    {
        auto updated = store_.set_entry_count(store_.entry_count() + 1);
        if (!updated.has_value()) {
            return std::unexpected(storage_error(std::move(updated.error())));
        }
        return {};
    }

    [[nodiscard]]
    std::expected<void, IndexError> create_root_leaf(btree_index::BTreeEntryKey entry)
    {
        auto leaf = store_.allocate_leaf_page();
        if (!leaf.has_value()) {
            return std::unexpected(storage_error(std::move(leaf.error())));
        }
        if (!leaf->insert(std::move(entry))) {
            return std::unexpected(corrupted_tree("new root leaf rejected its first entry"));
        }
        auto written = store_.write_page(btree_index::BTreePage {*leaf});
        if (!written.has_value()) {
            return std::unexpected(storage_error(std::move(written.error())));
        }
        auto rooted = store_.set_root_page_id(leaf->page_id());
        if (!rooted.has_value()) {
            return std::unexpected(storage_error(std::move(rooted.error())));
        }
        return {};
    }

    [[nodiscard]]
    std::expected<btree_index::BTreeLeafPage, IndexError> find_leaf(
        const btree_index::BTreeEntryKey & entry,
        SearchPath & path
    ) const
    {
        auto page_id = store_.root_page_id();
        std::unordered_set<PageId> visited;
        while (true) {
            if (!visited.insert(page_id).second) {
                return std::unexpected(corrupted_tree("cycle detected while locating an insert leaf"));
            }
            auto page = store_.read_page(page_id);
            if (!page.has_value()) {
                return std::unexpected(storage_error(std::move(page.error())));
            }
            if (auto * leaf = std::get_if<btree_index::BTreeLeafPage>(&*page)) {
                return std::move(*leaf);
            }
            const auto & internal = std::get<btree_index::BTreeInternalPage>(*page);
            path.push_back(internal.page_id());
            page_id = internal.child_for(entry);
        }
    }

    [[nodiscard]]
    static std::expected<btree_index::BTreeLeafPage, IndexError> build_leaf(
        PageId page_id,
        PageId previous_page_id,
        PageId next_page_id,
        const std::vector<btree_index::BTreeLeafEntry> & entries,
        std::size_t begin,
        std::size_t end
    )
    {
        btree_index::BTreeLeafPage leaf {page_id, previous_page_id, next_page_id};
        for (auto index = begin; index < end; ++index) {
            if (!leaf.insert(entries[index])) {
                return std::unexpected(corrupted_tree("duplicate entry found while splitting a leaf"));
            }
        }
        return leaf;
    }

    [[nodiscard]]
    std::expected<std::pair<btree_index::BTreeLeafPage, btree_index::BTreeLeafPage>, IndexError>
    partition_leaf(
        const btree_index::BTreeLeafPage & leaf,
        PageId right_page_id
    ) const
    {
        const auto & entries = leaf.entries();
        std::optional<std::pair<btree_index::BTreeLeafPage, btree_index::BTreeLeafPage>> best;
        auto best_largest_size = std::numeric_limits<std::size_t>::max();

        for (std::size_t split = 1; split < entries.size(); ++split) {
            auto left = build_leaf(
                leaf.page_id(), leaf.previous_page_id(), right_page_id,
                entries, 0, split
            );
            auto right = build_leaf(
                right_page_id, leaf.page_id(), leaf.next_page_id(),
                entries, split, entries.size()
            );
            if (!left.has_value()) {
                return std::unexpected(std::move(left.error()));
            }
            if (!right.has_value()) {
                return std::unexpected(std::move(right.error()));
            }

            auto left_size = page_size(btree_index::BTreePage {*left});
            auto right_size = page_size(btree_index::BTreePage {*right});
            if (!left_size.has_value()) {
                return std::unexpected(std::move(left_size.error()));
            }
            if (!right_size.has_value()) {
                return std::unexpected(std::move(right_size.error()));
            }
            if (*left_size > btree_index::BTreePageCodec::PageSize ||
                *right_size > btree_index::BTreePageCodec::PageSize) {
                continue;
            }

            const auto largest_size = std::max(*left_size, *right_size);
            if (largest_size < best_largest_size) {
                best_largest_size = largest_size;
                best = std::pair {std::move(*left), std::move(*right)};
            }
        }

        if (!best.has_value()) {
            return std::unexpected(IndexError {
                IndexErrorCode::InvalidKeyValue,
                "BTreeIndex leaf entries cannot be split into two physical pages",
            });
        }
        return std::move(*best);
    }

    [[nodiscard]]
    std::expected<void, IndexError> split_leaf(
        const btree_index::BTreeLeafPage & leaf,
        SearchPath & path
    )
    {
        auto allocated = store_.allocate_leaf_page(leaf.page_id(), leaf.next_page_id());
        if (!allocated.has_value()) {
            return std::unexpected(storage_error(std::move(allocated.error())));
        }
        const auto right_page_id = allocated->page_id();
        auto pages = partition_leaf(leaf, right_page_id);
        if (!pages.has_value()) {
            return std::unexpected(std::move(pages.error()));
        }
        auto & [left, right] = *pages;

        auto right_written = store_.write_page(btree_index::BTreePage {right});
        if (!right_written.has_value()) {
            return std::unexpected(storage_error(std::move(right_written.error())));
        }
        if (right.next_page_id() != btree_index::InvalidBTreePageId) {
            auto next_page = store_.read_page(right.next_page_id());
            if (!next_page.has_value()) {
                return std::unexpected(storage_error(std::move(next_page.error())));
            }
            auto * next_leaf = std::get_if<btree_index::BTreeLeafPage>(&*next_page);
            if (next_leaf == nullptr) {
                return std::unexpected(corrupted_tree("leaf split successor is not a leaf page"));
            }
            next_leaf->set_previous_page_id(right.page_id());
            auto next_written = store_.write_page(*next_page);
            if (!next_written.has_value()) {
                return std::unexpected(storage_error(std::move(next_written.error())));
            }
        }
        auto left_written = store_.write_page(btree_index::BTreePage {left});
        if (!left_written.has_value()) {
            return std::unexpected(storage_error(std::move(left_written.error())));
        }

        return propagate_split(
            left.page_id(), right.entries().front(), right.page_id(), path
        );
    }

    [[nodiscard]]
    static std::expected<btree_index::BTreeInternalPage, IndexError> build_internal(
        PageId page_id,
        PageId first_child_id,
        const std::vector<btree_index::BTreeInternalEntry> & entries,
        std::size_t begin,
        std::size_t end
    )
    {
        btree_index::BTreeInternalPage page {page_id, first_child_id};
        auto left_child_id = first_child_id;
        for (auto index = begin; index < end; ++index) {
            if (!page.insert_child_after(
                    left_child_id,
                    entries[index].separator,
                    entries[index].right_child_id
                )) {
                return std::unexpected(corrupted_tree("invalid child sequence while splitting an internal page"));
            }
            left_child_id = entries[index].right_child_id;
        }
        return page;
    }

    [[nodiscard]]
    std::expected<std::size_t, IndexError> choose_internal_split(
        const btree_index::BTreeInternalPage & page
    ) const
    {
        const auto & entries = page.entries();
        std::optional<std::size_t> best;
        auto best_largest_size = std::numeric_limits<std::size_t>::max();

        for (std::size_t promoted = 0; promoted < entries.size(); ++promoted) {
            auto left = build_internal(
                page.page_id(), page.first_child_id(), entries, 0, promoted
            );
            auto right = build_internal(
                page.page_id(), entries[promoted].right_child_id,
                entries, promoted + 1, entries.size()
            );
            if (!left.has_value()) {
                return std::unexpected(std::move(left.error()));
            }
            if (!right.has_value()) {
                return std::unexpected(std::move(right.error()));
            }

            auto left_size = page_size(btree_index::BTreePage {*left});
            auto right_size = page_size(btree_index::BTreePage {*right});
            if (!left_size.has_value()) {
                return std::unexpected(std::move(left_size.error()));
            }
            if (!right_size.has_value()) {
                return std::unexpected(std::move(right_size.error()));
            }
            if (*left_size > btree_index::BTreePageCodec::PageSize ||
                *right_size > btree_index::BTreePageCodec::PageSize) {
                continue;
            }

            const auto largest_size = std::max(*left_size, *right_size);
            if (largest_size < best_largest_size) {
                best_largest_size = largest_size;
                best = promoted;
            }
        }

        if (!best.has_value()) {
            return std::unexpected(IndexError {
                IndexErrorCode::InvalidKeyValue,
                "BTreeIndex internal entries cannot be split into two physical pages",
            });
        }
        return *best;
    }

    [[nodiscard]]
    std::expected<void, IndexError> propagate_split(
        PageId left_page_id,
        btree_index::BTreeEntryKey separator,
        PageId right_page_id,
        SearchPath & path
    )
    {
        while (true) {
            if (path.empty()) {
                auto root = store_.allocate_internal_page(left_page_id);
                if (!root.has_value()) {
                    return std::unexpected(storage_error(std::move(root.error())));
                }
                if (!root->insert_child_after(left_page_id, std::move(separator), right_page_id)) {
                    return std::unexpected(corrupted_tree("new root rejected a split separator"));
                }
                auto written = store_.write_page(btree_index::BTreePage {*root});
                if (!written.has_value()) {
                    return std::unexpected(storage_error(std::move(written.error())));
                }
                auto rooted = store_.set_root_page_id(root->page_id());
                if (!rooted.has_value()) {
                    return std::unexpected(storage_error(std::move(rooted.error())));
                }
                return {};
            }

            const auto parent_page_id = path.back();
            path.pop_back();
            auto parent_page = store_.read_page(parent_page_id);
            if (!parent_page.has_value()) {
                return std::unexpected(storage_error(std::move(parent_page.error())));
            }
            auto * parent = std::get_if<btree_index::BTreeInternalPage>(&*parent_page);
            if (parent == nullptr) {
                return std::unexpected(corrupted_tree("insert path contains a non-internal parent page"));
            }
            if (!parent->insert_child_after(left_page_id, std::move(separator), right_page_id)) {
                return std::unexpected(corrupted_tree("parent rejected a split separator"));
            }

            auto fits = page_fits(*parent_page);
            if (!fits.has_value()) {
                return std::unexpected(std::move(fits.error()));
            }
            if (*fits) {
                auto written = store_.write_page(*parent_page);
                if (!written.has_value()) {
                    return std::unexpected(storage_error(std::move(written.error())));
                }
                return {};
            }

            auto promoted = choose_internal_split(*parent);
            if (!promoted.has_value()) {
                return std::unexpected(std::move(promoted.error()));
            }
            const auto entries = parent->entries();
            auto right = store_.allocate_internal_page(entries[*promoted].right_child_id);
            if (!right.has_value()) {
                return std::unexpected(storage_error(std::move(right.error())));
            }

            auto left_partition = build_internal(
                parent->page_id(), parent->first_child_id(), entries, 0, *promoted
            );
            auto right_partition = build_internal(
                right->page_id(), entries[*promoted].right_child_id,
                entries, *promoted + 1, entries.size()
            );
            if (!left_partition.has_value()) {
                return std::unexpected(std::move(left_partition.error()));
            }
            if (!right_partition.has_value()) {
                return std::unexpected(std::move(right_partition.error()));
            }

            auto right_written = store_.write_page(btree_index::BTreePage {*right_partition});
            if (!right_written.has_value()) {
                return std::unexpected(storage_error(std::move(right_written.error())));
            }
            auto left_written = store_.write_page(btree_index::BTreePage {*left_partition});
            if (!left_written.has_value()) {
                return std::unexpected(storage_error(std::move(left_written.error())));
            }

            left_page_id = left_partition->page_id();
            separator = entries[*promoted].separator;
            right_page_id = right_partition->page_id();
        }
    }

private:
    btree_index::BTreePageStore & store_;
};

class BTreeBulkLoader final
{
private:
    using PageId = btree_index::BTreePageId;

    struct NodeRef
    {
        PageId page_id;
        btree_index::BTreeEntryKey minimum;
    };

public:
    explicit BTreeBulkLoader(btree_index::BTreePageStore & store) noexcept
        : store_(store)
    {
    }

    [[nodiscard]]
    std::expected<void, IndexError> load(std::vector<ScalarIndexEntry> entries)
    {
        if (store_.root_page_id() != btree_index::InvalidBTreePageId
            || store_.entry_count() != 0) {
            return std::unexpected(IndexError {
                IndexErrorCode::InvalidKeyValue,
                "BTreeIndex bulk load requires an empty tree",
            });
        }
        if (entries.empty()) {
            return {};
        }
        if (entries.size() > std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(IndexError {
                IndexErrorCode::StorageError,
                "BTreeIndex bulk load entry count is exhausted",
            });
        }

        std::vector<btree_index::BTreeEntryKey> sorted;
        sorted.reserve(entries.size());
        for (auto & entry : entries) {
            if (!entry.key.value().matches_type(store_.key_type())) {
                return std::unexpected(IndexError {
                    IndexErrorCode::KeyTypeMismatch,
                    "Bulk-loaded index key type does not match BTreeIndex key type",
                });
            }
            sorted.push_back({
                .key = std::move(entry.key),
                .record_id = entry.record_id,
            });
        }
        std::sort(sorted.begin(), sorted.end(), btree_index::BTreeEntryKeyLess {});
        if (std::adjacent_find(sorted.begin(), sorted.end(), [](const auto & left, const auto & right) {
                return btree_index::compare_btree_entry_keys(left, right)
                    == std::strong_ordering::equal;
            }) != sorted.end()) {
            return std::unexpected(IndexError {
                IndexErrorCode::DuplicateEntry,
                "BTreeIndex bulk load contains a duplicate entry",
            });
        }

        auto leaf_groups = partition_leaves(sorted);
        if (!leaf_groups.has_value()) {
            return std::unexpected(std::move(leaf_groups.error()));
        }
        auto level = write_leaves(*leaf_groups);
        if (!level.has_value()) {
            return std::unexpected(std::move(level.error()));
        }
        while (level->size() > 1) {
            auto parent_level = write_parent_level(*level);
            if (!parent_level.has_value()) {
                return std::unexpected(std::move(parent_level.error()));
            }
            level = std::move(parent_level);
        }

        auto published = store_.publish_tree(
            level->front().page_id,
            static_cast<std::uint64_t>(sorted.size())
        );
        if (!published.has_value()) {
            return std::unexpected(storage_error(std::move(published.error())));
        }
        return {};
    }

private:
    [[nodiscard]]
    std::expected<bool, IndexError> fits(const btree_index::BTreePage & page) const
    {
        auto result = btree_index::BTreePageCodec::can_fit(page, store_.key_type());
        if (!result.has_value()) {
            return std::unexpected(codec_error(std::move(result.error())));
        }
        return *result;
    }

    [[nodiscard]]
    std::expected<std::vector<std::vector<btree_index::BTreeEntryKey>>, IndexError>
    partition_leaves(const std::vector<btree_index::BTreeEntryKey> & sorted) const
    {
        std::vector<std::vector<btree_index::BTreeEntryKey>> groups;
        std::vector<btree_index::BTreeEntryKey> current;
        for (const auto & entry : sorted) {
            auto trial_entries = current;
            trial_entries.push_back(entry);
            auto trial = build_leaf(
                std::numeric_limits<PageId>::max(),
                trial_entries
            );
            auto trial_fits = fits(btree_index::BTreePage {std::move(trial)});
            if (!trial_fits.has_value()) {
                return std::unexpected(std::move(trial_fits.error()));
            }
            if (!*trial_fits) {
                if (current.empty()) {
                    return std::unexpected(IndexError {
                        IndexErrorCode::InvalidKeyValue,
                        "BTreeIndex entry is too large to bulk load",
                    });
                }
                groups.push_back(std::move(current));
                current.clear();
                auto single = build_leaf(
                    std::numeric_limits<PageId>::max(),
                    std::vector<btree_index::BTreeEntryKey> {entry}
                );
                auto single_fits = fits(btree_index::BTreePage {std::move(single)});
                if (!single_fits.has_value()) {
                    return std::unexpected(std::move(single_fits.error()));
                }
                if (!*single_fits) {
                    return std::unexpected(IndexError {
                        IndexErrorCode::InvalidKeyValue,
                        "BTreeIndex entry is too large to bulk load",
                    });
                }
                current.push_back(entry);
            } else {
                current.push_back(entry);
            }
        }
        if (!current.empty()) {
            groups.push_back(std::move(current));
        }
        return groups;
    }

    [[nodiscard]]
    static btree_index::BTreeLeafPage build_leaf(
        PageId page_id,
        const std::vector<btree_index::BTreeEntryKey> & entries,
        PageId previous = btree_index::InvalidBTreePageId,
        PageId next = btree_index::InvalidBTreePageId
    )
    {
        btree_index::BTreeLeafPage leaf {page_id, previous, next};
        for (const auto & entry : entries) {
            (void) leaf.insert(entry);
        }
        return leaf;
    }

    [[nodiscard]]
    std::expected<std::vector<NodeRef>, IndexError> write_leaves(
        const std::vector<std::vector<btree_index::BTreeEntryKey>> & groups
    )
    {
        std::vector<PageId> page_ids;
        page_ids.reserve(groups.size());
        auto previous = btree_index::InvalidBTreePageId;
        for (std::size_t index = 0; index < groups.size(); ++index) {
            auto allocated = store_.allocate_leaf_page(previous);
            if (!allocated.has_value()) {
                return std::unexpected(storage_error(std::move(allocated.error())));
            }
            previous = allocated->page_id();
            page_ids.push_back(previous);
        }

        std::vector<NodeRef> level;
        level.reserve(groups.size());
        for (std::size_t index = 0; index < groups.size(); ++index) {
            const auto previous_id = index == 0
                ? btree_index::InvalidBTreePageId
                : page_ids[index - 1];
            const auto next_id = index + 1 == groups.size()
                ? btree_index::InvalidBTreePageId
                : page_ids[index + 1];
            auto leaf = build_leaf(page_ids[index], groups[index], previous_id, next_id);
            auto written = store_.write_page(btree_index::BTreePage {leaf});
            if (!written.has_value()) {
                return std::unexpected(storage_error(std::move(written.error())));
            }
            level.push_back(NodeRef {
                .page_id = page_ids[index],
                .minimum = groups[index].front(),
            });
        }
        return level;
    }

    [[nodiscard]]
    btree_index::BTreeInternalPage build_internal_trial(
        const std::vector<NodeRef> & children,
        std::size_t begin,
        std::size_t end,
        PageId page_id
    ) const
    {
        btree_index::BTreeInternalPage page {page_id, children[begin].page_id};
        auto left_child = children[begin].page_id;
        for (auto index = begin + 1; index < end; ++index) {
            (void) page.insert_child_after(
                left_child,
                children[index].minimum,
                children[index].page_id
            );
            left_child = children[index].page_id;
        }
        return page;
    }

    [[nodiscard]]
    std::expected<std::vector<NodeRef>, IndexError> write_parent_level(
        const std::vector<NodeRef> & children
    )
    {
        std::vector<std::pair<std::size_t, std::size_t>> groups;
        std::size_t begin = 0;
        while (begin < children.size()) {
            auto end = std::min(children.size(), begin + 2);
            while (end < children.size()) {
                auto trial = build_internal_trial(
                    children,
                    begin,
                    end + 1,
                    std::numeric_limits<PageId>::max()
                );
                auto trial_fits = fits(btree_index::BTreePage {std::move(trial)});
                if (!trial_fits.has_value()) {
                    return std::unexpected(std::move(trial_fits.error()));
                }
                if (!*trial_fits) {
                    break;
                }
                ++end;
            }
            if (children.size() - end == 1 && end - begin > 2) {
                --end;
            }
            groups.emplace_back(begin, end);
            begin = end;
        }

        std::vector<NodeRef> parents;
        parents.reserve(groups.size());
        for (const auto [group_begin, group_end] : groups) {
            auto allocated = store_.allocate_internal_page(children[group_begin].page_id);
            if (!allocated.has_value()) {
                return std::unexpected(storage_error(std::move(allocated.error())));
            }
            auto page = build_internal_trial(
                children,
                group_begin,
                group_end,
                allocated->page_id()
            );
            auto written = store_.write_page(btree_index::BTreePage {page});
            if (!written.has_value()) {
                return std::unexpected(storage_error(std::move(written.error())));
            }
            parents.push_back(NodeRef {
                .page_id = page.page_id(),
                .minimum = children[group_begin].minimum,
            });
        }
        return parents;
    }

private:
    btree_index::BTreePageStore & store_;
};

class BTreeRangeCursor final : public ScalarIndexCursor
{
public:
    BTreeRangeCursor(
        const btree_index::BTreePageStore & store,
        IndexRange range
    ) noexcept
        : store_(&store)
        , range_(std::move(range))
    {
    }

    std::expected<std::optional<common::RecordId>, IndexError> next() override
    {
        if (!initialized_) {
            auto initialized = initialize();
            if (!initialized.has_value()) {
                return std::unexpected(std::move(initialized.error()));
            }
            initialized_ = true;
        }
        while (!exhausted_) {
            if (entry_index_ < leaf_->entries().size()) {
                const auto & entry = leaf_->entries()[entry_index_++];
                if (range_.upper().has_value()) {
                    const auto compared = compare_scalar_index_keys(
                        entry.key,
                        range_.upper()->key
                    );
                    if (compared == std::strong_ordering::greater
                        || (compared == std::strong_ordering::equal
                            && !range_.upper()->inclusive)) {
                        exhausted_ = true;
                        return std::nullopt;
                    }
                }
                return std::optional<common::RecordId> {entry.record_id};
            }
            const auto next_page_id = leaf_->next_page_id();
            if (next_page_id == btree_index::InvalidBTreePageId) {
                exhausted_ = true;
                return std::nullopt;
            }
            if (!visited_pages_.insert(next_page_id).second) {
                return std::unexpected(corrupted_tree("cycle detected in the range leaf chain"));
            }
            auto next_page = store_->read_page(next_page_id);
            if (!next_page.has_value()) {
                return std::unexpected(storage_error(std::move(next_page.error())));
            }
            const auto * next_leaf = std::get_if<btree_index::BTreeLeafPage>(&*next_page);
            if (next_leaf == nullptr) {
                return std::unexpected(corrupted_tree("range leaf links to a non-leaf page"));
            }
            leaf_ = *next_leaf;
            entry_index_ = 0;
        }
        return std::nullopt;
    }

private:
    std::expected<void, IndexError> initialize()
    {
        if (range_.lower().has_value()
            && !range_.lower()->key.value().matches_type(store_->key_type())) {
            return std::unexpected(IndexError {
                IndexErrorCode::KeyTypeMismatch,
                "Lower range key type does not match BTreeIndex key type",
            });
        }
        if (range_.upper().has_value()
            && !range_.upper()->key.value().matches_type(store_->key_type())) {
            return std::unexpected(IndexError {
                IndexErrorCode::KeyTypeMismatch,
                "Upper range key type does not match BTreeIndex key type",
            });
        }
        if (range_.lower().has_value() && range_.upper().has_value()) {
            const auto compared = compare_scalar_index_keys(
                range_.lower()->key,
                range_.upper()->key
            );
            if (compared == std::strong_ordering::greater
                || (compared == std::strong_ordering::equal
                    && (!range_.lower()->inclusive || !range_.upper()->inclusive))) {
                exhausted_ = true;
                return {};
            }
        }
        if (store_->root_page_id() == btree_index::InvalidBTreePageId) {
            exhausted_ = true;
            return {};
        }

        std::optional<btree_index::BTreeEntryKey> first_entry;
        if (range_.lower().has_value()) {
            first_entry = btree_index::BTreeEntryKey {
                .key = range_.lower()->key,
                .record_id = range_.lower()->inclusive
                    ? std::numeric_limits<common::RecordId>::min()
                    : std::numeric_limits<common::RecordId>::max(),
            };
        }
        auto page_id = store_->root_page_id();
        while (true) {
            if (!visited_pages_.insert(page_id).second) {
                return std::unexpected(corrupted_tree("cycle detected while locating the range start"));
            }
            auto page = store_->read_page(page_id);
            if (!page.has_value()) {
                return std::unexpected(storage_error(std::move(page.error())));
            }
            if (const auto * found_leaf = std::get_if<btree_index::BTreeLeafPage>(&*page)) {
                leaf_ = *found_leaf;
                entry_index_ = range_.lower().has_value()
                    ? (range_.lower()->inclusive
                        ? leaf_->lower_bound(range_.lower()->key)
                        : leaf_->upper_bound(range_.lower()->key))
                    : 0;
                return {};
            }
            const auto & internal = std::get<btree_index::BTreeInternalPage>(*page);
            page_id = first_entry.has_value()
                ? internal.child_for(*first_entry)
                : internal.first_child_id();
        }
    }

private:
    const btree_index::BTreePageStore * store_;
    IndexRange range_;
    std::optional<btree_index::BTreeLeafPage> leaf_;
    std::size_t entry_index_ {0};
    bool initialized_ {false};
    bool exhausted_ {false};
    std::unordered_set<btree_index::BTreePageId> visited_pages_;
};

class BTreeEraser final
{
private:
    using PageId = btree_index::BTreePageId;
    using SearchPath = std::vector<PageId>;

public:
    explicit BTreeEraser(btree_index::BTreePageStore & store) noexcept
        : store_(store)
    {
    }

    [[nodiscard]]
    std::expected<void, IndexError> erase(const btree_index::BTreeEntryKey & entry)
    {
        SearchPath path;
        auto leaf = find_leaf(entry, path);
        if (!leaf.has_value()) {
            return std::unexpected(std::move(leaf.error()));
        }

        const auto position = leaf->lower_bound(entry);
        if (position >= leaf->entries().size() ||
            compare_btree_entry_keys(leaf->entries()[position], entry) !=
                std::strong_ordering::equal) {
            return std::unexpected(corrupted_tree("validated erase entry is absent from its routed leaf"));
        }
        const auto minimum_changed = position == 0;
        if (!leaf->erase(entry)) {
            return std::unexpected(corrupted_tree("leaf rejected a validated erase entry"));
        }

        if (!leaf->empty()) {
            auto written = store_.write_page(btree_index::BTreePage {*leaf});
            if (!written.has_value()) {
                return std::unexpected(storage_error(std::move(written.error())));
            }
            if (minimum_changed) {
                auto updated = update_ancestor_minimum(
                    leaf->page_id(), leaf->entries().front(), path
                );
                if (!updated.has_value()) {
                    return std::unexpected(std::move(updated.error()));
                }
            }
        } else if (path.empty()) {
            auto emptied = store_.set_root_page_id(btree_index::InvalidBTreePageId);
            if (!emptied.has_value()) {
                return std::unexpected(storage_error(std::move(emptied.error())));
            }
            auto released = store_.release_page(leaf->page_id());
            if (!released.has_value()) {
                return std::unexpected(storage_error(std::move(released.error())));
            }
        } else {
            auto unlinked = unlink_leaf(*leaf);
            if (!unlinked.has_value()) {
                return std::unexpected(std::move(unlinked.error()));
            }
            auto pruned = prune_empty_child(leaf->page_id(), path);
            if (!pruned.has_value()) {
                return std::unexpected(std::move(pruned.error()));
            }
            auto released = store_.release_page(leaf->page_id());
            if (!released.has_value()) {
                return std::unexpected(storage_error(std::move(released.error())));
            }
        }

        auto counted = store_.set_entry_count(store_.entry_count() - 1);
        if (!counted.has_value()) {
            return std::unexpected(storage_error(std::move(counted.error())));
        }
        return {};
    }

private:
    [[nodiscard]]
    std::expected<btree_index::BTreeLeafPage, IndexError> find_leaf(
        const btree_index::BTreeEntryKey & entry,
        SearchPath & path
    ) const
    {
        auto page_id = store_.root_page_id();
        std::unordered_set<PageId> visited;
        while (true) {
            if (!visited.insert(page_id).second) {
                return std::unexpected(corrupted_tree("cycle detected while locating an erase leaf"));
            }
            auto page = store_.read_page(page_id);
            if (!page.has_value()) {
                return std::unexpected(storage_error(std::move(page.error())));
            }
            if (auto * leaf = std::get_if<btree_index::BTreeLeafPage>(&*page)) {
                return std::move(*leaf);
            }
            const auto & internal = std::get<btree_index::BTreeInternalPage>(*page);
            path.push_back(internal.page_id());
            page_id = internal.child_for(entry);
        }
    }

    [[nodiscard]]
    std::expected<void, IndexError> unlink_leaf(
        const btree_index::BTreeLeafPage & leaf
    )
    {
        if (leaf.previous_page_id() == leaf.page_id() ||
            leaf.next_page_id() == leaf.page_id() ||
            (leaf.previous_page_id() != btree_index::InvalidBTreePageId &&
             leaf.previous_page_id() == leaf.next_page_id())) {
            return std::unexpected(corrupted_tree("empty leaf has invalid sibling links"));
        }

        if (leaf.previous_page_id() != btree_index::InvalidBTreePageId) {
            auto previous_page = store_.read_page(leaf.previous_page_id());
            if (!previous_page.has_value()) {
                return std::unexpected(storage_error(std::move(previous_page.error())));
            }
            auto * previous = std::get_if<btree_index::BTreeLeafPage>(&*previous_page);
            if (previous == nullptr || previous->next_page_id() != leaf.page_id()) {
                return std::unexpected(corrupted_tree("empty leaf previous link is not reciprocal"));
            }
            previous->set_next_page_id(leaf.next_page_id());
            auto written = store_.write_page(*previous_page);
            if (!written.has_value()) {
                return std::unexpected(storage_error(std::move(written.error())));
            }
        }

        if (leaf.next_page_id() != btree_index::InvalidBTreePageId) {
            auto next_page = store_.read_page(leaf.next_page_id());
            if (!next_page.has_value()) {
                return std::unexpected(storage_error(std::move(next_page.error())));
            }
            auto * next = std::get_if<btree_index::BTreeLeafPage>(&*next_page);
            if (next == nullptr || next->previous_page_id() != leaf.page_id()) {
                return std::unexpected(corrupted_tree("empty leaf next link is not reciprocal"));
            }
            next->set_previous_page_id(leaf.previous_page_id());
            auto written = store_.write_page(*next_page);
            if (!written.has_value()) {
                return std::unexpected(storage_error(std::move(written.error())));
            }
        }
        return {};
    }

    [[nodiscard]]
    std::expected<void, IndexError> update_ancestor_minimum(
        PageId child_page_id,
        const btree_index::BTreeEntryKey & minimum,
        SearchPath & path
    )
    {
        while (!path.empty()) {
            const auto parent_page_id = path.back();
            path.pop_back();
            auto parent_page = store_.read_page(parent_page_id);
            if (!parent_page.has_value()) {
                return std::unexpected(storage_error(std::move(parent_page.error())));
            }
            auto * parent = std::get_if<btree_index::BTreeInternalPage>(&*parent_page);
            if (parent == nullptr) {
                return std::unexpected(corrupted_tree("erase path contains a non-internal parent page"));
            }
            const auto child_position = parent->find_child(child_page_id);
            if (!child_position.has_value()) {
                return std::unexpected(corrupted_tree("erase path parent does not reference its child"));
            }
            if (*child_position == 0) {
                child_page_id = parent->page_id();
                continue;
            }
            if (!parent->replace_separator(child_page_id, minimum)) {
                return std::unexpected(corrupted_tree("parent rejected an updated subtree minimum"));
            }
            auto written = store_.write_page(*parent_page);
            if (!written.has_value()) {
                return std::unexpected(storage_error(std::move(written.error())));
            }
            return {};
        }
        return {};
    }

    [[nodiscard]]
    std::expected<void, IndexError> prune_empty_child(
        PageId child_page_id,
        SearchPath & path
    )
    {
        std::vector<PageId> orphaned_internal_pages;
        const auto release_orphans = [&]() -> std::expected<void, IndexError> {
            for (const auto page_id : orphaned_internal_pages) {
                auto released = store_.release_page(page_id);
                if (!released.has_value()) {
                    return std::unexpected(storage_error(std::move(released.error())));
                }
            }
            return {};
        };

        while (!path.empty()) {
            const auto parent_page_id = path.back();
            path.pop_back();
            auto parent_page = store_.read_page(parent_page_id);
            if (!parent_page.has_value()) {
                return std::unexpected(storage_error(std::move(parent_page.error())));
            }
            auto * parent = std::get_if<btree_index::BTreeInternalPage>(&*parent_page);
            if (parent == nullptr) {
                return std::unexpected(corrupted_tree("empty-child path contains a non-internal parent"));
            }
            const auto child_position = parent->find_child(child_page_id);
            if (!child_position.has_value()) {
                return std::unexpected(corrupted_tree("empty-child parent does not reference its child"));
            }

            if (parent->child_count() == 1) {
                if (path.empty()) {
                    auto emptied = store_.set_root_page_id(btree_index::InvalidBTreePageId);
                    if (!emptied.has_value()) {
                        return std::unexpected(storage_error(std::move(emptied.error())));
                    }
                    orphaned_internal_pages.push_back(parent->page_id());
                    return release_orphans();
                }
                orphaned_internal_pages.push_back(parent->page_id());
                child_page_id = parent->page_id();
                continue;
            }

            std::optional<btree_index::BTreeEntryKey> replacement_minimum;
            if (*child_position == 0) {
                replacement_minimum = parent->entries().front().separator;
            }
            if (!parent->erase_child(child_page_id)) {
                return std::unexpected(corrupted_tree("parent rejected removal of an empty child"));
            }

            if (path.empty() && parent->child_count() == 1) {
                const auto new_root_page_id = parent->child_at(0);
                if (!new_root_page_id.has_value()) {
                    return std::unexpected(corrupted_tree("collapsed root has no remaining child"));
                }
                auto rooted = store_.set_root_page_id(*new_root_page_id);
                if (!rooted.has_value()) {
                    return std::unexpected(storage_error(std::move(rooted.error())));
                }
                orphaned_internal_pages.push_back(parent->page_id());
                return release_orphans();
            }

            auto written = store_.write_page(*parent_page);
            if (!written.has_value()) {
                return std::unexpected(storage_error(std::move(written.error())));
            }
            if (replacement_minimum.has_value()) {
                auto updated = update_ancestor_minimum(
                    parent->page_id(), *replacement_minimum, path
                );
                if (!updated.has_value()) {
                    return std::unexpected(std::move(updated.error()));
                }
            }
            return release_orphans();
        }
        return std::unexpected(corrupted_tree("empty non-root leaf has no parent path"));
    }

private:
    btree_index::BTreePageStore & store_;
};

} // namespace

BTreeIndex::BTreeIndex(btree_index::BTreePageStore store) noexcept
    : store_(std::move(store))
{
}

std::expected<BTreeIndex, btree_index::BTreePageStoreError> BTreeIndex::create(
    std::filesystem::path path,
    common::IndexId index_id,
    common::LogicalType key_type,
    filesystem::FileSystem & filesystem
)
{
    auto store = btree_index::BTreePageStore::create(
        std::move(path),
        index_id,
        std::move(key_type),
        filesystem
    );
    if (!store.has_value()) {
        return std::unexpected(std::move(store.error()));
    }
    return BTreeIndex {std::move(store.value())};
}

std::expected<BTreeIndex, btree_index::BTreePageStoreError> BTreeIndex::open(
    std::filesystem::path path,
    common::IndexId expected_index_id,
    common::LogicalType expected_key_type,
    filesystem::FileSystem & filesystem
)
{
    auto store = btree_index::BTreePageStore::open(
        std::move(path),
        expected_index_id,
        std::move(expected_key_type),
        filesystem
    );
    if (!store.has_value()) {
        return std::unexpected(std::move(store.error()));
    }
    return BTreeIndex {std::move(store.value())};
}

IndexKind BTreeIndex::kind() const noexcept
{
    return IndexKind::BTree;
}

std::expected<void, IndexError> BTreeIndex::insert(
    const ScalarIndexKey & key,
    common::RecordId record_id
)
{
    if (!key.value().matches_type(store_.key_type())) {
        return std::unexpected(IndexError {
            IndexErrorCode::KeyTypeMismatch,
            "Index key type does not match BTreeIndex key type",
        });
    }
    return BTreeInserter {store_}.insert(btree_index::BTreeEntryKey {
        .key = key,
        .record_id = record_id,
    });
}

std::expected<void, IndexError> BTreeIndex::erase(
    const ScalarIndexKey & key,
    common::RecordId record_id
)
{
    if (!key.value().matches_type(store_.key_type())) {
        return std::unexpected(IndexError {
            IndexErrorCode::KeyTypeMismatch,
            "Index key type does not match BTreeIndex key type",
        });
    }

    auto records = find_equal(key);
    if (!records.has_value()) {
        return std::unexpected(std::move(records.error()));
    }
    if (records->empty()) {
        return std::unexpected(IndexError {
            IndexErrorCode::KeyNotFound,
            "BTreeIndex key not found",
        });
    }
    if (std::find(records->begin(), records->end(), record_id) == records->end()) {
        return std::unexpected(IndexError {
            IndexErrorCode::RecordNotFound,
            "Record id not found for BTreeIndex key",
        });
    }

    return BTreeEraser {store_}.erase(btree_index::BTreeEntryKey {
        .key = key,
        .record_id = record_id,
    });
}

std::expected<std::vector<common::RecordId>, IndexError> BTreeIndex::find_equal(
    const ScalarIndexKey & key
) const
{
    if (!key.value().matches_type(store_.key_type())) {
        return std::unexpected(IndexError {
            IndexErrorCode::KeyTypeMismatch,
            "Index key type does not match BTreeIndex key type",
        });
    }

    if (store_.root_page_id() == btree_index::InvalidBTreePageId) {
        return std::vector<common::RecordId> {};
    }

    const btree_index::BTreeEntryKey first_entry {
        .key = key,
        .record_id = std::numeric_limits<common::RecordId>::min(),
    };
    auto page_id = store_.root_page_id();
    std::unordered_set<btree_index::BTreePageId> visited_pages;

    while (true) {
        if (auto [_, inserted] = visited_pages.insert(page_id); !inserted) {
            return std::unexpected(corrupted_tree("cycle detected while descending to a leaf page"));
        }

        auto page = store_.read_page(page_id);
        if (!page.has_value()) {
            return std::unexpected(storage_error(std::move(page.error())));
        }
        if (const auto * leaf = std::get_if<btree_index::BTreeLeafPage>(&*page)) {
            std::vector<common::RecordId> records;
            auto current_leaf = *leaf;

            while (true) {
                const auto begin = current_leaf.lower_bound(key);
                for (std::size_t index = begin; index < current_leaf.entries().size(); ++index) {
                    const auto & entry = current_leaf.entries()[index];
                    const auto compared = compare_scalar_index_keys(entry.key, key);
                    if (compared == std::strong_ordering::greater) {
                        return records;
                    }
                    if (compared == std::strong_ordering::equal) {
                        records.push_back(entry.record_id);
                    }
                }

                const auto next_page_id = current_leaf.next_page_id();
                if (next_page_id == btree_index::InvalidBTreePageId) {
                    return records;
                }
                if (!visited_pages.insert(next_page_id).second) {
                    return std::unexpected(corrupted_tree("cycle detected in the leaf page chain"));
                }

                auto next_page = store_.read_page(next_page_id);
                if (!next_page.has_value()) {
                    return std::unexpected(storage_error(std::move(next_page.error())));
                }
                const auto * next_leaf = std::get_if<btree_index::BTreeLeafPage>(&*next_page);
                if (next_leaf == nullptr) {
                    return std::unexpected(corrupted_tree("leaf page links to a non-leaf page"));
                }
                current_leaf = *next_leaf;
            }
        }

        const auto & internal = std::get<btree_index::BTreeInternalPage>(*page);
        page_id = internal.child_for(first_entry);
    }
}

std::expected<std::vector<common::RecordId>, IndexError> BTreeIndex::scan_range(
    const IndexRange & range
) const
{
    if (range.lower().has_value() &&
        !range.lower()->key.value().matches_type(store_.key_type())) {
        return std::unexpected(IndexError {
            IndexErrorCode::KeyTypeMismatch,
            "Lower range key type does not match BTreeIndex key type",
        });
    }
    if (range.upper().has_value() &&
        !range.upper()->key.value().matches_type(store_.key_type())) {
        return std::unexpected(IndexError {
            IndexErrorCode::KeyTypeMismatch,
            "Upper range key type does not match BTreeIndex key type",
        });
    }
    if (range.lower().has_value() && range.upper().has_value()) {
        const auto compared = compare_scalar_index_keys(
            range.lower()->key,
            range.upper()->key
        );
        if (compared == std::strong_ordering::greater ||
            (compared == std::strong_ordering::equal &&
             (!range.lower()->inclusive || !range.upper()->inclusive))) {
            return std::vector<common::RecordId> {};
        }
    }
    if (store_.root_page_id() == btree_index::InvalidBTreePageId) {
        return std::vector<common::RecordId> {};
    }

    std::optional<btree_index::BTreeEntryKey> first_entry;
    if (range.lower().has_value()) {
        first_entry = btree_index::BTreeEntryKey {
            .key = range.lower()->key,
            .record_id = range.lower()->inclusive
                ? std::numeric_limits<common::RecordId>::min()
                : std::numeric_limits<common::RecordId>::max(),
        };
    }

    auto page_id = store_.root_page_id();
    std::unordered_set<btree_index::BTreePageId> visited_pages;
    while (true) {
        if (!visited_pages.insert(page_id).second) {
            return std::unexpected(corrupted_tree("cycle detected while locating the range start"));
        }

        auto page = store_.read_page(page_id);
        if (!page.has_value()) {
            return std::unexpected(storage_error(std::move(page.error())));
        }
        if (const auto * leaf = std::get_if<btree_index::BTreeLeafPage>(&*page)) {
            std::vector<common::RecordId> records;
            auto current_leaf = *leaf;
            while (true) {
                std::size_t begin = 0;
                if (range.lower().has_value()) {
                    begin = range.lower()->inclusive
                        ? current_leaf.lower_bound(range.lower()->key)
                        : current_leaf.upper_bound(range.lower()->key);
                }

                for (auto index = begin; index < current_leaf.entries().size(); ++index) {
                    const auto & entry = current_leaf.entries()[index];
                    if (range.upper().has_value()) {
                        const auto compared = compare_scalar_index_keys(
                            entry.key,
                            range.upper()->key
                        );
                        if (compared == std::strong_ordering::greater ||
                            (compared == std::strong_ordering::equal &&
                             !range.upper()->inclusive)) {
                            return records;
                        }
                    }
                    records.push_back(entry.record_id);
                }

                const auto next_page_id = current_leaf.next_page_id();
                if (next_page_id == btree_index::InvalidBTreePageId) {
                    return records;
                }
                if (!visited_pages.insert(next_page_id).second) {
                    return std::unexpected(corrupted_tree("cycle detected in the range leaf chain"));
                }

                auto next_page = store_.read_page(next_page_id);
                if (!next_page.has_value()) {
                    return std::unexpected(storage_error(std::move(next_page.error())));
                }
                const auto * next_leaf = std::get_if<btree_index::BTreeLeafPage>(&*next_page);
                if (next_leaf == nullptr) {
                    return std::unexpected(corrupted_tree("range leaf links to a non-leaf page"));
                }
                current_leaf = *next_leaf;
            }
        }

        const auto & internal = std::get<btree_index::BTreeInternalPage>(*page);
        page_id = first_entry.has_value()
            ? internal.child_for(*first_entry)
            : internal.first_child_id();
    }
}

std::expected<std::unique_ptr<ScalarIndexCursor>, IndexError>
BTreeIndex::scan_range_cursor(const IndexRange & range) const
{
    return std::unique_ptr<ScalarIndexCursor> {
        std::make_unique<BTreeRangeCursor>(store_, range)
    };
}

std::expected<void, IndexError> BTreeIndex::bulk_load(std::vector<ScalarIndexEntry> entries)
{
    return BTreeBulkLoader {store_}.load(std::move(entries));
}

std::size_t BTreeIndex::size() const noexcept
{
    return static_cast<std::size_t>(store_.entry_count());
}

const std::filesystem::path & BTreeIndex::path() const noexcept
{
    return store_.path();
}

common::IndexId BTreeIndex::index_id() const noexcept
{
    return store_.index_id();
}

const common::LogicalType & BTreeIndex::key_type() const noexcept
{
    return store_.key_type();
}

btree_index::BTreePageId BTreeIndex::root_page_id() const noexcept
{
    return store_.root_page_id();
}

std::uint64_t BTreeIndex::page_count() const noexcept
{
    return store_.page_count();
}

std::uint64_t BTreeIndex::free_page_count() const noexcept
{
    return store_.free_page_count();
}

std::uint64_t BTreeIndex::entry_count() const noexcept
{
    return store_.entry_count();
}

} // namespace litedb::core::index
