#include "core/index/hash_index/hash_index.hpp"
#include "core/index/index_store.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
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

void test_scalar_key_validation()
{
    require(ScalarIndexKey::from_value(Value {true}).has_value(), "bool key should be supported");
    require(ScalarIndexKey::from_value(Value {std::int32_t {1}}).has_value(), "int key should be supported");
    require(ScalarIndexKey::from_value(Value {std::int64_t {1}}).has_value(), "bigint key should be supported");
    require(ScalarIndexKey::from_value(Value {1.5F}).has_value(), "float key should be supported");
    require(ScalarIndexKey::from_value(Value {1.5}).has_value(), "double key should be supported");
    require(ScalarIndexKey::from_value(Value {std::string {"abc"}}).has_value(), "varchar key should be supported");

    auto vector_key = ScalarIndexKey::from_value(Value {VectorValue {1.0, 2.0}});
    require(!vector_key.has_value(), "vector key should be rejected");
    require(vector_key.error().code == IndexErrorCode::UnsupportedKeyType, "vector key error code mismatch");

    auto null_key = ScalarIndexKey::from_value(Value::null());
    require(!null_key.has_value(), "null key should be rejected");
    require(null_key.error().code == IndexErrorCode::InvalidKeyValue, "null key error code mismatch");

    auto float_nan_key = ScalarIndexKey::from_value(Value {std::numeric_limits<float>::quiet_NaN()});
    require(!float_nan_key.has_value(), "float NaN key should be rejected");
    require(float_nan_key.error().code == IndexErrorCode::InvalidKeyValue, "float NaN key error code mismatch");

    auto double_nan_key = ScalarIndexKey::from_value(Value {std::numeric_limits<double>::quiet_NaN()});
    require(!double_nan_key.has_value(), "double NaN key should be rejected");
    require(double_nan_key.error().code == IndexErrorCode::InvalidKeyValue, "double NaN key error code mismatch");
}

void test_scalar_key_exact_type_semantics()
{
    const auto int_key = key(Value {std::int32_t {1}});
    const auto bigint_key = key(Value {std::int64_t {1}});
    const auto double_key = key(Value {1.0});
    const auto large = key(Value {std::int64_t {9'007'199'254'740'992LL}});
    const auto next_large = key(Value {std::int64_t {9'007'199'254'740'993LL}});

    ScalarIndexEqual equal;
    ScalarIndexHash hash;
    ScalarIndexLess less;

    require(!equal(int_key, bigint_key), "int32 and int64 keys should keep distinct physical types");
    require(!equal(bigint_key, double_key), "integer and double keys should keep distinct physical types");
    require(hash(int_key) == hash(key(Value {std::int32_t {1}})), "equal keys should have equal hashes");
    require(less(large, next_large), "adjacent large bigint keys should retain exact ordering");
}

void test_scalar_key_ordering_is_stable()
{
    ScalarIndexLess less;
    require(less(key(Value {false}), key(Value {true})), "boolean ordering mismatch");
    require(less(key(Value {std::int32_t {9}}), key(Value {std::int32_t {10}})), "integer ordering mismatch");
    require(!less(key(Value {-0.0}), key(Value {0.0})), "negative zero should equal positive zero");
    require(less(key(Value {std::string {"a"}}), key(Value {std::string {"b"}})), "string ordering mismatch");
}

void test_hash_index_equal_lookup_and_erase()
{
    HashIndex index;
    const auto one = key(Value {std::int32_t {1}});

    require(index.kind() == IndexKind::Hash, "hash index kind mismatch");
    require(index.insert(one, 10).has_value(), "hash insert failed");
    require(index.insert(one, 20).has_value(), "hash insert duplicate key failed");
    require(index.insert(key(Value {std::int32_t {2}}), 30).has_value(), "hash insert second key failed");
    require(index.size() == 3, "hash size mismatch");

    require_ids(ids(index.find_equal(one)), {10, 20}, "hash equal lookup mismatch");

    auto duplicate = index.insert(one, 10);
    require(!duplicate.has_value(), "duplicate hash index entry should fail");
    require(duplicate.error().code == IndexErrorCode::DuplicateEntry, "duplicate hash entry error mismatch");

    require(index.erase(one, 10).has_value(), "hash erase one record failed");
    require_ids(ids(index.find_equal(one)), {20}, "hash erase should keep remaining duplicate key");
    require(index.erase(one, 20).has_value(), "hash erase final record failed");
    require(ids(index.find_equal(one)).empty(), "hash empty bucket should be removed");
    require(index.size() == 1, "hash size after erase mismatch");
}

void test_index_store_enforces_descriptor_constraints()
{
    IndexStore store {IndexDescriptor {
        .index_id = 1,
        .collection_id = 2,
        .column_id = 3,
        .column_ordinal = 0,
        .key_type = LogicalType {LogicalTypeId::Integer, std::nullopt},
        .kind = IndexKind::Hash,
        .unique = true,
    }, std::make_unique<HashIndex>()};

    const auto one = key(Value {std::int32_t {1}});
    require(store.insert(one, 10).has_value(), "store insert failed");

    auto duplicate_key = store.validate_insert(one);
    require(!duplicate_key.has_value(), "unique store should reject an existing key");
    require(duplicate_key.error().code == IndexErrorCode::DuplicateKey, "unique store error mismatch");

    auto wrong_type = store.find_equal(key(Value {std::int64_t {1}}));
    require(!wrong_type.has_value(), "store should reject a mismatched key type");
    require(wrong_type.error().code == IndexErrorCode::KeyTypeMismatch, "store key type error mismatch");

    auto unsupported_range = store.scan_range(IndexRange::all());
    require(!unsupported_range.has_value(), "hash store should reject range scans");
    require(
        unsupported_range.error().code == IndexErrorCode::UnsupportedRangeScan,
        "hash store range error mismatch"
    );
}

} // namespace

int main()
{
    try {
        test_scalar_key_validation();
        test_scalar_key_exact_type_semantics();
        test_scalar_key_ordering_is_stable();
        test_hash_index_equal_lookup_and_erase();
        test_index_store_enforces_descriptor_constraints();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
