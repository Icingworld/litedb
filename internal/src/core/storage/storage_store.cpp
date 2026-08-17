#include "core/storage/storage_store.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <span>
#include <utility>

#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/storage/value_codec.hpp"

namespace litedb::core::storage
{

namespace
{

constexpr std::size_t MaxEncodedRecordSize =
    StoragePageSize - StoragePageHeaderSize - StorageSlotSize;

template <typename T>
T read_number(const std::byte * source)
{
    T value {};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value |= static_cast<T>(std::to_integer<unsigned int>(source[index])) << (index * 8U);
    }
    return value;
}

template <typename T>
void write_number(std::byte * target, T value)
{
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        target[index] = static_cast<std::byte>((value >> (index * 8U)) & static_cast<T>(0xffU));
    }
}

StorageError make_error(
    StorageErrorCode code,
    std::string message,
    StorageOperation operation,
    const std::filesystem::path & path = {},
    common::CollectionId collection_id = 0,
    common::RecordId record_id = 0,
    std::optional<std::uint32_t> page_id = {},
    std::optional<std::uint16_t> slot_id = {},
    std::optional<std::uint16_t> source_code = {}
)
{
    return make_storage_error(code, std::move(message), {
        .operation = operation,
        .path = path,
        .collection_id = collection_id,
        .record_id = record_id,
        .page_id = page_id,
        .slot_id = slot_id,
        .source_code = source_code,
    });
}

StorageError from_filesystem_error(
    error::Error source,
    StorageOperation operation,
    const std::filesystem::path & path,
    common::CollectionId collection_id,
    std::optional<std::uint32_t> page_id = {}
)
{
    return make_error(
        StorageErrorCode::FileSystemFailure,
        source.message(),
        operation,
        path,
        collection_id,
        0,
        page_id,
        {},
        source.encode_code()
    );
}

StorageError from_io_error(
    io::IoError source,
    StorageOperation operation,
    common::RecordId record_id = 0
)
{
    auto code = source.category() == error::ErrorCategory::FileSystem
        ? StorageErrorCode::FileSystemFailure
        : StorageErrorCode::IoFailure;
    if (source.is(io::IoErrorCode::UnexpectedEof)) code = StorageErrorCode::UnexpectedEof;
    else if (source.is(io::IoErrorCode::InvalidData)) code = StorageErrorCode::InvalidFormat;
    else if (source.is(io::IoErrorCode::ValueTooLarge)) code = StorageErrorCode::RecordTooLarge;
    return make_error(code, source.message(), operation, {}, 0, record_id, {}, {}, source.encode_code());
}

StorageError add_context(
    StorageError source,
    StorageOperation operation,
    const std::filesystem::path & path,
    common::CollectionId collection_id,
    common::RecordId record_id = 0,
    std::optional<std::uint32_t> page_id = {},
    std::optional<std::uint16_t> slot_id = {}
)
{
    if (const auto * context = source.context<StorageErrorContext>();
        context != nullptr && record_id == 0) {
        record_id = context->record_id;
    }
    return make_error(
        static_cast<StorageErrorCode>(source.code()),
        source.message(),
        operation,
        path,
        collection_id,
        record_id,
        page_id,
        slot_id,
        source.encode_code()
    );
}

std::expected<std::vector<std::byte>, StorageError> encode_record(
    common::RecordId id,
    const common::RecordData & data
)
{
    io::BufferByteWriter bytes {MaxEncodedRecordSize};
    io::LittleEndianBinaryWriter writer {bytes};
    if (auto result = writer.write_u64(id); !result) {
        return std::unexpected(from_io_error(std::move(result.error()), StorageOperation::Encode, id));
    }
    if (data.values.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(make_error(
            StorageErrorCode::RecordTooLarge,
            "Record has too many values",
            StorageOperation::Encode,
            {},
            0,
            id
        ));
    }
    if (auto result = writer.write_u32(static_cast<std::uint32_t>(data.values.size())); !result) {
        return std::unexpected(from_io_error(std::move(result.error()), StorageOperation::Encode, id));
    }
    for (const auto & value : data.values) {
        if (auto result = write_value(writer, value); !result) {
            return std::unexpected(from_io_error(std::move(result.error()), StorageOperation::Encode, id));
        }
    }
    return bytes.take_bytes();
}

