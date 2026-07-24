#include "core/index/btree_index/btree_page_codec.hpp"

#include <cstdint>
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/common/value.hpp"
#include "core/io/checksum.hpp"

namespace litedb::core::index::btree_index
{

namespace
{

constexpr std::uint32_t PageMagic = 0x31505442; // BTP1
constexpr std::uint16_t LegacyPageVersion = 1;
constexpr std::uint16_t PageVersion = 2;
constexpr std::uint8_t EncodedInternalPage = 1;
constexpr std::uint8_t EncodedLeafPage = 2;

constexpr std::size_t MagicOffset = 0;
constexpr std::size_t VersionOffset = 4;
constexpr std::size_t HeaderSizeOffset = 6;
constexpr std::size_t PageSizeOffset = 8;
constexpr std::size_t PageTypeOffset = 12;
constexpr std::size_t ReservedByteOffset = 13;
constexpr std::size_t EntryCountOffset = 14;
constexpr std::size_t PageIdOffset = 16;
constexpr std::size_t FirstLinkOffset = 24;
constexpr std::size_t SecondLinkOffset = 32;
constexpr std::size_t FreeStartOffset = 40;
constexpr std::size_t FreeEndOffset = 42;
constexpr std::size_t ChecksumOffset = 44;

[[nodiscard]]
BTreePageCodecError make_codec_error(BTreePageCodecErrorCode code, std::string message)
{
    return BTreePageCodecError {code, std::move(message)};
}

/**
 * @brief 写入数字到目标缓冲区
 */
template <typename T>
requires std::is_integral_v<T>
void write_number(std::byte * target, T value) noexcept
{
    using Unsigned = std::make_unsigned_t<T>;
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        target[index] = static_cast<std::byte>((bits >> (index * 8U)) & static_cast<Unsigned>(0xffU));
    }
}

/**
 * @brief 从源缓冲区读取数字
 */
template <typename T>
requires std::is_integral_v<T>
[[nodiscard]]
T read_number(const std::byte * source) noexcept
{
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned value {0};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value |= static_cast<Unsigned>(std::to_integer<unsigned int>(source[index])) << (index * 8U);
    }
    return static_cast<T>(value);
}

[[nodiscard]]
std::uint32_t checksum_with_zeroed_field(std::span<const std::byte> bytes)
{
    BTreePageCodec::PageBuffer checked {};
    std::copy(bytes.begin(), bytes.end(), checked.begin());
    write_number(checked.data() + ChecksumOffset, std::uint32_t {0});
    return io::crc32(checked);
}

/**
 * @brief 追加数字到目标缓冲区
 */
template <typename T>
requires std::is_integral_v<T>
void append_number(std::vector<std::byte> & target, T value)
{
    const auto offset = target.size();
    target.resize(offset + sizeof(T));
    write_number(target.data() + offset, value);
}

/**
 * @brief 判断键类型是否支持
 */
[[nodiscard]]
bool is_supported_key_type(const common::LogicalType & key_type) noexcept
{
    switch (key_type.id) {
    case common::LogicalTypeId::Boolean:
    case common::LogicalTypeId::Integer:
    case common::LogicalTypeId::BigInt:
    case common::LogicalTypeId::Float:
    case common::LogicalTypeId::Double:
    case common::LogicalTypeId::Varchar:
        return true;
    case common::LogicalTypeId::Null:
    case common::LogicalTypeId::Vector:
        return false;
    }
    return false;
}

/**
 * @brief 验证键是否有效
 */
[[nodiscard]]
std::expected<void, BTreePageCodecError> validate_key(
    const ScalarIndexKey & key,
    const common::LogicalType & key_type
)
{
    if (!is_supported_key_type(key_type)) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::UnsupportedKeyType, "Unsupported B+ tree key type"));
    }
    if (!key.value().matches_type(key_type) || key.value().is_null()) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::KeyTypeMismatch, "B+ tree key does not match page key type"));
    }
    if (key_type.id == common::LogicalTypeId::Varchar && key_type.parameter.has_value()) {
        const auto & value = std::get<std::string>(key.value().data());
        if (value.size() > *key_type.parameter) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::KeyTypeMismatch, "B+ tree string key exceeds declared length"));
        }
    }
    return {};
}

