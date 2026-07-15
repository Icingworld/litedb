#include "core/index/btree_index/btree_index.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/filesystem/platform_filesystem.hpp"
#include "core/schema/value.hpp"

namespace
{

using namespace litedb::core;
using namespace litedb::core::index;

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
    auto result = ScalarIndexKey::from_value(schema::Value {value});
    if (!result.has_value()) {
        throw std::runtime_error("failed to create scalar index key");
    }
    return std::move(result.value());
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

void test_operation_framework_reports_not_implemented(const std::filesystem::path & directory)
{
    const auto path = directory / "indexes" / "43.bti";
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    auto filesystem = filesystem::create_platform_filesystem();
    auto created = BTreeIndex::create(path, 43, type, filesystem);
    require(created.has_value(), "operation framework BTreeIndex create failed");

    auto index = std::move(created.value());
    const auto one = key(1);
    require(index.kind() == IndexKind::BTree, "BTreeIndex kind mismatch");
    require(index.size() == 0, "new BTreeIndex size mismatch");

    const auto inserted = index.insert(one, 10);
    require(!inserted.has_value() && inserted.error().code == IndexErrorCode::NotImplemented,
            "BTreeIndex insert placeholder should report NotImplemented");

    const auto erased = index.erase(one, 10);
    require(!erased.has_value() && erased.error().code == IndexErrorCode::NotImplemented,
            "BTreeIndex erase placeholder should report NotImplemented");

    const auto equal = index.find_equal(one);
    require(!equal.has_value() && equal.error().code == IndexErrorCode::NotImplemented,
            "BTreeIndex find_equal placeholder should report NotImplemented");

    const auto range = index.scan_range(IndexRange::all());
    require(!range.has_value() && range.error().code == IndexErrorCode::NotImplemented,
            "BTreeIndex scan_range placeholder should report NotImplemented");
}

} // namespace

int main()
{
    const auto directory = make_temp_directory();
    try {
        test_create_and_open_owns_page_store(directory);
        test_operation_framework_reports_not_implemented(directory);
        std::filesystem::remove_all(directory);
    } catch (const std::exception & error) {
        std::filesystem::remove_all(directory);
        std::cerr << "btree_index_tests failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
