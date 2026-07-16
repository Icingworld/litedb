#include "core/index/btree_index/btree_page.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/schema/value.hpp"

namespace
{

using namespace litedb::core;
using namespace litedb::core::index;
using namespace litedb::core::index::btree_index;
using litedb::core::schema::Value;

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
    return std::move(result.value());
}

BTreeEntryKey entry(std::int32_t value, common::RecordId record_id)
{
    return BTreeEntryKey {
        .key = key(value),
        .record_id = record_id,
    };
}

void test_entry_key_uses_record_id_as_tie_breaker()
{
    require(
        compare_btree_entry_keys(entry(1, 10), entry(1, 11)) == std::strong_ordering::less,
        "record id should break equal scalar-key ties"
    );
    require(
        compare_btree_entry_keys(entry(1, 99), entry(2, 1)) == std::strong_ordering::less,
        "scalar key should be compared before record id"
    );
    require(
        compare_btree_entry_keys(entry(2, 7), entry(2, 7)) == std::strong_ordering::equal,
        "identical entry keys should compare equal"
    );
}

void test_leaf_page_orders_entries_and_finds_equal_key_range()
{
    BTreeLeafPage page {10, 9, 11};
    require(page.type() == BTreePageType::Leaf, "leaf page type mismatch");
    require(page.page_id() == 10, "leaf page id mismatch");
    require(page.previous_page_id() == 9, "leaf previous page mismatch");
    require(page.next_page_id() == 11, "leaf next page mismatch");

    require(page.insert(entry(2, 20)), "leaf insert 2/20 failed");
    require(page.insert(entry(1, 11)), "leaf insert 1/11 failed");
    require(page.insert(entry(1, 10)), "leaf insert 1/10 failed");
    require(page.insert(entry(3, 30)), "leaf insert 3/30 failed");
    require(!page.insert(entry(1, 10)), "duplicate leaf entry should be rejected");

    require(page.size() == 4, "leaf size mismatch");
    require(page.entries()[0].record_id == 10, "leaf first duplicate entry mismatch");
    require(page.entries()[1].record_id == 11, "leaf second duplicate entry mismatch");
    require(page.entries()[2].record_id == 20, "leaf ordered entry mismatch");
    require(page.lower_bound(key(1)) == 0, "leaf scalar lower bound mismatch");
    require(page.upper_bound(key(1)) == 2, "leaf scalar upper bound mismatch");
    require(page.lower_bound(entry(1, 11)) == 1, "leaf composite lower bound mismatch");
    require(page.contains(entry(1, 11)), "leaf should contain inserted entry");
    require(!page.contains(entry(1, 12)), "leaf should not contain missing entry");

    require(page.erase(entry(1, 10)), "leaf erase failed");
    require(!page.erase(entry(1, 10)), "missing leaf erase should fail");
    require(page.lower_bound(key(1)) == 0, "remaining duplicate lower bound mismatch");
    require(page.upper_bound(key(1)) == 1, "remaining duplicate upper bound mismatch");

    page.set_previous_page_id(8);
    page.set_next_page_id(12);
    require(page.previous_page_id() == 8, "leaf previous link update failed");
    require(page.next_page_id() == 12, "leaf next link update failed");
}

void test_internal_page_routes_composite_keys()
{
    BTreeInternalPage page {20, 100};
    require(page.type() == BTreePageType::Internal, "internal page type mismatch");
    require(page.insert_child_after(100, entry(10, 100), 200), "first internal insert failed");
    require(page.insert_child_after(200, entry(20, 200), 300), "second internal insert failed");

    require(page.child_count() == 3, "internal child count mismatch");
    require(page.child_for(entry(5, 1)) == 100, "key below first separator routed incorrectly");
    require(page.child_for(entry(10, 99)) == 100, "key below composite separator routed incorrectly");
    require(page.child_for(entry(10, 100)) == 200, "separator key should route right");
    require(page.child_for(entry(15, 1)) == 200, "middle key routed incorrectly");
    require(page.child_for(entry(20, 200)) == 300, "second separator should route right");

    require(page.child_at(0) == 100, "first child lookup mismatch");
    require(page.child_at(1) == 200, "second child lookup mismatch");
    require(page.child_at(2) == 300, "third child lookup mismatch");
    require(!page.child_at(3).has_value(), "out-of-range child lookup should fail");
    require(page.find_child(300) == 2, "child position lookup mismatch");
    require(!page.find_child(999).has_value(), "missing child should not have a position");
}

void test_internal_page_maintains_separator_invariants()
{
    BTreeInternalPage page {20, 100};
    require(page.insert_child_after(100, entry(10, 100), 200), "first internal insert failed");
    require(page.insert_child_after(200, entry(20, 200), 300), "second internal insert failed");
    require(page.insert_child_after(200, entry(15, 150), 250), "middle internal insert failed");

    require(!page.insert_child_after(100, entry(18, 1), 400), "unordered separator should be rejected");
    require(!page.insert_child_after(999, entry(30, 1), 400), "missing left child should be rejected");
    require(!page.insert_child_after(300, entry(30, 1), 250), "duplicate child should be rejected");
    require(page.child_count() == 4, "failed insert should not mutate internal page");

    require(page.replace_separator(250, entry(16, 160)), "separator replacement failed");
    require(!page.replace_separator(250, entry(25, 1)), "unordered replacement should be rejected");
    require(page.entries()[1].separator.record_id == 160, "failed replacement should roll back separator");
    require(!page.replace_separator(999, entry(30, 1)), "missing right child replacement should fail");

    require(page.erase_child(100), "first child erase failed");
    require(page.first_child_id() == 200, "first child erase should promote its right sibling");
    require(page.erase_child(250), "middle child erase failed");
    require(page.child_count() == 2, "internal child count after erase mismatch");
    require(!page.erase_child(999), "missing child erase should fail");

    BTreeInternalPage single_child {30, 500};
    require(!single_child.erase_child(500), "internal page must retain its only child");
}

void test_page_variant_reports_type_and_id()
{
    BTreePage leaf = BTreeLeafPage {7};
    require(btree_page_type(leaf) == BTreePageType::Leaf, "variant leaf type mismatch");
    require(btree_page_id(leaf) == 7, "variant leaf id mismatch");

    BTreePage internal = BTreeInternalPage {8, 9};
    require(btree_page_type(internal) == BTreePageType::Internal, "variant internal type mismatch");
    require(btree_page_id(internal) == 8, "variant internal id mismatch");
}

} // namespace

int main()
{
    try {
        test_entry_key_uses_record_id_as_tie_breaker();
        test_leaf_page_orders_entries_and_finds_equal_key_range();
        test_internal_page_routes_composite_keys();
        test_internal_page_maintains_separator_invariants();
        test_page_variant_reports_type_and_id();
    } catch (const std::exception & error) {
        std::cerr << "btree_page_tests failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