/**
 * @brief 编码键
 */
[[nodiscard]]
std::expected<std::vector<std::byte>, BTreePageCodecError> encode_key(
    const ScalarIndexKey & key,
    const common::LogicalType & key_type
)
{
    auto valid = validate_key(key, key_type);
    if (!valid.has_value()) {
        return std::unexpected(std::move(valid.error()));
    }

    std::vector<std::byte> bytes;
    switch (key_type.id) {
    case common::LogicalTypeId::Boolean:
        append_number(bytes, static_cast<std::uint8_t>(std::get<bool>(key.value().data()) ? 1 : 0));
        break;
    case common::LogicalTypeId::Integer:
        append_number(bytes, std::get<std::int32_t>(key.value().data()));
        break;
    case common::LogicalTypeId::BigInt:
        append_number(bytes, std::get<std::int64_t>(key.value().data()));
        break;
    case common::LogicalTypeId::Float:
        append_number(bytes, std::bit_cast<std::uint32_t>(std::get<float>(key.value().data())));
        break;
    case common::LogicalTypeId::Double:
        append_number(bytes, std::bit_cast<std::uint64_t>(std::get<double>(key.value().data())));
        break;
    case common::LogicalTypeId::Varchar: {
        const auto & value = std::get<std::string>(key.value().data());
        bytes.resize(value.size());
        if (!value.empty()) {
            std::memcpy(bytes.data(), value.data(), value.size());
        }
        break;
    }
    case common::LogicalTypeId::Null:
    case common::LogicalTypeId::Vector:
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::UnsupportedKeyType, "Unsupported B+ tree key type"));
    }
    return bytes;
}

/**
 * @brief 获取固定键大小
 */
[[nodiscard]]
std::optional<std::size_t> fixed_key_size(const common::LogicalType & key_type) noexcept
{
    switch (key_type.id) {
    case common::LogicalTypeId::Boolean:
        return 1;
    case common::LogicalTypeId::Integer:
    case common::LogicalTypeId::Float:
        return 4;
    case common::LogicalTypeId::BigInt:
    case common::LogicalTypeId::Double:
        return 8;
    case common::LogicalTypeId::Varchar:
    case common::LogicalTypeId::Null:
    case common::LogicalTypeId::Vector:
        return std::nullopt;
    }
    return std::nullopt;
}

/**
 * @brief 解码键
 */
[[nodiscard]]
std::expected<ScalarIndexKey, BTreePageCodecError> decode_key(
    std::span<const std::byte> bytes,
    const common::LogicalType & key_type
)
{
    if (!is_supported_key_type(key_type)) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::UnsupportedKeyType, "Unsupported B+ tree key type"));
    }

    common::Value value;
    switch (key_type.id) {
    case common::LogicalTypeId::Boolean:
        if (bytes.size() != 1 || std::to_integer<std::uint8_t>(bytes[0]) > 1) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Invalid encoded Boolean index key"));
        }
        value = common::Value {std::to_integer<std::uint8_t>(bytes[0]) == 1};
        break;
    case common::LogicalTypeId::Integer:
        if (bytes.size() != sizeof(std::int32_t)) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Invalid encoded Integer index key"));
        }
        value = common::Value {read_number<std::int32_t>(bytes.data())};
        break;
    case common::LogicalTypeId::BigInt:
        if (bytes.size() != sizeof(std::int64_t)) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Invalid encoded BigInt index key"));
        }
        value = common::Value {read_number<std::int64_t>(bytes.data())};
        break;
    case common::LogicalTypeId::Float:
        if (bytes.size() != sizeof(std::uint32_t)) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Invalid encoded Float index key"));
        }
        value = common::Value {std::bit_cast<float>(read_number<std::uint32_t>(bytes.data()))};
        break;
    case common::LogicalTypeId::Double:
        if (bytes.size() != sizeof(std::uint64_t)) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Invalid encoded Double index key"));
        }
        value = common::Value {std::bit_cast<double>(read_number<std::uint64_t>(bytes.data()))};
        break;
    case common::LogicalTypeId::Varchar:
        if (key_type.parameter.has_value() && bytes.size() > *key_type.parameter) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Encoded Varchar index key exceeds declared length"));
        }
        value = common::Value {bytes.empty()
            ? std::string {}
            : std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size())};
        break;
    case common::LogicalTypeId::Null:
    case common::LogicalTypeId::Vector:
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::UnsupportedKeyType, "Unsupported B+ tree key type"));
    }

    auto key = ScalarIndexKey::from_value(std::move(value));
    if (!key.has_value()) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Encoded index key is invalid"));
    }
    return std::move(*key);
}

