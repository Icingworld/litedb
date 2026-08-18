#include "core/storage/storage_store.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>
#include <array>
#include <algorithm>

#include "core/common/record.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/checksum.hpp"

namespace litedb::core::storage
{

namespace
{

constexpr std::size_t StoragePageSize = 4096; // 存储页大小

constexpr std::uint16_t StorageFormatVersion = 1;

constexpr std::uint32_t StorageFileMagic = 0x3253444c; // LDS2

constexpr std::uint32_t StoragePageMagic = 0x3247504C; // LPG2

constexpr std::uint16_t StoragePageHeaderSize = 22; // 存储页头大小

constexpr std::uint16_t StoragePageSlotSize = 8; // 存储页槽大小

// 存储页槽状态
enum class StorageSlotState : std::uint8_t
{
    Active = 0,
    Deleted = 1,
};

// 存储页槽
struct StorageSlot
{
    std::uint16_t offset;
    std::uint16_t length;
    StorageSlotState state;
};

// 编码值类型
enum class EncodedValueKind : std::uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    BigInt = 3,
    Float = 4,
    Double = 5,
    String = 6,
    Vector = 7,
};

} // namespace

StorageStore::StorageStore(
    std::filesystem::path path,
    common::CollectionId collection_id,
    filesystem::FileHandle file
) noexcept
    : path_(std::move(path))
    , collection_id_(collection_id)
    , file_(std::move(file))
{}

std::expected<std::unique_ptr<StorageStore>, StorageError> StorageStore::create(
    std::filesystem::path path,
    filesystem::FileSystem & filesystem,
    common::CollectionId collection_id
)
{
    // 创建目录及其所有父目录
    if (auto created = filesystem.create_dir_all(path.parent_path()); !created) [[unlikely]] {
        return std::unexpected(std::move(created.error()));
    }

    // 创建新文件并打开
    auto file = filesystem.open(
        path,
        {
            filesystem::FileAccess::ReadWrite,
            filesystem::FileCreateMode::CreateNew,
        }
    );
    if (!file) [[unlikely]] {
        return std::unexpected(std::move(file.error()));
    }

    auto store = std::unique_ptr<StorageStore>(new StorageStore(path, collection_id, std::move(*file)));
    if (auto result = store->initialize(); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }

    return store;
}

std::expected<std::unique_ptr<StorageStore>, StorageError> StorageStore::open(
    std::filesystem::path path,
    filesystem::FileSystem & filesystem,
    common::CollectionId collection_id
)
{
    auto file = filesystem.open(
        path,
        {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        }
    );
    if (!file) [[unlikely]] {
        return std::unexpected(std::move(file.error()));
    }

    auto store = std::unique_ptr<StorageStore>(new StorageStore(path, collection_id, std::move(*file)));
    if (auto result = store->load(); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }

    return store;
}