std::expected<common::Record, StorageError> decode_record(std::span<const std::byte> bytes)
{
    if (bytes.size() > MaxEncodedRecordSize) {
        return std::unexpected(make_error(
            StorageErrorCode::RecordTooLarge,
            "Encoded record exceeds the storage page limit",
            StorageOperation::Decode
        ));
    }
    io::BufferByteReader source {bytes};
    io::LittleEndianBinaryReader reader {
        source,
        {
            .max_total_bytes = bytes.size(),
            .max_string_bytes = static_cast<std::uint32_t>(bytes.size()),
        },
    };
    auto id = reader.read_u64();
    auto count = reader.read_u32();
    if (!id) return std::unexpected(from_io_error(std::move(id.error()), StorageOperation::Decode));
    if (!count) return std::unexpected(from_io_error(std::move(count.error()), StorageOperation::Decode));
    if (*id == 0 || *count > reader.remaining_bytes()) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidFormat,
            "Invalid record id or value count",
            StorageOperation::Decode,
            {},
            0,
            *id
        ));
    }
    common::RecordData data;
    data.values.reserve(std::min<std::uint32_t>(*count, 1024));
    for (std::uint32_t index = 0; index < *count; ++index) {
        auto value = read_value(reader, {
            .max_vector_elements =
                static_cast<std::uint32_t>(reader.remaining_bytes() / sizeof(double)),
        });
        if (!value) {
            return std::unexpected(from_io_error(std::move(value.error()), StorageOperation::Decode, *id));
        }
        data.values.push_back(std::move(*value));
    }
    if (reader.remaining_bytes() != 0) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidFormat,
            "Record payload contains trailing bytes",
            StorageOperation::Decode,
            {},
            0,
            *id
        ));
    }
    return common::Record {*id, std::move(data)};
}

std::size_t slot_base(std::uint16_t slot_id)
{
    return StoragePageHeaderSize + static_cast<std::size_t>(slot_id) * StorageSlotSize;
}

void write_slot(StoragePageBuffer & page, std::uint16_t slot_id, const StorageSlot & slot)
{
    const auto base = slot_base(slot_id);
    write_number(page.data() + base, slot.offset);
    write_number(page.data() + base + 2, slot.length);
    write_number(page.data() + base + 4, static_cast<std::uint8_t>(slot.state));
    page[base + 5] = std::byte {0};
    page[base + 6] = std::byte {0};
    page[base + 7] = std::byte {0};
}

} // namespace

StorageStore::StorageStore(
    std::filesystem::path path,
    common::CollectionId collection_id,
    filesystem::FileHandle file
) noexcept
    : path_(std::move(path))
    , collection_id_(collection_id)
    , file_(std::move(file))
{
}

std::expected<std::unique_ptr<StorageStore>, StorageError> StorageStore::create(
    std::filesystem::path path,
    common::CollectionId collection_id,
    filesystem::FileSystem & filesystem
)
{
    if (auto made = filesystem.create_dir_all(path.parent_path()); !made) {
        return std::unexpected(from_filesystem_error(
            std::move(made.error()), StorageOperation::Create, path, collection_id
        ));
    }
    auto opened = filesystem.open(path, {
        filesystem::FileAccess::ReadWrite,
        filesystem::FileCreateMode::CreateNew,
    });
    if (!opened) {
        return std::unexpected(from_filesystem_error(
            std::move(opened.error()), StorageOperation::Create, path, collection_id
        ));
    }
    auto store = std::unique_ptr<StorageStore>(
        new StorageStore(path, collection_id, std::move(*opened))
    );
    if (auto result = store->initialize(); !result) {
        (void) store->file_.close();
        (void) filesystem.remove(path);
        return std::unexpected(std::move(result.error()));
    }
    return store;
}

