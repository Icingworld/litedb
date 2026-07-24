#include "core/index/btree_index/btree_page_store.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/io/checksum.hpp"

namespace litedb::core::index::btree_index
{

namespace
{

constexpr std::uint32_t StoreMagic = 0x3149424c; // LBI1
constexpr std::uint16_t LegacyStoreVersion = 1;
constexpr std::uint16_t StoreVersion = 2;

constexpr std::size_t MagicOffset = 0;
constexpr std::size_t VersionOffset = 4;
constexpr std::size_t HeaderSizeOffset = 6;
constexpr std::size_t PageSizeOffset = 8;
constexpr std::size_t KeyTypeOffset = 12;
constexpr std::size_t FlagsOffset = 13;
constexpr std::size_t ReservedOffset = 14;
constexpr std::size_t IndexIdOffset = 16;
constexpr std::size_t KeyParameterOffset = 24;
constexpr std::size_t RootPageIdOffset = 32;
constexpr std::size_t NextPageIdOffset = 40;
constexpr std::size_t EntryCountOffset = 48;
constexpr std::size_t HeaderChecksumOffset = 56;
constexpr std::size_t FreePageHeadOffset = 64;
constexpr std::size_t FreePageCountOffset = 72;

constexpr std::uint8_t HasKeyParameter = 0x01;

constexpr std::uint32_t FreePageMagic = 0x31465242; // BRF1
constexpr std::uint16_t FreePageVersion = 1;
constexpr std::size_t FreePageVersionOffset = 4;
constexpr std::size_t FreePageReservedOffset = 6;
constexpr std::size_t FreePageIdOffset = 8;
constexpr std::size_t FreePageNextOffset = 16;
constexpr std::size_t FreePageChecksumOffset = 24;

using Error = BTreePageStoreError;
using ErrorCode = BTreePageStoreErrorCode;

[[nodiscard]]
Error error(ErrorCode code, std::string message)
{
    return Error {code, std::move(message), std::nullopt};
}

[[nodiscard]]
Error filesystem_error(litedb::core::error::Error value)
{
    return error(ErrorCode::FileSystemError, value.message());
}

[[nodiscard]]
Error codec_error(BTreePageCodecError value)
{
    const auto code = value.code == BTreePageCodecErrorCode::ChecksumMismatch
        ? ErrorCode::ChecksumMismatch
        : value.code == BTreePageCodecErrorCode::CorruptedPage ||
                      value.code == BTreePageCodecErrorCode::InvalidFormat ||
                      value.code == BTreePageCodecErrorCode::UnsupportedVersion
        ? ErrorCode::CorruptedPage
        : ErrorCode::PageCodecError;
    return Error {code, std::move(value.message), value.code};
}

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
std::uint32_t header_checksum_with_zeroed_field(std::span<const std::byte> bytes)
{
    std::array<std::byte, BTreePageStore::HeaderSize> checked {};
    std::copy(bytes.begin(), bytes.end(), checked.begin());
    write_number(checked.data() + HeaderChecksumOffset, std::uint32_t {0});
    return io::crc32(checked);
}

[[nodiscard]]
std::uint32_t free_page_checksum_with_zeroed_field(std::span<const std::byte> bytes)
{
    BTreePageCodec::PageBuffer checked {};
    std::copy(bytes.begin(), bytes.end(), checked.begin());
    write_number(checked.data() + FreePageChecksumOffset, std::uint32_t {0});
    return io::crc32(checked);
}

[[nodiscard]]
std::optional<std::uint8_t> encode_key_type(const common::LogicalType & type) noexcept
{
    switch (type.id) {
    case common::LogicalTypeId::Boolean:
        return 1;
    case common::LogicalTypeId::Integer:
        return 2;
    case common::LogicalTypeId::BigInt:
        return 3;
    case common::LogicalTypeId::Float:
        return 4;
    case common::LogicalTypeId::Double:
        return 5;
    case common::LogicalTypeId::Varchar:
        return 6;
    case common::LogicalTypeId::Null:
    case common::LogicalTypeId::Vector:
        return std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]]
bool valid_key_type(const common::LogicalType & type) noexcept
{
    if (!encode_key_type(type).has_value()) {
        return false;
    }
    return type.id == common::LogicalTypeId::Varchar || !type.parameter.has_value();
}

[[nodiscard]]
bool same_key_type(const common::LogicalType & left, const common::LogicalType & right) noexcept
{
    return left.id == right.id && left.parameter == right.parameter;
}

[[nodiscard]]
std::optional<common::LogicalTypeId> decode_key_type(std::uint8_t encoded) noexcept
{
    switch (encoded) {
    case 1:
        return common::LogicalTypeId::Boolean;
    case 2:
        return common::LogicalTypeId::Integer;
    case 3:
        return common::LogicalTypeId::BigInt;
    case 4:
        return common::LogicalTypeId::Float;
    case 5:
        return common::LogicalTypeId::Double;
    case 6:
        return common::LogicalTypeId::Varchar;
    default:
        return std::nullopt;
    }
}

} // namespace

BTreePageStore::BTreePageStore(
    std::filesystem::path path,
    common::IndexId index_id,
    common::LogicalType key_type,
    filesystem::FileHandle file
) noexcept
    : path_(std::move(path))
    , index_id_(index_id)
    , key_type_(std::move(key_type))
    , file_(std::move(file))
{
}

std::expected<BTreePageStore, BTreePageStoreError> BTreePageStore::create(
    std::filesystem::path path,
    common::IndexId index_id,
    common::LogicalType key_type,
    filesystem::FileSystem & filesystem
)
{
    if (!valid_key_type(key_type)) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree store key type"));
    }
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        auto created = filesystem.create_dir_all(parent);
        if (!created.has_value()) {
            return std::unexpected(filesystem_error(std::move(created.error())));
        }
    }
    auto opened = filesystem.open(path, {
        .access = filesystem::FileAccess::ReadWrite,
        .create_mode = filesystem::FileCreateMode::CreateNew,
    });
    if (!opened.has_value()) {
        return std::unexpected(filesystem_error(std::move(opened.error())));
    }
    BTreePageStore store {std::move(path), index_id, std::move(key_type), std::move(*opened)};
    auto initialized = store.initialize();
    if (!initialized.has_value()) {
        return std::unexpected(std::move(initialized.error()));
    }
    return store;
}

