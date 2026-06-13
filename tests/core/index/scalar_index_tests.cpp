#include "core/index/btree_index.hpp"
#include "core/index/hash_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace litedb::core::common;
using namespace litedb::core::index;
using namespace litedb::core::schema;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ScalarIndexKey key(Value value)
{
    auto result = ScalarIndexKey::from_value(std::move(value));
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::vector<RecordId> ids(std::expected<std::vector<RecordId>, IndexError> result)
{
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

void require_ids(std::vector<RecordId> actual, std::vector<RecordId> expected, const char * message)
{
    if (actual != expected) {
        throw std::runtime_error(message);
    }
}

void test_scalar_key_rejects_vector()
{
    require(ScalarIndexKey::from_value(Value::null()).has_value(), "null key should be supported");
    require(ScalarIndexKey::from_value(Value {true}).has_value(), "bool key should be supported");
    require(ScalarIndexKey::from_value(Value {std::int32_t {1}}).has_value(), "int key should be supported");
    require(ScalarIndexKey::from_value(Value {std::int64_t {1}}).has_value(), "bigint key should be supported");
    require(ScalarIndexKey::from_value(Value {1.5F}).has_value(), "float key should be supported");
    require(ScalarIndexKey::from_value(Value {1.5}).has_value(), "double key should be supported");
    require(ScalarIndexKey::from_value(Value {std::string {"abc"}}).has_value(), "varchar key should be supported");

    auto vector_key = ScalarIndexKey::from_value(Value {VectorValue {1.0, 2.0}});
    require(!vector_key.has_value(), "vector key should be rejected");
    require(vector_key.error().code == IndexErrorCode::UnsupportedKeyType, "vector key error code mismatch");
}

void test_scalar_key_numeric_cross_type_semantics()
{
    const auto int_key = key(Value {std::int32_t {1}});
    const auto bigint_key = key(Value {std::int64_t {1}});
    const auto double_key = key(Value {1.0});
    const auto greater_key = key(Value {2.0F});

    ScalarIndexEqual equal;
    ScalarIndexHash hash;
    ScalarIndexLess less;

    require(equal(int_key, bigint_key), "int32 and int64 with same value should be equal");
    require(equal(bigint_key, double_key), "integer and double with same value should be equal");
    require(hash(int_key) == hash(bigint_key), "equal numeric keys should have same hash");
    require(hash(bigint_key) == hash(double_key), "equal numeric keys should have same hash across double");
    require(less(double_key, greater_key), "numeric ordering should work across numeric types");
    require(!less(bigint_key, double_key), "equal numeric keys should not compare less");
}

void test_scalar_key_ordering_is_stable()
{
    ScalarIndexLess less;
    require(less(key(Value::null()), key(Value {false})), "null should sort before bool");
    require(less(key(Value {false}), key(Value {std::int32_t {0}})), "bool should sort before numeric");
    require(less(key(Value {std::int32_t {10}}), key(Value {std::string {"a"}})), "numeric should sort before string");
    require(less(key(Value {std::string {"a"}}), key(Value {std::string {"b"}})), "string ordering mismatch");
}

void test_hash_index_equal_lookup_and_erase()
{
    HashIndex index;
    const auto one = key(Value {std::int32_t {1}});
    const auto one_double = key(Value {1.0});

    require(index.kind() == IndexKind::Hash, "hash index kind mismatch");
    require(!index.supports_range_scan(), "hash index should not support range scan");
    require(index.insert(one, 10).has_value(), "hash insert failed");
    require(index.insert(one_double, 20).has_value(), "hash insert duplicate numeric key failed");
    require(index.insert(key(Value {std::string {"name"}}), 30).has_value(), "hash insert string failed");
    require(index.size() == 3, "hash size mismatch");

    require_ids(ids(index.find_equal(one)), {10, 20}, "hash equal lookup mismatch");

    auto range = index.scan_range(IndexRange::all());
    require(!range.has_value(), "hash range scan should fail");
    require(range.error().code == IndexErrorCode::UnsupportedRangeScan, "hash range error code mismatch");

    require(index.erase(one_double, 10).has_value(), "hash erase one record failed");
    require_ids(ids(index.find_equal(one)), {20}, "hash erase should keep remaining duplicate key");
    require(index.erase(one, 20).has_value(), "hash erase final record failed");
    require(ids(index.find_equal(one)).empty(), "hash empty bucket should be removed");
    require(index.size() == 1, "hash size after erase mismatch");
}

void test_btree_index_equal_lookup_and_ranges()
{
    BTreeIndex index;
    require(index.kind() == IndexKind::BTree, "btree index kind mismatch");
    require(index.supports_range_scan(), "btree index should support range scan");

    require(index.insert(key(Value::null()), 1).has_value(), "btree insert null failed");
    require(index.insert(key(Value {std::int32_t {1}}), 10).has_value(), "btree insert 1 failed");
    require(index.insert(key(Value {std::int64_t {2}}), 20).has_value(), "btree insert 2 failed");
    require(index.insert(key(Value {3.0}), 30).has_value(), "btree insert 3 failed");
    require(index.insert(key(Value {std::string {"a"}}), 40).has_value(), "btree insert string failed");
    require(index.insert(key(Value {1.0}), 11).has_value(), "btree insert duplicate numeric failed");
    require(index.size() == 6, "btree size mismatch");

    require_ids(ids(index.find_equal(key(Value {std::int64_t {1}}))), {10, 11}, "btree equal lookup mismatch");
    require_ids(
        ids(index.scan_range(IndexRange::closed(key(Value {std::int32_t {1}}), key(Value {3.0F})))),
        {10, 11, 20, 30},
        "btree closed range mismatch"
    );
    require_ids(
        ids(index.scan_range(IndexRange::between(
            key(Value {std::int32_t {1}}),
            false,
            key(Value {3.0}),
            false
        ))),
        {20},
        "btree open range mismatch"
    );
    require_ids(
        ids(index.scan_range(IndexRange::upper_bound(key(Value {std::int32_t {1}})))),
        {1, 10, 11},
        "btree upper-bound range mismatch"
    );
    require_ids(
        ids(index.scan_range(IndexRange::lower_bound(key(Value {std::int64_t {2}}), false))),
        {30, 40},
        "btree lower-bound exclusive range mismatch"
    );
    require_ids(ids(index.scan_range(IndexRange::all())), {1, 10, 11, 20, 30, 40}, "btree full range mismatch");
    require(
        ids(index.scan_range(IndexRange::closed(key(Value {3.0}), key(Value {std::int32_t {1}})))).empty(),
        "btree reversed range should be empty"
    );
    require(
        ids(index.scan_range(IndexRange::between(
            key(Value {std::int32_t {1}}),
            false,
            key(Value {1.0}),
            true
        ))).empty(),
        "btree equal open range should be empty"
    );
}

void test_btree_index_erase_errors_and_cleanup()
{
    BTreeIndex index;
    const auto one = key(Value {std::int32_t {1}});

    require(index.insert(one, 10).has_value(), "btree insert failed");
    auto missing_record = index.erase(one, 11);
    require(!missing_record.has_value(), "btree missing record erase should fail");
    require(missing_record.error().code == IndexErrorCode::RecordNotFound, "btree missing record error mismatch");

    require(index.erase(one, 10).has_value(), "btree erase failed");
    require(index.size() == 0, "btree size should be zero after erase");
    require(ids(index.find_equal(one)).empty(), "btree empty bucket should be removed");

    auto missing_key = index.erase(one, 10);
    require(!missing_key.has_value(), "btree missing key erase should fail");
    require(missing_key.error().code == IndexErrorCode::KeyNotFound, "btree missing key error mismatch");
}

} // namespace

int main()
{
    try {
        test_scalar_key_rejects_vector();
        test_scalar_key_numeric_cross_type_semantics();
        test_scalar_key_ordering_is_stable();
        test_hash_index_equal_lookup_and_erase();
        test_btree_index_equal_lookup_and_ranges();
        test_btree_index_erase_errors_and_cleanup();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
