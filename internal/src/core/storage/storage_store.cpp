#include "core/storage/storage_store.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"

namespace litedb::core::storage
{

namespace
{

constexpr std::uint32_t StoreMagic = 0x3253444c; // LDS2
constexpr std::uint16_t StoreVersion = 1;
constexpr std::uint32_t PageMagic = 0x3247504c;  // LPG2
constexpr std::size_t HeaderSize = StorageStore::PageSize;
constexpr std::size_t PageHeaderSize = 16;
constexpr std::size_t SlotSize = 8;
constexpr std::uint8_t Active = 1;
constexpr std::uint8_t Deleted = 2;

/**
 * @brief 创建持久化存储器错误
 * @param code 错误码
 * @param message 错误消息
 * @return 持久化存储器错误
 */
StorageStoreError error(StorageStoreErrorCode code, std::string message)
{
    return {code, std::move(message)};
}

/**
 * @brief 创建文件系统错误
 * @param value 文件系统错误
 * @return 持久化存储器错误
 */
StorageStoreError fs_error(filesystem::FileSystemError value)
{
    return error(StorageStoreErrorCode::FileSystemError, std::move(value.message));
}

/**
 * @brief 创建 IO 错误
 * @param value IO 错误
 * @return 持久化存储器错误
 */
StorageStoreError io_error(io::IoError value) { return error(StorageStoreErrorCode::IoError, std::move(value.message)); }

/**
 * @brief 读取数字
 * @param source 源数据
 * @return 数字
 */
template <typename T>
T read_number(const std::byte * source)
{
    T value {};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value |= static_cast<T>(std::to_integer<unsigned int>(source[index])) << (index * 8U);
    }
    return value;
}

/**
 * @brief 写入数字
 * @param target 目标数据
 * @param value 数字
 */
template <typename T>
void write_number(std::byte * target, T value)
{
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        target[index] = static_cast<std::byte>((value >> (index * 8U)) & static_cast<T>(0xffU));
    }
}

/**
 * @brief 编码记录
 * @param id 记录 ID
 * @param data 记录数据
 * @return 编码后的数据
 */
std::expected<std::vector<std::byte>, StorageStoreError> encode(common::RecordId id, const schema::RecordData & data)
{
    io::BufferByteWriter bytes;
    io::BinaryWriter writer {bytes};
    if (auto result = writer.write_u64(id); !result) {
        return std::unexpected(io_error(std::move(result.error())));
    }
    if (data.values.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(error(StorageStoreErrorCode::RecordTooLarge, "Record has too many values"));
    }
    if (auto result = writer.write_u32(static_cast<std::uint32_t>(data.values.size())); !result) {
        return std::unexpected(io_error(std::move(result.error())));
    }
    for (const auto & value : data.values) {
        if (auto result = writer.write_value(value); !result) {
            return std::unexpected(io_error(std::move(result.error())));
        }
    }
    return bytes.take_bytes();
}

/**
 * @brief 解码记录
 * @param bytes 编码后的数据
 * @return 记录
 */
std::expected<schema::Record, StorageStoreError> decode(std::span<const std::byte> bytes)
{
    io::BufferByteReader source {bytes};
    io::BinaryReader reader {source};
    auto id = reader.read_u64();
    auto count = reader.read_u32();
    if (!id) {
        return std::unexpected(io_error(std::move(id.error())));
    }
    if (!count) {
        return std::unexpected(io_error(std::move(count.error())));
    }
    schema::RecordData data;
    data.values.reserve(*count);
    for (std::uint32_t index = 0; index < *count; ++index) {
        auto value = reader.read_value();
        if (!value) {
            return std::unexpected(io_error(std::move(value.error())));
        }
        data.values.push_back(std::move(*value));
    }
    if (*id == 0) {
        return std::unexpected(error(StorageStoreErrorCode::InvalidFormat, "Record id cannot be zero"));
    }
    return schema::Record {*id, std::move(data)};
}

/**
 * @brief 页信息
 * @param slot_count 槽数量
 * @param free_start 空闲起始位置
 * @param free_end 空闲结束位置
 */
struct PageInfo
{
    std::uint16_t slot_count;
    std::uint16_t free_start;
    std::uint16_t free_end;
};

/**
 * @brief 验证页
 * @param page 页
 * @param expected_page 期望页
 * @return 页信息
 */