std::expected<BTreePageStore, BTreePageStoreError> BTreePageStore::open(
    std::filesystem::path path,
    common::IndexId expected_index_id,
    common::LogicalType expected_key_type,
    filesystem::FileSystem & filesystem
)
{
    if (!valid_key_type(expected_key_type)) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid expected B+ tree key type"));
    }
    auto opened = filesystem.open(path, {
        .access = filesystem::FileAccess::ReadWrite,
        .create_mode = filesystem::FileCreateMode::OpenExisting,
    });
    if (!opened.has_value()) {
        return std::unexpected(filesystem_error(std::move(opened.error())));
    }
    BTreePageStore store {
        std::move(path),
        expected_index_id,
        expected_key_type,
        std::move(*opened),
    };
    auto loaded = store.load(expected_index_id, expected_key_type);
    if (!loaded.has_value()) {
        return std::unexpected(std::move(loaded.error()));
    }
    return store;
}

const std::filesystem::path & BTreePageStore::path() const noexcept
{
    return path_;
}

common::IndexId BTreePageStore::index_id() const noexcept
{
    return index_id_;
}

const common::LogicalType & BTreePageStore::key_type() const noexcept
{
    return key_type_;
}

BTreePageId BTreePageStore::root_page_id() const noexcept
{
    return root_page_id_;
}

std::uint64_t BTreePageStore::page_count() const noexcept
{
    return next_page_id_ - 1;
}

std::uint64_t BTreePageStore::free_page_count() const noexcept
{
    return free_page_count_;
}

std::uint64_t BTreePageStore::entry_count() const noexcept
{
    return entry_count_;
}