/**
 * @brief 编码条目
 */
[[nodiscard]]
std::expected<std::vector<std::byte>, BTreePageCodecError> encode_entry(
    const BTreeEntryKey & entry,
    const common::LogicalType & key_type,
    std::optional<BTreePageId> right_child_id
)
{
    auto bytes = encode_key(entry.key, key_type);
    if (!bytes.has_value()) {
        return std::unexpected(std::move(bytes.error()));
    }
    append_number(*bytes, entry.record_id);
    if (right_child_id.has_value()) {
        if (*right_child_id == InvalidBTreePageId) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidPage, "Internal page contains an invalid child page id"));
        }
        append_number(*bytes, *right_child_id);
    }
    return bytes;
}

/**
 * @brief 解码条目
 */
struct DecodedEntry
{
    BTreeEntryKey key;                             ///< 条目键
    std::optional<BTreePageId> right_child_id;     ///< 右子页 ID
};

/**
 * @brief 解码条目
 */
[[nodiscard]]
std::expected<DecodedEntry, BTreePageCodecError> decode_entry(
    std::span<const std::byte> bytes,
    const common::LogicalType & key_type,
    bool internal
)
{
    const std::size_t tail_size = sizeof(common::RecordId) + (internal ? sizeof(BTreePageId) : 0);
    if (bytes.size() < tail_size) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "B+ tree entry payload is truncated"));
    }
    const auto key_size = bytes.size() - tail_size;
    if (const auto fixed = fixed_key_size(key_type); fixed.has_value() && key_size != *fixed) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "B+ tree entry key has an invalid encoded size"));
    }
    auto key = decode_key(bytes.first(key_size), key_type);
    if (!key.has_value()) {
        return std::unexpected(std::move(key.error()));
    }
    const auto record_id = read_number<common::RecordId>(bytes.data() + key_size);
    std::optional<BTreePageId> child;
    if (internal) {
        child = read_number<BTreePageId>(bytes.data() + key_size + sizeof(common::RecordId));
        if (*child == InvalidBTreePageId) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Internal entry has an invalid child page id"));
        }
    }
    return DecodedEntry {
        .key = BTreeEntryKey {
            .key = std::move(*key),
            .record_id = record_id,
        },
        .right_child_id = child,
    };
}

/**
 * @brief 验证页身份
 */
[[nodiscard]]
std::expected<void, BTreePageCodecError> validate_page_identity(const BTreePage & page)
{
    const auto page_id = btree_page_id(page);
    if (page_id == InvalidBTreePageId) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidPage, "B+ tree page id is invalid"));
    }
    if (const auto * leaf = std::get_if<BTreeLeafPage>(&page)) {
        if (leaf->previous_page_id() == page_id || leaf->next_page_id() == page_id) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidPage, "Leaf page links to itself"));
        }
        return {};
    }

    const auto & internal = std::get<BTreeInternalPage>(page);
    if (internal.first_child_id() == InvalidBTreePageId || internal.first_child_id() == page_id) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidPage, "Internal page has an invalid first child"));
    }
    for (const auto & entry : internal.entries()) {
        if (entry.right_child_id == page_id) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidPage, "Internal page links to itself"));
        }
    }
    return {};
}