std::expected<common::Record, StorageError> StorageStore::get(common::RecordId record_id) const
{
    const auto it = locations_.find(record_id);
    if (it == locations_.end()) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::RecordNotFound,
            "Record not found",
            {
                .operation = StorageOperation::ReadPage,
                .path = path_,
                .collection_id = collection_id_,
                .record_id = record_id,
            }
        ));
    }

    if (it->second.page_id == 0 || it->second.page_id > page_count_) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidState,
            "Record page ID out of range",
            {
                .operation = StorageOperation::ReadPage,
            }
        ));
    }

    // 读取页数据
    std::array<std::byte, StoragePageSize> page_bytes {};
    auto read = file_.read_at(it->second.page_id * StoragePageSize, page_bytes);
    if (!read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    if (*read != StoragePageSize) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::UnexpectedEof,
            "Truncated page data",
            {
                .operation = StorageOperation::ReadPage,
            }
        ));
    }

    io::BufferByteReader page_resource {std::span<const std::byte> {page_bytes}};
    io::LittleEndianBinaryReader page_reader {
        page_resource,
        {
            .max_total_bytes = StoragePageSize,
            .max_string_bytes = 0,
        }
    };

    // 验证页魔数
    auto magic = page_reader.read_u32();
    if (!magic) [[unlikely]] {
        return std::unexpected(std::move(magic.error()));
    }
    if (*magic != StoragePageMagic) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage page magic",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
                .page_id = it->second.page_id,
            }
        ));
    }

    // 验证页 ID
    auto page_id_read = page_reader.read_u32();
    if (!page_id_read) [[unlikely]] {
        return std::unexpected(std::move(page_id_read.error()));
    }
    if (*page_id_read != it->second.page_id) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage page ID",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
                .page_id = it->second.page_id,
            }
        ));
    }

    // 读取 free_start
    auto free_start = page_reader.read_u16();
    if (!free_start) [[unlikely]] {
        return std::unexpected(std::move(free_start.error()));
    }
    // free_start 必须位于 [StoragePageHeaderSize, StoragePageSize] 之间，并且包含完整的 slot 数据
    if (*free_start < StoragePageHeaderSize || *free_start > StoragePageSize || (*free_start - StoragePageHeaderSize) % StoragePageSlotSize != 0) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage free start",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
                .page_id = it->second.page_id,
            }
        ));
    }

    // 读取 free_end
    auto free_end = page_reader.read_u16();
    if (!free_end) [[unlikely]] {
        return std::unexpected(std::move(free_end.error()));
    }
    if (*free_end < *free_start || *free_end > StoragePageSize) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage free end",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
                .page_id = it->second.page_id,
            }
        ));
    }

    // 读取页 flag
    auto flag = page_reader.read_u16();
    if (!flag) [[unlikely]] {
        return std::unexpected(std::move(flag.error()));
    }
    if (*flag != 0) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage page flag",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
                .page_id = it->second.page_id,
            }
        ));
    }

    // 读取 generation
    auto generation = page_reader.read_u32();
    if (!generation) [[unlikely]] {
        return std::unexpected(std::move(generation.error()));
    }

    // 获取前缀字节，用于计算校验和
    const auto prefix_size = StoragePageSize - page_reader.remaining_bytes();
    const auto prefix_bytes = std::span<const std::byte> {page_bytes}.first(prefix_size);

    // 读取 checksum
    auto checksum = page_reader.read_u32();
    if (!checksum) [[unlikely]] {
        return std::unexpected(std::move(checksum.error()));
    }

    // 获取后缀字节，用于计算校验和
    const auto suffix_size = page_reader.remaining_bytes();
    const auto suffix_bytes = std::span<const std::byte> {page_bytes}.last(suffix_size);

    // 验证校验和
    io::Crc32Calculator crc32_calculator;
    crc32_calculator.update(prefix_bytes);
    crc32_calculator.update(suffix_bytes);
    if (*checksum != crc32_calculator.value()) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ChecksumMismatch,
            "Storage page checksum mismatch",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
                .page_id = it->second.page_id,
            }
        ));
    }

    // 读取并加载所有的槽
    const auto slot_count = (*free_start - StoragePageHeaderSize) / StoragePageSlotSize;

    // 验证目标槽是否存在
    if (it->second.slot_id >= slot_count) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidState,
            "Record slot ID out of range",
            {
                .operation = StorageOperation::Load,
            }
        ));
    }

    // 保存每条记录的区间，用于最后验证区间是否存在重叠
    std::vector<std::pair<std::uint16_t, std::uint16_t>> record_ranges;
    record_ranges.reserve(slot_count);

    // 保存目标槽的偏移和长度
    std::uint16_t target_offset = 0;
    std::uint16_t target_length = 0;
    
    // 下标即 slot_id
    for (std::uint16_t slot_id = 0; slot_id < slot_count; ++slot_id) {
        // 读取记录在页内的起始偏移
        auto offset = page_reader.read_u16();
        if (!offset) [[unlikely]] {
            return std::unexpected(std::move(offset.error()));
        }

        // 读取记录长度
        auto length = page_reader.read_u16();
        if (!length) [[unlikely]] {
            return std::unexpected(std::move(length.error()));
        }

        if (slot_id == it->second.slot_id) {
            target_offset = *offset;
            target_length = *length;
        }

        // 读取槽状态
        auto state = page_reader.read_u8();
        if (!state) [[unlikely]] {
            return std::unexpected(std::move(state.error()));
        }
        if (*state > static_cast<std::uint8_t>(StorageSlotState::Deleted)) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::CorruptedPage,
                "Invalid storage slot state",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .page_id = it->second.page_id,
                    .slot_id = slot_id,
                }
            ));
        }

        // 验证目标槽是否处于 Active 状态
        if (slot_id == it->second.slot_id && *state != static_cast<std::uint8_t>(StorageSlotState::Active)) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidState,
                "Record slot is not active",
                {
                    .operation = StorageOperation::Load,
                }
            ));
        }

        if (*length == 0) {
            // 空槽必须是 Deleted，且 offset 必须为 0
            if (*state != static_cast<std::uint8_t>(StorageSlotState::Deleted) || *offset != 0) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::CorruptedPage,
                    "Invalid empty storage slot",
                    {
                        .operation = StorageOperation::Load,
                        .path = path_,
                        .collection_id = collection_id_,
                        .page_id = it->second.page_id,
                        .slot_id = slot_id,
                    }
                ));
            }
        } else {
            // 非空槽的 offset / length 与是否 Deleted 无关
            // offset 必须位于 [free_end, StoragePageSize] 之间
            if (*offset < *free_end || *offset > StoragePageSize) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::CorruptedPage,
                    "Invalid storage slot offset",
                    {
                        .operation = StorageOperation::Load,
                        .path = path_,
                        .collection_id = collection_id_,
                        .page_id = it->second.page_id,
                        .slot_id = slot_id,
                    }
                ));
            }
            // length 必须大于 0，且不超过剩余预算
            if (*length > StoragePageSize - *offset) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::CorruptedPage,
                    "Invalid storage slot length",
                    {
                        .operation = StorageOperation::Load,
                        .path = path_,
                        .collection_id = collection_id_,
                        .page_id = it->second.page_id,
                        .slot_id = slot_id,
                    }
                ));
            }
            record_ranges.emplace_back(*offset, *offset + *length);
        }

        // 读取 3 字节的 reserved 区域，必须全部为零
        for (std::size_t i = 0; i < 3; ++i) {
            auto reserved = page_reader.read_u8();
            if (!reserved) [[unlikely]] {
                return std::unexpected(std::move(reserved.error()));
            }
            if (*reserved != 0) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::CorruptedPage,
                    "Invalid storage slot reserved bytes",
                    {
                        .operation = StorageOperation::Load,
                        .path = path_,
                        .collection_id = collection_id_,
                        .page_id = it->second.page_id,
                        .slot_id = slot_id,
                    }
                ));
            }
        }
    }

    // 验证区间是否存在重叠
    std::ranges::sort(record_ranges);
    for (std::size_t i = 1; i < record_ranges.size(); ++i) {
        if (record_ranges[i].first < record_ranges[i - 1].second) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::CorruptedPage,
                "Invalid storage record ranges",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .page_id = it->second.page_id,
                }
            ));
        }
    }

    // 读取目标记录
    auto record_bytes = std::span<const std::byte> {page_bytes}.subspan(target_offset, target_length);
    io::BufferByteReader record_resource {record_bytes};
    io::LittleEndianBinaryReader record_reader {
        record_resource,
        {
            .max_total_bytes = target_length,
            .max_string_bytes = target_length,
        }
    };

    // 读取 Record ID
    auto record_id_read = record_reader.read_u64();
    if (!record_id_read) [[unlikely]] {
        return std::unexpected(std::move(record_id_read.error()));
    }
    if (*record_id_read != record_id) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::CorruptedPage,
            "Invalid storage record ID",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
                .record_id = record_id,
                .page_id = it->second.page_id,
                .slot_id = it->second.slot_id,
            }
        ));
    }

    // 读取 Record 元素数量
    auto element_count = record_reader.read_u32();
    if (!element_count) [[unlikely]] {
        return std::unexpected(std::move(element_count.error()));
    }
    if (*element_count > record_reader.remaining_bytes() / sizeof(std::uint8_t)) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::CorruptedPage,
            "Invalid storage record element count",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
                .record_id = record_id,
                .page_id = it->second.page_id,
                .slot_id = it->second.slot_id,                
            }
        ));
    }

    common::Record record;
    record.id = record_id;
    record.data.values.reserve(*element_count);

    for (std::uint32_t i = 0; i < *element_count; ++i) {
        auto kind = record_reader.read_u8();
        if (!kind) [[unlikely]] {
            return std::unexpected(std::move(kind.error()));
        }
        if (*kind > static_cast<std::uint8_t>(EncodedValueKind::Vector)) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidData,
                "invalid encoded value kind",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .record_id = record_id,
                    .page_id = it->second.page_id,
                    .slot_id = it->second.slot_id,
                }
            ));
        }

        switch (static_cast<EncodedValueKind>(*kind)) {
        case EncodedValueKind::Null:
            record.data.values.push_back(common::Value::null());
            break;
        case EncodedValueKind::Boolean: {
            auto value = record_reader.read_u8();
            if (!value) [[unlikely]] {
                return std::unexpected(std::move(value.error()));
            }
            if (*value > 1) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::InvalidData,
                    "boolean value must be encoded as zero or one",
                    {
                        .operation = StorageOperation::Decode,
                    }
                ));
            }
            record.data.values.push_back(common::Value {*value == 1});
            break;
        }
        case EncodedValueKind::Integer: {
            auto value = record_reader.read_i32();
            if (!value) [[unlikely]] {
                return std::unexpected(std::move(value.error()));
            }
            record.data.values.push_back(common::Value {*value});
            break;
        }
        case EncodedValueKind::BigInt: {
            auto value = record_reader.read_i64();
            if (!value) [[unlikely]] {
                return std::unexpected(std::move(value.error()));
            }
            record.data.values.push_back(common::Value {*value});
            break;
        }
        case EncodedValueKind::Float: {
            auto value = record_reader.read_f32();
            if (!value) [[unlikely]] {
                return std::unexpected(std::move(value.error()));
            }
            record.data.values.push_back(common::Value {*value});
            break;
        }
        case EncodedValueKind::Double: {
            auto value = record_reader.read_f64();
            if (!value) [[unlikely]] {
                return std::unexpected(std::move(value.error()));
            }
            record.data.values.push_back(common::Value {*value});
            break;
        }
        case EncodedValueKind::String: {
            auto value = record_reader.read_string();
            if (!value) [[unlikely]] {
                return std::unexpected(std::move(value.error()));
            }
            record.data.values.push_back(common::Value {std::move(*value)});
            break;
        }
        case EncodedValueKind::Vector: {
            auto count = record_reader.read_u32();
            if (!count) [[unlikely]] {
                return std::unexpected(std::move(count.error()));
            }
            if (*count > static_cast<std::uint32_t>(record_reader.remaining_bytes() / sizeof(double))) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::ResourceLimitExceeded,
                    "vector length exceeds the remaining binary data",
                    {
                        .operation = StorageOperation::Decode,
                    }
                ));
            }
            common::VectorValue values;
            values.reserve(*count);
            for (std::uint32_t index = 0; index < *count; ++index) {
                auto value = record_reader.read_f64();
                if (!value) [[unlikely]] {
                    return std::unexpected(std::move(value.error()));
                }
                values.push_back(*value);
            }
            record.data.values.push_back(common::Value {std::move(values)});
            break;
        }
        }
    }

    // 验证剩余字节是否为零
    if (record_reader.remaining_bytes() != 0) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::CorruptedPage,
            "Invalid storage record remaining bytes",
            {
                .operation = StorageOperation::Load,
            }
        ));
    }

    return record;
}

