#include "core/index/btree_index/btree_page_store.hpp"

#include <array>
#include <limits>
#include <type_traits>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::index::btree_index
{

namespace
{

constexpr std::uint32_t StoreMagic = 0x3149424c; // LBI1
constexpr std::uint16_t StoreVersion = 1;

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

constexpr std::uint8_t HasKeyParameter = 0x01;

using Error = BTreePageStoreError;
using ErrorCode = BTreePageStoreErrorCode;

[[nodiscard]]
Error error(ErrorCode code, std::string message)
{
    return Error {code, std::move(message), std::nullopt};
}

[[nodiscard]]
Error filesystem_error(filesystem::FileSystemError value)
{
    return error(ErrorCode::FileSystemError, std::move(value.message));
}

[[nodiscard]]
Error codec_error(BTreePageCodecError value)
{
    const auto code = value.code == BTreePageCodecErrorCode::CorruptedPage ||
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
        .access = filesystem::backend::FileAccess::ReadWrite,
        .create_mode = filesystem::backend::FileCreateMode::CreateNew,
    });
    if (!opened.has_value()) {
        return std::unexpected(filesystem_error(std::move(opened.error())));
    }
    BTreePageStore store {std::move(path), index_id, std::move(key_type), std::move(opened.value())};
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
        .access = filesystem::backend::FileAccess::ReadWrite,
        .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
    });
    if (!opened.has_value()) {
        return std::unexpected(filesystem_error(std::move(opened.error())));
    }
    BTreePageStore store {
        std::move(path),
        expected_index_id,
        expected_key_type,
        std::move(opened.value()),
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
    if (read_number<std::uint16_t>(header.data() + VersionOffset) != StoreVersion) {
        return std::unexpected(error(ErrorCode::UnsupportedVersion, "Unsupported B+ tree store version"));
    }
    if (read_number<std::uint16_t>(header.data() + HeaderSizeOffset) != HeaderSize ||
        read_number<std::uint32_t>(header.data() + PageSizeOffset) != BTreePageCodec::PageSize ||
        read_number<std::uint16_t>(header.data() + ReservedOffset) != 0) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "Invalid B+ tree store header"));
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
    if (next_page_id == InvalidBTreePageId ||
        (root_page_id != InvalidBTreePageId && root_page_id >= next_page_id)) {
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
            header.error().message += "; failed to roll back appended page: " + rolled_back.error().message;
        }
        return header;
    }
    return {};
}

std::uint64_t BTreePageStore::page_offset(BTreePageId page_id) const noexcept
{
    return HeaderSize + (page_id - 1) * BTreePageCodec::PageSize;
}

} // namespace litedb::core::index::btree_index