std::expected<std::unique_ptr<StorageStore>, StorageError> StorageStore::open(
    std::filesystem::path path,
    common::CollectionId collection_id,
    filesystem::FileSystem & filesystem
)
{
    auto opened = filesystem.open(path, {
        filesystem::FileAccess::ReadWrite,
        filesystem::FileCreateMode::OpenExisting,
    });
    if (!opened) {
        return std::unexpected(from_filesystem_error(
            std::move(opened.error()), StorageOperation::Open, path, collection_id
        ));
    }
    auto store = std::unique_ptr<StorageStore>(
        new StorageStore(path, collection_id, std::move(*opened))
    );
    if (auto result = store->load(); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return store;
}

std::expected<void, StorageError> StorageStore::initialize()
{
    next_record_id_ = 1;
    page_count_ = 0;
    locations_.clear();
    page_spaces_.clear();
    free_space_index_.clear();
    return write_header();
}

std::expected<void, StorageError> StorageStore::write_header()
{
    const auto header = encode_storage_header({
        .collection_id = collection_id_,
        .next_record_id = next_record_id_,
        .page_count = page_count_,
    });
    auto result = file_.write_at(0, header);
    if (!result) {
        return std::unexpected(from_filesystem_error(
            std::move(result.error()), StorageOperation::WriteHeader, path_, collection_id_
        ));
    }
    metrics_.bytes_written += header.size();
    return {};
}

std::expected<StoragePageBuffer, StorageError> StorageStore::load_page(std::uint32_t page_id) const
{
    StoragePageBuffer page {};
    auto read = file_.read_at(
        StoragePageSize + static_cast<std::uint64_t>(page_id) * StoragePageSize,
        page
    );
    if (!read) {
        return std::unexpected(from_filesystem_error(
            std::move(read.error()), StorageOperation::ReadPage, path_, collection_id_, page_id
        ));
    }
    if (*read != StoragePageSize) {
        return std::unexpected(make_error(
            StorageErrorCode::UnexpectedEof,
            "Truncated storage page",
            StorageOperation::ReadPage,
            path_,
            collection_id_,
            0,
            page_id
        ));
    }
    ++metrics_.page_reads;
    metrics_.bytes_read += page.size();
    return page;
}

std::expected<void, StorageError> StorageStore::write_page(
    std::uint32_t page_id,
    const StoragePageBuffer & page
)
{
    auto written = file_.write_at(
        StoragePageSize + static_cast<std::uint64_t>(page_id) * StoragePageSize,
        page
    );
    if (!written) {
        return std::unexpected(from_filesystem_error(
            std::move(written.error()), StorageOperation::WritePage, path_, collection_id_, page_id
        ));
    }
    ++metrics_.page_writes;
    metrics_.bytes_written += page.size();
    return {};
}

std::expected<void, StorageError> StorageStore::load()
{
    auto size = file_.size();
    if (!size) {
        return std::unexpected(from_filesystem_error(
            std::move(size.error()), StorageOperation::Load, path_, collection_id_
        ));
    }
    if (*size < StoragePageSize || (*size - StoragePageSize) % StoragePageSize != 0) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage file size",
            StorageOperation::Load,
            path_,
            collection_id_
        ));
    }
    StoragePageBuffer header_bytes {};
    auto header_read = file_.read_at(0, header_bytes);
    if (!header_read) {
        return std::unexpected(from_filesystem_error(
            std::move(header_read.error()), StorageOperation::ReadHeader, path_, collection_id_
        ));
    }
    if (*header_read != StoragePageSize) {
        return std::unexpected(make_error(
            StorageErrorCode::UnexpectedEof,
            "Truncated storage header",
            StorageOperation::ReadHeader,
            path_,
            collection_id_
        ));
    }
    auto header = decode_storage_header(header_bytes, collection_id_);
    if (!header) {
        if (header.error().is(StorageErrorCode::ChecksumMismatch)) ++metrics_.checksum_failures;
        return std::unexpected(add_context(
            std::move(header.error()), StorageOperation::ReadHeader, path_, collection_id_
        ));
    }
    next_record_id_ = header->next_record_id;
    page_count_ = header->page_count;
    if (page_count_ != (*size - StoragePageSize) / StoragePageSize) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidFormat,
            "Storage page count does not match file size",
            StorageOperation::Load,
            path_,
            collection_id_
        ));
    }

    locations_.clear();
    page_spaces_.assign(page_count_, {});
    free_space_index_.clear();
    common::RecordId maximum {0};
    for (std::uint32_t page_id = 0; page_id < page_count_; ++page_id) {
        auto loaded = load_page(page_id);
        if (!loaded) return std::unexpected(std::move(loaded.error()));
        auto info = decode_storage_page(*loaded, page_id);
        if (!info) {
            if (info.error().is(StorageErrorCode::ChecksumMismatch)) ++metrics_.checksum_failures;
            return std::unexpected(add_context(
                std::move(info.error()),
                StorageOperation::ReadPage,
                path_,
                collection_id_,
                0,
                page_id
            ));
        }
        for (std::uint16_t slot_id = 0; slot_id < info->slot_count; ++slot_id) {
            const auto & slot = info->slots[slot_id];
            if (slot.state != StorageSlotState::Active) continue;
            auto record = decode_record(std::span(*loaded).subspan(slot.offset, slot.length));
            if (!record) {
                return std::unexpected(add_context(
                    std::move(record.error()),
                    StorageOperation::Decode,
                    path_,
                    collection_id_,
                    0,
                    page_id,
                    slot_id
                ));
            }
            if (!locations_.emplace(record->id, PhysicalRid {page_id, slot_id}).second) {
                return std::unexpected(make_error(
                    StorageErrorCode::InvalidFormat,
                    "Duplicate record id",
                    StorageOperation::Load,
                    path_,
                    collection_id_,
                    record->id,
                    page_id,
                    slot_id
                ));
            }
            maximum = std::max(maximum, record->id);
        }
        update_page_space(page_id, *info);
    }
    if (next_record_id_ <= maximum) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidFormat,
            "Invalid next record id",
            StorageOperation::Load,
            path_,
            collection_id_
        ));
    }
    return {};
}

