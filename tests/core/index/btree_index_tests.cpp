#include "core/index/btree_index/btree_index.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "core/filesystem/platform_filesystem.hpp"
#include "core/index/index_store.hpp"
#include "core/common/value.hpp"

namespace
{

using namespace litedb::core;
using namespace litedb::core::index;

static_assert(std::is_base_of_v<OrderedScalarIndex, BTreeIndex>);

void require(bool condition, const std::string & message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path make_temp_directory()
{
    return std::filesystem::temp_directory_path() /
        ("litedb-btree-index-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

ScalarIndexKey key(std::int32_t value)
{
    auto result = ScalarIndexKey::from_value(common::Value {value});
    if (!result.has_value()) {
        throw std::runtime_error("failed to create scalar index key");
    }
    return std::move(*result);
}

ScalarIndexKey bigint_key(std::int64_t value)
{
    auto result = ScalarIndexKey::from_value(common::Value {value});
    if (!result.has_value()) {
        throw std::runtime_error("failed to create bigint scalar index key");
    }
    return std::move(*result);
}

ScalarIndexKey varchar_key(std::string value)
{
    auto result = ScalarIndexKey::from_value(common::Value {std::move(value)});
    if (!result.has_value()) {
        throw std::runtime_error("failed to create varchar scalar index key");
    }
    return std::move(*result);
}

ScalarIndexKey padded_varchar_key(std::size_t value, std::size_t length)
{
    auto prefix = std::to_string(value);
    require(prefix.size() <= 8 && length >= 8, "invalid padded varchar test key size");
    prefix.insert(prefix.begin(), 8 - prefix.size(), '0');
    prefix.append(length - prefix.size(), 'x');
    return varchar_key(std::move(prefix));
}

btree_index::BTreeEntryKey entry(std::int32_t value, common::RecordId record_id)
{
    return btree_index::BTreeEntryKey {
        .key = key(value),
        .record_id = record_id,
    };
}

void test_create_and_open_owns_page_store(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "42.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();

    {
        auto created = BTreeIndex::create(path, 42, type, filesystem);
        require(created.has_value(), "BTreeIndex create failed");
        require(created->path() == path, "BTreeIndex path mismatch");
        require(created->index_id() == 42, "BTreeIndex id mismatch");
        require(created->key_type().id == type.id &&
                created->key_type().parameter == type.parameter,
                "BTreeIndex key type mismatch");
        require(created->root_page_id() == btree_index::InvalidBTreePageId,
                "new BTreeIndex should have no root");
        require(created->page_count() == 0, "new BTreeIndex page count mismatch");
        require(created->entry_count() == 0, "new BTreeIndex entry count mismatch");
    }

    auto opened = BTreeIndex::open(path, 42, type, filesystem);
    require(opened.has_value(), "BTreeIndex open failed");
    require(opened->index_id() == 42, "opened BTreeIndex id mismatch");
    require(opened->root_page_id() == btree_index::InvalidBTreePageId,
            "opened BTreeIndex root mismatch");
}

void test_insert_simple_duplicate_and_reopen(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "43.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();
    const auto one = key(1);
    {
        auto created = BTreeIndex::create(path, 43, type, filesystem);
        require(created.has_value(), "operation framework BTreeIndex create failed");

        auto index = std::move(*created);
        require(index.kind() == IndexKind::BTree, "BTreeIndex kind mismatch");
        require(index.size() == 0, "new BTreeIndex size mismatch");

        const auto inserted = index.insert(one, 10);
        require(inserted.has_value(), "first BTreeIndex insert failed");
        require(index.size() == 1 && index.entry_count() == 1,
                "first BTreeIndex insert count mismatch");
        require(index.root_page_id() != btree_index::InvalidBTreePageId,
                "first BTreeIndex insert should create a root leaf");

        const auto duplicate = index.insert(one, 10);
        require(!duplicate.has_value() && duplicate.error().is(IndexErrorCode::DuplicateEntry),
                "duplicate BTreeIndex entry should be rejected");
        require(index.insert(one, 11).has_value(),
                "same BTreeIndex key with a different record id should succeed");

        const auto found = index.find_equal(one);
        require(found.has_value() && *found == std::vector<common::RecordId>({10, 11}),
                "simple inserted entries lookup mismatch");

        const auto mismatched = index.insert(bigint_key(1), 12);
        require(!mismatched.has_value() && mismatched.error().is(IndexErrorCode::KeyTypeMismatch),
                "mismatched insert key type should be rejected");

        require(index.erase(one, 10).has_value(), "simple BTreeIndex erase failed");
        require(index.entry_count() == 1, "simple BTreeIndex erase count mismatch");
        const auto missing_record = index.erase(one, 10);
        require(!missing_record.has_value() &&
                missing_record.error().is(IndexErrorCode::RecordNotFound),
                "missing BTreeIndex record id should report RecordNotFound");
        const auto missing_key = index.erase(key(2), 10);
        require(!missing_key.has_value() &&
                missing_key.error().is(IndexErrorCode::KeyNotFound),
                "missing BTreeIndex key should report KeyNotFound");
        const auto mismatched_erase = index.erase(bigint_key(1), 11);
        require(!mismatched_erase.has_value() &&
                mismatched_erase.error().is(IndexErrorCode::KeyTypeMismatch),
                "mismatched BTreeIndex erase key should be rejected");

        const auto range = index.scan_range(IndexRange::all());
        require(range.has_value() && *range == std::vector<common::RecordId>({11}),
                "simple BTreeIndex full range mismatch");
    }

    auto reopened = BTreeIndex::open(path, 43, type, filesystem);
    require(reopened.has_value(), "simple inserted BTreeIndex reopen failed");
    require(reopened->entry_count() == 1, "reopened simple BTreeIndex count mismatch");
    const auto reopened_found = reopened->find_equal(one);
    require(reopened_found.has_value() && *reopened_found == std::vector<common::RecordId>({11}),
            "reopened simple BTreeIndex lookup mismatch");
}

void test_insert_rejects_oversized_entry(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "46.bti";
    const common::LogicalType type {common::LogicalTypeId::Varchar, 5000};
    auto filesystem = filesystem::create_platform_filesystem();
    auto created = BTreeIndex::create(path, 46, type, filesystem);
    require(created.has_value(), "oversized-entry BTreeIndex create failed");

    const auto inserted = created->insert(varchar_key(std::string(5000, 'x')), 1);
    require(!inserted.has_value() && inserted.error().is(IndexErrorCode::InvalidKeyValue),
            "oversized BTreeIndex entry should be rejected");
    require(created->root_page_id() == btree_index::InvalidBTreePageId &&
            created->page_count() == 0 && created->entry_count() == 0,
            "oversized entry rejection should not mutate an empty BTreeIndex");
}

void test_insert_splits_leaf_and_preserves_duplicate_order(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "47.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();

    {
        auto created = BTreeIndex::create(path, 47, type, filesystem);
        require(created.has_value(), "leaf-split BTreeIndex create failed");
        for (common::RecordId record_id = 600; record_id > 0; --record_id) {
            auto inserted = created->insert(key(20), record_id - 1);
            require(inserted.has_value(), "leaf-split duplicate-key insert failed");
        }
        require(created->entry_count() == 600, "leaf-split entry count mismatch");
        require(created->page_count() > 2, "leaf-split insert should create multiple pages");

        const auto found = created->find_equal(key(20));
        require(found.has_value() && found->size() == 600,
                "leaf-split duplicate-key lookup size mismatch");
        for (common::RecordId record_id = 0; record_id < 600; ++record_id) {
            require((*found)[record_id] == record_id,
                    "leaf-split duplicate-key lookup order mismatch");
        }

        const auto closed = created->scan_range(IndexRange::closed(key(20), key(20)));
        require(closed.has_value() && *closed == *found,
                "leaf-split closed range should include duplicate keys across leaves");
        const auto exclusive = created->scan_range(IndexRange::lower_bound(key(20), false));
        require(exclusive.has_value() && exclusive->empty(),
                "exclusive lower range should skip all duplicate lower-bound keys");

        require(created->erase(key(20), 0).has_value(),
                "leftmost duplicate-key erase failed");
        require(created->erase(key(20), 300).has_value(),
                "middle duplicate-key erase failed");
        require(created->erase(key(20), 599).has_value(),
                "rightmost duplicate-key erase failed");
        const auto after_erase = created->find_equal(key(20));
        require(after_erase.has_value() && after_erase->size() == 597,
                "duplicate-key erase lookup size mismatch");
        require(after_erase->front() == 1 && (*after_erase)[299] == 301 &&
                after_erase->back() == 598,
                "duplicate-key erase removed the wrong records");
    }

    auto reopened = BTreeIndex::open(path, 47, type, filesystem);
    require(reopened.has_value(), "leaf-split BTreeIndex reopen failed");
    const auto found = reopened->find_equal(key(20));
    require(found.has_value() && found->size() == 597,
            "reopened leaf-split duplicate-key lookup mismatch");
}

void test_insert_recursively_splits_internal_pages(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "48.bti";
    constexpr std::size_t KeyLength = 512;
    constexpr std::size_t EntryCount = 100;
    const common::LogicalType type {common::LogicalTypeId::Varchar, KeyLength};
    auto filesystem = filesystem::create_platform_filesystem();

    {
        auto created = BTreeIndex::create(path, 48, type, filesystem);
        require(created.has_value(), "internal-split BTreeIndex create failed");
        for (std::size_t value = EntryCount; value > 0; --value) {
            auto inserted = created->insert(
                padded_varchar_key(value - 1, KeyLength), value - 1 + 1000
            );
            require(inserted.has_value(), "internal-split BTreeIndex insert failed");
        }
        require(created->entry_count() == EntryCount,
                "internal-split BTreeIndex entry count mismatch");
        require(created->page_count() > 20,
                "internal-split workload should create more pages than one root can reference");

        for (const auto value : {std::size_t {0}, std::size_t {49}, std::size_t {99}}) {
            const auto found = created->find_equal(padded_varchar_key(value, KeyLength));
            require(found.has_value() && *found == std::vector<common::RecordId>({value + 1000}),
                    "internal-split BTreeIndex lookup mismatch");
        }
    }

    auto reopened = BTreeIndex::open(path, 48, type, filesystem);
    require(reopened.has_value(), "internal-split BTreeIndex reopen failed");
    require(reopened->entry_count() == EntryCount,
            "reopened internal-split BTreeIndex entry count mismatch");
    const auto found = reopened->find_equal(padded_varchar_key(75, KeyLength));
    require(found.has_value() && *found == std::vector<common::RecordId>({1075}),
            "reopened internal-split BTreeIndex lookup mismatch");
}

void test_bulk_load_builds_sorted_persistent_tree(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "53.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();
    constexpr std::int32_t EntryCount = 3000;

    {
        auto created = BTreeIndex::create(path, 53, type, filesystem);
        require(created.has_value(), "bulk-load BTreeIndex create failed");
        std::vector<ScalarIndexEntry> entries;
        entries.reserve(EntryCount);
        for (std::int32_t value = EntryCount - 1; value >= 0; --value) {
            entries.push_back({
                .key = key(value),
                .record_id = static_cast<common::RecordId>(value + 5000),
            });
        }
        auto loaded = created->bulk_load(std::move(entries));
        require(loaded.has_value(), "bulk-load BTreeIndex build failed");
        require(created->entry_count() == EntryCount, "bulk-load entry count mismatch");
        const auto range = created->scan_range(IndexRange::closed(key(100), key(102)));
        require(range.has_value()
                    && *range == std::vector<common::RecordId>({5100, 5101, 5102}),
                "bulk-load range lookup mismatch");
        auto cursor = created->scan_range_cursor(IndexRange::closed(key(100), key(102)));
        require(cursor.has_value(), "bulk-load range cursor create failed");
        for (const auto expected : {common::RecordId {5100}, common::RecordId {5101},
                                    common::RecordId {5102}}) {
            auto next = (*cursor)->next();
            require(next.has_value() && next->has_value() && **next == expected,
                    "bulk-load range cursor result mismatch");
        }
        auto exhausted = (*cursor)->next();
        require(exhausted.has_value() && !exhausted->has_value(),
                "bulk-load range cursor should be exhausted");
    }

    auto reopened = BTreeIndex::open(path, 53, type, filesystem);
    require(reopened.has_value(), "bulk-load BTreeIndex reopen failed");
    const auto found = reopened->find_equal(key(2999));
    require(found.has_value()
                && *found == std::vector<common::RecordId>({7999}),
            "reopened bulk-load lookup mismatch");
}

void test_find_equal_on_empty_tree(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "44.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();
    auto created = BTreeIndex::create(path, 44, type, filesystem);
    require(created.has_value(), "empty-tree BTreeIndex create failed");

    const auto found = created->find_equal(key(10));
    require(found.has_value() && found->empty(), "empty tree should return no records");
    const auto range = created->scan_range(IndexRange::all());
    require(range.has_value() && range->empty(), "empty tree range should return no records");
}

void test_find_equal_routes_and_scans_duplicate_keys(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "45.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();

    {
        auto created = btree_index::BTreePageStore::create(path, 45, type, filesystem);
        require(created.has_value(), "find-equal page store create failed");
        auto store = std::move(*created);

        auto first = store.allocate_leaf_page();
        require(first.has_value(), "first find-equal leaf allocation failed");
        require(first->insert(entry(10, 100)), "first find-equal entry insert failed");
        require(first->insert(entry(20, 1)), "first duplicate entry insert failed");

        auto second = store.allocate_leaf_page(first->page_id());
        require(second.has_value(), "second find-equal leaf allocation failed");
        require(second->insert(entry(20, 2)), "second duplicate entry insert failed");
        require(second->insert(entry(20, 3)), "third duplicate entry insert failed");

        auto third = store.allocate_leaf_page(second->page_id());
        require(third.has_value(), "third find-equal leaf allocation failed");
        require(third->insert(entry(20, 4)), "fourth duplicate entry insert failed");
        require(third->insert(entry(30, 300)), "last find-equal entry insert failed");

        first->set_next_page_id(second->page_id());
        second->set_next_page_id(third->page_id());
        require(store.write_page(btree_index::BTreePage {*first}).has_value(),
                "first find-equal leaf write failed");
        require(store.write_page(btree_index::BTreePage {*second}).has_value(),
                "second find-equal leaf write failed");
        require(store.write_page(btree_index::BTreePage {*third}).has_value(),
                "third find-equal leaf write failed");

        auto branch = store.allocate_internal_page(first->page_id());
        require(branch.has_value(), "find-equal branch allocation failed");
        require(branch->insert_child_after(first->page_id(), entry(20, 2), second->page_id()),
                "second leaf separator insert failed");
        require(branch->insert_child_after(second->page_id(), entry(20, 4), third->page_id()),
                "third leaf separator insert failed");
        require(store.write_page(btree_index::BTreePage {*branch}).has_value(),
                "find-equal branch write failed");

        auto root = store.allocate_internal_page(branch->page_id());
        require(root.has_value(), "find-equal root allocation failed");
        require(store.write_page(btree_index::BTreePage {*root}).has_value(),
                "find-equal root write failed");
        require(store.set_root_page_id(root->page_id()).has_value(),
                "find-equal root metadata update failed");
        require(store.set_entry_count(6).has_value(),
                "find-equal entry count update failed");
    }

    auto opened = BTreeIndex::open(path, 45, type, filesystem);
    require(opened.has_value(), "find-equal BTreeIndex open failed");

    const auto duplicates = opened->find_equal(key(20));
    require(duplicates.has_value(), "duplicate-key lookup failed");
    require(*duplicates == std::vector<common::RecordId>({1, 2, 3, 4}),
            "duplicate-key lookup did not scan all matching leaves");

    const auto first = opened->find_equal(key(10));
    require(first.has_value() && *first == std::vector<common::RecordId>({100}),
            "first-leaf lookup mismatch");

    const auto last = opened->find_equal(key(30));
    require(last.has_value() && *last == std::vector<common::RecordId>({300}),
            "last-leaf lookup mismatch");

    const auto missing = opened->find_equal(key(25));
    require(missing.has_value() && missing->empty(), "missing key should return no records");

    const auto mismatched = opened->find_equal(bigint_key(20));
    require(!mismatched.has_value() && mismatched.error().is(IndexErrorCode::KeyTypeMismatch),
            "mismatched lookup key type should be rejected");
}

void test_scan_range_boundaries_and_reopen(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "49.bti";
    constexpr std::int32_t KeyCount = 400;
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();

    std::vector<common::RecordId> all_records;
    all_records.reserve(KeyCount * 2);
    for (std::int32_t value = 0; value < KeyCount; ++value) {
        all_records.push_back(static_cast<common::RecordId>(value) * 10);
        all_records.push_back(static_cast<common::RecordId>(value) * 10 + 1);
    }

    {
        auto created = BTreeIndex::create(path, 49, type, filesystem);
        require(created.has_value(), "range-scan BTreeIndex create failed");
        for (std::int32_t value = KeyCount; value > 0; --value) {
            const auto current = value - 1;
            require(created->insert(key(current), static_cast<common::RecordId>(current) * 10 + 1).has_value(),
                    "range-scan second record insert failed");
            require(created->insert(key(current), static_cast<common::RecordId>(current) * 10).has_value(),
                    "range-scan first record insert failed");
        }

        const auto all = created->scan_range(IndexRange::all());
        require(all.has_value() && *all == all_records, "full BTreeIndex range scan mismatch");

        const auto closed = created->scan_range(IndexRange::closed(key(100), key(102)));
        require(closed.has_value() &&
                *closed == std::vector<common::RecordId>({1000, 1001, 1010, 1011, 1020, 1021}),
                "closed BTreeIndex range mismatch");

        const auto open = created->scan_range(IndexRange::between(key(100), false, key(102), false));
        require(open.has_value() && *open == std::vector<common::RecordId>({1010, 1011}),
                "open BTreeIndex range mismatch");

        const auto lower = created->scan_range(IndexRange::lower_bound(key(398), false));
        require(lower.has_value() && *lower == std::vector<common::RecordId>({3990, 3991}),
                "exclusive lower BTreeIndex range mismatch");

        const auto upper = created->scan_range(IndexRange::upper_bound(key(1), true));
        require(upper.has_value() &&
                *upper == std::vector<common::RecordId>({0, 1, 10, 11}),
                "inclusive upper BTreeIndex range mismatch");

        const auto excluded_upper = created->scan_range(IndexRange::upper_bound(key(0), false));
        require(excluded_upper.has_value() && excluded_upper->empty(),
                "exclusive first-key upper range should be empty");

        const auto reversed = created->scan_range(IndexRange::closed(key(3), key(2)));
        require(reversed.has_value() && reversed->empty(),
                "reversed BTreeIndex range should be empty");
        const auto empty_equal = created->scan_range(
            IndexRange::between(key(3), true, key(3), false)
        );
        require(empty_equal.has_value() && empty_equal->empty(),
                "equal BTreeIndex range with an exclusive bound should be empty");

        const auto mismatched_lower = created->scan_range(
            IndexRange::lower_bound(bigint_key(10))
        );
        require(!mismatched_lower.has_value() &&
                mismatched_lower.error().is(IndexErrorCode::KeyTypeMismatch),
                "mismatched lower range key type should be rejected");
        const auto mismatched_upper = created->scan_range(
            IndexRange::upper_bound(bigint_key(10))
        );
        require(!mismatched_upper.has_value() &&
                mismatched_upper.error().is(IndexErrorCode::KeyTypeMismatch),
                "mismatched upper range key type should be rejected");
    }

    auto reopened = BTreeIndex::open(path, 49, type, filesystem);
    require(reopened.has_value(), "range-scan BTreeIndex reopen failed");
    const auto reopened_range = reopened->scan_range(IndexRange::closed(key(250), key(251)));
    require(reopened_range.has_value() &&
            *reopened_range == std::vector<common::RecordId>({2500, 2501, 2510, 2511}),
            "reopened BTreeIndex range mismatch");
}

void test_erase_last_root_leaf_entry(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "50.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();
    {
        auto created = BTreeIndex::create(path, 50, type, filesystem);
        require(created.has_value(), "last-entry BTreeIndex create failed");
        require(created->insert(key(7), 70).has_value(), "last-entry insert failed");
        require(created->erase(key(7), 70).has_value(), "last root leaf erase failed");
        require(created->entry_count() == 0 &&
                created->root_page_id() == btree_index::InvalidBTreePageId,
                "last root leaf erase should restore an empty tree");
        const auto found = created->find_equal(key(7));
        require(found.has_value() && found->empty(),
                "last root leaf erase should remove the entry");
    }

    auto reopened = BTreeIndex::open(path, 50, type, filesystem);
    require(reopened.has_value(), "empty erased BTreeIndex reopen failed");
    require(reopened->entry_count() == 0 &&
            reopened->root_page_id() == btree_index::InvalidBTreePageId,
            "reopened erased BTreeIndex should remain empty");
}

void test_erase_prunes_empty_subtrees_and_allows_reinsert(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "51.bti";
    constexpr std::size_t KeyLength = 512;
    constexpr std::size_t EntryCount = 80;
    const common::LogicalType type {common::LogicalTypeId::Varchar, KeyLength};
    auto filesystem = filesystem::create_platform_filesystem();

    {
        auto created = BTreeIndex::create(path, 51, type, filesystem);
        require(created.has_value(), "erase-prune BTreeIndex create failed");
        for (std::size_t value = 0; value < EntryCount; ++value) {
            require(created->insert(
                        padded_varchar_key(value, KeyLength), value + 2000
                    ).has_value(),
                    "erase-prune insert failed");
        }
        require(created->page_count() > 20,
                "erase-prune workload should create a multi-level tree");

        for (std::size_t value = 20; value < 60; ++value) {
            require(created->erase(
                        padded_varchar_key(value, KeyLength), value + 2000
                    ).has_value(),
                    "middle subtree erase failed");
        }
        require(created->entry_count() == 40, "middle subtree erase count mismatch");

        const auto remaining = created->scan_range(IndexRange::all());
        require(remaining.has_value() && remaining->size() == 40,
                "middle subtree erase range size mismatch");
        for (std::size_t index = 0; index < 20; ++index) {
            require((*remaining)[index] == index + 2000,
                    "left records changed after middle subtree erase");
            require((*remaining)[index + 20] == index + 60 + 2000,
                    "right records changed after middle subtree erase");
        }

        require(created->insert(padded_varchar_key(40, KeyLength), 2040).has_value(),
                "reinsert into a pruned key range failed");
        const auto reinserted = created->find_equal(padded_varchar_key(40, KeyLength));
        require(reinserted.has_value() &&
                *reinserted == std::vector<common::RecordId>({2040}),
                "reinserted pruned-range key lookup mismatch");
        require(created->erase(padded_varchar_key(40, KeyLength), 2040).has_value(),
                "reinserted pruned-range key erase failed");

        for (std::size_t value = 0; value < 20; ++value) {
            require(created->erase(
                        padded_varchar_key(value, KeyLength), value + 2000
                    ).has_value(),
                    "left subtree final erase failed");
        }
        for (std::size_t value = 60; value < EntryCount; ++value) {
            require(created->erase(
                        padded_varchar_key(value, KeyLength), value + 2000
                    ).has_value(),
                    "right subtree final erase failed");
        }

        require(created->entry_count() == 0 &&
                created->root_page_id() == btree_index::InvalidBTreePageId,
                "full multi-level erase should restore an empty tree");
        require(created->page_count() > 0,
                "free-page reuse should retain allocated physical slots");
        require(created->free_page_count() == created->page_count(),
                "full erase should place every physical page on the free list");
        const auto empty = created->scan_range(IndexRange::all());
        require(empty.has_value() && empty->empty(),
                "fully erased multi-level tree range should be empty");
    }

    auto reopened = BTreeIndex::open(path, 51, type, filesystem);
    require(reopened.has_value(), "fully erased multi-level BTreeIndex reopen failed");
    require(reopened->entry_count() == 0 &&
            reopened->root_page_id() == btree_index::InvalidBTreePageId,
            "reopened fully erased multi-level tree should remain empty");
    const auto pages_before_reinsert = reopened->page_count();
    const auto free_pages_before_reinsert = reopened->free_page_count();
    require(reopened->insert(padded_varchar_key(42, KeyLength), 2042).has_value(),
            "insert after reopening a fully erased tree failed");
    require(reopened->page_count() == pages_before_reinsert
                && reopened->free_page_count() + 1 == free_pages_before_reinsert,
            "reinsert should reuse one persisted free page");
    const auto found = reopened->find_equal(padded_varchar_key(42, KeyLength));
    require(found.has_value() && *found == std::vector<common::RecordId>({2042}),
            "insert-after-full-erase lookup mismatch");
}

void test_ordered_interface_and_index_store_reopen(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "52.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();

    {
        auto created = BTreeIndex::create(path, 52, type, filesystem);
        require(created.has_value(), "polymorphic BTreeIndex create failed");
        for (std::int32_t value = 0; value < 400; ++value) {
            require(created->insert(key(value), value + 3000).has_value(),
                    "polymorphic BTreeIndex insert failed");
        }
        require(created->page_count() > 1,
                "polymorphic IndexStore test should create multiple pages");

        std::unique_ptr<ScalarIndex> backend =
            std::make_unique<BTreeIndex>(std::move(*created));
        IndexStore store {IndexDescriptor {
            .index_id = 52,
            .collection_id = 1,
            .column_id = 2,
            .column_ordinal = 0,
            .key_type = type,
            .kind = IndexKind::BTree,
            .unique = false,
        }, std::move(backend)};

        const auto found = store.find_equal(key(200));
        require(found.has_value() && *found == std::vector<common::RecordId>({3200}),
                "IndexStore polymorphic equality lookup failed");
        const auto range = store.scan_range(IndexRange::closed(key(10), key(12)));
        require(range.has_value() &&
                *range == std::vector<common::RecordId>({3010, 3011, 3012}),
                "IndexStore polymorphic range scan failed");
        require(store.erase(key(200), 3200).has_value(),
                "IndexStore polymorphic erase failed");
        require(store.size() == 399, "IndexStore polymorphic erase size mismatch");
        const auto erased = store.find_equal(key(200));
        require(erased.has_value() && erased->empty(),
                "IndexStore polymorphic erase lookup mismatch");
    }

    auto reopened = BTreeIndex::open(path, 52, type, filesystem);
    require(reopened.has_value(), "polymorphic BTreeIndex reopen failed");
    require(reopened->entry_count() == 399,
            "polymorphic BTreeIndex count mismatch after reopen");
    const auto erased = reopened->find_equal(key(200));
    require(erased.has_value() && erased->empty(),
            "polymorphic BTreeIndex erase was not persisted");
    require(reopened->insert(key(400), 3400).has_value(),
            "insert after polymorphic BTreeIndex reopen failed");
}

} // namespace

int main()
{
    const auto directory = make_temp_directory();
    try {
        test_create_and_open_owns_page_store(directory);
        test_insert_simple_duplicate_and_reopen(directory);
        test_insert_rejects_oversized_entry(directory);
        test_insert_splits_leaf_and_preserves_duplicate_order(directory);
        test_insert_recursively_splits_internal_pages(directory);
        test_bulk_load_builds_sorted_persistent_tree(directory);
        test_find_equal_on_empty_tree(directory);
        test_find_equal_routes_and_scans_duplicate_keys(directory);
        test_scan_range_boundaries_and_reopen(directory);
        test_erase_last_root_leaf_entry(directory);
        test_erase_prunes_empty_subtrees_and_allows_reinsert(directory);
        test_ordered_interface_and_index_store_reopen(directory);
        std::filesystem::remove_all(directory);
    } catch (const std::exception & error) {
        std::filesystem::remove_all(directory);
        std::cerr << "btree_index_tests failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