std::expected<PageInfo, StorageStoreError> validate_page(const std::array<std::byte, StorageStore::PageSize> & page,
                                                   std::uint32_t expected_page)
{
    if (read_number<std::uint32_t>(page.data()) != PageMagic ||
        read_number<std::uint32_t>(page.data() + 4) != expected_page)
        return std::unexpected(error(StorageStoreErrorCode::CorruptedPage, "Invalid data page header"));
    PageInfo info {read_number<std::uint16_t>(page.data() + 8), read_number<std::uint16_t>(page.data() + 10),
                   read_number<std::uint16_t>(page.data() + 12)};
    if (info.free_start != PageHeaderSize + static_cast<std::size_t>(info.slot_count) * SlotSize ||
        info.free_start > info.free_end || info.free_end > StorageStore::PageSize)
        return std::unexpected(error(StorageStoreErrorCode::CorruptedPage, "Invalid slot directory"));
    return info;
}

}

StorageStore::StorageStore(common::CollectionId collection_id, filesystem::FileHandle file) noexcept
    : collection_id_(collection_id)
    , file_(std::move(file))
{
}

std::expected<std::unique_ptr<StorageStore>, StorageStoreError> StorageStore::create(
    std::filesystem::path path, common::CollectionId collection_id, filesystem::FileSystem & filesystem)
{
    auto parent = path.parent_path();
    if (auto made = filesystem.create_dir_all(parent); !made) {
        return std::unexpected(fs_error(std::move(made.error())));
    }
    auto opened = filesystem.open(path, {filesystem::backend::FileAccess::ReadWrite,
                                         filesystem::backend::FileCreateMode::CreateNew});
    if (!opened) {
        return std::unexpected(fs_error(std::move(opened.error())));
    }
    auto store = std::unique_ptr<StorageStore>(new StorageStore(collection_id, std::move(*opened)));
    if (auto result = store->initialize(); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return store;
}

std::expected<std::unique_ptr<StorageStore>, StorageStoreError> StorageStore::open(
    std::filesystem::path path, common::CollectionId collection_id, filesystem::FileSystem & filesystem)
{
    auto opened = filesystem.open(path, {filesystem::backend::FileAccess::ReadWrite,
                                         filesystem::backend::FileCreateMode::OpenExisting});
    if (!opened) {
        return std::unexpected(fs_error(std::move(opened.error())));
    }
    auto store = std::unique_ptr<StorageStore>(new StorageStore(collection_id, std::move(*opened)));
    if (auto result = store->load(); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return store;
}

std::expected<void, StorageStoreError> StorageStore::initialize()
{
    page_count_ = 0;
    next_record_id_ = 1;
    return write_header();
}

std::expected<void, StorageStoreError> StorageStore::write_header()
{
    std::array<std::byte, HeaderSize> header {};
    write_number(header.data(), StoreMagic);
    write_number(header.data() + 4, StoreVersion);
    write_number(header.data() + 6, static_cast<std::uint16_t>(HeaderSize));
    write_number(header.data() + 8, static_cast<std::uint32_t>(StorageStore::PageSize));
    write_number(header.data() + 16, static_cast<std::uint64_t>(collection_id_));
    write_number(header.data() + 24, static_cast<std::uint64_t>(next_record_id_));
    write_number(header.data() + 32, page_count_);
    auto result = file_.write_at(0, header);
    if (!result) {
        return std::unexpected(fs_error(std::move(result.error())));
    }
    return {};
}

std::expected<void, StorageStoreError> StorageStore::load()
{
    auto size = file_.size();
    if (!size) {
        return std::unexpected(fs_error(std::move(size.error())));
    }
    if (*size < HeaderSize || (*size - HeaderSize) % PageSize != 0)
        return std::unexpected(error(StorageStoreErrorCode::InvalidFormat, "Invalid store file size"));
    std::array<std::byte, HeaderSize> header {};
    auto read_header = file_.read_at(0, header);
    if (!read_header) {
        return std::unexpected(fs_error(std::move(read_header.error())));
    }
    if (*read_header != HeaderSize || read_number<std::uint32_t>(header.data()) != StoreMagic)
        return std::unexpected(error(StorageStoreErrorCode::InvalidFormat, "Invalid store magic"));
    if (read_number<std::uint16_t>(header.data() + 4) != StoreVersion)
        return std::unexpected(error(StorageStoreErrorCode::UnsupportedVersion, "Unsupported store version"));
    if (read_number<std::uint32_t>(header.data() + 8) != PageSize ||
        read_number<std::uint64_t>(header.data() + 16) != collection_id_)
        return std::unexpected(error(StorageStoreErrorCode::InvalidFormat, "Store header does not match collection"));
    next_record_id_ = read_number<std::uint64_t>(header.data() + 24);
    page_count_ = read_number<std::uint32_t>(header.data() + 32);
    if (page_count_ != (*size - HeaderSize) / PageSize || next_record_id_ == 0)
        return std::unexpected(error(StorageStoreErrorCode::InvalidFormat, "Store header counters are invalid"));

    common::RecordId maximum {0};
    for (std::uint32_t page_id = 0; page_id < page_count_; ++page_id) {
        std::array<std::byte, PageSize> page {};
        auto read = file_.read_at(HeaderSize + static_cast<std::uint64_t>(page_id) * PageSize, page);
        if (!read || *read != PageSize) {
            return std::unexpected(read ? error(StorageStoreErrorCode::CorruptedPage, "Truncated page") : fs_error(std::move(read.error())));
        }
        auto info = validate_page(page, page_id);
        if (!info) {
            return std::unexpected(std::move(info.error()));
        }
        for (std::uint16_t slot = 0; slot < info->slot_count; ++slot) {
            const auto base = PageHeaderSize + static_cast<std::size_t>(slot) * SlotSize;
            const auto offset = read_number<std::uint16_t>(page.data() + base);
            const auto length = read_number<std::uint16_t>(page.data() + base + 2);
            const auto state = read_number<std::uint8_t>(page.data() + base + 4);
            if (state == Deleted) {
                continue;
            }
            if (state != Active || offset < info->free_end || static_cast<std::size_t>(offset) + length > PageSize)
                return std::unexpected(error(StorageStoreErrorCode::CorruptedPage, "Invalid active slot"));
            auto record = decode(std::span(page).subspan(offset, length));
            if (!record) {
                return std::unexpected(std::move(record.error()));
            }
            if (!locations_.emplace(record->record_id, PhysicalRid {page_id, slot}).second) {
                return std::unexpected(error(StorageStoreErrorCode::InvalidFormat, "Duplicate record id"));
            }
            maximum = std::max(maximum, record->record_id);
        }
    }
    if (next_record_id_ <= maximum) {
        return std::unexpected(error(StorageStoreErrorCode::InvalidFormat, "Invalid next record id"));
    }
    return {};
}

std::expected<PhysicalRid, StorageStoreError> StorageStore::place(common::RecordId id, const schema::RecordData & data)
{
    auto encoded = encode(id, data);
    if (!encoded) {
        return std::unexpected(std::move(encoded.error()));
    }
    if (encoded->size() + PageHeaderSize + SlotSize > PageSize) {
        return std::unexpected(error(StorageStoreErrorCode::RecordTooLarge, "Encoded record does not fit in a data page"));
    }

    for (std::uint32_t candidate = 0; candidate <= page_count_; ++candidate) {
        std::array<std::byte, PageSize> page {};
        PageInfo info {};
        if (candidate == page_count_) {
            write_number(page.data(), PageMagic);
            write_number(page.data() + 4, candidate);
            info = {0, static_cast<std::uint16_t>(PageHeaderSize), static_cast<std::uint16_t>(PageSize)};
        } else {
            auto read = file_.read_at(HeaderSize + static_cast<std::uint64_t>(candidate) * PageSize, page);
            if (!read || *read != PageSize) {
                return std::unexpected(read ? error(StorageStoreErrorCode::CorruptedPage, "Truncated page") : fs_error(std::move(read.error())));
            }
            auto checked = validate_page(page, candidate);
            if (!checked) {
                return std::unexpected(std::move(checked.error()));
            }
            info = *checked;
        }
        std::optional<std::uint16_t> reusable;
        for (std::uint16_t slot = 0; slot < info.slot_count; ++slot) {
            if (read_number<std::uint8_t>(page.data() + PageHeaderSize + slot * SlotSize + 4) == Deleted) {
                reusable = slot;
                break;
            }
        }
        const std::size_t directory_cost = reusable ? 0 : SlotSize;
        if (static_cast<std::size_t>(info.free_end - info.free_start) < encoded->size() + directory_cost) {
            continue;
        }
        const auto slot = reusable.value_or(info.slot_count);
        const auto offset = static_cast<std::uint16_t>(info.free_end - encoded->size());
        std::copy(encoded->begin(), encoded->end(), page.begin() + offset);
        const auto slot_base = PageHeaderSize + static_cast<std::size_t>(slot) * SlotSize;
        write_number(page.data() + slot_base, offset);
        write_number(page.data() + slot_base + 2, static_cast<std::uint16_t>(encoded->size()));
        write_number(page.data() + slot_base + 4, Active);
        if (!reusable) {
            ++info.slot_count;
            info.free_start += SlotSize;
        }
        info.free_end = offset;
        write_number(page.data() + 8, info.slot_count);
        write_number(page.data() + 10, info.free_start);
        write_number(page.data() + 12, info.free_end);
        auto written = file_.write_at(HeaderSize + static_cast<std::uint64_t>(candidate) * PageSize, page);
        if (!written) {
            return std::unexpected(fs_error(std::move(written.error())));
        }
        if (candidate == page_count_) {
            ++page_count_;
            if (auto header = write_header(); !header) {
                return std::unexpected(std::move(header.error()));
            }
        }
        return PhysicalRid {candidate, slot};
    }
    return std::unexpected(error(StorageStoreErrorCode::InvalidStoreState, "Unable to allocate record slot"));
}

std::expected<schema::Record, StorageStoreError> StorageStore::read(PhysicalRid rid) const
{
    std::array<std::byte, PageSize> page {};
    auto loaded = file_.read_at(HeaderSize + static_cast<std::uint64_t>(rid.page_id) * PageSize, page);
    if (!loaded || *loaded != PageSize) {
        return std::unexpected(loaded ? error(StorageStoreErrorCode::CorruptedPage, "Truncated page") : fs_error(std::move(loaded.error())));
    }
    auto info = validate_page(page, rid.page_id);
    if (!info || rid.slot_id >= info->slot_count) {
        return std::unexpected(info ? error(StorageStoreErrorCode::CorruptedPage, "Invalid slot id") : std::move(info.error()));
    }
    const auto base = PageHeaderSize + static_cast<std::size_t>(rid.slot_id) * SlotSize;
    if (read_number<std::uint8_t>(page.data() + base + 4) != Active) {
        return std::unexpected(error(StorageStoreErrorCode::RecordNotFound, "Record not found"));
    }
    const auto offset = read_number<std::uint16_t>(page.data() + base);
    const auto length = read_number<std::uint16_t>(page.data() + base + 2);
    if (static_cast<std::size_t>(offset) + length > PageSize) {
        return std::unexpected(error(StorageStoreErrorCode::CorruptedPage, "Slot payload is outside page"));
    }
    return decode(std::span(page).subspan(offset, length));
}

std::expected<schema::Record, StorageStoreError> StorageStore::get(common::RecordId id) const
{
    const auto it = locations_.find(id);
    if (it == locations_.end()) {
        return std::unexpected(error(StorageStoreErrorCode::RecordNotFound, "Record not found"));
    }
    return read(it->second);
}

std::expected<common::RecordId, StorageStoreError> StorageStore::insert(schema::RecordData data)
{
    const auto id = next_record_id_;
    auto rid = place(id, data);
    if (!rid) {
        return std::unexpected(std::move(rid.error()));
    }
    locations_.emplace(id, *rid);
    ++next_record_id_;
    if (auto header = write_header(); !header) {
        return std::unexpected(std::move(header.error()));
    }
    return id;
}

std::expected<void, StorageStoreError> StorageStore::mark_deleted(PhysicalRid rid)
{
    const auto offset = HeaderSize + static_cast<std::uint64_t>(rid.page_id) * PageSize + PageHeaderSize +
                        static_cast<std::uint64_t>(rid.slot_id) * SlotSize + 4;
    const std::array state {std::byte {Deleted}};
    auto written = file_.write_at(offset, state);
    if (!written) {
        return std::unexpected(fs_error(std::move(written.error())));
    }
    return {};
}

std::expected<void, StorageStoreError> StorageStore::update(common::RecordId id, schema::RecordData data)
{
    const auto it = locations_.find(id);
    if (it == locations_.end()) {
        return std::unexpected(error(StorageStoreErrorCode::RecordNotFound, "Record not found"));
    }
    auto replacement = place(id, data);
    if (!replacement) {
        return std::unexpected(std::move(replacement.error()));
    }
    if (auto removed = mark_deleted(it->second); !removed) {
        return removed;
    }
    it->second = *replacement;
    return {};
}

std::expected<void, StorageStoreError> StorageStore::erase(common::RecordId id)
{
    const auto it = locations_.find(id);
    if (it == locations_.end()) {
        return std::unexpected(error(StorageStoreErrorCode::RecordNotFound, "Record not found"));
    }
    if (auto removed = mark_deleted(it->second); !removed) {
        return removed;
    }
    locations_.erase(it);
    return {};
}

StorageCursor StorageStore::scan() const
{
    std::vector<std::pair<common::RecordId, PhysicalRid>> ordered(locations_.begin(), locations_.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto & lhs, const auto & rhs) {
        return lhs.first < rhs.first;
    });
    std::vector<common::RecordId> record_ids;
    record_ids.reserve(ordered.size());
    for (const auto & [id, rid] : ordered) {
        record_ids.push_back(id);
    }
    return StorageCursor {*this, std::move(record_ids)};
}

} // namespace litedb::core::storage