void StorageStore::remove_page_space(std::uint32_t page_id)
{
    if (page_id >= page_spaces_.size()) return;
    free_space_index_.erase({page_spaces_[page_id].reclaimable, page_id});
}

void StorageStore::update_page_space(std::uint32_t page_id, const StoragePageInfo & info)
{
    if (page_id >= page_spaces_.size()) page_spaces_.resize(static_cast<std::size_t>(page_id) + 1);
    remove_page_space(page_id);
    PageSpace space {
        .contiguous = static_cast<std::size_t>(info.free_end - info.free_start),
        .reclaimable = static_cast<std::size_t>(info.free_end - info.free_start),
        .has_deleted_slot = false,
    };
    for (const auto & slot : info.slots) {
        if (slot.state == StorageSlotState::Deleted) {
            space.reclaimable += slot.length;
            space.has_deleted_slot = true;
        }
    }
    page_spaces_[page_id] = space;
    free_space_index_.emplace(space.reclaimable, page_id);
}

std::expected<void, StorageError> StorageStore::compact_page(
    std::uint32_t page_id,
    StoragePageBuffer & page,
    StoragePageInfo & info
)
{
    StoragePageBuffer compacted {};
    auto free_end = static_cast<std::uint16_t>(StoragePageSize);
    for (std::uint16_t slot_id = 0; slot_id < info.slot_count; ++slot_id) {
        auto & slot = info.slots[slot_id];
        if (slot.state == StorageSlotState::Active) {
            free_end = static_cast<std::uint16_t>(free_end - slot.length);
            std::copy_n(page.begin() + slot.offset, slot.length, compacted.begin() + free_end);
            slot.offset = free_end;
        } else {
            slot.offset = 0;
            slot.length = 0;
        }
        write_slot(compacted, slot_id, slot);
    }
    info.free_start = static_cast<std::uint16_t>(
        StoragePageHeaderSize + static_cast<std::size_t>(info.slot_count) * StorageSlotSize
    );
    info.free_end = free_end;
    ++info.generation;
    write_storage_page_metadata(compacted, page_id, info);
    page = std::move(compacted);
    ++metrics_.compactions;
    return {};
}