/**
 * @brief 编码条目
 */
[[nodiscard]]
std::expected<std::vector<std::vector<std::byte>>, BTreePageCodecError> encode_entries(
    const BTreePage & page,
    const common::LogicalType & key_type
)
{
    if (!is_supported_key_type(key_type)) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::UnsupportedKeyType, "Unsupported B+ tree key type"));
    }
    auto valid_page = validate_page_identity(page);
    if (!valid_page.has_value()) {
        return std::unexpected(std::move(valid_page.error()));
    }

    std::vector<std::vector<std::byte>> encoded;
    if (const auto * leaf = std::get_if<BTreeLeafPage>(&page)) {
        encoded.reserve(leaf->entries().size());
        for (const auto & entry : leaf->entries()) {
            auto bytes = encode_entry(entry, key_type, std::nullopt);
            if (!bytes.has_value()) {
                return std::unexpected(std::move(bytes.error()));
            }
            encoded.push_back(std::move(bytes.value()));
        }
    } else {
        const auto & internal = std::get<BTreeInternalPage>(page);
        encoded.reserve(internal.entries().size());
        for (const auto & entry : internal.entries()) {
            auto bytes = encode_entry(entry.separator, key_type, entry.right_child_id);
            if (!bytes.has_value()) {
                return std::unexpected(std::move(bytes.error()));
            }
            encoded.push_back(std::move(bytes.value()));
        }
    }
    return encoded;
}

/**
 * @brief 从条目计算编码后的实际占用字节数
 */
[[nodiscard]]
std::expected<std::size_t, BTreePageCodecError> encoded_size_from_entries(
    const std::vector<std::vector<std::byte>> & entries
)
{
    if (entries.size() > std::numeric_limits<std::uint16_t>::max()) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::PageTooLarge, "B+ tree page has too many entries"));
    }
    std::size_t size = BTreePageCodec::HeaderSize + entries.size() * BTreePageCodec::SlotSize;
    for (const auto & entry : entries) {
        if (entry.size() > std::numeric_limits<std::uint16_t>::max() ||
            size > std::numeric_limits<std::size_t>::max() - entry.size()) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::PageTooLarge, "B+ tree entry is too large"));
        }
        size += entry.size();
    }
    return size;
}

/**
 * @brief 写入物理页头部
 */
void write_header(
    BTreePageCodec::PageBuffer & buffer,
    const BTreePage & page,
    std::uint16_t entry_count,
    std::uint16_t free_start,
    std::uint16_t free_end
) noexcept
{
    write_number(buffer.data() + MagicOffset, PageMagic);
    write_number(buffer.data() + VersionOffset, PageVersion);
    write_number(buffer.data() + HeaderSizeOffset, static_cast<std::uint16_t>(BTreePageCodec::HeaderSize));
    write_number(buffer.data() + PageSizeOffset, static_cast<std::uint32_t>(BTreePageCodec::PageSize));
    write_number(
        buffer.data() + PageTypeOffset,
        btree_page_type(page) == BTreePageType::Leaf ? EncodedLeafPage : EncodedInternalPage
    );
    write_number(buffer.data() + ReservedByteOffset, static_cast<std::uint8_t>(0));
    write_number(buffer.data() + EntryCountOffset, entry_count);
    write_number(buffer.data() + PageIdOffset, btree_page_id(page));
    if (const auto * leaf = std::get_if<BTreeLeafPage>(&page)) {
        write_number(buffer.data() + FirstLinkOffset, leaf->previous_page_id());
        write_number(buffer.data() + SecondLinkOffset, leaf->next_page_id());
    } else {
        write_number(buffer.data() + FirstLinkOffset, std::get<BTreeInternalPage>(page).first_child_id());
        write_number(buffer.data() + SecondLinkOffset, static_cast<BTreePageId>(0));
    }
    write_number(buffer.data() + FreeStartOffset, free_start);
    write_number(buffer.data() + FreeEndOffset, free_end);
    write_number(buffer.data() + ChecksumOffset, static_cast<std::uint32_t>(0));
}

} // namespace

