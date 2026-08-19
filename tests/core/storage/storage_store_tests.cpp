#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <iterator>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <span>
#include <variant>
#include <vector>

#include "core/filesystem/platform_filesystem.hpp"
#include "core/io/checksum.hpp"
#include "core/storage/storage_store.hpp"

namespace
{

using namespace litedb::core;

constexpr std::size_t PageSize = 4096;
constexpr std::size_t PageHeaderSize = 22;
constexpr std::size_t SlotSize = 8;
using PageBuffer = std::array<std::byte, PageSize>;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

const storage::StorageErrorContext & require_storage_error(
    const error::Error & actual,
    storage::StorageErrorCode code,
    storage::StorageOperation operation,
    const char * message
)
{
    require(actual.is(code), message);
    const auto * context = actual.context<storage::StorageErrorContext>();
    require(context != nullptr, message);
    require(context->operation == operation, message);
    return *context;
}

std::filesystem::path temporary_directory(std::string_view suffix)
{
    return std::filesystem::temp_directory_path() /
        ("litedb-storage-store-" + std::string(suffix) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

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

void reseal_header(PageBuffer & header)
{
    io::Crc32Calculator calculator;
    calculator.update(std::span<const std::byte> {header}.first(32));
    calculator.update(std::span<const std::byte> {header}.subspan(36));
    write_number(header.data() + 32, calculator.value());
}

void reseal_page(PageBuffer & page)
{
    io::Crc32Calculator calculator;
    calculator.update(std::span<const std::byte> {page}.first(18));
    calculator.update(std::span<const std::byte> {page}.subspan(22));
    write_number(page.data() + 18, calculator.value());
}

PageBuffer read_page(const std::filesystem::path & path, std::uint32_t page_id)
{
    PageBuffer page {};
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(static_cast<std::uint64_t>(page_id) * PageSize));
    input.read(reinterpret_cast<char *>(page.data()), page.size());
    require(input.gcount() == static_cast<std::streamsize>(page.size()), "failed to read fixture page");
    return page;
}

void write_page(const std::filesystem::path & path, std::uint32_t page_id, const PageBuffer & page)
{
    std::fstream output(path, std::ios::binary | std::ios::in | std::ios::out);
    output.seekp(static_cast<std::streamoff>(static_cast<std::uint64_t>(page_id) * PageSize));
    output.write(reinterpret_cast<const char *>(page.data()), page.size());
    require(output.good(), "failed to write fixture page");
}

PageBuffer read_header(const std::filesystem::path & path)
{
    return read_page(path, 0);
}

void write_header(const std::filesystem::path & path, const PageBuffer & header)
{
    write_page(path, 0, header);
}

std::size_t storage_file_size(const std::filesystem::path & path)
{
    return static_cast<std::size_t>(std::filesystem::file_size(path));
}

void test_exact_format_and_all_values()
{
    const auto directory = temporary_directory("values");
    const auto path = directory / "collections" / "11.store";
    auto filesystem = filesystem::create_platform_filesystem();
    auto created = storage::StorageStore::create(path, filesystem, 11);
    require(created.has_value(), "store create failed");
    auto store = std::move(*created);
    common::RecordData data {{
        common::Value::null(),
        common::Value {true},
        common::Value {std::int32_t {-2}},
        common::Value {std::int64_t {7}},
        common::Value {1.0F},
        common::Value {1.0},
        common::Value {std::string {"hi"}},
        common::Value {common::VectorValue {1.0, 2.0}},
    }};
    auto id = store->insert(data);
    require(id && *id == 1, "all-values insert failed");
    auto record = store->get(*id);
    require(record && record->data.values.size() == data.values.size(), "all-values read failed");
    for (std::size_t index = 0; index < data.values.size(); ++index) {
        require(record->data.values[index].data() == data.values[index].data(), "value round-trip mismatch");
    }

    auto cursor = store->scan();
    require(cursor.has_value(), "all-values scan failed");
    auto next = cursor->next();
    require(next && *next && (**next).id == 1, "all-values scan row mismatch");
    auto end = cursor->next();
    require(end && !*end, "all-values scan did not terminate");

    auto missing = store->get(99);
    require(!missing, "missing record was returned");
    const auto & get_context = require_storage_error(
        missing.error(),
        storage::StorageErrorCode::RecordNotFound,
        storage::StorageOperation::Get,
        "missing get operation mismatch"
    );
    require(get_context.path == path && get_context.collection_id == 11 && get_context.record_id == 99,
            "missing get context mismatch");

    auto missing_update = store->update(99, data);
    require(!missing_update, "missing record was updated");
    require_storage_error(
        missing_update.error(),
        storage::StorageErrorCode::RecordNotFound,
        storage::StorageOperation::Update,
        "missing update operation mismatch"
    );

    auto missing_erase = store->erase(99);
    require(!missing_erase, "missing record was erased");
    require_storage_error(
        missing_erase.error(),
        storage::StorageErrorCode::RecordNotFound,
        storage::StorageOperation::Erase,
        "missing erase operation mismatch"
    );

    auto oversized = store->insert({{common::Value {std::string(PageSize, 'x')}}});
    require(!oversized, "oversized record was inserted");
    const auto & encode_context = require_storage_error(
        oversized.error(),
        storage::StorageErrorCode::RecordTooLarge,
        storage::StorageOperation::Encode,
        "oversized record operation mismatch"
    );
    require(encode_context.collection_id == 11 && encode_context.record_id == 2,
            "oversized record context mismatch");

    store.reset();
    require(storage_file_size(path) == 2 * PageSize, "storage file is not page aligned");
    const auto header = read_header(path);
    require(read_number<std::uint32_t>(header.data()) == 0x5342444cU, "storage magic mismatch");
    require(read_number<std::uint16_t>(header.data() + 4) == 1, "storage version mismatch");
    require(read_number<std::uint16_t>(header.data() + 6) == PageSize &&
                read_number<std::uint16_t>(header.data() + 8) == PageSize,
            "storage header page sizes mismatch");

    const auto page = read_page(path, 1);
    require(read_number<std::uint32_t>(page.data()) == 0x5042444cU, "storage page magic mismatch");
    require(read_number<std::uint32_t>(page.data() + 4) == 1, "storage page id mismatch");
    const auto free_start = read_number<std::uint16_t>(page.data() + 8);
    require(free_start == PageHeaderSize + SlotSize, "storage slot directory size mismatch");
    require(page[PageHeaderSize + 4] == std::byte {0}, "active slot state mismatch");
    const auto record_offset = read_number<std::uint16_t>(page.data() + PageHeaderSize);
    const auto record_length = read_number<std::uint16_t>(page.data() + PageHeaderSize + 2);
    const std::vector<std::byte> expected_record {
        std::byte {0x01}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x08}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00},
        std::byte {0x01}, std::byte {0x01},
        std::byte {0x02}, std::byte {0xfe}, std::byte {0xff}, std::byte {0xff}, std::byte {0xff},
        std::byte {0x03}, std::byte {0x07}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x04}, std::byte {0x00}, std::byte {0x00}, std::byte {0x80}, std::byte {0x3f},
        std::byte {0x05}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0xf0}, std::byte {0x3f},
        std::byte {0x06}, std::byte {0x02}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x68}, std::byte {0x69},
        std::byte {0x07}, std::byte {0x02}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0xf0}, std::byte {0x3f},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x40},
    };
    require(record_length == expected_record.size(), "encoded record length changed");
    const std::vector<std::byte> actual_record(
        page.begin() + record_offset,
        page.begin() + record_offset + record_length
    );
    require(actual_record == expected_record, "record wire format changed");

    auto reopened = storage::StorageStore::open(path, filesystem, 11);
    require(reopened && (*reopened)->get(1).has_value(), "store reopen failed");
    reopened->reset();
    std::filesystem::remove_all(directory);
}

