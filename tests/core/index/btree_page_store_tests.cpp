#include "core/index/btree_index/btree_page_store.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/common/value.hpp"

namespace
{

using namespace litedb::core;
using namespace litedb::core::index;
using namespace litedb::core::index::btree_index;
using litedb::core::common::Value;

void require(bool condition, const std::string & message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ScalarIndexKey key(std::int32_t value)
{
    auto result = ScalarIndexKey::from_value(Value {value});
    if (!result.has_value()) {
        throw std::runtime_error("failed to create scalar index key");
    }
    return std::move(*result);
}

BTreeEntryKey entry(std::int32_t value, common::RecordId record_id)
{
    return BTreeEntryKey {
        .key = key(value),
        .record_id = record_id,
    };
}

std::filesystem::path make_temp_directory()
{
    return std::filesystem::temp_directory_path() /
        ("litedb-btree-page-store-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

void test_create_allocate_write_and_reopen(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "42.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();

    {
        auto created = BTreePageStore::create(path, 42, type, filesystem);
        require(created.has_value(), "page store create failed");
        auto store = std::move(*created);
        require(store.index_id() == 42, "created store index id mismatch");
        require(store.root_page_id() == InvalidBTreePageId, "new store should have no root");
        require(store.page_count() == 0, "new store page count mismatch");
        require(store.entry_count() == 0, "new store entry count mismatch");

        auto first = store.allocate_leaf_page();
        require(first.has_value() && first->page_id() == 1, "first leaf allocation failed");
        require(first->insert(entry(10, 100)), "first leaf entry insert failed");
        require(first->insert(entry(20, 200)), "second leaf entry insert failed");
        require(store.write_page(BTreePage {*first}).has_value(), "first leaf write failed");

        auto second = store.allocate_leaf_page(first->page_id());
        require(second.has_value() && second->page_id() == 2, "second leaf allocation failed");
        require(second->insert(entry(30, 300)), "second leaf entry insert failed");
        require(store.write_page(BTreePage {*second}).has_value(), "second leaf write failed");
        first->set_next_page_id(second->page_id());
        require(store.write_page(BTreePage {*first}).has_value(), "leaf link update failed");

        auto root = store.allocate_internal_page(first->page_id());
        require(root.has_value() && root->page_id() == 3, "root allocation failed");
        require(root->insert_child_after(first->page_id(), entry(30, 300), second->page_id()),
                "root separator insert failed");
        require(store.write_page(BTreePage {*root}).has_value(), "root write failed");
        require(store.set_root_page_id(root->page_id()).has_value(), "root metadata update failed");
        require(store.set_entry_count(3).has_value(), "entry count update failed");
        require(store.sync_all().has_value(), "page store sync failed");

        require(store.page_count() == 3, "allocated page count mismatch");
        require(std::filesystem::file_size(path) ==
                    BTreePageStore::HeaderSize + 3 * BTreePageCodec::PageSize,
                "page store file size mismatch");
    }

    {
        auto opened = BTreePageStore::open(path, 42, type, filesystem);
        require(opened.has_value(), "page store reopen failed");
        auto store = std::move(*opened);
        require(store.root_page_id() == 3, "reopened root page mismatch");
        require(store.page_count() == 3, "reopened page count mismatch");
        require(store.entry_count() == 3, "reopened entry count mismatch");

        auto first = store.read_page(1);
        require(first.has_value(), "reopened first leaf read failed");
        const auto * first_leaf = std::get_if<BTreeLeafPage>(&*first);
        require(first_leaf != nullptr && first_leaf->next_page_id() == 2,
                "reopened first leaf links mismatch");
        require(first_leaf->entries().size() == 2, "reopened first leaf entries mismatch");

        auto root = store.read_page(store.root_page_id());
        require(root.has_value(), "reopened root read failed");
        const auto * internal = std::get_if<BTreeInternalPage>(&*root);
        require(internal != nullptr && internal->child_for(entry(30, 300)) == 2,
                "reopened root routing mismatch");

        auto fourth = store.allocate_leaf_page(2);
        require(fourth.has_value() && fourth->page_id() == 4, "reopened allocation did not continue page ids");
        require(store.release_page(fourth->page_id()).has_value(), "free page release failed");
        require(store.free_page_count() == 1, "free page count mismatch");
        auto reused = store.allocate_leaf_page(2);
        require(reused.has_value() && reused->page_id() == 4,
                "page allocation should reuse the persisted free page");
        require(store.page_count() == 4 && store.free_page_count() == 0,
                "free page reuse should not grow the file");
        require(store.sync_data().has_value(), "page store data sync failed");

        auto missing = store.read_page(99);
        require(!missing.has_value() && missing.error().is(BTreePageStoreErrorCode::PageNotFound),
                "missing page read error mismatch");
        auto unknown_root = store.set_root_page_id(99);
        require(!unknown_root.has_value() && unknown_root.error().is(BTreePageStoreErrorCode::PageNotFound),
                "unknown root error mismatch");

        BTreeLeafPage invalid_links {1, InvalidBTreePageId, 99};
        auto invalid_write = store.write_page(BTreePage {std::move(invalid_links)});
        require(!invalid_write.has_value() && invalid_write.error().is(BTreePageStoreErrorCode::InvalidPage),
                "unknown leaf link should fail write");

        auto invalid_internal = store.allocate_internal_page(99);
        require(!invalid_internal.has_value() && invalid_internal.error().is(BTreePageStoreErrorCode::PageNotFound),
                "unknown first child should fail internal allocation");
    }

    auto wrong_index = BTreePageStore::open(path, 43, type, filesystem);
    require(!wrong_index.has_value() && wrong_index.error().is(BTreePageStoreErrorCode::InvalidFormat),
            "wrong index identity should fail open");
    const common::LogicalType wrong_type {common::LogicalTypeId::BigInt, std::nullopt};
    auto mismatched_type = BTreePageStore::open(path, 42, wrong_type, filesystem);
    require(!mismatched_type.has_value() && mismatched_type.error().is(BTreePageStoreErrorCode::InvalidFormat),
            "wrong key type should fail open");
}

void test_varchar_key_parameter_round_trip(const std::filesystem::path & directory)
{
    const auto path = directory / "varchar.bti";
    const common::LogicalType type {common::LogicalTypeId::Varchar, 128};
    auto filesystem = filesystem::create_platform_filesystem();
    {
        auto created = BTreePageStore::create(path, 77, type, filesystem);
        require(created.has_value(), "varchar page store create failed");
    }
    auto opened = BTreePageStore::open(path, 77, type, filesystem);
    require(opened.has_value(), "varchar page store reopen failed");
    require(opened->key_type().parameter == 128, "varchar key parameter was not persisted");
}

void test_corrupted_node_is_reported_on_read(const std::filesystem::path & directory)
{
    const auto path = directory / "corrupted-page.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();
    {
        auto created = BTreePageStore::create(path, 88, type, filesystem);
        require(created.has_value(), "corrupted page store create failed");
        auto page = created->allocate_leaf_page();
        require(page.has_value(), "corrupted page allocation failed");
    }
    {
        auto file = filesystem.open(path, {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "corrupted page raw open failed");
        const std::array invalid_magic {std::byte {0}};
        require(file->write_at(BTreePageStore::HeaderSize, invalid_magic).has_value(),
                "corrupted page raw write failed");
    }
    auto opened = BTreePageStore::open(path, 88, type, filesystem);
    require(opened.has_value(), "store open should not eagerly decode node pages");
    auto page = opened->read_page(1);
    require(!page.has_value() && page.error().is(BTreePageStoreErrorCode::CorruptedPage),
            "corrupted node page should fail on read");
    const auto * context = page.error().context<BTreePageStoreErrorContext>();
    require(context != nullptr && context->codec_code == BTreePageCodecErrorCode::InvalidFormat,
            "corrupted node codec error mismatch");
}

void test_invalid_header_and_file_size_are_rejected(const std::filesystem::path & directory)
{
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();

    const auto version_path = directory / "bad-version.bti";
    {
        auto created = BTreePageStore::create(version_path, 90, type, filesystem);
        require(created.has_value(), "bad version store create failed");
    }
    {
        auto file = filesystem.open(version_path, {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "bad version raw open failed");
        const std::array version {std::byte {3}, std::byte {0}};
        require(file->write_at(4, version).has_value(), "bad version raw write failed");
    }
    auto unsupported = BTreePageStore::open(version_path, 90, type, filesystem);
    require(!unsupported.has_value() && unsupported.error().is(BTreePageStoreErrorCode::UnsupportedVersion),
            "unsupported store version should fail open");

    const auto legacy_path = directory / "legacy-v1-header.bti";
    {
        auto created = BTreePageStore::create(legacy_path, 93, type, filesystem);
        require(created.has_value(), "legacy header store create failed");
    }
    {
        auto file = filesystem.open(legacy_path, {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "legacy header raw open failed");
        const std::array version {std::byte {1}, std::byte {0}};
        const std::array checksum {
            std::byte {0}, std::byte {0}, std::byte {0}, std::byte {0},
        };
        require(file->write_at(4, version).has_value(), "legacy header version write failed");
        require(file->write_at(56, checksum).has_value(), "legacy header checksum write failed");
    }
    auto legacy = BTreePageStore::open(legacy_path, 93, type, filesystem);
    require(legacy.has_value(), "version 1 store header should remain readable");

    const auto checksum_path = directory / "bad-header-checksum.bti";
    {
        auto created = BTreePageStore::create(checksum_path, 92, type, filesystem);
        require(created.has_value(), "header checksum store create failed");
    }
    {
        auto file = filesystem.open(checksum_path, {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "header checksum raw open failed");
        const std::array changed_count {std::byte {1}};
        require(file->write_at(48, changed_count).has_value(),
                "header checksum corruption write failed");
    }
    auto checksum = BTreePageStore::open(checksum_path, 92, type, filesystem);
    require(!checksum.has_value()
                && checksum.error().is(BTreePageStoreErrorCode::ChecksumMismatch),
            "header bit flip should fail store checksum");

    const auto truncated_path = directory / "truncated.bti";
    {
        auto created = BTreePageStore::create(truncated_path, 91, type, filesystem);
        require(created.has_value(), "truncated store create failed");
        require(created->allocate_leaf_page().has_value(), "truncated store allocation failed");
    }
    {
        auto file = filesystem.open(truncated_path, {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "truncated raw open failed");
        require(file->truncate(BTreePageStore::HeaderSize + BTreePageCodec::PageSize - 1).has_value(),
                "truncated raw resize failed");
    }
    auto truncated = BTreePageStore::open(truncated_path, 91, type, filesystem);
    require(!truncated.has_value() && truncated.error().is(BTreePageStoreErrorCode::InvalidFormat),
            "truncated store should fail open");
}

} // namespace

int main()
{
    const auto directory = make_temp_directory();
    try {
        test_create_allocate_write_and_reopen(directory);
        test_varchar_key_parameter_round_trip(directory);
        test_corrupted_node_is_reported_on_read(directory);
        test_invalid_header_and_file_size_are_rejected(directory);
        std::filesystem::remove_all(directory);
    } catch (const std::exception & error) {
        std::filesystem::remove_all(directory);
        std::cerr << "btree_page_store_tests failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
