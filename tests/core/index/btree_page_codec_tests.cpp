#include "core/index/btree_index/btree_page_codec.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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

ScalarIndexKey key(Value value)
{
    auto result = ScalarIndexKey::from_value(std::move(value));
    if (!result.has_value()) {
        throw std::runtime_error("failed to create scalar index key");
    }
    return std::move(result.value());
}

BTreeEntryKey entry(Value value, common::RecordId record_id)
{
    return BTreeEntryKey {
        .key = key(std::move(value)),
        .record_id = record_id,
    };
}

template <typename T>
void write_number(std::byte * target, T value)
{
    using Unsigned = std::make_unsigned_t<T>;
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        target[index] = static_cast<std::byte>((bits >> (index * 8U)) & static_cast<Unsigned>(0xffU));
    }
}

template <typename T>
T read_number(const std::byte * source)
{
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned value {0};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value |= static_cast<Unsigned>(std::to_integer<unsigned int>(source[index])) << (index * 8U);
    }
    return static_cast<T>(value);
}

void require_entry_equal(
    const BTreeEntryKey & actual,
    const BTreeEntryKey & expected,
    const std::string & message
)
{
    require(
        compare_btree_entry_keys(actual, expected) == std::strong_ordering::equal,
        message
    );
}

void test_leaf_round_trip_for_all_scalar_key_types()
{
    struct Case
    {
        common::LogicalType type;
        std::vector<Value> values;
    };

    std::vector<Case> cases;
    cases.push_back({{common::LogicalTypeId::Boolean, std::nullopt}, {Value {false}, Value {true}}});
    cases.push_back({{common::LogicalTypeId::Integer, std::nullopt}, {Value {std::int32_t {-7}}, Value {std::int32_t {42}}}});
    cases.push_back({{common::LogicalTypeId::BigInt, std::nullopt}, {Value {std::int64_t {-9000000000}}, Value {std::int64_t {9000000000}}}});
    cases.push_back({{common::LogicalTypeId::Float, std::nullopt}, {Value {-1.25F}, Value {3.5F}}});
    cases.push_back({{common::LogicalTypeId::Double, std::nullopt}, {Value {-9.125}, Value {100.5}}});
    cases.push_back({{common::LogicalTypeId::Varchar, 32}, {Value {std::string {}}, Value {std::string {"alice"}}}});

    BTreePageId page_id = 10;
    for (auto & test : cases) {
        BTreeLeafPage leaf {page_id, page_id - 1, page_id + 1};
        std::vector<BTreeEntryKey> expected;
        common::RecordId record_id = 100;
        for (auto & value : test.values) {
            auto item = entry(std::move(value), record_id++);
            expected.push_back(item);
            require(leaf.insert(std::move(item)), "leaf test entry insert failed");
        }

        BTreePage page {std::move(leaf)};
        auto size = BTreePageCodec::encoded_size(page, test.type);
        require(size.has_value() && *size <= BTreePageCodec::PageSize, "leaf encoded size failed");
        auto fits = BTreePageCodec::can_fit(page, test.type);
        require(fits.has_value() && *fits, "leaf should fit in physical page");
        auto encoded = BTreePageCodec::encode(page, test.type);
        require(encoded.has_value(), "leaf encode failed");
        auto decoded = BTreePageCodec::decode(*encoded, test.type, page_id);
        require(decoded.has_value(), "leaf decode failed");
        const auto * decoded_leaf = std::get_if<BTreeLeafPage>(&*decoded);
        require(decoded_leaf != nullptr, "decoded page should be a leaf");
        require(decoded_leaf->previous_page_id() == page_id - 1, "leaf previous link round trip failed");
        require(decoded_leaf->next_page_id() == page_id + 1, "leaf next link round trip failed");
        require(decoded_leaf->entries().size() == expected.size(), "leaf entry count round trip failed");
        for (std::size_t index = 0; index < expected.size(); ++index) {
            require_entry_equal(decoded_leaf->entries()[index], expected[index], "leaf entry round trip failed");
        }
        ++page_id;
    }
}