std::expected<PhysicalRid, StorageError> StorageStore::place_encoded_on_page(
    std::uint32_t page_id,
    StoragePageBuffer & page,
    StoragePageInfo & info,
    std::span<const std::byte> encoded
)
{
    std::optional<std::uint16_t> reusable;
    for (std::uint16_t slot_id = 0; slot_id < info.slot_count; ++slot_id) {
        if (info.slots[slot_id].state == StorageSlotState::Deleted) {
            reusable = slot_id;
            break;
        }
    }
    const auto directory_cost = reusable ? 0 : StorageSlotSize;
    if (static_cast<std::size_t>(info.free_end - info.free_start) < encoded.size() + directory_cost) {
        return std::unexpected(make_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Page has insufficient contiguous space",
            StorageOperation::Insert,
            path_,
            collection_id_,
            0,
            page_id
        ));
    }
    const auto slot_id = reusable.value_or(info.slot_count);
    const auto offset = static_cast<std::uint16_t>(info.free_end - encoded.size());
    std::copy(encoded.begin(), encoded.end(), page.begin() + offset);
    StorageSlot slot {
        .offset = offset,
        .length = static_cast<std::uint16_t>(encoded.size()),
        .state = StorageSlotState::Active,
    };
    if (reusable) {
        info.slots[slot_id] = slot;
    } else {
        info.slots.push_back(slot);
        ++info.slot_count;
        info.free_start = static_cast<std::uint16_t>(info.free_start + StorageSlotSize);
    }
    info.free_end = offset;
    ++info.generation;
    write_slot(page, slot_id, slot);
    write_storage_page_metadata(page, page_id, info);
    return PhysicalRid {page_id, slot_id};
}

std::expected<PhysicalRid, StorageError> StorageStore::place(
    common::RecordId id,
    const common::RecordData & data,
    std::optional<std::uint32_t> preferred_page
)
{
    auto encoded = encode_record(id, data);
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    if (encoded->size() + StoragePageHeaderSize + StorageSlotSize > StoragePageSize) {
        return std::unexpected(make_error(
            StorageErrorCode::RecordTooLarge,
            "Encoded record does not fit in a data page",
            StorageOperation::Insert,
            path_,
            collection_id_,
            id
        ));
    }

    std::vector<std::uint32_t> candidates;
    if (preferred_page && *preferred_page < page_count_) candidates.push_back(*preferred_page);
    for (auto it = free_space_index_.lower_bound({encoded->size(), 0}); it != free_space_index_.end(); ++it) {
        if (!preferred_page || it->second != *preferred_page) candidates.push_back(it->second);
    }

    for (const auto page_id : candidates) {
        const auto & cached = page_spaces_[page_id];
        const auto directory_cost = cached.has_deleted_slot ? 0 : StorageSlotSize;
        if (cached.reclaimable < encoded->size() + directory_cost) continue;
        auto page = load_page(page_id);
        if (!page) return std::unexpected(std::move(page.error()));
        auto info = decode_storage_page(*page, page_id);
        if (!info) return std::unexpected(std::move(info.error()));
        if (static_cast<std::size_t>(info->free_end - info->free_start) < encoded->size() + directory_cost) {
            if (auto compacted = compact_page(page_id, *page, *info); !compacted) {
                return std::unexpected(std::move(compacted.error()));
            }
        }
        auto rid = place_encoded_on_page(page_id, *page, *info, *encoded);
        if (!rid) continue;
        if (auto written = write_page(page_id, *page); !written) {
            return std::unexpected(std::move(written.error()));
        }
        update_page_space(page_id, *info);
        ++metrics_.reused_pages;
        return rid;
    }

    if (page_count_ == std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(make_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Storage page id space is exhausted",
            StorageOperation::Insert,
            path_,
            collection_id_,
            id
        ));
    }
    const auto page_id = page_count_;
    auto page = make_storage_page(page_id);
    auto info = decode_storage_page(page, page_id);
    if (!info) return std::unexpected(std::move(info.error()));
    auto rid = place_encoded_on_page(page_id, page, *info, *encoded);
    if (!rid) return std::unexpected(std::move(rid.error()));
    if (auto written = write_page(page_id, page); !written) {
        return std::unexpected(std::move(written.error()));
    }
    ++page_count_;
    page_spaces_.resize(page_count_);
    update_page_space(page_id, *info);
    ++metrics_.new_pages;
    if (auto header = write_header(); !header) {
        return std::unexpected(std::move(header.error()));
    }
    return rid;
}

