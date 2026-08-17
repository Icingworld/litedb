#include "core/storage/storage_store.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/io/buffer_byte_writer.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/checksum.hpp"

namespace litedb::core::storage
{

namespace
{

constexpr std::size_t StoragePageSize = 4096; // 存储页大小

constexpr std::uint16_t StorageFormatVersion = 1;

constexpr std::uint32_t StorageMagic = 0x3253444c; // LDS2

constexpr std::uint32_t PageMagic = 0x3247504l; // LPG2

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
    if (auto created = filesystem.create_dir_all(path.parent_path()); !created) {
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
    if (!file) {
        return std::unexpected(std::move(file.error()));
    }

    auto store = std::unique_ptr<StorageStore>(new StorageStore(path, collection_id, std::move(*file)));
    if (auto result = store->initialize(); !result) {
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

}


std::expected<void, StorageError> StorageStore::initialize()
{
    next_record_id_ = 1;
    page_count_ = 0;
    locations_.clear();

    return write_header();
}

std::expected<void, StorageError> StorageStore::write_header()
{
    io::BufferByteWriter header_bytes(StoragePageSize);
    io::LittleEndianBinaryWriter header_writer {header_bytes};

    // 写入魔数
    if (auto magic = header_writer.write_u32(StorageMagic); !magic) {
        return std::unexpected(std::move(magic.error()));
    }

    // 写入格式版本
    if (auto version = header_writer.write_u16(StorageFormatVersion); !version) {
        return std::unexpected(std::move(version.error()));
    }

    // 写入 Header 页大小
    // 文件 Header 页在当前版本使用与 Page 页相同大小，固定为第 0 页
    if (auto header_page_size = header_writer.write_u16(StoragePageSize); !header_page_size) {
        return std::unexpected(std::move(header_page_size.error()));
    }

    // 写入 Page 页大小
    if (auto page_size = header_writer.write_u16(StoragePageSize); !page_size) {
        return std::unexpected(std::move(page_size.error()));
    }

    // 写入 flag
    // 当前版本没有使用 flag，写入 0
    if (auto flag = header_writer.write_u32(0); !flag) {
        return std::unexpected(std::move(flag.error()));
    }

    // 写入 collection ID
    if (auto collection_id = header_writer.write_u64(collection_id_); !collection_id) {
        return std::unexpected(std::move(collection_id.error()));
    }

    // 写入下一个记录 ID
    if (auto next_record_id = header_writer.write_u64(next_record_id_); !next_record_id) {
        return std::unexpected(std::move(next_record_id.error()));
    }

    // 写入页数量
    if (auto page_count = header_writer.write_u32(page_count_); !page_count) {
        return std::unexpected(std::move(page_count.error()));
    }

    // 写入校验和
    // 校验的范围是前面的所有字节，不包括自身
    if (auto checksum = header_writer.write_u32(io::crc32(header_bytes.bytes())); !checksum) {
        return std::unexpected(std::move(checksum.error()));
    }

    // 填充剩余空间
    const auto remaining = StoragePageSize - header_bytes.bytes().size();
    const std::vector<std::byte> padding(remaining, std::byte {0});
    if (auto padded = header_bytes.write_bytes(padding); !padded) {
        return std::unexpected(std::move(padded.error()));
    }

    // 写入文件
    auto result = file_.write_at(0, header_bytes.bytes());
    if (!result) {
        return std::unexpected(std::move(result.error()));
    }

    return {};
}

} // namespace litedb::core::storage