std::expected<BTreeLeafPage, BTreePageStoreError> BTreePageStore::allocate_leaf_page(
    BTreePageId previous_page_id,
    BTreePageId next_page_id
)
{
    if ((previous_page_id != InvalidBTreePageId && previous_page_id >= next_page_id_) ||
        (next_page_id != InvalidBTreePageId && next_page_id >= next_page_id_)) {
        return std::unexpected(error(ErrorCode::PageNotFound, "Leaf page link references an unknown page"));
    }
    auto free_page = acquire_free_page();
    if (!free_page.has_value()) {
        return std::unexpected(std::move(free_page.error()));
    }
    if (free_page->has_value()) {
        BTreeLeafPage page {**free_page, previous_page_id, next_page_id};
        auto written = write_page(BTreePage {page});
        if (!written.has_value()) {
            return std::unexpected(std::move(written.error()));
        }
        return page;
    }

    BTreeLeafPage page {next_page_id_, previous_page_id, next_page_id};
    auto appended = append_page(BTreePage {page});
    if (!appended.has_value()) {
        return std::unexpected(std::move(appended.error()));
    }
    return page;
}

std::expected<BTreeInternalPage, BTreePageStoreError> BTreePageStore::allocate_internal_page(
    BTreePageId first_child_id
)
{
    if (first_child_id == InvalidBTreePageId || first_child_id >= next_page_id_) {
        return std::unexpected(error(ErrorCode::PageNotFound, "Internal page first child does not exist"));
    }
    auto free_page = acquire_free_page();
    if (!free_page.has_value()) {
        return std::unexpected(std::move(free_page.error()));
    }
    if (free_page->has_value()) {
        BTreeInternalPage page {**free_page, first_child_id};
        auto written = write_page(BTreePage {page});
        if (!written.has_value()) {
            return std::unexpected(std::move(written.error()));
        }
        return page;
    }

    BTreeInternalPage page {next_page_id_, first_child_id};
    auto appended = append_page(BTreePage {page});
    if (!appended.has_value()) {
        return std::unexpected(std::move(appended.error()));
    }
    return page;
}

std::expected<BTreePage, BTreePageStoreError> BTreePageStore::read_page(BTreePageId page_id) const
{
    if (page_id == InvalidBTreePageId || page_id >= next_page_id_) {
        return std::unexpected(error(ErrorCode::PageNotFound, "B+ tree page does not exist"));
    }
    BTreePageCodec::PageBuffer buffer {};
    auto read = file_.read_at(page_offset(page_id), buffer);
    if (!read.has_value()) {
        return std::unexpected(filesystem_error(std::move(read.error())));
    }
    if (*read != buffer.size()) {
        return std::unexpected(error(ErrorCode::CorruptedPage, "B+ tree page is truncated"));
    }
    auto decoded = BTreePageCodec::decode(buffer, key_type_, page_id);
    if (!decoded.has_value()) {
        return std::unexpected(codec_error(std::move(decoded.error())));
    }
    auto valid = validate_page_references(*decoded);
    if (!valid.has_value()) {
        return std::unexpected(std::move(valid.error()));
    }
    return std::move(decoded.value());
}