void test_compaction_reuse_scan_and_exhaustion()
{
    const auto directory = temporary_directory("space");
    const auto path = directory / "collections" / "12.store";
    auto filesystem = filesystem::create_platform_filesystem();
    auto created = storage::StorageStore::create(path, filesystem, 12);
    require(created.has_value(), "space store create failed");
    auto store = std::move(*created);
    auto stable = store->insert({{common::Value {std::string(400, 'a')}}});
    require(stable.has_value(), "stable record insert failed");
    const auto initial_size = storage_file_size(path);
    for (std::size_t index = 0; index < 1000; ++index) {
        const auto length = index % 2 == 0 ? 700U : 150U;
        require(store->update(*stable, {{common::Value {std::string(length, 'u')}}}).has_value(), "repeated update failed");
    }
    require(storage_file_size(path) == initial_size, "repeated update increased the file size");

    std::vector<common::RecordId> ids;
    for (std::size_t index = 0; index < 80; ++index) {
        auto id = store->insert({{common::Value {std::string(180, 'r')}}});
        require(id.has_value(), "reuse fixture insert failed");
        ids.push_back(*id);
    }
    const auto populated_size = storage_file_size(path);
    for (const auto id : ids) require(store->erase(id).has_value(), "reuse fixture erase failed");
    for (std::size_t index = 0; index < ids.size(); ++index) {
        require(store->insert({{common::Value {std::string(180, 'n')}}}).has_value(), "reuse fixture reinsert failed");
    }
    require(storage_file_size(path) == populated_size, "deleted pages were not reused");

    auto cursor = store->scan();
    require(cursor.has_value(), "snapshot scan failed");
    store.reset();
    std::size_t scanned {0};
    while (true) {
        auto row = cursor->next();
        require(row.has_value(), "snapshot cursor failed after store destruction");
        if (!*row) break;
        ++scanned;
    }
    require(scanned == ids.size() + 1, "snapshot cursor row count mismatch");

    auto header = read_header(path);
    write_number(header.data() + 20, std::numeric_limits<common::RecordId>::max());
    reseal_header(header);
    write_header(path, header);
    auto exhausted = storage::StorageStore::open(path, filesystem, 12);
    require(exhausted.has_value(), "exhausted-id fixture did not open");
    auto rejected = (*exhausted)->insert({{common::Value {std::string {"x"}}}});
    require(!rejected, "record id exhaustion was not rejected");
    require_storage_error(
        rejected.error(),
        storage::StorageErrorCode::ResourceLimitExceeded,
        storage::StorageOperation::Insert,
        "record id exhaustion operation mismatch"
    );
    exhausted->reset();
    std::filesystem::remove_all(directory);
}