void test_internal_page_round_trip()
{
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    BTreeInternalPage internal {50, 100};
    require(internal.insert_child_after(100, entry(Value {std::int32_t {10}}, 1000), 200), "internal insert failed");
    require(internal.insert_child_after(200, entry(Value {std::int32_t {20}}, 2000), 300), "internal insert failed");

    BTreePage page {std::move(internal)};
    auto encoded = BTreePageCodec::encode(page, type);
    require(encoded.has_value(), "internal encode failed");
    auto decoded = BTreePageCodec::decode(*encoded, type, 50);
    require(decoded.has_value(), "internal decode failed");
    const auto * decoded_internal = std::get_if<BTreeInternalPage>(&*decoded);
    require(decoded_internal != nullptr, "decoded page should be internal");
    require(decoded_internal->first_child_id() == 100, "internal first child round trip failed");
    require(decoded_internal->entries().size() == 2, "internal entry count round trip failed");
    require(decoded_internal->entries()[0].right_child_id == 200, "internal first right child round trip failed");
    require(decoded_internal->entries()[1].right_child_id == 300, "internal second right child round trip failed");
    require(decoded_internal->child_for(entry(Value {std::int32_t {20}}, 2000)) == 300, "decoded internal routing failed");
}

void test_page_capacity_uses_encoded_bytes()
{
    const common::LogicalType type {common::LogicalTypeId::Varchar, 5000};
    constexpr auto FixedLeafCost = BTreePageCodec::HeaderSize + BTreePageCodec::SlotSize + sizeof(common::RecordId);
    const auto maximum_key_size = BTreePageCodec::PageSize - FixedLeafCost;

    BTreeLeafPage exact_leaf {70};
    require(exact_leaf.insert(entry(Value {std::string(maximum_key_size, 'x')}, 1)), "exact leaf insert failed");
    BTreePage exact_page {std::move(exact_leaf)};
    auto exact_size = BTreePageCodec::encoded_size(exact_page, type);
    require(exact_size.has_value() && *exact_size == BTreePageCodec::PageSize, "exact page size mismatch");
    auto exact_fits = BTreePageCodec::can_fit(exact_page, type);
    require(exact_fits.has_value() && *exact_fits, "exactly full page should fit");
    require(BTreePageCodec::encode(exact_page, type).has_value(), "exactly full page encode failed");

    BTreeLeafPage overflow_leaf {71};
    require(overflow_leaf.insert(entry(Value {std::string(maximum_key_size + 1, 'x')}, 1)), "overflow leaf insert failed");
    BTreePage overflow_page {std::move(overflow_leaf)};
    auto overflow_fits = BTreePageCodec::can_fit(overflow_page, type);
    require(overflow_fits.has_value() && !*overflow_fits, "overflow page should not fit");
    auto overflow = BTreePageCodec::encode(overflow_page, type);
    require(!overflow.has_value(), "overflow page encode should fail");
    require(overflow.error().code == BTreePageCodecErrorCode::PageTooLarge, "overflow error code mismatch");
}

void test_encode_rejects_invalid_page_and_key_contracts()
{
    const common::LogicalType integer {common::LogicalTypeId::Integer, std::nullopt};
    BTreePage invalid_id = BTreeLeafPage {InvalidBTreePageId};
    auto invalid_page = BTreePageCodec::encode(invalid_id, integer);
    require(!invalid_page.has_value(), "invalid page id should fail encode");
    require(invalid_page.error().code == BTreePageCodecErrorCode::InvalidPage, "invalid page error code mismatch");

    BTreeLeafPage leaf {80};
    require(leaf.insert(entry(Value {std::int32_t {1}}, 1)), "type mismatch leaf insert failed");
    BTreePage page {std::move(leaf)};
    const common::LogicalType bigint {common::LogicalTypeId::BigInt, std::nullopt};
    auto mismatch = BTreePageCodec::encode(page, bigint);
    require(!mismatch.has_value(), "key type mismatch should fail encode");
    require(mismatch.error().code == BTreePageCodecErrorCode::KeyTypeMismatch, "key type mismatch error code mismatch");

    const common::LogicalType vector {common::LogicalTypeId::Vector, 3};
    auto unsupported = BTreePageCodec::encode(page, vector);
    require(!unsupported.has_value(), "vector key type should fail encode");
    require(unsupported.error().code == BTreePageCodecErrorCode::UnsupportedKeyType, "unsupported key error code mismatch");
}