std::expected<std::size_t, BTreePageCodecError> BTreePageCodec::encoded_size(
    const BTreePage & page,
    const common::LogicalType & key_type
)
{
    auto entries = encode_entries(page, key_type);
    if (!entries.has_value()) {
        return std::unexpected(std::move(entries.error()));
    }
    return encoded_size_from_entries(*entries);
}

std::expected<bool, BTreePageCodecError> BTreePageCodec::can_fit(
    const BTreePage & page,
    const common::LogicalType & key_type
)
{
    auto size = encoded_size(page, key_type);
    if (!size.has_value()) {
        return std::unexpected(std::move(size.error()));
    }
    return *size <= PageSize;
}

std::expected<BTreePageCodec::PageBuffer, BTreePageCodecError> BTreePageCodec::encode(
    const BTreePage & page,
    const common::LogicalType & key_type
)
{
    auto entries = encode_entries(page, key_type);
    if (!entries.has_value()) {
        return std::unexpected(std::move(entries.error()));
    }
    auto size = encoded_size_from_entries(*entries);
    if (!size.has_value()) {
        return std::unexpected(std::move(size.error()));
    }
    if (*size > PageSize) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::PageTooLarge, "B+ tree page does not fit in one physical page"));
    }

    PageBuffer buffer {};
    auto free_end = static_cast<std::uint16_t>(PageSize);
    for (std::size_t index = 0; index < entries->size(); ++index) {
        const auto & entry = (*entries)[index];
        free_end = static_cast<std::uint16_t>(free_end - entry.size());
        std::copy(entry.begin(), entry.end(), buffer.begin() + free_end);
        const auto slot_offset = HeaderSize + index * SlotSize;
        write_number(buffer.data() + slot_offset, free_end);
        write_number(buffer.data() + slot_offset + 2, static_cast<std::uint16_t>(entry.size()));
    }
    const auto free_start = static_cast<std::uint16_t>(HeaderSize + entries->size() * SlotSize);
    write_header(
        buffer,
        page,
        static_cast<std::uint16_t>(entries->size()),
        free_start,
        free_end
    );
    write_number(buffer.data() + ChecksumOffset, io::crc32(buffer));
    return buffer;
}