void test_corruption_rejection()
{
    const auto directory = temporary_directory("corruption");
    const auto base = directory / "collections" / "base.store";
    auto filesystem = filesystem::create_platform_filesystem();
    {
        auto created = storage::StorageStore::create(base, filesystem, 13);
        require(created.has_value(), "corruption fixture create failed");
        require((*created)->insert({{common::Value {std::int32_t {10}}}}).has_value(), "first corruption fixture insert failed");
        require((*created)->insert({{common::Value {std::int32_t {20}}}}).has_value(), "second corruption fixture insert failed");
    }

    auto copy_fixture = [&](std::string_view name) {
        const auto path = directory / "collections" / (std::string(name) + ".store");
        std::filesystem::copy_file(base, path);
        return path;
    };
    {
        const auto path = copy_fixture("old-version");
        auto header = read_header(path);
        write_number(header.data() + 4, std::uint16_t {0});
        reseal_header(header);
        write_header(path, header);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(!opened, "unsupported storage version was not rejected");
        require_storage_error(
            opened.error(),
            storage::StorageErrorCode::UnsupportedVersion,
            storage::StorageOperation::ReadHeader,
            "unsupported version operation mismatch"
        );
    }
    {
        const auto path = copy_fixture("header-crc");
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(100);
        const std::byte changed {0x7f};
        file.write(reinterpret_cast<const char *>(&changed), 1);
        file.close();
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(!opened, "header checksum corruption was not rejected");
        require_storage_error(
            opened.error(),
            storage::StorageErrorCode::ChecksumMismatch,
            storage::StorageOperation::ReadHeader,
            "header checksum operation mismatch"
        );
    }
    {
        const auto path = copy_fixture("page-crc");
        auto page = read_page(path, 1);
        page.back() ^= std::byte {1};
        write_page(path, 1, page);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(!opened, "page checksum corruption was not rejected");
        const auto & context = require_storage_error(
            opened.error(),
            storage::StorageErrorCode::ChecksumMismatch,
            storage::StorageOperation::ReadPage,
            "page checksum operation mismatch"
        );
        require(context.page_id == 1, "page checksum context mismatch");
    }
    {
        const auto path = copy_fixture("flags");
        auto page = read_page(path, 1);
        write_number(page.data() + 12, std::uint16_t {1});
        reseal_page(page);
        write_page(path, 1, page);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(!opened, "illegal page flags were not rejected");
        require_storage_error(
            opened.error(),
            storage::StorageErrorCode::CorruptedPage,
            storage::StorageOperation::ReadPage,
            "illegal page flags operation mismatch"
        );
    }
    {
        const auto path = copy_fixture("overlap");
        auto page = read_page(path, 1);
        const auto first_slot = PageHeaderSize;
        const auto second_slot = first_slot + SlotSize;
        write_number(page.data() + second_slot, read_number<std::uint16_t>(page.data() + first_slot));
        write_number(page.data() + second_slot + 2, read_number<std::uint16_t>(page.data() + first_slot + 2));
        reseal_page(page);
        write_page(path, 1, page);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(!opened, "overlapping payloads were not rejected");
        require_storage_error(
            opened.error(),
            storage::StorageErrorCode::CorruptedPage,
            storage::StorageOperation::ReadPage,
            "overlapping payload operation mismatch"
        );
    }
    {
        const auto path = copy_fixture("trailing");
        auto page = read_page(path, 1);
        const auto first_slot = PageHeaderSize;
        const auto offset = read_number<std::uint16_t>(page.data() + first_slot);
        write_number(page.data() + offset + 8, std::uint32_t {0});
        reseal_page(page);
        write_page(path, 1, page);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(opened.has_value(), "shallow load rejected a record-content corruption");
        auto record = (*opened)->get(1);
        require(!record, "get did not reject trailing record bytes");
        const auto & context = require_storage_error(
            record.error(),
            storage::StorageErrorCode::CorruptedPage,
            storage::StorageOperation::Decode,
            "trailing record operation mismatch"
        );
        require(context.record_id == 1 && context.page_id == 1 && context.slot_id == 0,
                "trailing record context mismatch");
        opened->reset();
    }
    {
        const auto path = copy_fixture("invalid-bool");
        auto page = read_page(path, 1);
        const auto first_offset = read_number<std::uint16_t>(page.data() + PageHeaderSize);
        page[first_offset + 12] = std::byte {0x01};
        page[first_offset + 13] = std::byte {0x02};
        reseal_page(page);
        write_page(path, 1, page);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(opened.has_value(), "shallow load rejected invalid boolean payload");
        auto record = (*opened)->get(1);
        require(!record, "non-canonical boolean was accepted");
        require_storage_error(
            record.error(),
            storage::StorageErrorCode::InvalidData,
            storage::StorageOperation::Decode,
            "invalid boolean operation mismatch"
        );
        opened->reset();
    }
    {
        const auto path = copy_fixture("oversized-vector");
        auto page = read_page(path, 1);
        const auto first_offset = read_number<std::uint16_t>(page.data() + PageHeaderSize);
        page[first_offset + 12] = std::byte {0x07};
        write_number(page.data() + first_offset + 13, std::numeric_limits<std::uint32_t>::max());
        reseal_page(page);
        write_page(path, 1, page);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(opened.has_value(), "shallow load rejected oversized vector payload");
        auto record = (*opened)->get(1);
        require(!record, "oversized vector count was accepted");
        require_storage_error(
            record.error(),
            storage::StorageErrorCode::ResourceLimitExceeded,
            storage::StorageOperation::Decode,
            "oversized vector operation mismatch"
        );
        opened->reset();
    }
    {
        const auto path = copy_fixture("duplicate");
        auto page = read_page(path, 1);
        const auto first_slot = PageHeaderSize;
        const auto second_slot = first_slot + SlotSize;
        const auto first_offset = read_number<std::uint16_t>(page.data() + first_slot);
        const auto second_offset = read_number<std::uint16_t>(page.data() + second_slot);
        write_number(page.data() + second_offset, read_number<std::uint64_t>(page.data() + first_offset));
        reseal_page(page);
        write_page(path, 1, page);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(!opened, "duplicate record IDs were not rejected");
        require_storage_error(
            opened.error(),
            storage::StorageErrorCode::InvalidFormat,
            storage::StorageOperation::Load,
            "duplicate record operation mismatch"
        );
    }
    {
        const auto path = copy_fixture("truncated");
        std::filesystem::resize_file(path, std::filesystem::file_size(path) - 1);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(!opened, "truncated storage page was not rejected");
        require_storage_error(
            opened.error(),
            storage::StorageErrorCode::InvalidFormat,
            storage::StorageOperation::Load,
            "truncated file operation mismatch"
        );
    }
    {
        const auto path = copy_fixture("truncated-header");
        std::filesystem::resize_file(path, PageSize - 1);
        auto opened = storage::StorageStore::open(path, filesystem, 13);
        require(!opened, "truncated storage header was not rejected");
        require_storage_error(
            opened.error(),
            storage::StorageErrorCode::UnexpectedEof,
            storage::StorageOperation::ReadHeader,
            "truncated header operation mismatch"
        );
    }
    std::filesystem::remove_all(directory);
}