std::expected<common::Record, StorageError> StorageStore::read(PhysicalRid rid) const
{
    auto page = load_page(rid.page_id);
    if (!page) return std::unexpected(std::move(page.error()));
    auto info = decode_storage_page(*page, rid.page_id);
    if (!info) {
        if (info.error().is(StorageErrorCode::ChecksumMismatch)) ++metrics_.checksum_failures;
        return std::unexpected(std::move(info.error()));
    }
    if (rid.slot_id >= info->slot_count ||
        info->slots[rid.slot_id].state != StorageSlotState::Active) {
        return std::unexpected(make_error(
            StorageErrorCode::RecordNotFound,
            "Record not found",
            StorageOperation::ReadPage,
            path_,
            collection_id_,
            0,
            rid.page_id,
            rid.slot_id
        ));
    }
    const auto & slot = info->slots[rid.slot_id];
    return decode_record(std::span(*page).subspan(slot.offset, slot.length));
}

std::expected<common::Record, StorageError> StorageStore::get(common::RecordId id) const
{
    const auto it = locations_.find(id);
    if (it == locations_.end()) {
        return std::unexpected(make_error(
            StorageErrorCode::RecordNotFound,
            "Record not found",
            StorageOperation::ReadPage,
            path_,
            collection_id_,
            id
        ));
    }
    return read(it->second);
}

std::expected<common::RecordId, StorageError> StorageStore::insert(common::RecordData data)
{
    if (next_record_id_ == std::numeric_limits<common::RecordId>::max()) {
        return std::unexpected(make_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Record id space is exhausted",
            StorageOperation::Insert,
            path_,
            collection_id_
        ));
    }
    const auto id = next_record_id_;
    auto rid = place(id, data);
    if (!rid) return std::unexpected(std::move(rid.error()));
    locations_.emplace(id, *rid);
    ++next_record_id_;
    if (auto header = write_header(); !header) {
        return std::unexpected(std::move(header.error()));
    }
    return id;
}