std::expected<void, BTreePageStoreError> BTreePageStore::write_page(const BTreePage & page)
{
    const auto page_id = btree_page_id(page);
    if (page_id == InvalidBTreePageId || page_id >= next_page_id_) {
        return std::unexpected(error(ErrorCode::PageNotFound, "Cannot write an unknown B+ tree page"));
    }
    auto valid = validate_page_references(page);
    if (!valid.has_value()) {
        return valid;
    }
    auto encoded = BTreePageCodec::encode(page, key_type_);
    if (!encoded.has_value()) {
        return std::unexpected(codec_error(std::move(encoded.error())));
    }
    auto written = file_.write_at(page_offset(page_id), *encoded);
    if (!written.has_value()) {
        return std::unexpected(filesystem_error(std::move(written.error())));
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::set_root_page_id(BTreePageId page_id)
{
    if (page_id != InvalidBTreePageId && page_id >= next_page_id_) {
        return std::unexpected(error(ErrorCode::PageNotFound, "Root page does not exist"));
    }
    const auto previous = root_page_id_;
    root_page_id_ = page_id;
    auto written = write_header();
    if (!written.has_value()) {
        root_page_id_ = previous;
        return written;
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::set_entry_count(std::uint64_t count)
{
    const auto previous = entry_count_;
    entry_count_ = count;
    auto written = write_header();
    if (!written.has_value()) {
        entry_count_ = previous;
        return written;
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::publish_tree(
    BTreePageId root_page_id,
    std::uint64_t entry_count
)
{
    if (root_page_id == InvalidBTreePageId || root_page_id >= next_page_id_) {
        return std::unexpected(error(ErrorCode::PageNotFound, "Published root page does not exist"));
    }
    if (root_page_id_ != InvalidBTreePageId || entry_count_ != 0) {
        return std::unexpected(error(ErrorCode::InvalidPage, "Only an empty tree can publish a built root"));
    }
    const auto previous_root = root_page_id_;
    const auto previous_count = entry_count_;
    root_page_id_ = root_page_id;
    entry_count_ = entry_count;
    auto written = write_header();
    if (!written.has_value()) {
        root_page_id_ = previous_root;
        entry_count_ = previous_count;
        return written;
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::release_page(BTreePageId page_id)
{
    if (page_id == InvalidBTreePageId || page_id >= next_page_id_ || page_id == root_page_id_) {
        return std::unexpected(error(ErrorCode::InvalidPage, "Cannot release an active or unknown B+ tree page"));
    }
    if (free_page_count_ == std::numeric_limits<std::uint64_t>::max()) {
        return std::unexpected(error(ErrorCode::InvalidPage, "B+ tree free page count is exhausted"));
    }

    BTreePageCodec::PageBuffer current {};
    auto read = file_.read_at(page_offset(page_id), current);
    if (!read.has_value()) {
        return std::unexpected(filesystem_error(std::move(read.error())));
    }
    if (*read != current.size()) {
        return std::unexpected(error(ErrorCode::CorruptedPage, "B+ tree page is truncated"));
    }
    if (read_number<std::uint32_t>(current.data()) == FreePageMagic) {
        return std::unexpected(error(ErrorCode::InvalidPage, "B+ tree page is already free"));
    }

    auto written = write_free_page(page_id, free_page_head_);
    if (!written.has_value()) {
        return written;
    }

    const auto previous_head = free_page_head_;
    const auto previous_count = free_page_count_;
    free_page_head_ = page_id;
    ++free_page_count_;
    auto header = write_header();
    if (!header.has_value()) {
        free_page_head_ = previous_head;
        free_page_count_ = previous_count;
        return header;
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::sync_data()
{
    auto synced = file_.sync_data();
    if (!synced.has_value()) {
        return std::unexpected(filesystem_error(std::move(synced.error())));
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::sync_all()
{
    auto synced = file_.sync_all();
    if (!synced.has_value()) {
        return std::unexpected(filesystem_error(std::move(synced.error())));
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::initialize()
{
    root_page_id_ = InvalidBTreePageId;
    next_page_id_ = 1;
    entry_count_ = 0;
    free_page_head_ = InvalidBTreePageId;
    free_page_count_ = 0;
    return write_header();
}

std::expected<void, BTreePageStoreError> BTreePageStore::load(
    common::IndexId expected_index_id,
    const common::LogicalType & expected_key_type
)
{
    auto file_size = file_.size();
    if (!file_size.has_value()) {
        return std::unexpected(filesystem_error(std::move(file_size.error())));
    }
    if (*file_size < HeaderSize || (*file_size - HeaderSize) % BTreePageCodec::PageSize != 0) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree store file size"));
    }

    std::array<std::byte, HeaderSize> header {};
    auto read = file_.read_at(0, header);
    if (!read.has_value()) {
        return std::unexpected(filesystem_error(std::move(read.error())));
    }
    if (*read != header.size() || read_number<std::uint32_t>(header.data() + MagicOffset) != StoreMagic) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree store magic"));
    }
    const auto version = read_number<std::uint16_t>(header.data() + VersionOffset);
    if (version != LegacyStoreVersion && version != StoreVersion) {
        return std::unexpected(error(ErrorCode::UnsupportedVersion, "Unsupported B+ tree store version"));
    }
    if (read_number<std::uint16_t>(header.data() + HeaderSizeOffset) != HeaderSize ||
        read_number<std::uint32_t>(header.data() + PageSizeOffset) != BTreePageCodec::PageSize ||
        read_number<std::uint16_t>(header.data() + ReservedOffset) != 0) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree store header"));
    }
    if (version == LegacyStoreVersion) {
        if (read_number<std::uint32_t>(header.data() + HeaderChecksumOffset) != 0) {
            return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid legacy B+ tree store header"));
        }
    } else if (read_number<std::uint32_t>(header.data() + HeaderChecksumOffset)
               != header_checksum_with_zeroed_field(header)) {
        return std::unexpected(error(ErrorCode::ChecksumMismatch, "B+ tree store header checksum mismatch"));
    }

    const auto decoded_type_id = decode_key_type(read_number<std::uint8_t>(header.data() + KeyTypeOffset));
    const auto flags = read_number<std::uint8_t>(header.data() + FlagsOffset);
    if (!decoded_type_id.has_value() || (flags & ~HasKeyParameter) != 0) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree store key type"));
    }
    const auto parameter_value = read_number<std::uint64_t>(header.data() + KeyParameterOffset);
    std::optional<std::size_t> parameter;
    if ((flags & HasKeyParameter) != 0) {
        if (parameter_value > std::numeric_limits<std::size_t>::max()) {
            return std::unexpected(error(ErrorCode::InvalidFormat, "B+ tree key parameter is too large"));
        }
        parameter = static_cast<std::size_t>(parameter_value);
    } else if (parameter_value != 0) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Unexpected B+ tree key parameter"));
    }
    common::LogicalType decoded_type {*decoded_type_id, parameter};
    if (!valid_key_type(decoded_type) ||
        read_number<common::IndexId>(header.data() + IndexIdOffset) != expected_index_id ||
        !same_key_type(decoded_type, expected_key_type)) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "B+ tree store identity does not match the requested index"));
    }

    const auto root_page_id = read_number<BTreePageId>(header.data() + RootPageIdOffset);
    const auto next_page_id = read_number<BTreePageId>(header.data() + NextPageIdOffset);
    const auto entry_count = read_number<std::uint64_t>(header.data() + EntryCountOffset);
    const auto free_page_head = version == LegacyStoreVersion
        ? InvalidBTreePageId
        : read_number<BTreePageId>(header.data() + FreePageHeadOffset);
    const auto free_page_count = version == LegacyStoreVersion
        ? std::uint64_t {0}
        : read_number<std::uint64_t>(header.data() + FreePageCountOffset);
    if (next_page_id == InvalidBTreePageId ||
        (root_page_id != InvalidBTreePageId && root_page_id >= next_page_id) ||
        (free_page_head != InvalidBTreePageId && free_page_head >= next_page_id)) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree store page counters"));
    }
    const auto page_count = next_page_id - 1;
    if (page_count > (std::numeric_limits<std::uint64_t>::max() - HeaderSize) / BTreePageCodec::PageSize ||
        HeaderSize + page_count * BTreePageCodec::PageSize != *file_size) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "B+ tree store page count does not match file size"));
    }

    index_id_ = expected_index_id;
    key_type_ = std::move(decoded_type);
    root_page_id_ = root_page_id;
    next_page_id_ = next_page_id;
    entry_count_ = entry_count;
    free_page_head_ = free_page_head;
    free_page_count_ = free_page_count;
    if (free_page_count_ > next_page_id_ - 1) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree free page count"));
    }
    std::unordered_set<BTreePageId> visited_free_pages;
    auto free_page_id = free_page_head_;
    for (std::uint64_t index = 0; index < free_page_count_; ++index) {
        if (free_page_id == InvalidBTreePageId || !visited_free_pages.insert(free_page_id).second) {
            return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree free page chain"));
        }
        auto next = read_free_page_next(free_page_id);
        if (!next.has_value()) {
            return std::unexpected(std::move(next.error()));
        }
        free_page_id = *next;
    }
    if (free_page_id != InvalidBTreePageId) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "B+ tree free page chain exceeds its count"));
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::write_header()
{
    const auto encoded_type = encode_key_type(key_type_);
    if (!encoded_type.has_value() || !valid_key_type(key_type_)) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree store key type"));
    }
    std::array<std::byte, HeaderSize> header {};
    write_number(header.data() + MagicOffset, StoreMagic);
    write_number(header.data() + VersionOffset, StoreVersion);
    write_number(header.data() + HeaderSizeOffset, static_cast<std::uint16_t>(HeaderSize));
    write_number(header.data() + PageSizeOffset, static_cast<std::uint32_t>(BTreePageCodec::PageSize));
    write_number(header.data() + KeyTypeOffset, *encoded_type);
    const auto flags = key_type_.parameter.has_value() ? HasKeyParameter : 0;
    write_number(header.data() + FlagsOffset, static_cast<std::uint8_t>(flags));
    write_number(header.data() + ReservedOffset, static_cast<std::uint16_t>(0));
    write_number(header.data() + IndexIdOffset, index_id_);
    write_number(
        header.data() + KeyParameterOffset,
        key_type_.parameter.has_value() ? static_cast<std::uint64_t>(*key_type_.parameter) : 0
    );
    write_number(header.data() + RootPageIdOffset, root_page_id_);
    write_number(header.data() + NextPageIdOffset, next_page_id_);
    write_number(header.data() + EntryCountOffset, entry_count_);
    write_number(header.data() + FreePageHeadOffset, free_page_head_);
    write_number(header.data() + FreePageCountOffset, free_page_count_);
    write_number(header.data() + HeaderChecksumOffset, std::uint32_t {0});
    write_number(header.data() + HeaderChecksumOffset, io::crc32(header));
    auto written = file_.write_at(0, header);
    if (!written.has_value()) {
        return std::unexpected(filesystem_error(std::move(written.error())));
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::validate_page_references(
    const BTreePage & page
) const
{
    const auto page_id = btree_page_id(page);
    if (const auto * leaf = std::get_if<BTreeLeafPage>(&page)) {
        if ((leaf->previous_page_id() != InvalidBTreePageId && leaf->previous_page_id() >= next_page_id_) ||
            (leaf->next_page_id() != InvalidBTreePageId && leaf->next_page_id() >= next_page_id_)) {
            return std::unexpected(error(ErrorCode::InvalidPage, "Leaf page links to an unknown page"));
        }
        return {};
    }

    const auto & internal = std::get<BTreeInternalPage>(page);
    if (internal.first_child_id() == InvalidBTreePageId || internal.first_child_id() >= next_page_id_) {
        return std::unexpected(error(ErrorCode::InvalidPage, "Internal page first child does not exist"));
    }
    for (const auto & entry : internal.entries()) {
        if (entry.right_child_id == InvalidBTreePageId || entry.right_child_id >= next_page_id_) {
            return std::unexpected(error(ErrorCode::InvalidPage, "Internal page links to an unknown child"));
        }
    }
    if (page_id == InvalidBTreePageId || page_id >= next_page_id_) {
        return std::unexpected(error(ErrorCode::PageNotFound, "B+ tree page does not exist"));
    }
    return {};
}

std::expected<void, BTreePageStoreError> BTreePageStore::append_page(const BTreePage & page)
{
    if (btree_page_id(page) != next_page_id_) {
        return std::unexpected(error(ErrorCode::InvalidPage, "New B+ tree page id is not the next allocatable id"));
    }
    constexpr auto MaximumPageCount =
        (std::numeric_limits<std::uint64_t>::max() - HeaderSize) / BTreePageCodec::PageSize;
    if (page_count() >= MaximumPageCount) {
        return std::unexpected(error(ErrorCode::InvalidPage, "B+ tree page id space is exhausted"));
    }
    auto encoded = BTreePageCodec::encode(page, key_type_);
    if (!encoded.has_value()) {
        return std::unexpected(codec_error(std::move(encoded.error())));
    }
    const auto offset = page_offset(next_page_id_);
    auto written = file_.write_at(offset, *encoded);
    if (!written.has_value()) {
        return std::unexpected(filesystem_error(std::move(written.error())));
    }

    ++next_page_id_;
    auto header = write_header();
    if (!header.has_value()) {
        --next_page_id_;
        auto rolled_back = file_.truncate(offset);
        if (!rolled_back.has_value()) {
            header.error().message += "; failed to roll back appended page: " + rolled_back.error().message();
        }
        return header;
    }
    return {};
}

std::expected<std::optional<BTreePageId>, BTreePageStoreError>
BTreePageStore::acquire_free_page()
{
    if (free_page_head_ == InvalidBTreePageId) {
        if (free_page_count_ != 0) {
            return std::unexpected(error(ErrorCode::InvalidFormat, "B+ tree free page count has no head"));
        }
        return std::nullopt;
    }
    if (free_page_count_ == 0) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "B+ tree free page head has zero count"));
    }

    const auto page_id = free_page_head_;
    auto next = read_free_page_next(page_id);
    if (!next.has_value()) {
        return std::unexpected(std::move(next.error()));
    }
    const auto previous_head = free_page_head_;
    const auto previous_count = free_page_count_;
    free_page_head_ = *next;
    --free_page_count_;
    auto header = write_header();
    if (!header.has_value()) {
        free_page_head_ = previous_head;
        free_page_count_ = previous_count;
        return std::unexpected(std::move(header.error()));
    }
    return std::optional<BTreePageId> {page_id};
}