void test_random_crud_model()
{
    const auto directory = temporary_directory("random");
    const auto path = directory / "collections" / "14.store";
    auto filesystem = filesystem::create_platform_filesystem();
    auto created = storage::StorageStore::create(path, filesystem, 14);
    require(created.has_value(), "random model store create failed");
    auto store = std::move(*created);
    std::map<common::RecordId, std::string> model;
    std::mt19937_64 random {0x5a17c0deULL};
    auto pick = [&]() {
        auto iterator = model.begin();
        std::advance(iterator, static_cast<std::ptrdiff_t>(random() % model.size()));
        return iterator;
    };
    for (std::size_t operation = 1; operation <= 10000; ++operation) {
        const auto choice = model.empty() ? 0 : random() % 100;
        if (choice < 6) {
            const auto value = "i-" + std::to_string(operation);
            auto id = store->insert({{common::Value {value}}});
            require(id.has_value(), "random insert failed");
            model.emplace(*id, value);
        } else if (choice < 12) {
            auto iterator = pick();
            const auto value = "u-" + std::to_string(operation) + std::string(static_cast<std::size_t>(random() % 80), 'x');
            require(store->update(iterator->first, {{common::Value {value}}}).has_value(), "random update failed");
            iterator->second = value;
        } else if (choice < 18) {
            auto iterator = pick();
            require(store->erase(iterator->first).has_value(), "random erase failed");
            model.erase(iterator);
        } else {
            auto iterator = pick();
            auto record = store->get(iterator->first);
            require(record && std::get<std::string>(record->data.values[0].data()) == iterator->second, "random get diverged from model");
        }
        if (operation % 1000 == 0) {
            store.reset();
            auto reopened = storage::StorageStore::open(path, filesystem, 14);
            require(reopened.has_value(), "periodic random-model reopen failed");
            store = std::move(*reopened);
            auto cursor = store->scan();
            require(cursor.has_value(), "random-model scan failed");
            std::size_t count {0};
            while (true) {
                auto next = cursor->next();
                require(next.has_value(), "random-model cursor failed");
                if (!*next) break;
                const auto expected = model.find((**next).id);
                require(expected != model.end() && std::get<std::string>((**next).data.values[0].data()) == expected->second, "random-model scan diverged");
                ++count;
            }
            require(count == model.size(), "random-model row count diverged");
        }
    }
    store.reset();
    std::filesystem::remove_all(directory);
}

} // namespace

int main()
{
    test_exact_format_and_all_values();
    test_compaction_reuse_scan_and_exhaustion();
    test_corruption_rejection();
    test_random_crud_model();
    return 0;
}