std::expected<BTreePage, BTreePageCodecError> BTreePageCodec::decode(
    std::span<const std::byte> bytes,
    const common::LogicalType & key_type,
    BTreePageId expected_page_id
)
{
    if (!is_supported_key_type(key_type)) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::UnsupportedKeyType, "Unsupported B+ tree key type"));
    }
    if (bytes.size() != PageSize) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidFormat, "B+ tree physical page has an invalid size"));
    }
    if (read_number<std::uint32_t>(bytes.data() + MagicOffset) != PageMagic) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidFormat, "Invalid B+ tree page magic"));
    }
    const auto version = read_number<std::uint16_t>(bytes.data() + VersionOffset);
    if (version != LegacyPageVersion && version != PageVersion) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::UnsupportedVersion, "Unsupported B+ tree page version"));
    }
    if (read_number<std::uint16_t>(bytes.data() + HeaderSizeOffset) != HeaderSize ||
        read_number<std::uint32_t>(bytes.data() + PageSizeOffset) != PageSize ||
        read_number<std::uint8_t>(bytes.data() + ReservedByteOffset) != 0) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidFormat, "Invalid B+ tree page header"));
    }
    if (version == LegacyPageVersion) {
        if (read_number<std::uint32_t>(bytes.data() + ChecksumOffset) != 0) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidFormat, "Invalid legacy B+ tree page header"));
        }
    } else if (read_number<std::uint32_t>(bytes.data() + ChecksumOffset)
               != checksum_with_zeroed_field(bytes)) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::ChecksumMismatch, "B+ tree page checksum mismatch"));
    }

    const auto page_id = read_number<BTreePageId>(bytes.data() + PageIdOffset);
    if (page_id == InvalidBTreePageId || page_id != expected_page_id) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidFormat, "B+ tree page id does not match requested page"));
    }
    const auto encoded_type = read_number<std::uint8_t>(bytes.data() + PageTypeOffset);
    if (encoded_type != EncodedLeafPage && encoded_type != EncodedInternalPage) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::InvalidFormat, "Invalid B+ tree page type"));
    }
    const auto type = encoded_type == EncodedLeafPage ? BTreePageType::Leaf : BTreePageType::Internal;
    const auto entry_count = read_number<std::uint16_t>(bytes.data() + EntryCountOffset);
    const auto free_start = read_number<std::uint16_t>(bytes.data() + FreeStartOffset);
    const auto free_end = read_number<std::uint16_t>(bytes.data() + FreeEndOffset);
    const auto expected_free_start = HeaderSize + static_cast<std::size_t>(entry_count) * SlotSize;
    if (expected_free_start > PageSize || free_start != expected_free_start || free_end < free_start) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Invalid B+ tree slot directory bounds"));
    }

    std::vector<std::span<const std::byte>> payloads;
    payloads.reserve(entry_count);
    auto expected_payload_end = PageSize;
    for (std::size_t index = 0; index < entry_count; ++index) {
        const auto slot_offset = HeaderSize + index * SlotSize;
        const auto payload_offset = read_number<std::uint16_t>(bytes.data() + slot_offset);
        const auto payload_size = read_number<std::uint16_t>(bytes.data() + slot_offset + 2);
        if (payload_size == 0 || payload_offset < free_end ||
            static_cast<std::size_t>(payload_offset) + payload_size != expected_payload_end) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Invalid B+ tree slot payload"));
        }
        payloads.push_back(bytes.subspan(payload_offset, payload_size));
        expected_payload_end = payload_offset;
    }
    if (expected_payload_end != free_end) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "B+ tree payload area is not canonical"));
    }

    const auto first_link = read_number<BTreePageId>(bytes.data() + FirstLinkOffset);
    const auto second_link = read_number<BTreePageId>(bytes.data() + SecondLinkOffset);
    if (type == BTreePageType::Leaf) {
        if (first_link == page_id || second_link == page_id) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Leaf page links to itself"));
        }
        BTreeLeafPage leaf {page_id, first_link, second_link};
        for (const auto payload : payloads) {
            auto entry = decode_entry(payload, key_type, false);
            if (!entry.has_value()) {
                return std::unexpected(std::move(entry.error()));
            }
            if (!leaf.empty() &&
                compare_btree_entry_keys(leaf.entries().back(), entry->key) != std::strong_ordering::less) {
                return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Leaf page entries are not strictly ordered"));
            }
            if (!leaf.insert(std::move(entry->key))) {
                return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Leaf page entries are duplicated"));
            }
        }
        return BTreePage {std::move(leaf)};
    }

    if (first_link == InvalidBTreePageId || first_link == page_id || second_link != InvalidBTreePageId) {
        return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Internal page links are invalid"));
    }
    BTreeInternalPage internal {page_id, first_link};
    auto left_child_id = first_link;
    for (const auto payload : payloads) {
        auto entry = decode_entry(payload, key_type, true);
        if (!entry.has_value()) {
            return std::unexpected(std::move(entry.error()));
        }
        if (*entry->right_child_id == page_id ||
            !internal.insert_child_after(left_child_id, std::move(entry->key), *entry->right_child_id)) {
            return std::unexpected(make_codec_error(BTreePageCodecErrorCode::CorruptedPage, "Internal page entries or child links are invalid"));
        }
        left_child_id = *entry->right_child_id;
    }
    return BTreePage {std::move(internal)};
}

} // namespace litedb::core::index::btree_index