std::expected<BTreePageId, BTreePageStoreError>
BTreePageStore::read_free_page_next(BTreePageId page_id) const
{
    if (page_id == InvalidBTreePageId || page_id >= next_page_id_) {
        return std::unexpected(error(ErrorCode::PageNotFound, "Free B+ tree page does not exist"));
    }
    BTreePageCodec::PageBuffer buffer {};
    auto read = file_.read_at(page_offset(page_id), buffer);
    if (!read.has_value()) {
        return std::unexpected(filesystem_error(std::move(read.error())));
    }
    if (*read != buffer.size()) {
        return std::unexpected(error(ErrorCode::CorruptedPage, "Free B+ tree page is truncated"));
    }
    if (read_number<std::uint32_t>(buffer.data()) != FreePageMagic
        || read_number<std::uint16_t>(buffer.data() + FreePageVersionOffset) != FreePageVersion
        || read_number<std::uint16_t>(buffer.data() + FreePageReservedOffset) != 0
        || read_number<BTreePageId>(buffer.data() + FreePageIdOffset) != page_id) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid free B+ tree page"));
    }
    if (read_number<std::uint32_t>(buffer.data() + FreePageChecksumOffset)
        != free_page_checksum_with_zeroed_field(buffer)) {
        return std::unexpected(error(ErrorCode::ChecksumMismatch, "Free B+ tree page checksum mismatch"));
    }
    const auto next = read_number<BTreePageId>(buffer.data() + FreePageNextOffset);
    if (next == page_id || (next != InvalidBTreePageId && next >= next_page_id_)) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid free B+ tree page link"));
    }
    return next;
}

std::expected<void, BTreePageStoreError> BTreePageStore::write_free_page(
    BTreePageId page_id,
    BTreePageId next_free_page_id
)
{
    BTreePageCodec::PageBuffer buffer {};
    write_number(buffer.data(), FreePageMagic);
    write_number(buffer.data() + FreePageVersionOffset, FreePageVersion);
    write_number(buffer.data() + FreePageReservedOffset, std::uint16_t {0});
    write_number(buffer.data() + FreePageIdOffset, page_id);
    write_number(buffer.data() + FreePageNextOffset, next_free_page_id);
    write_number(buffer.data() + FreePageChecksumOffset, std::uint32_t {0});
    write_number(buffer.data() + FreePageChecksumOffset, io::crc32(buffer));
    auto written = file_.write_at(page_offset(page_id), buffer);
    if (!written.has_value()) {
        return std::unexpected(filesystem_error(std::move(written.error())));
    }
    return {};
}

std::uint64_t BTreePageStore::page_offset(BTreePageId page_id) const noexcept
{
    return HeaderSize + (page_id - 1) * BTreePageCodec::PageSize;
}

} // namespace litedb::core::index::btree_index