void test_decode_rejects_invalid_headers_and_slots()
{
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    BTreeLeafPage leaf {90, 89, 91};
    require(leaf.insert(entry(Value {std::int32_t {7}}, 700)), "corruption test insert failed");
    auto encoded = BTreePageCodec::encode(BTreePage {std::move(leaf)}, type);
    require(encoded.has_value(), "corruption test encode failed");

    auto truncated = BTreePageCodec::decode(
        std::span<const std::byte>(*encoded).first(BTreePageCodec::PageSize - 1),
        type,
        90
    );
    require(!truncated.has_value() && truncated.error().code == BTreePageCodecErrorCode::InvalidFormat,
            "truncated page should fail");

    auto bad_magic = *encoded;
    bad_magic[0] = std::byte {0};
    auto magic = BTreePageCodec::decode(bad_magic, type, 90);
    require(!magic.has_value() && magic.error().code == BTreePageCodecErrorCode::InvalidFormat,
            "bad magic should fail");

    auto bad_version = *encoded;
    write_number<std::uint16_t>(bad_version.data() + 4, 2);
    auto version = BTreePageCodec::decode(bad_version, type, 90);
    require(!version.has_value() && version.error().code == BTreePageCodecErrorCode::UnsupportedVersion,
            "bad version should fail");

    auto bad_type = *encoded;
    bad_type[12] = std::byte {99};
    auto page_type = BTreePageCodec::decode(bad_type, type, 90);
    require(!page_type.has_value() && page_type.error().code == BTreePageCodecErrorCode::InvalidFormat,
            "bad page type should fail");

    auto wrong_id = BTreePageCodec::decode(*encoded, type, 999);
    require(!wrong_id.has_value() && wrong_id.error().code == BTreePageCodecErrorCode::InvalidFormat,
            "unexpected page id should fail");

    auto bad_slot = *encoded;
    write_number<std::uint16_t>(bad_slot.data() + BTreePageCodec::HeaderSize, 0);
    auto slot = BTreePageCodec::decode(bad_slot, type, 90);
    require(!slot.has_value() && slot.error().code == BTreePageCodecErrorCode::CorruptedPage,
            "bad slot should fail");
}

void test_decode_rejects_invalid_boolean_payload()
{
    const common::LogicalType type {common::LogicalTypeId::Boolean, std::nullopt};
    BTreeLeafPage leaf {100};
    require(leaf.insert(entry(Value {true}, 1)), "boolean test insert failed");
    auto encoded = BTreePageCodec::encode(BTreePage {std::move(leaf)}, type);
    require(encoded.has_value(), "boolean test encode failed");

    const auto payload_offset = read_number<std::uint16_t>(encoded->data() + BTreePageCodec::HeaderSize);
    (*encoded)[payload_offset] = std::byte {2};
    auto decoded = BTreePageCodec::decode(*encoded, type, 100);
    require(!decoded.has_value(), "invalid boolean payload should fail decode");
    require(decoded.error().code == BTreePageCodecErrorCode::CorruptedPage, "invalid boolean error code mismatch");
}

void test_decode_rejects_out_of_order_entries()
{
    const common::LogicalType type {common::LogicalTypeId::Integer, std::nullopt};
    BTreeLeafPage leaf {110};
    require(leaf.insert(entry(Value {std::int32_t {1}}, 1)), "ordered test first insert failed");
    require(leaf.insert(entry(Value {std::int32_t {2}}, 2)), "ordered test second insert failed");
    auto encoded = BTreePageCodec::encode(BTreePage {std::move(leaf)}, type);
    require(encoded.has_value(), "ordered test encode failed");

    const auto first_payload = read_number<std::uint16_t>(encoded->data() + BTreePageCodec::HeaderSize);
    write_number<std::int32_t>(encoded->data() + first_payload, 3);
    auto decoded = BTreePageCodec::decode(*encoded, type, 110);
    require(!decoded.has_value(), "out-of-order leaf entries should fail decode");
    require(decoded.error().code == BTreePageCodecErrorCode::CorruptedPage, "out-of-order error code mismatch");
}

} // namespace

int main()
{
    try {
        test_leaf_round_trip_for_all_scalar_key_types();
        test_internal_page_round_trip();
        test_page_capacity_uses_encoded_bytes();
        test_encode_rejects_invalid_page_and_key_contracts();
        test_decode_rejects_invalid_headers_and_slots();
        test_decode_rejects_invalid_boolean_payload();
        test_decode_rejects_out_of_order_entries();
    } catch (const std::exception & error) {
        std::cerr << "btree_page_codec_tests failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