std::expected<void, StorageError> StorageStore::initialize()
{
    next_record_id_ = 1;
    page_count_ = 0;
    locations_.clear();
    page_space_summaries_.clear();
    free_space_index_.clear();

    return write_header();
}

std::expected<void, StorageError> StorageStore::write_header()
{
    io::BufferByteWriter header_bytes(StoragePageSize);
    io::LittleEndianBinaryWriter header_writer {header_bytes};

    // 写入魔数
    if (auto magic = header_writer.write_u32(StorageFileMagic); !magic) [[unlikely]] {
        return std::unexpected(std::move(magic.error()));
    }

    // 写入格式版本
    if (auto version = header_writer.write_u16(StorageFormatVersion); !version) [[unlikely]] {
        return std::unexpected(std::move(version.error()));
    }

    // 写入 Header 页大小
    // 文件 Header 页在当前版本使用与 Page 页相同大小，固定为第 0 页
    if (auto header_page_size = header_writer.write_u16(StoragePageSize); !header_page_size) [[unlikely]] {
        return std::unexpected(std::move(header_page_size.error()));
    }

    // 写入 Page 页大小
    if (auto page_size = header_writer.write_u16(StoragePageSize); !page_size) [[unlikely]] {
        return std::unexpected(std::move(page_size.error()));
    }

    // 写入 flag
    // 当前版本没有使用 flag，写入 0
    if (auto flag = header_writer.write_u16(0); !flag) [[unlikely]] {
        return std::unexpected(std::move(flag.error()));
    }

    // 写入 collection ID
    if (auto collection_id = header_writer.write_u64(collection_id_); !collection_id) [[unlikely]] {
        return std::unexpected(std::move(collection_id.error()));
    }

    // 写入下一个记录 ID
    if (auto next_record_id = header_writer.write_u64(next_record_id_); !next_record_id) [[unlikely]] {
        return std::unexpected(std::move(next_record_id.error()));
    }

    // 写入页数量
    if (auto page_count = header_writer.write_u32(page_count_); !page_count) [[unlikely]] {
        return std::unexpected(std::move(page_count.error()));
    }

    // 写入校验和
    // 校验的范围是前面的所有字节加后续所有零字节，不包括自身
    io::Crc32Calculator crc32_calculator;
    crc32_calculator.update(header_bytes.bytes());
    crc32_calculator.update(std::vector<std::byte>(StoragePageSize - header_bytes.bytes().size() - sizeof(std::uint32_t), std::byte {0}));
    if (auto checksum = header_writer.write_u32(crc32_calculator.value()); !checksum) [[unlikely]] {
        return std::unexpected(std::move(checksum.error()));
    }

    // 写入 reserved 区域，全部为零
    const auto remaining = StoragePageSize - header_bytes.bytes().size();
    const std::vector<std::byte> padding(remaining, std::byte {0});
    if (auto padded = header_bytes.write_bytes(padding); !padded) [[unlikely]] {
        return std::unexpected(std::move(padded.error()));
    }

    // 写入文件
    // 仅写入一次，使用 file_ 或者 io::FileByteWriter 都一样
    auto result = file_.write_at(0, header_bytes.bytes());
    if (!result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }

    return {};
}