std::expected<void, StorageError> StorageStore::update(common::RecordId id, common::RecordData data)
{
    const auto location = locations_.find(id);
    if (location == locations_.end()) {
        return std::unexpected(make_error(
            StorageErrorCode::RecordNotFound,
            "Record not found",
            StorageOperation::Update,
            path_,
            collection_id_,
            id
        ));
    }
    auto encoded = encode_record(id, data);
    if (!encoded) return std::unexpected(std::move(encoded.error()));

    const auto old = location->second;
    auto page = load_page(old.page_id);
    if (!page) return std::unexpected(std::move(page.error()));
    auto info = decode_storage_page(*page, old.page_id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (old.slot_id >= info->slot_count ||
        info->slots[old.slot_id].state != StorageSlotState::Active) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Stored record location is invalid",
            StorageOperation::Update,
            path_,
            collection_id_,
            id,
            old.page_id,
            old.slot_id
        ));
    }
    info->slots[old.slot_id].state = StorageSlotState::Deleted;
    write_slot(*page, old.slot_id, info->slots[old.slot_id]);
    if (auto compacted = compact_page(old.page_id, *page, *info); !compacted) {
        return std::unexpected(std::move(compacted.error()));
    }
    auto replacement = place_encoded_on_page(old.page_id, *page, *info, *encoded);
    if (replacement) {
        if (auto written = write_page(old.page_id, *page); !written) {
            return std::unexpected(std::move(written.error()));
        }
        update_page_space(old.page_id, *info);
        location->second = *replacement;
        return {};
    }

    if (auto written = write_page(old.page_id, *page); !written) {
        return std::unexpected(std::move(written.error()));
    }
    update_page_space(old.page_id, *info);
    auto elsewhere = place(id, data);
    if (!elsewhere) return std::unexpected(std::move(elsewhere.error()));
    location->second = *elsewhere;
    return {};
}

std::expected<void, StorageError> StorageStore::erase(common::RecordId id)
{
    const auto location = locations_.find(id);
    if (location == locations_.end()) {
        return std::unexpected(make_error(
            StorageErrorCode::RecordNotFound,
            "Record not found",
            StorageOperation::Erase,
            path_,
            collection_id_,
            id
        ));
    }
    auto page = load_page(location->second.page_id);
    if (!page) return std::unexpected(std::move(page.error()));
    auto info = decode_storage_page(*page, location->second.page_id);
    if (!info) return std::unexpected(std::move(info.error()));
    auto & slot = info->slots[location->second.slot_id];
    slot.state = StorageSlotState::Deleted;
    write_slot(*page, location->second.slot_id, slot);
    ++info->generation;
    write_storage_page_metadata(*page, location->second.page_id, *info);
    if (auto written = write_page(location->second.page_id, *page); !written) {
        return std::unexpected(std::move(written.error()));
    }
    update_page_space(location->second.page_id, *info);
    locations_.erase(location);
    return {};
}

std::expected<StorageCursor, StorageError> StorageStore::scan() const
{
    std::unordered_map<common::RecordId, common::Record> decoded;
    decoded.reserve(locations_.size());
    for (std::uint32_t page_id = 0; page_id < page_count_; ++page_id) {
        auto page = load_page(page_id);
        if (!page) return std::unexpected(std::move(page.error()));
        auto info = decode_storage_page(*page, page_id);
        if (!info) return std::unexpected(std::move(info.error()));
        for (const auto & slot : info->slots) {
            if (slot.state != StorageSlotState::Active) continue;
            auto record = decode_record(std::span(*page).subspan(slot.offset, slot.length));
            if (!record) return std::unexpected(std::move(record.error()));
            const auto id = record->id;
            if (!decoded.emplace(id, std::move(*record)).second) {
                return std::unexpected(make_error(
                    StorageErrorCode::InvalidFormat,
                    "Duplicate record id during scan",
                    StorageOperation::Scan,
                    path_,
                    collection_id_,
                    id,
                    page_id
                ));
            }
        }
    }
    std::vector<common::Record> records;
    records.reserve(locations_.size());
    for (const auto & [id, location] : locations_) {
        auto record = decoded.find(id);
        if (record == decoded.end()) {
            return std::unexpected(make_error(
                StorageErrorCode::InvalidState,
                "Record directory and scanned pages disagree",
                StorageOperation::Scan,
                path_,
                collection_id_,
                id,
                location.page_id,
                location.slot_id
            ));
        }
        records.push_back(std::move(record->second));
    }
    if (records.size() != decoded.size()) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Scanned pages contain an unknown record",
            StorageOperation::Scan,
            path_,
            collection_id_
        ));
    }
    return StorageCursor {std::move(records)};
}

StorageMetrics StorageStore::metrics() const noexcept
{
    return metrics_;
}

std::uint32_t StorageStore::page_count() const noexcept
{
    return page_count_;
}

} // namespace litedb::core::storage