std::expected<void, StorageError> StorageStore::load()
{
    // 验证文件大小是否合法
    auto size = file_.size();
    if (!size) [[unlikely]] {
        return std::unexpected(std::move(size.error()));
    }
    if (*size < StoragePageSize) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::UnexpectedEof,
            "Truncated storage header",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }
    if (*size % StoragePageSize != 0) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage file size",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    io::FileByteReader file_reader {file_};

    // 读取文件头
    std::array<std::byte, StoragePageSize> header_bytes {};
    if (auto read = file_reader.read_exact(header_bytes); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }

    io::BufferByteReader header_resource {std::span<const std::byte> {header_bytes}};
    io::LittleEndianBinaryReader header_reader {
        header_resource,
        {
            .max_total_bytes = StoragePageSize,
            .max_string_bytes = 0,
        }
    };

    // 验证文件魔数
    auto magic = header_reader.read_u32();
    if (!magic) [[unlikely]] {
        return std::unexpected(std::move(magic.error()));
    }
    if (*magic != StorageFileMagic) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage magic",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 验证文件格式版本
    auto version = header_reader.read_u16();
    if (!version) [[unlikely]] {
        return std::unexpected(std::move(version.error()));
    }
    if (*version != StorageFormatVersion) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::UnsupportedVersion,
            "Unsupported storage format version",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 验证文件 Header 页大小
    auto header_page_size = header_reader.read_u16();
    if (!header_page_size) [[unlikely]] {
        return std::unexpected(std::move(header_page_size.error()));
    }
    if (*header_page_size != StoragePageSize) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage header page size",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 验证文件 Page 页大小
    auto page_size = header_reader.read_u16();
    if (!page_size) [[unlikely]] {
        return std::unexpected(std::move(page_size.error()));
    }
    if (*page_size != StoragePageSize) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage page size",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 验证文件 flag
    auto flag = header_reader.read_u16();
    if (!flag) [[unlikely]] {
        return std::unexpected(std::move(flag.error()));
    }
    if (*flag != 0) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage flag",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 读取 collection ID
    auto collection_id = header_reader.read_u64();
    if (!collection_id) [[unlikely]] {
        return std::unexpected(std::move(collection_id.error()));
    }
    if (*collection_id != collection_id_) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage collection ID",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 读取下一个记录 ID
    auto next_record_id = header_reader.read_u64();
    if (!next_record_id) [[unlikely]] {
        return std::unexpected(std::move(next_record_id.error()));
    }
    if (*next_record_id == 0) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage next record ID",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 读取页数量
    auto page_count = header_reader.read_u32();
    if (!page_count) [[unlikely]] {
        return std::unexpected(std::move(page_count.error()));
    }
    if (*page_count != (*size - StoragePageSize) / StoragePageSize) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Storage page count does not match file size",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 获取前缀字节，用于计算校验和
    const auto prefix_size = StoragePageSize - header_reader.remaining_bytes();
    const auto prefix_bytes = std::span<const std::byte> {header_bytes}.first(prefix_size);

    // 读取校验和
    auto checksum = header_reader.read_u32();
    if (!checksum) [[unlikely]] {
        return std::unexpected(std::move(checksum.error()));
    }

    // 获取后缀字节，用于计算校验和
    const auto suffix_size = header_reader.remaining_bytes();
    const auto suffix_bytes = std::span<const std::byte> {header_bytes}.last(suffix_size);

    // 验证校验和
    io::Crc32Calculator crc32_calculator;
    crc32_calculator.update(prefix_bytes);
    crc32_calculator.update(suffix_bytes);
    if (*checksum != crc32_calculator.value()) [[unlikely]] {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ChecksumMismatch,
            "Storage header checksum mismatch",
            {
                .operation = StorageOperation::Load,
                .path = path_,
                .collection_id = collection_id_,
            }
        ));
    }

    // 校验 reserved 区域是否全部为零
    for (const auto byte : suffix_bytes) {
        if (byte != std::byte {0}) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidFormat,
                "Invalid storage suffix bytes",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                }
            ));
        }
    }

    // 验证完毕，更新初始数据
    next_record_id_ = *next_record_id;
    page_count_ = *page_count;
    page_space_summaries_.resize(page_count_);

    // 加载页数据
    for (std::uint32_t page_id = 1; page_id <= page_count_; ++page_id) {
        std::array<std::byte, StoragePageSize> page_bytes {};
        if (auto read = file_reader.read_exact(page_bytes); !read) [[unlikely]] {
            return std::unexpected(std::move(read.error()));
        }

        io::BufferByteReader page_resource {std::span<const std::byte> {page_bytes}};
        io::LittleEndianBinaryReader page_reader {
            page_resource,
            {
                .max_total_bytes = StoragePageSize,
                .max_string_bytes = 0,
            }
        };

        // 验证页魔数
        auto magic = page_reader.read_u32();
        if (!magic) [[unlikely]] {
            return std::unexpected(std::move(magic.error()));
        }
        if (*magic != StoragePageMagic) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidFormat,
                "Invalid storage page magic",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .page_id = page_id,
                }
            ));
        }

        // 验证页 ID
        auto page_id_read = page_reader.read_u32();
        if (!page_id_read) [[unlikely]] {
            return std::unexpected(std::move(page_id_read.error()));
        }
        if (*page_id_read != page_id) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidFormat,
                "Invalid storage page ID",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .page_id = page_id,
                }
            ));
        }

        // 读取 free_start
        auto free_start = page_reader.read_u16();
        if (!free_start) [[unlikely]] {
            return std::unexpected(std::move(free_start.error()));
        }
        // free_start 必须位于 [StoragePageHeaderSize, StoragePageSize] 之间，并且包含完整的 slot 数据
        if (*free_start < StoragePageHeaderSize || *free_start > StoragePageSize || (*free_start - StoragePageHeaderSize) % StoragePageSlotSize != 0) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidFormat,
                "Invalid storage free start",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .page_id = page_id,
                }
            ));
        }

        // 读取 free_end
        auto free_end = page_reader.read_u16();
        if (!free_end) [[unlikely]] {
            return std::unexpected(std::move(free_end.error()));
        }
        if (*free_end < *free_start || *free_end > StoragePageSize) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidFormat,
                "Invalid storage free end",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .page_id = page_id,
                }
            ));
        }

        // 读取页 flag
        auto flag = page_reader.read_u16();
        if (!flag) [[unlikely]] {
            return std::unexpected(std::move(flag.error()));
        }
        if (*flag != 0) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidFormat,
                "Invalid storage page flag",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .page_id = page_id,
                }
            ));
        }

        // 读取 generation
        auto generation = page_reader.read_u32();
        if (!generation) [[unlikely]] {
            return std::unexpected(std::move(generation.error()));
        }

        // 获取前缀字节，用于计算校验和
        const auto prefix_size = StoragePageSize - page_reader.remaining_bytes();
        const auto prefix_bytes = std::span<const std::byte> {page_bytes}.first(prefix_size);

        // 读取 checksum
        auto checksum = page_reader.read_u32();
        if (!checksum) [[unlikely]] {
            return std::unexpected(std::move(checksum.error()));
        }

        // 获取后缀字节，用于计算校验和
        const auto suffix_size = page_reader.remaining_bytes();
        const auto suffix_bytes = std::span<const std::byte> {page_bytes}.last(suffix_size);

        // 验证校验和
        io::Crc32Calculator crc32_calculator;
        crc32_calculator.update(prefix_bytes);
        crc32_calculator.update(suffix_bytes);
        if (*checksum != crc32_calculator.value()) [[unlikely]] {
            return std::unexpected(make_storage_error(
                StorageErrorCode::ChecksumMismatch,
                "Storage page checksum mismatch",
                {
                    .operation = StorageOperation::Load,
                    .path = path_,
                    .collection_id = collection_id_,
                    .page_id = page_id,
                }
            ));
        }

        // 读取并加载所有的槽
        const auto slot_count = (*free_start - StoragePageHeaderSize) / StoragePageSlotSize;

        // 保存所有的槽
        std::vector<StorageSlot> slots;
        slots.reserve(slot_count);
        // 保存每条记录的区间，用于最后验证区间是否存在重叠
        std::vector<std::pair<std::uint16_t, std::uint16_t>> record_ranges;
        record_ranges.reserve(slot_count);
        
        // 下标即 slot_id
        for (std::uint16_t slot_id = 0; slot_id < slot_count; ++slot_id) {
            // 读取记录在页内的起始偏移
            auto offset = page_reader.read_u16();
            if (!offset) [[unlikely]] {
                return std::unexpected(std::move(offset.error()));
            }

            // 读取记录长度
            auto length = page_reader.read_u16();
            if (!length) [[unlikely]] {
                return std::unexpected(std::move(length.error()));
            }

            // 读取槽状态
            auto state = page_reader.read_u8();
            if (!state) [[unlikely]] {
                return std::unexpected(std::move(state.error()));
            }
            if (*state > static_cast<std::uint8_t>(StorageSlotState::Deleted)) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::InvalidFormat,
                    "Invalid storage slot state",
                    {
                        .operation = StorageOperation::Load,
                        .path = path_,
                        .collection_id = collection_id_,
                        .page_id = page_id,
                        .slot_id = slot_id,
                    }
                ));
            }

            if (*length == 0) {
                // 空槽必须是 Deleted，且 offset 必须为 0
                if (*state != static_cast<std::uint8_t>(StorageSlotState::Deleted) || *offset != 0) [[unlikely]] {
                    return std::unexpected(make_storage_error(
                        StorageErrorCode::InvalidFormat,
                        "Invalid empty storage slot",
                        {
                            .operation = StorageOperation::Load,
                            .path = path_,
                            .collection_id = collection_id_,
                            .page_id = page_id,
                            .slot_id = slot_id,
                        }
                    ));
                }
            } else {
                // 非空槽的 offset / length 与是否 Deleted 无关
                // offset 必须位于 [free_end, StoragePageSize] 之间
                if (*offset < *free_end || *offset > StoragePageSize) [[unlikely]] {
                    return std::unexpected(make_storage_error(
                        StorageErrorCode::InvalidFormat,
                        "Invalid storage slot offset",
                        {
                            .operation = StorageOperation::Load,
                            .path = path_,
                            .collection_id = collection_id_,
                            .page_id = page_id,
                            .slot_id = slot_id,
                        }
                    ));
                }
                // length 必须大于 0，且不超过剩余预算
                if (*length > StoragePageSize - *offset) [[unlikely]] {
                    return std::unexpected(make_storage_error(
                        StorageErrorCode::InvalidFormat,
                        "Invalid storage slot length",
                        {
                            .operation = StorageOperation::Load,
                            .path = path_,
                            .collection_id = collection_id_,
                            .page_id = page_id,
                            .slot_id = slot_id,
                        }
                    ));
                }
                record_ranges.emplace_back(*offset, *offset + *length);
            }

            // 读取 3 字节的 reserved 区域，必须全部为零
            for (std::size_t i = 0; i < 3; ++i) {
                auto reserved = page_reader.read_u8();
                if (!reserved) [[unlikely]] {
                    return std::unexpected(std::move(reserved.error()));
                }
                if (*reserved != 0) [[unlikely]] {
                    return std::unexpected(make_storage_error(
                        StorageErrorCode::InvalidFormat,
                        "Invalid storage slot reserved bytes",
                        {
                            .operation = StorageOperation::Load,
                            .path = path_,
                            .collection_id = collection_id_,
                            .page_id = page_id,
                            .slot_id = slot_id,
                        }
                    ));
                }
            }

            slots.emplace_back(StorageSlot {
                .offset = *offset,
                .length = *length,
                .state = static_cast<StorageSlotState>(*state),
            });
        }

        // 验证区间是否存在重叠
        std::ranges::sort(record_ranges);
        for (std::size_t i = 1; i < record_ranges.size(); ++i) {
            if (record_ranges[i].first < record_ranges[i - 1].second) {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::InvalidFormat,
                    "Invalid storage record ranges",
                    {
                        .operation = StorageOperation::Load,
                        .path = path_,
                        .collection_id = collection_id_,
                        .page_id = page_id,
                    }
                ));
            }
        }

        // 根据读取到的槽加载所有的数据

        // 初始化页空间摘要
        // 初始时，页空间连续空间大小与可回收空间大小相同
        // 页连续空间即空洞大小，可回收空间大小为连续空间大小加上所有已删除的记录的空间大小
        page_space_summaries_[page_id].contiguous = static_cast<std::size_t>(*free_end - *free_start);
        page_space_summaries_[page_id].reclaimable = static_cast<std::size_t>(*free_end - *free_start);
        page_space_summaries_[page_id].has_deleted_slot = false;

        for (std::uint16_t slot_id = 0; slot_id < slot_count; ++slot_id) {
            const auto & slot = slots[slot_id];
            if (slot.state == StorageSlotState::Deleted) {
                page_space_summaries_[page_id].reclaimable += slot.length;
                page_space_summaries_[page_id].has_deleted_slot = true;
                continue;
            }

            // 读取数据
            auto record_bytes = std::span<const std::byte> {page_bytes}.subspan(slot.offset, slot.length);

            io::BufferByteReader record_resource {record_bytes};
            io::LittleEndianBinaryReader record_reader {
                record_resource,
                {
                    .max_total_bytes = slot.length,
                    .max_string_bytes = static_cast<std::uint32_t>(slot.length),
                }
            };

            // 读取 Record ID
            auto record_id = record_reader.read_u64();
            if (!record_id) [[unlikely]] {
                return std::unexpected(std::move(record_id.error()));
            }
            if (*record_id == 0 || *record_id >= next_record_id_) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::InvalidFormat,
                    "Invalid storage record ID",
                    {
                        .operation = StorageOperation::Load,
                        .path = path_,
                        .collection_id = collection_id_,
                    }
                ));
            }

            // 加载时，只对文件做轻度校验，不保证整个文件完全正确
            // 对于 Record 等数据的正确性校验，到查找时进行
            // 或以后提供统一的 verify_full 接口，供分析、验证工具调用

            if (!locations_.emplace(*record_id, PhysicalRid {page_id, slot_id}).second) [[unlikely]] {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::InvalidFormat,
                    "Duplicate record ID",
                    {
                        .operation = StorageOperation::Load,
                        .path = path_,
                        .collection_id = collection_id_,
                        .record_id = *record_id,
                        .page_id = page_id,
                        .slot_id = slot_id,
                    }
                ));
            }
        }

        // 更新空闲空间索引
        free_space_index_.emplace(page_space_summaries_[page_id].reclaimable, page_id);
    }

    return {};
}

} // namespace litedb::core::storage
